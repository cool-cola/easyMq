#include "master_ctrl.h"

#include "task.h"
#include "worker_manager.h"

#include "tinyxml.h"
#include "asn-incl.h"
#include "lottery_bons_pkt.h"

using namespace NS_LotteryBons;

const WORKERUID invaildUID(0,0);
const WorkerInfo invaildInfo;

extern CMasterCtrl g_tMasterCtrl;
extern void ConvertAsnTaskToCTask(const NS_LotteryBons::TASK* pAsnTask, NS_LotteryBons::CTask* ptTask);

CWorkerManager::CWorkerManager()
	:	m_iHeartBeatInterval(3),m_iAliveToDeadTime(20),m_iSeqID(0)
{
}

CWorkerManager::~CWorkerManager()
{
}

int32_t CWorkerManager::Init(const char* pszConf)
{
	// TODO 加载文件配置
	if (NULL == pszConf)
	{
		fprintf(stderr, "Load Conf failed in CWorkerManager Init.pszConf is NULL\n");
		return -1;
	}
	TiXmlDocument xmlConfig;
	xmlConfig.LoadFile(pszConf);
	TiXmlElement *xmlRootNode = xmlConfig.RootElement();
	if (xmlRootNode == NULL)
	{
		fprintf(stderr,"Load %s failed in CWorkerManager Init\n",pszConf);
		return -1;
	}

	TiXmlElement* xmlWorkNode = xmlRootNode->FirstChildElement("worker_configure");
	if(xmlWorkNode == NULL)
	{
		fprintf(stderr,"Load %s failed in CWorkerManager Init\n",pszConf);
		return -1;
	}

	xmlWorkNode->Attribute("heart_beat_interval",&m_iHeartBeatInterval);
	xmlWorkNode->Attribute("alive_to_dead_time",&m_iAliveToDeadTime);

	for(TiXmlElement* ptWorkerXml = xmlWorkNode->FirstChildElement("worker");
		ptWorkerXml != NULL;
		ptWorkerXml = ptWorkerXml->NextSiblingElement("worker"))
	{
		TiXmlElement* xmlIPNode = ptWorkerXml->FirstChildElement("ip");
		TiXmlElement* xmlPortNode = ptWorkerXml->FirstChildElement("port");
		if (NULL != xmlIPNode && NULL != xmlIPNode->GetText()
		 && NULL != xmlPortNode && NULL != xmlPortNode->GetText())
		{
			WORKERUID workerUID((uint32_t)ntohl(inet_addr(xmlIPNode->GetText())),(uint32_t)atoi(xmlPortNode->GetText()));
			m_stdWorkerMap[workerUID].iLastAckTime = (int32_t)time(NULL);
			// master启动时，尝试拉取所有的worker的工作任务
			ReqQueryTask(workerUID);
		}
	}

	return 0;
}

int32_t CWorkerManager::OnTick(const timeval* ptTick)
{
	static time_t stLastTickTime = time(NULL);
	if (stLastTickTime + m_iHeartBeatInterval >= ptTick->tv_sec)
	{
		return 0;
	}
	stLastTickTime = ptTick->tv_sec;
	
	for(std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.begin();
		fIt != m_stdWorkerMap.end();
		fIt++)
	{
		// 存活判断
		if(fIt->second.iLastAckTime + m_iAliveToDeadTime < ptTick->tv_sec)
		{
			fIt->second.iState = em_dead_state;
			fIt->second.tTask.Reset();
			FORCE_WARN("Worker [%u:%u] dead!",fIt->first.first, fIt->first.second);
		}

		switch(fIt->second.iState)
		{
		case em_dead_state:
			// 每隔m_iHeartBeatInterval时间，尝试心跳激活
			if(fIt->second.iLastAckTime + m_iHeartBeatInterval < ptTick->tv_sec)
			{
				fIt->second.iLastAckTime = ptTick->tv_sec;
				ReqHeartBeat(fIt->first);
			}
			break;
		case em_idle_state:
		case em_work_finished:
			// 保持心跳
			if(fIt->second.iLastAckTime + m_iHeartBeatInterval < ptTick->tv_sec)
			{
				ReqHeartBeat(fIt->first);
			}
			break;
		case em_init_state:
		case em_work_doing:
			// 保持查询任务
			if(fIt->second.iLastAckTime + m_iHeartBeatInterval < ptTick->tv_sec)
			{
				ReqQueryTask(fIt->first);
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

WORKERUID CWorkerManager::GetWorker()
{
	uint32_t uMaxAllocTimes = (uint32_t)(-1);
	WORKERUID retUID = invaildUID;
	std::map<WORKERUID,WorkerInfo>::iterator selectIt = m_stdWorkerMap.end();
	for(std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.begin();
		fIt != m_stdWorkerMap.end();
		fIt++)
	{
		if(uMaxAllocTimes > fIt->second.uAllocTimes)
		{
			uMaxAllocTimes = fIt->second.uAllocTimes;
			selectIt = fIt;
		}
	}
	if(selectIt != m_stdWorkerMap.end())
	{
		selectIt->second.uAllocTimes++;
		retUID = selectIt->first;
	}
	FORCE_WARN("Get Worker workerIP[%d]  workerPort[%d] workerAllocTimes[%d]", selectIt->first.first, selectIt->first.second, selectIt->second.uAllocTimes);
	return retUID;
}

WORKERUID CWorkerManager::GetIdleWorker()
{
	uint32_t uMaxAllocTimes = (uint32_t)(-1);
	WORKERUID retUID = invaildUID;
	std::map<WORKERUID,WorkerInfo>::iterator selectIt = m_stdWorkerMap.end();
	for(std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.begin();
		fIt != m_stdWorkerMap.end();
		fIt++)
	{
		if(uMaxAllocTimes > fIt->second.uAllocTimes && fIt->second.iState == em_idle_state)
		{
			uMaxAllocTimes = fIt->second.uAllocTimes;
			selectIt = fIt;
		}
	}
	if(selectIt != m_stdWorkerMap.end())
	{
		selectIt->second.uAllocTimes++;
		retUID = selectIt->first;
	}
	FORCE_WARN("Get Idle Worker workerIP[%d]  workerPort[%d] workerAllocTimes[%d]", selectIt->first.first, selectIt->first.second, selectIt->second.uAllocTimes);
	return retUID;
}

const WorkerInfo& CWorkerManager::GetWorkerInfo(const WORKERUID& workerUID) const
{
	std::map<WORKERUID,WorkerInfo>::const_iterator fIt = m_stdWorkerMap.find(workerUID);
	if(fIt != m_stdWorkerMap.end())
	{
		return fIt->second;
	}
	return invaildInfo;
}

int32_t CWorkerManager::ResetWorker(const WORKERUID& workerUID)
{
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUID);
	if(fIt != m_stdWorkerMap.end())
	{
		switch(fIt->second.iState)
		{
		case em_work_doing:
			ReqStopTask(workerUID,&fIt->second.tTask);
			break;
		case em_work_finished:
			fIt->second.iState = em_idle_state;
			fIt->second.tTask.Reset();
			break;
		case em_dead_state:
		case em_idle_state:
		case em_init_state:
		default:
			break;
		}
	}
	return 0;
}

int32_t CWorkerManager::ReqHeartBeat(const WORKERUID& workerUID)
{
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUID);
	if(fIt == m_stdWorkerMap.end())
	{
		return -1;
	}

	NS_LotteryBons::LotteryBonsMessage reqMsg;
	ReqLBWorkerHeartBeat* ptReqLBWorkerHeartBeat = NULL;

	MAKE_LOTTERYBONS_ASN_REQ(reqMsg,ptReqLBWorkerHeartBeat,LBWorkerHeartBeat,m_iSeqID++);
	ptReqLBWorkerHeartBeat->hbMod = 0;

	return g_tMasterCtrl.SendReq(workerUID.first,workerUID.second,&reqMsg);
}

int32_t CWorkerManager::ReqQueryTask(const WORKERUID& workerUID)
{
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUID);
	if(fIt == m_stdWorkerMap.end())
	{
		return -1;
	}

	NS_LotteryBons::LotteryBonsMessage reqMsg;
	ReqLBWorkerQueryTask* ptReqLBWorkerQueryTask = NULL;

	MAKE_LOTTERYBONS_ASN_REQ(reqMsg,ptReqLBWorkerQueryTask,LBWorkerQueryTask,m_iSeqID++);

	INFO("query worker ip %u, port %u",workerUID.first, workerUID.second);
	return g_tMasterCtrl.SendReq(workerUID.first,workerUID.second,&reqMsg);
}

int32_t CWorkerManager::ReqSetTask(const WORKERUID& workerUID, const CTask* ptTask)
{
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUID);
	if(fIt == m_stdWorkerMap.end() || fIt->second.iState != em_idle_state)
	{
		INFO("task not send with worker state %d", fIt->second.iState);
		return -1;
	}
	fIt->second.iState = em_work_doing;
	//fIt->second.uAllocTimes++;
	fIt->second.tTask = *ptTask;

	NS_LotteryBons::LotteryBonsMessage reqMsg;
	ReqLBWorkerSetTask* ptReqLBWorkerSetTask = NULL;

	MAKE_LOTTERYBONS_ASN_REQ(reqMsg,ptReqLBWorkerSetTask,LBWorkerSetTask,m_iSeqID++);
	ptReqLBWorkerSetTask->taskInfo = new TASK;
	ptTask->ToAsnTask(ptReqLBWorkerSetTask->taskInfo);

	INFO("task send with id %d, type %d", ptTask->iTaskID, ptTask->iTaskType);
	return g_tMasterCtrl.SendReq(workerUID.first,workerUID.second,&reqMsg);
}

int32_t CWorkerManager::ReqStopTask(const WORKERUID& workerUID, const CTask* ptTask)
{
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUID);
	if(fIt == m_stdWorkerMap.end() || fIt->second.iState != em_work_doing || fIt->second.tTask.iTaskID != ptTask->iTaskID)
	{
		return -1;
	}

	NS_LotteryBons::LotteryBonsMessage reqMsg;
	ReqLBWorkerStopTask* ptReqLBWorkerStopTask = NULL;

	MAKE_LOTTERYBONS_ASN_REQ(reqMsg,ptReqLBWorkerStopTask,LBWorkerStopTask,m_iSeqID++);
	ptReqLBWorkerStopTask->taskInfo = new TASK;
	ptTask->ToAsnTask(ptReqLBWorkerStopTask->taskInfo);

	return g_tMasterCtrl.SendReq(workerUID.first,workerUID.second,&reqMsg);
}

int32_t CWorkerManager::RspHeartBeat(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg)
{
	WORKERUID workerUid(ntohl(pMQHeadInfo->m_unClientIP),pMQHeadInfo->m_usClientPort);
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUid);
	if(fIt != m_stdWorkerMap.end())
	{
		fIt->second.iLastAckTime = g_pFrameCtrl->m_tNow.tv_sec;
		if(em_dead_state == fIt->second.iState)
		{
			fIt->second.tTask.Reset();
			fIt->second.iState = em_idle_state;
			FORCE_WARN("Worker IP[%u] PORT[%u] restart!\n",
				ntohl(pMQHeadInfo->m_unClientIP), pMQHeadInfo->m_usClientPort);
		}
	}

	return 0;
}

int32_t CWorkerManager::RspQueryTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg)
{
	WORKERUID workerUid(ntohl(pMQHeadInfo->m_unClientIP),pMQHeadInfo->m_usClientPort);
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUid);
	if(fIt == m_stdWorkerMap.end())
	{
		return 0;
	}

	fIt->second.iLastAckTime = g_pFrameCtrl->m_tNow.tv_sec;
	RspLBWorkerQueryTask *ptRspLBWorkerQueryTask = ptRspMsg->body->rspLBWorkerQueryTask;

	// 更新查询的任务信息
	switch(fIt->second.iState)
	{
	case em_work_doing:
		// 查询失败，将任务置为失败
		if(0 != ptRspLBWorkerQueryTask->retCode)
		{
		    ERR("query ret error %d", ptRspLBWorkerQueryTask->retCode);
			fIt->second.tTask.iTaskState = CTask::em_failed_state;
		}
		else
		{
			// 正在做的任务，需对比任务信息
			TASK *pAsnTask = ptRspLBWorkerQueryTask->taskInfo;
			if (fIt->second.tTask.iTaskID != (int32_t)pAsnTask->taskID
				|| fIt->second.tTask.iTaskType != (int32_t)pAsnTask->taskType
				|| fIt->second.tTask.iFracMod != (int32_t)pAsnTask->fracMod
				|| fIt->second.tTask.iDenoMod != (int32_t)pAsnTask->denoMod)
			{
				fIt->second.tTask.iTaskState = CTask::em_failed_state;
				FORCE_WARN("Check Task[%d] mod[%d%%%d] info failed as Task[%d] mod[%d%%%d] in [%d:%d]\n",
						fIt->second.tTask.iTaskID, fIt->second.tTask.iFracMod, fIt->second.tTask.iDenoMod,
						(int32_t)pAsnTask->taskID, (int32_t)pAsnTask->fracMod, (int32_t)pAsnTask->denoMod,
						workerUid.first, workerUid.second);
			}
			else
			{
				fIt->second.tTask.iTaskState = (int32_t)pAsnTask->taskState;
			}
			// 任务结束，将状态置为完成
			if(fIt->second.tTask.iTaskState == CTask::em_success_state || fIt->second.tTask.iTaskState == CTask::em_failed_state)
			{
				fIt->second.iState = em_work_finished;
			}
		}
		break;
	case em_init_state:
		{
			// 找回的任务信息，如果任务有效，则将worker置为工作状态
			CTask tTask;
			tTask.FromAsnTask(ptRspLBWorkerQueryTask->taskInfo);
			if(tTask.iTaskID < 0)
			{
				fIt->second.iState = em_idle_state;
			}
			else
			{
				fIt->second.iState = em_work_doing;
				fIt->second.tTask = tTask;
			}
		}
		break;
	case em_idle_state:
	case em_dead_state:
	case em_work_finished:
	default:
		break;
	}

	return 0;
}

int32_t CWorkerManager::RspSetTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg)
{
	WORKERUID workerUid(ntohl(pMQHeadInfo->m_unClientIP),pMQHeadInfo->m_usClientPort);
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUid);
	if(fIt == m_stdWorkerMap.end() || fIt->second.iState != em_work_doing)
	{
		return 0;
	}

	fIt->second.iLastAckTime = g_pFrameCtrl->m_tNow.tv_sec;
	RspLBWorkerSetTask *ptRspLBWorkerSetTask = ptRspMsg->body->rspLBWorkerSetTask;

	// set失败，将任务置为失败
	if(0 != ptRspLBWorkerSetTask->retCode)
	{
	    ERR("set task failed %d", ptRspLBWorkerSetTask->retCode);
		fIt->second.tTask.iTaskState = CTask::em_failed_state;
		fIt->second.iState = em_work_finished;
	}

	return 0;
}

int32_t CWorkerManager::RspStopTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg)
{
	WORKERUID workerUid(ntohl(pMQHeadInfo->m_unClientIP),pMQHeadInfo->m_usClientPort);
	std::map<WORKERUID,WorkerInfo>::iterator fIt = m_stdWorkerMap.find(workerUid);
	if(fIt == m_stdWorkerMap.end() || fIt->second.iState != em_work_doing)
	{
		return 0;
	}

	fIt->second.iLastAckTime = g_pFrameCtrl->m_tNow.tv_sec;
	RspLBWorkerStopTask *ptRspLBWorkerStopTask = ptRspMsg->body->rspLBWorkerStopTask;

	// worker停止任务了，将worker状态置为idle
	fIt->second.iState = em_idle_state;
	fIt->second.tTask.Reset();

	return 0;
}
