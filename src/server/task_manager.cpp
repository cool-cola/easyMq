#include "frame_ctrl.h"
#include "master_ctrl.h"

#include "task.h"
#include "worker_manager.h"
#include "task_manager.h"

#include "tinyxml.h"

using namespace NS_LotteryBons;

CTaskManager::CTaskManager()
	:	m_uDBPort(0)
{
	m_szDbCmd[0] = 0;
}

CTaskManager::~CTaskManager()
{
}

int32_t CTaskManager::Init(const char* pszConf)
{
	TiXmlDocument xmlConfig;
	xmlConfig.LoadFile(pszConf);
	TiXmlElement *xmlRootNode = xmlConfig.RootElement();
	if (xmlRootNode == NULL)
	{
		fprintf(stderr,"Load %s failed in CTaskManager Init\n",pszConf);
		return -1;
	}

	TiXmlElement *xmlDBNode = xmlRootNode->FirstChildElement("db_configure");
	TiXmlElement *xmlDetectorNode = xmlRootNode->FirstChildElement("detector_configure");
	TiXmlElement *xmlSchedulerNode = xmlRootNode->FirstChildElement("scheduler_configure");
	if(xmlDBNode == NULL || xmlDetectorNode == NULL || xmlSchedulerNode == NULL)
	{
		fprintf(stderr,"Load %s failed in CTaskManager Init\n",pszConf);
		return -1;
	}
    
	if(InitMysql(xmlDBNode))
	{
		fprintf(stderr,"InitMysql failed\n");
		return -1;
	}
	
	if(m_tTaskDetector.Init(xmlDetectorNode))
	{
		fprintf(stderr,"TaskDetector Load configure failed\n");
		return -1;
	}
	if(m_tTaskScheduler.Init(xmlSchedulerNode))
	{
		fprintf(stderr,"TaskScheduler Load configure failed\n");
		return -1;
	}

	return LoadTaskFromDB();
}

int32_t CTaskManager::OnTick(const timeval* ptTick)
{
	if(m_tTaskScheduler.OnTick(ptTick) > 0)
	{
		OnTaskSchedComplete();
	}

	if (m_tTaskDetector.OnTick(ptTick) > 0)
	{
		OnTaskDetectFound();
	}

	return 0;
}

int32_t CTaskManager::DoTask(CTask* ptTask)
{
	if(NULL == ptTask)
	{
		return -1;
	}

	// 新增任务
	if(ptTask->iTaskID < 0)
	{
		// 新增任务到DB，ptTask中会填写taskID
		if(AddTaskToDB(ptTask) != 0)
		{
			return -1;
		}
		// 将新增任务加入到检测中
		return AddTaskToDetector(ptTask);
	}
	
	// 非新增任务，直接加入到调度中，跳过检测器触发
	AddTaskToScheduler(ptTask);

	return 0;
}

int32_t CTaskManager::OnTaskSchedComplete()
{
	CTask tTask;
	while (m_tTaskScheduler.Pop(&tTask) == 0)
	{
		switch(tTask.iTaskState)
		{
		case CTask::em_success_state:
		case CTask::em_failed_state:
			{
				WARN("task[%d] step[%d] scheduled in state[%d]\n",tTask.iTaskID,tTask.iTaskType,tTask.iTaskState);
			    // 写入数据库
				if(UpdateMysql(&tTask) != 0)
				{
					WARN("task[%d] step[%d] update mysql failed\n",tTask.iTaskID,tTask.iTaskType);
				}

                // 继续加入到detector中
				AddTaskToDetector(&tTask);
			}
			break;
		case CTask::em_waitting_state:
		case CTask::em_doing_state:
		default:
			ERR("task[%d] unknown state[%d] in step[%d]\n", tTask.iTaskID, tTask.iTaskState, tTask.iTaskType);
			break;
		}
	}
	return 0;
}

int32_t CTaskManager::OnTaskDetectFound()
{
	CTask tTask;
	while (m_tTaskDetector.Pop(&tTask) == 0)
	{
	    if(tTask.iTaskID < 0)
	    {
            if(0 != AddTaskToDB(&tTask))
            {
                continue;
            }
	    }
	    
		AddTaskToScheduler(&tTask);
	}
	return 0;
}

int32_t CTaskManager::AddTaskToDB(CTask* ptTask)
{
	// 检查是否有任务重复
	snprintf(m_szDbCmd,sizeof(m_szDbCmd),"select `task_id`, `pay_step`, `pay_state` from `%s` "
		"where `qihao_begin`=%d and `lottery_name`='%s' ;\n"
		,m_strTableName.c_str(),ptTask->iQihaoBegin, ptTask->strLotteryName.c_str());

	INFO("AddTaskToDB , sql %s", m_szDbCmd);
	if(m_tDbConn.ExecSelect(m_szDbCmd) != 0)
	{
		ERR("AddTask FUNC do[%s] failed in %s\n", m_szDbCmd, m_tDbConn.GetErrMsg());
		return -1;
	}

	//生成派奖任务时，该任务需要在DB里已经存在
    if (m_tDbConn.GetResNum() > 0)
    {
        if (m_tDbConn.FetchRow() == 0
         && m_tDbConn.m_stRow[0] != NULL
         && m_tDbConn.m_stRow[1] != NULL
         && m_tDbConn.m_stRow[2] != NULL)
        {
            int32_t iTaskId = atoi(m_tDbConn.m_stRow[0]);
            int32_t iTaskType = atoi(m_tDbConn.m_stRow[1]);
            int32_t iTaskState = atoi(m_tDbConn.m_stRow[2]);

            if(iTaskType != 0 || iTaskState != 0)
            {
                //任务已经在做
                INFO("task already doing");
                return -1;
            }
            else
            {
                // 更新任务状态
                snprintf(m_szDbCmd, sizeof(m_szDbCmd), "update `%s` set `pay_step` = %d, `pay_state` = %d, `retry_times` = %d where `qihao_begin` = %d and `lottery_name` = '%s' ",
                            m_strTableName.c_str(), ptTask->iTaskType, ptTask->iTaskState, ptTask->iMaxRetryTimes, ptTask->iQihaoBegin, ptTask->strLotteryName.c_str());

                INFO("update paicai task , sql %s", m_szDbCmd);
                int32_t iInsertID = 0,iEffetNum = 0;
                if(m_tDbConn.ExecUpdate(m_szDbCmd,iInsertID,iEffetNum) != 0 || iEffetNum == 0)
                {
                    ERR("AddTask do[%s] failed in %s\n",m_szDbCmd,m_tDbConn.GetErrMsg());
                    return -1;
                }
                ptTask->iTaskID = iTaskId;
            }
        }
    }
    else
    {
        //插入前需要有对应的记录
        //TODO: 自动插入新纪录，保证后续流程可以运行
        return -1;
    }

	FORCE_WARN("into db, task id %d, pay step %d, pay state %d",ptTask->iTaskID, ptTask->iTaskType, ptTask->iTaskState);
	
	return 0;
}

int32_t CTaskManager::LoadTaskFromDB()
{
    snprintf(m_szDbCmd,sizeof(m_szDbCmd),"select `task_id`,`pay_step`,`pay_state`,`lottery_name`,`play_name`,"
		"`qihao_begin`,`qihao_end`,`project_begin`,`project_end`,`parallel_num`, `retry_times` from `%s`"
			"where `pay_step` < %d and `pay_step` > 0 and `pay_state` <> %d;",
		m_strTableName.c_str(), CTask::em_finished_task, CTask::em_doing_state);

	if (0 != m_tDbConn.ExecSelect(m_szDbCmd))
	{
		ERR("LoadTaskFromDB select from %s failed in %s\n", m_strTableName.c_str(), m_tDbConn.GetErrMsg());
		return -1;
	}

	while (m_tDbConn.FetchRow() == 0
		&& m_tDbConn.m_stRow != NULL
		&& m_tDbConn.m_stRow[0] != NULL)
	{
		INFO("LoadTaskFromDB get task[%s] step[%s] state[%s] from db", m_tDbConn.m_stRow[0], m_tDbConn.m_stRow[1], m_tDbConn.m_stRow[2]);

        CTask tTask;
		tTask.iTaskID = atoi(m_tDbConn.m_stRow[0]);
		tTask.iTaskType = atoi(m_tDbConn.m_stRow[1]);
		tTask.iTaskState = atoi(m_tDbConn.m_stRow[2]);
        tTask.strLotteryName.assign(m_tDbConn.m_stRow[3]);
		tTask.iQihaoBegin = atoi(m_tDbConn.m_stRow[5]);
		tTask.iQihaoEnd = atoi(m_tDbConn.m_stRow[6]);
		tTask.iProjectBegin = atoi(m_tDbConn.m_stRow[7]);
		tTask.iProjectEnd = atoi(m_tDbConn.m_stRow[8]);
		tTask.iFracMod = atoi(m_tDbConn.m_stRow[9]);
		tTask.iMaxRetryTimes = atoi(m_tDbConn.m_stRow[10]);

		AddTaskToDetector(&tTask);
	}
		        
	return 0;
}

int32_t CTaskManager::AddTaskToDetector(CTask* ptTask)
{
	int32_t iRet = m_tTaskDetector.Push(ptTask);
	// 检测器确认已经完成的任务，更新数据库
	if (iRet == 1)
	{
		ptTask->iTaskType = CTask::em_finished_task;
		ptTask->iTaskState = CTask::em_success_state;
		UpdateMysql(ptTask);
		return 0;
	}
	else if(iRet == 2)
	{
	    ptTask->iTaskType = CTask::em_finished_task;
        ptTask->iTaskState = CTask::em_failed_state;
        UpdateMysql(ptTask);
        return 0;
	}

	return iRet;
}

int32_t CTaskManager::AddTaskToScheduler(CTask* ptTask)
{
	ptTask->iTaskState = CTask::em_doing_state;
	UpdateMysql(ptTask);
	return m_tTaskScheduler.Push(ptTask);
}

int32_t CTaskManager::InitMysql(TiXmlElement* ptXmlNode)
{
#define XML_SUBELE_TO_STRING(ptEle,eleName,outStr) \
	TiXmlElement* pSub##eleName##Ele = ptEle->FirstChildElement(#eleName);\
	if(NULL != pSub##eleName##Ele && NULL != pSub##eleName##Ele->GetText())\
	outStr.assign(pSub##eleName##Ele->GetText())
#define XML_SUBELE_TO_INT(ptEle,eleName,outInt) \
	TiXmlElement* pSub##eleName##Ele = ptEle->FirstChildElement(#eleName);\
	if(NULL != pSub##eleName##Ele && NULL != pSub##eleName##Ele->GetText())\
	outInt = atoi(pSub##eleName##Ele->GetText())

	XML_SUBELE_TO_STRING(ptXmlNode,db_ip,m_strDBIP);
	XML_SUBELE_TO_INT(ptXmlNode,db_port,m_uDBPort);
	XML_SUBELE_TO_STRING(ptXmlNode,db_user,m_strDBUser);
	XML_SUBELE_TO_STRING(ptXmlNode,db_passwd,m_strDBPasswd);
	XML_SUBELE_TO_STRING(ptXmlNode,db_name,m_strDBName);
	XML_SUBELE_TO_STRING(ptXmlNode,table_name,m_strTableName);
	XML_SUBELE_TO_STRING(ptXmlNode,character_set,m_strCharSet);

	if(m_tDbConn.Connect(m_strDBIP.c_str(),m_strDBUser.c_str(),m_strDBPasswd.c_str(),m_strDBName.c_str(),m_strCharSet.c_str(),m_uDBPort))
	{
		fprintf(stderr,"connect db failed in %s\n",m_tDbConn.GetErrMsg());
		return -1;
	}

	return 0;
}

int32_t CTaskManager::UpdateMysql(CTask* ptTask)
{
	if(CTask::em_pay_bons_task == ptTask->iTaskType && CTask::em_doing_state == ptTask->iTaskState)
    {
        snprintf(m_szDbCmd, sizeof(m_szDbCmd), "update `%s` set `pay_step`=%d, `pay_state`=%d, `start_pay_time` = %d where `task_id`=%d;\n",
            m_strTableName.c_str(), ptTask->iTaskType, ptTask->iTaskState, time(NULL), ptTask->iTaskID);
    }
    else if(CTask::em_pay_bons_task == ptTask->iTaskType && CTask::em_success_state == ptTask->iTaskState)
	{
		snprintf(m_szDbCmd, sizeof(m_szDbCmd), "update `%s` set `pay_step`=%d, `pay_state`=%d, `finish_pay_time` = %d where `task_id`=%d;\n",
			m_strTableName.c_str(), ptTask->iTaskType, ptTask->iTaskState, time(NULL), ptTask->iTaskID);
	}
	else
	{
	    snprintf(m_szDbCmd, sizeof(m_szDbCmd), "update `%s` set `pay_step`=%d, `pay_state`=%d where `task_id`=%d;\n",
	                m_strTableName.c_str(), ptTask->iTaskType, ptTask->iTaskState, ptTask->iTaskID);
	}

	int32_t iInsertID = 0,iEffetNum = 0;
	//看日志是不是多次更新数据库。还是只有状态有变化时更新。
	INFO("update sql is %s",m_szDbCmd);
	if(m_tDbConn.ExecUpdate(m_szDbCmd,iInsertID,iEffetNum) != 0)
	{
		ERR("Update[%s] failed in %s\n",m_szDbCmd,m_tDbConn.GetErrMsg());
		return -1;
	}

	return 0;
}

