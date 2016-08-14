#include <sys/types.h>
#include "master_ctrl.h"

#include "task.h"
#include "worker_manager.h"
#include "task_scheduler.h"

#include "tinyxml.h"

using namespace NS_LotteryBons;

extern CWorkerManager g_tWorkerManager;

CTaskScheduler::CTaskScheduler()
	:	m_iUpdateTaskInterval(15),m_iMaxTaskTime(3600)
{
}

CTaskScheduler::~CTaskScheduler()
{
	SaveScheInfo();
	SaveScheduledTask();
}

int32_t CTaskScheduler::Init(TiXmlElement* ptXmlEle)
{
#define XML_SUBELE_TO_STRING(ptEle,eleName,outStr) \
	TiXmlElement* pSub##eleName##Ele = ptEle->FirstChildElement(#eleName);\
	if(NULL != pSub##eleName##Ele && NULL != pSub##eleName##Ele->GetText())\
	outStr.assign(pSub##eleName##Ele->GetText())
#define XML_SUBELE_TO_INT(ptEle,eleName,outInt) \
	TiXmlElement* pSub##eleName##Ele = ptEle->FirstChildElement(#eleName);\
	if(NULL != pSub##eleName##Ele && NULL != pSub##eleName##Ele->GetText())\
	outInt = atoi(pSub##eleName##Ele->GetText())

	XML_SUBELE_TO_STRING(ptXmlEle,scheduled_task,m_strScheTaskFile);
	XML_SUBELE_TO_STRING(ptXmlEle,scheduled_info,m_strScheInfoFile);

	if(LoadScheInfo())
	{
		ERR("CTaskScheduler LoadScheInfo failed when init\n");
		return -1;
	}
	if(LoadScheduledTask())
	{
		ERR("CTaskScheduler LoadScheduledTask failed when init\n");
		return -1;
	}
	return 0;
}

int32_t CTaskScheduler::OnTick(const timeval* ptTick)
{
	int32_t iRet = 0;
	static time_t sLastSchedTime = time(NULL);

	if(sLastSchedTime + m_iUpdateTaskInterval > ptTick->tv_sec)
	{
		return 0;
	}

    sLastSchedTime = ptTick->tv_sec;

	for (std::list<TaskInfo>::iterator fIt = m_stdTaskList.begin();
		fIt != m_stdTaskList.end();
		/*fIt++*/)
	{
		if(CheckTask(*fIt) > 0)
		{
			std::list<TaskInfo>::iterator dIt = fIt++;
			m_stdTaskList.erase(dIt);
			iRet++;
		}
		else
		{
			fIt++;
		}
	}

	return iRet;
}

int32_t CTaskScheduler::Push(CTask* ptTask)
{
	TaskInfo tTaskInfo;

	tTaskInfo.tTask = *ptTask;
	tTaskInfo.stdScheVec = m_stdSchedTaskInfoMap[ptTask->iTaskID];

	if(tTaskInfo.stdScheVec.size() == 0 || tTaskInfo.stdScheVec.size() == ptTask->iDenoMod)
	{
		return ScheTask(tTaskInfo);
	}

	ERR("Push task failed for task denomod[%d] and scheinfo[%ld] error\n", ptTask->iDenoMod, tTaskInfo.stdScheVec.size());
	return -1;
}

int32_t CTaskScheduler::Pop(CTask* ptTask)
{
	if(m_stdSchedTaskList.size() == 0)
	{
		return -1;
	}

	TaskInfo& tTaskInfo = m_stdSchedTaskList.front();
	*ptTask = tTaskInfo.tTask;
	ptTask->iTaskState = ((tTaskInfo.iFailWorker == 0)?(CTask::em_success_state):(CTask::em_failed_state));

	m_stdSchedTaskList.pop_front();
	return 0;
}

int32_t CTaskScheduler::Terminate(CTask* ptTask)
{
	for (std::list<TaskInfo>::iterator fIt = m_stdTaskList.begin();
		fIt != m_stdTaskList.end();
		fIt++)
	{
		if(fIt->tTask.iTaskID == ptTask->iTaskID)
		{
			for(int64_t idx=0;idx<fIt->stdScheVec.size();idx++)
			{
				g_tWorkerManager.ReqStopTask(fIt->stdScheVec[idx],&fIt->tTask);
			}
			fIt->iFailWorker = fIt->tTask.iDenoMod;
			m_stdSchedTaskList.push_back(*fIt);
			m_stdTaskList.erase(fIt);
		}
	}

	return 0;
}

int32_t CTaskScheduler::CheckTask(TaskInfo& tTaskInfo)
{
	std::vector<WORKERUID> &scheInfo = tTaskInfo.stdScheVec;
	for (int64_t idx = 0 ; idx < scheInfo.size() ; idx++)
	{
		if (scheInfo[idx].first == 0 || scheInfo[idx].second == 0)
		{
			continue;
		}

		const WorkerInfo& workerInfo = g_tWorkerManager.GetWorkerInfo(scheInfo[idx]);

		FORCE_WARN("check task, worker[%d] state [%d] scheduleTime[%d]", idx, workerInfo.iState,workerInfo.uAllocTimes);
		
		switch(workerInfo.iState)
		{
		case em_idle_state:
            {
			    // 空闲了，将任务下发给worker
			    tTaskInfo.tTask.iFracMod = idx;
			    int32_t iRet = g_tWorkerManager.ReqSetTask(scheInfo[idx],&tTaskInfo.tTask);
                FORCE_WARN("CheckTask shec task[%d] mod[%d%%%d] in queue to worker[%u:%u] in %d\n",tTaskInfo.tTask.iTaskID,
                        tTaskInfo.tTask.iFracMod, tTaskInfo.tTask.iDenoMod, scheInfo[idx].first, scheInfo[idx].second, iRet);
            }
            break;
		case em_work_finished:
			// 任务完成了，释放worker，并将waitting数组内worker信息清除
			if(workerInfo.tTask.iTaskID == tTaskInfo.tTask.iTaskID && workerInfo.tTask.iFracMod == idx)
			{
				if(workerInfo.tTask.iTaskState == CTask::em_success_state)
				{
					tTaskInfo.iSuccWorker++;
                    FORCE_WARN("Task[%d] mod[%d%%%d] done success.WORKER IP[%d]  PORT[%d]\n",tTaskInfo.tTask.iTaskID,
						workerInfo.tTask.iFracMod, workerInfo.tTask.iDenoMod, scheInfo[idx].first, scheInfo[idx].second);
				}
				else
				{
					tTaskInfo.iFailWorker++;
                    FORCE_WARN("Task[%d] mod[%d%%%d] done failed. WORKER IP[%d]  PORT[%d]\n",tTaskInfo.tTask.iTaskID,
						workerInfo.tTask.iFracMod, workerInfo.tTask.iDenoMod, scheInfo[idx].first, scheInfo[idx].second);
				}
				g_tWorkerManager.ResetWorker(scheInfo[idx]);
				scheInfo[idx].first = 0;
				scheInfo[idx].second = 0;
			}
            break;
		case em_init_state:
		case em_work_doing:
			break;
		case em_dead_state:
		default:
			// worker死了，找个新的且空闲的重做，且更新调度信息
			WORKERUID newWorker = g_tWorkerManager.GetIdleWorker();
			if(newWorker.first != 0 && newWorker.second != 0)
			{
				tTaskInfo.tTask.iFracMod = idx;
				int32_t iRet = g_tWorkerManager.ReqSetTask(scheInfo[idx],&tTaskInfo.tTask);
				m_stdSchedTaskInfoMap[tTaskInfo.tTask.iTaskID][idx] = newWorker;
				scheInfo[idx] = newWorker;
				SaveScheInfo();
				FORCE_WARN("Primary Worker dead,Schedule info changed. Task[%d] mod[%d%%%d] scheduled to worker[%u:%u] in %d\n", tTaskInfo.tTask.iTaskID,
					tTaskInfo.tTask.iFracMod, tTaskInfo.tTask.iDenoMod, scheInfo[idx].first, scheInfo[idx].second, iRet);

			}
			break;
		}
	}
	if(tTaskInfo.iSuccWorker + tTaskInfo.iFailWorker == scheInfo.size())
	{
		FORCE_WARN("Check Task LotyName[%s] Qihao[%d] TaskType[%d] done in succ[%d] fail[%d]\n",tTaskInfo.tTask.strLotteryName.c_str(),tTaskInfo.tTask.iQihaoBegin, 
			tTaskInfo.tTask.iTaskType,tTaskInfo.iSuccWorker,tTaskInfo.iFailWorker);
			m_stdSchedTaskList.push_back(tTaskInfo);
		return 1;
	}

	return 0;
}

int32_t CTaskScheduler::ScheTask(TaskInfo& tTaskInfo)
{
	if(tTaskInfo.stdScheVec.size() == 0)//如果之前没有此任务ID的调度信息，找空闲的worker来分配任务
	{
		for(int32_t idx=0;idx<tTaskInfo.tTask.iDenoMod;idx++)
		{
			WORKERUID newWorker = g_tWorkerManager.GetIdleWorker();
			if(newWorker.first == 0 || newWorker.second == 0)
			{
				newWorker = g_tWorkerManager.GetWorker();
			}

			tTaskInfo.stdScheVec.push_back(newWorker);
		}
		m_stdSchedTaskInfoMap[tTaskInfo.tTask.iTaskID] = tTaskInfo.stdScheVec;
		SaveScheInfo();
	}
	
	for(int64_t idx=0;idx<tTaskInfo.stdScheVec.size();idx++)
	{
		tTaskInfo.tTask.iFracMod = idx;
		int32_t iRet = g_tWorkerManager.ReqSetTask(tTaskInfo.stdScheVec[idx],&tTaskInfo.tTask);
		FORCE_WARN("SET Task ID[%d] FracMod[%d] LotyName[%s] TaskType[%d] To Worker[%u:%u] in %d\n", tTaskInfo.tTask.iTaskID,
			tTaskInfo.tTask.iFracMod,tTaskInfo.tTask.strLotteryName.c_str(), tTaskInfo.tTask.iTaskType, tTaskInfo.stdScheVec[idx].first, tTaskInfo.stdScheVec[idx].second, iRet);
	}

	m_stdTaskList.push_back(tTaskInfo);
    WARN("Sche Task[%d] begin\n",tTaskInfo.tTask.iTaskID);
	return 0;
}

int32_t CTaskScheduler::SaveScheduledTask()
{
	TiXmlDocument xmlConfig(m_strScheTaskFile.c_str());
	TiXmlDeclaration *dec = new TiXmlDeclaration("1.0","utf-8","");
	TiXmlElement *xmlRootNode = new TiXmlElement("task_scheduled");
	xmlConfig.LinkEndChild(dec);
	xmlConfig.LinkEndChild(xmlRootNode);

	for(std::list<TaskInfo>::iterator lIt = m_stdTaskList.begin();
		lIt != m_stdTaskList.end();
		lIt++)
	{
		TiXmlElement *xmlTaskNode = new TiXmlElement("task_info");
		lIt->tTask.ToXmlNode(xmlTaskNode);
		xmlRootNode->LinkEndChild(xmlTaskNode);
	}

	for(std::list<TaskInfo>::iterator lIt = m_stdSchedTaskList.begin();
		lIt != m_stdSchedTaskList.end();
		lIt++)
	{
		TiXmlElement *xmlTaskNode = new TiXmlElement("task_info");
		lIt->tTask.ToXmlNode(xmlTaskNode);
		xmlRootNode->LinkEndChild(xmlTaskNode);
	}

	return xmlConfig.SaveFile();
}

int32_t CTaskScheduler::LoadScheduledTask()
{
	TiXmlDocument xmlConfig;
	xmlConfig.LoadFile(m_strScheTaskFile.c_str());
	TiXmlElement *xmlRootNode = xmlConfig.RootElement();
	if(NULL == xmlRootNode)
	{
		return 0;
	}

	for(TiXmlElement *ptTaskNode = xmlRootNode->FirstChildElement("task_info");
		ptTaskNode != NULL;
		ptTaskNode = ptTaskNode->NextSiblingElement("task_info"))
	{
		TaskInfo tTaskInfo;
		tTaskInfo.tTask.FromXmlNode(ptTaskNode);
		tTaskInfo.stdScheVec = m_stdSchedTaskInfoMap[tTaskInfo.tTask.iTaskID];
		if(tTaskInfo.tTask.iTaskState == CTask::em_success_state || tTaskInfo.tTask.iTaskState == CTask::em_failed_state)
		{
			m_stdSchedTaskList.push_back(tTaskInfo);
		}
		else
		{
			m_stdTaskList.push_back(tTaskInfo);
		}
	}

	return 0;
}

int32_t CTaskScheduler::SaveScheInfo()
{
	TiXmlDocument xmlConfig(m_strScheInfoFile.c_str());
	TiXmlDeclaration *dec = new TiXmlDeclaration("1.0","utf-8","");
	TiXmlElement *xmlRootNode = new TiXmlElement("task_sche_info");
	xmlConfig.LinkEndChild(dec);
	xmlConfig.LinkEndChild(xmlRootNode);

	for(std::map<int32_t,std::vector<WORKERUID> >::iterator bIt = m_stdSchedTaskInfoMap.begin();
		bIt != m_stdSchedTaskInfoMap.end();
		bIt++)
	{
		TiXmlElement *xmlScheInfo = new TiXmlElement("sche_info");
		xmlScheInfo->SetAttribute("task_id",bIt->first);
		xmlScheInfo->SetAttribute("sche_num",bIt->second.size());

		std::vector<WORKERUID> &scheInfo = bIt->second;
		for(int32_t idx = 0 ; idx < scheInfo.size() ; idx++)
		{
			TiXmlElement *xmlScheNode = new TiXmlElement("sche_node");
			xmlScheNode->SetAttribute("node_id",idx);
			xmlScheNode->SetAttribute("uid_first",(int32_t)scheInfo[idx].first);
			xmlScheNode->SetAttribute("uid_second",(int32_t)scheInfo[idx].second);
			xmlScheInfo->LinkEndChild(xmlScheNode);
		}
		
		xmlRootNode->LinkEndChild(xmlScheInfo);
	}

	return xmlConfig.SaveFile();
}

int32_t CTaskScheduler::LoadScheInfo()
{
	TiXmlDocument xmlConfig;
	xmlConfig.LoadFile(m_strScheInfoFile.c_str());
	TiXmlElement *xmlRootNode = xmlConfig.RootElement();
	if(NULL == xmlRootNode)
	{
		return 0;
	}

	for(TiXmlElement *ptScheNode = xmlRootNode->FirstChildElement("sche_info");
		ptScheNode != NULL;
		ptScheNode = ptScheNode->NextSiblingElement("sche_info"))
	{
		int32_t taskID = 0,scheNum = 0;
		ptScheNode->Attribute("task_id",&taskID);
		ptScheNode->Attribute("sche_num",&scheNum);
		if(taskID != 0 && scheNum > 0)
		{
			std::vector<WORKERUID> &scheInfo = m_stdSchedTaskInfoMap[taskID];
			scheInfo.resize(scheNum,WORKERUID(0,0));

			for(TiXmlElement *ptSche = ptScheNode->FirstChildElement("sche_node");
				ptSche != NULL;
				ptSche = ptSche->NextSiblingElement("sche_node"))
			{
				int32_t node_id = 0, uid_first = 0, uid_second = 0;
				ptSche->Attribute("node_id", &node_id);
				ptSche->Attribute("uid_first", &uid_first);
				ptSche->Attribute("uid_second", &uid_second);
				scheInfo[node_id] = WORKERUID((uint32_t)uid_first,(uint32_t)uid_second);
			}
		}
	}

	return 0;
}


