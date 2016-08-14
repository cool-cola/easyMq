#include <sys/types.h>
#include "master_ctrl.h"

#include "task.h"
#include "worker_manager.h"
#include "task_manager.h"
#include "task_detector.h"
//#include "./ticket_detector/digital_detector.h"
#include "./trade_detector/digital_detector.h"
#include "./trade_detector/jc_detector.h"
#include "tinyxml.h"
//redis  head
#include "bc/bc_mq_define.h"
#include "redis_helper.h"
#include "SendNotice.hpp"
using namespace NS_LotteryBons;
using namespace bc::library::_3rd;
using namespace Notice;
///////////////////////////////////////////////////////////////////////////////

const int32_t UPDATE_TASK_INTERVAL = 15;	//更新任务时钟的时间间隔

CTaskDetector::CTaskDetector()
    : m_iUpdateTaskInterval(15)
{
}

CTaskDetector::~CTaskDetector()
{
	DestroyDetector();
	m_TaskList.clear();
}

//生成新的任务detector，push进m_stdNewTaskDetectorList。
int32_t CTaskDetector::Init(TiXmlElement* ptXmlNode)
{
	CTask tTask;

	for (TiXmlElement *ptLotteryCatogory = ptXmlNode->FirstChildElement("lottery_configure");
		 ptLotteryCatogory != NULL;
		 ptLotteryCatogory = ptLotteryCatogory->NextSiblingElement("lottery_configure"))
	{
		const char *pszLotteryName = ptLotteryCatogory->Attribute("lottery_name");
		const char *pszLotteryArea = ptLotteryCatogory->Attribute("area");
		const char *pszLotteryCfg = ptLotteryCatogory->GetText();
		if (pszLotteryName == NULL || pszLotteryArea == NULL || pszLotteryCfg == NULL)
		{
			ERR("Xml configure failed\n");
			continue;
		}

		// 生成新任务检测器
		//TODO: 转化成挂so的方式
		CBaseDetector* ptDetectorImpl = NULL;
		if(0 == strcmp(pszLotteryArea, "trade"))
		{
		    //TODO: 后续按彩种将detector封装起来
		    if(strcmp(pszLotteryName, "jczq") == 0
		            || strcmp(pszLotteryName, "jclq") == 0)
		    {
		        ptDetectorImpl = new CJcDetector(tTask);
		    }
		    else
		    {
		        ptDetectorImpl = new CDigitalDetector(tTask);//生成专门检测新任务的detector，这里tTask的taskId为-1，表示这个detector是专门用来检测新任务的。后面的任务已经有了taskId。
		    }
		}
		else if(0 == strcmp(pszLotteryArea, "ticket"))
		{
		    ptDetectorImpl = new CDigitalDetector(tTask);
		}
		else
		{
			ERR("Xml configure failed\n");
			continue;
		}
		
		if (ptDetectorImpl->InitDetector(pszLotteryCfg) != 0)
		{
			ERR("Load %s failed in %s", pszLotteryCfg, ptDetectorImpl->GetErrMsg());
			delete ptDetectorImpl;
			continue;
		}
		m_stdNewTaskDetectorList.push_back(ptDetectorImpl);

        INFO("name is %s, qihao is %d", tTask.strLotteryName.c_str(), tTask.iQihaoBegin);

		// 添加到配置MAP内
		std::pair<std::string,std::string> lotteryUID(pszLotteryName,pszLotteryArea);
		m_stdLotteryConfName[lotteryUID] = pszLotteryCfg;
	}

	//初始化redis
	CRedisConf stConf;
	if (0 != stConf.Init())
	{
		ERR("REDIS CONF INIT ERROR");
		//return REDISCONF_INIT_ERROR;
	}
	else
	{
		/*if (0 != REDIS_INIT(stConf.m_strIP, stConf.m_ushPort))
		{
			ERR("REDIS INIT ERROR");
			//return REDIS_INIT_ERROR;
		}*/
	}

	m_iUpdateTaskInterval = UPDATE_TASK_INTERVAL;
	return 0;
}

int32_t CTaskDetector::Reload(TiXmlElement* ptXmlNode)
{
	DestroyDetector();
	return Init(ptXmlNode);
}

int32_t CTaskDetector::OnTick(const timeval* ptTick)
{
	static time_t sLastSchedTime = time(NULL);

	if(sLastSchedTime + m_iUpdateTaskInterval > ptTick->tv_sec)
	{
		return 0;
	}
	sLastSchedTime = ptTick->tv_sec;
	INFO("time is %d", ptTick->tv_sec);

	for (std::map<int32_t,CBaseDetector*>::iterator fIt = m_stdTaskDetectorMap.begin();
		fIt != m_stdTaskDetectorMap.end();
		/*fIt++*/)
	{
		INFO("begin to loop m_stdTaskDetectorMap");
		CTask tTask;
        if (fIt->second->Check(&tTask) > 0)
        {
			std::map<int32_t,CBaseDetector*>::iterator dIt = fIt++;
			delete dIt->second;
			dIt->second = NULL;
			m_stdTaskDetectorMap.erase(dIt);

			m_TaskList.push_back(tTask);
			FORCE_WARN("lottery[%s] play[%s] detector task[%d] do step[%d] : qihao[%d,%d) time[%d,%d) mod[%d]\n",
				tTask.strLotteryName.c_str(), tTask.strLotteryPlay.c_str(), tTask.iTaskID, tTask.iTaskType,
				tTask.iQihaoBegin, tTask.iQihaoEnd, tTask.iProjectBegin, tTask.iProjectEnd, tTask.iDenoMod);
        }
		else
		{
			fIt++;
		}
	}

	for (std::list<CBaseDetector*>::iterator fIt = m_stdNewTaskDetectorList.begin();
		fIt != m_stdNewTaskDetectorList.end();
		fIt++)
	{
		INFO("begin to loop 2");
		CBaseDetector* ptDetector = (*fIt);
		CTask tTask;
		if (ptDetector->Check(&tTask) > 0)
		{
			m_TaskList.push_back(tTask);
			FORCE_WARN("lottery[%s] play[%s] detector new task : qihao[%d,%d) time[%d,%d) mod[%d]\n",
				tTask.strLotteryName.c_str(), tTask.strLotteryPlay.c_str(),
				tTask.iQihaoBegin, tTask.iQihaoEnd, tTask.iProjectBegin, tTask.iProjectEnd, tTask.iDenoMod);
		}
	}

	return m_TaskList.size();
}

CBaseDetector *CTaskDetector::GetDetectorFromTask(const CTask &tTask)
{
    CBaseDetector *pDetector = NULL;
    if(tTask.strLotteryName == "jczq"
            || tTask.strLotteryName == "jclq")
    {
        pDetector = new CJcDetector(tTask);
    }
    else //增加新彩种时需要注意类型
    {
        pDetector = new CDigitalDetector(tTask);
    }

    return pDetector;
}

int32_t CTaskDetector::Push(CTask* ptTask)
{
	if (ptTask->iTaskID < 0)
	{
		return -1;
	}

	// 已存在检测器内的直接判断是否完成
	std::map<int32_t,CBaseDetector*>::iterator fIt = m_stdTaskDetectorMap.find(ptTask->iTaskID);
	if (fIt != m_stdTaskDetectorMap.end())
	{
		if (fIt->second->OnTaskDone(ptTask) == 0)
		{
			delete fIt->second;
			m_stdTaskDetectorMap.erase(fIt);
			return 1;
		}

		return 0;
	}

	std::map<int32_t, int32_t>::iterator it = m_stdTaskRetryTimes.find(ptTask->iTaskID);
    if(it != m_stdTaskRetryTimes.end())
    {
        if(ptTask->iMaxRetryTimes > 0 &&
                it->second > ptTask->iMaxRetryTimes)
        {
            char errMsg[256];
            snprintf(errMsg, sizeof(errMsg), "redo %d more than %d times. please check, lottery %s, qihao %d",
                    it->second, ptTask->iMaxRetryTimes, ptTask->strLotteryName.c_str(), ptTask->iQihaoBegin);
            ERR("%s", errMsg);
            //告警
            SendNotice::SendNotify(std::string(errMsg), EM_GENERAL);

            /* 先不终止重试任务
            //清除次数
            it->second = 0;
            //删除对应的detector
            std::map<int32_t,CBaseDetector*>::iterator fIt = m_stdTaskDetectorMap.find(ptTask->iTaskID);
            if (fIt != m_stdTaskDetectorMap.end())
            {
                delete fIt->second;
                m_stdTaskDetectorMap.erase(fIt);
            }

            //超过重试次数后，直接返回在DB置为failed state
            return 2;
            */
        }
    }
    else
    {
        //初始化第一次
        m_stdTaskRetryTimes[ptTask->iTaskID] = 0;
    }

	// 不在检测器内的生成检测器检测任务
	std::pair<std::string,std::string> lotteryUID(ptTask->strLotteryName,"trade");
	if (m_stdLotteryConfName.find(lotteryUID) == m_stdLotteryConfName.end())
	{
		ERR("CTaskDetector not configure for lottery[%s] in area[%s]\n",lotteryUID.first.c_str(),lotteryUID.second.c_str());
		return -1;
	}

	CBaseDetector* ptDetector = this->GetDetectorFromTask(*ptTask);
	if(NULL == ptDetector)
	{
	    ERR("get detector err, task id", ptTask->iTaskID);
	    return -1;
	}

	if (ptDetector->InitDetector(m_stdLotteryConfName[lotteryUID].c_str()) != 0)
	{
		delete ptDetector;
		ptDetector = NULL;
		ERR("CTaskDetector load detector for lottery[%s] in area[%s] failed in %s\n",
			lotteryUID.first.c_str(), lotteryUID.second.c_str(), ptDetector->GetErrMsg());
		return -1;
	}

	// 已完成的任务删除检测器，未完成的任务加入到检测MAP内
	if (ptDetector->OnTaskDone(ptTask) == 0)
	{
		delete ptDetector;
		ptDetector = NULL;
		return 1;
	}

	m_stdTaskDetectorMap[ptTask->iTaskID] = ptDetector;
	m_stdTaskRetryTimes[ptTask->iTaskID]++;
	INFO("task with id %d pushed", ptTask->iTaskID);
	return 0;
}

int32_t CTaskDetector::Pop(CTask* ptTask)
{
    if(m_TaskList.size() == 0)
    {
        return -1;
    }

    *ptTask = m_TaskList.front();
    INFO("task with id %d poped", ptTask->iTaskID);

    m_TaskList.pop_front();
	return 0;
}

void CTaskDetector::DestroyDetector(bool bClearAll /*= false*/)
{
	for (std::list<CBaseDetector*>::iterator fIt = m_stdNewTaskDetectorList.begin();
		fIt != m_stdNewTaskDetectorList.end();
		fIt++)
	{
		delete (*fIt);
	}
	m_stdNewTaskDetectorList.clear();

	if (bClearAll == true)
	{
		for (std::map<int32_t,CBaseDetector*>::iterator fIt = m_stdTaskDetectorMap.begin();
			fIt != m_stdTaskDetectorMap.end();
			fIt++)
		{
			delete fIt->second;
		}
		m_stdTaskDetectorMap.clear();
	}
}
