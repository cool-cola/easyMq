/*************************************************************
Copyright (C), 2012-2099
Author:lambygao
Version :1.0
Date: 2014-07
Description: ��Ϣ�������
***********************************************************/
#ifndef _DEFIEN_FRAME_CTRL_HEADER_
#define _DEFIEN_FRAME_CTRL_HEADER_

#include <errno.h>

#include "Base.hpp"
#include "tlib_log.h"
#include "tlib_cfg.h"
#include "MQHeadDefine.h"
#include "CodeQueue.hpp"
#include "CodeQueueMutil.hpp"
#include "EpollFlow.hpp"
#include "Statistic.hpp"

#define FRAME_MAX_BUFF_LEN		(1*1024*1024)
#define FRAME_MAX_MSG_LEN		(1000*1024)

enum
{
	Flag_ReloadCfg = 1,
	Flag_Exit = 2
};

enum emHandleLogLevel
{
	KEYLOG = -1,
	NOLOG = 0,
	ERRORLOG,
	RUNLOG,
	DEBUGLOG
};

typedef struct
{
	int32_t m_iLogLevel;
	int32_t m_iLogKey;
}TShmLog;

typedef struct
{
	//����
	char m_szSvrName[256];
	char m_szCfgFileName[256];
	int32_t m_iBindCpu;

	//ͨ�Źܵ�
	char m_szSCCToMeMQ[256];
	char m_szMeToSCCMQ[256];

	char m_szCCSToMeMQ[256];
	char m_szMeToCCSMQ[256];

	// ����
	char m_szCtrlConf[256];

	//��־
	TShmLog* m_pShmLog;
	int32_t m_iStatTimeGap;
	int32_t m_iMaxLogSize;
	int32_t m_iMaxLogNum;
	char m_szLogFileBase[256];

	void Print()
	{
		TLib_Log_LogMsg("====base smcp config====\n");
		TLib_Log_LogMsg("m_szSvrName %s\n",m_szSvrName);
		TLib_Log_LogMsg("m_szCfgFileName %s\n",m_szCfgFileName);

		TLib_Log_LogMsg("m_iBindCpu %d\n",m_iBindCpu);

		TLib_Log_LogMsg("m_szCCSToMeMQ %s\n",m_szCCSToMeMQ);
		TLib_Log_LogMsg("m_szMeToCCSMQ %s\n",m_szMeToCCSMQ);
		TLib_Log_LogMsg("m_szSCCToMeMQ %s\n",m_szSCCToMeMQ);
		TLib_Log_LogMsg("m_szMeToSCCMQ %s\n",m_szMeToSCCMQ);

		TLib_Log_LogMsg("m_iStatTimeGap %d\n",m_iStatTimeGap);
		TLib_Log_LogMsg("m_iMaxLogSize %d\n",m_iMaxLogSize);
		TLib_Log_LogMsg("m_iMaxLogNum %d\n",m_iMaxLogNum);
	}
}TConfig;

class CFrameCtrl
{
public:
	CFrameCtrl();
	~CFrameCtrl();
	void Ver(FILE* pFileFp);
	int32_t Initialize(char* pProName,char* pConfigFile);
	int32_t Run();
	int32_t SetRunFlag(int32_t iRunFlag);
	void Log(int32_t iKey,int32_t iLevel,const char *sFormat, ...);
	void FrequencyLog(int32_t iKey,int32_t iLevel,const char *sFormat, ...);

	char* GetSendBuff()	{	return (m_pSendBuffer==NULL)?NULL:(m_pSendBuffer+sizeof(TMQHeadInfo));	}

	// SendReq�����ڷ���GetSendBuff��õ�BUFF
	int32_t SendReq(uint32_t uIp, uint32_t uPort, char* pReq, uint32_t reqLen);
	// SendRsp�����ڷ���GetSendBuff��õ�BUFF
	int32_t SendRsp(TMQHeadInfo* pMQHeadInfo, char* pRsp, uint32_t rspLen);

private:
	int32_t TimeTick();
	int32_t ReadCfgFile(char*);
	int32_t WriteStat();
	int32_t WaitData();
	int32_t LogInit();
	int32_t LoadMcpIdx();
	int32_t InitCodeQueue();

public:
	int32_t m_iMcpIdx;
	int32_t m_iRunFlag;
    TConfig m_stConfig;
	timeval m_tNow;
	CStatistic* m_pStatistic;

private:
	CCodeQueue* m_CCSToMeMQ;
	CCodeQueue* m_MeToCCSMQ;
	CCodeQueue* m_SCCToMeMQ;
	CCodeQueue* m_MeToSCCMQ;
	CEPollFlow	m_tEpoll;

	char* m_pRecvBuffer;
	char* m_pSendBuffer;
};

#endif//_DEFIEN_FRAME_CTRL_HEADER_
