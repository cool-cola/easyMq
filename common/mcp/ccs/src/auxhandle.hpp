/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2010
Description: 工作线程类
***********************************************************/

#ifndef __AUXHANDLE_H__
#define __AUXHANDLE_H__

#include "CodeQueueMutil.hpp"
#include "IdxObjMng.hpp"
#include "EpollFlow.hpp"
#include "tlib_log.h"
#include "tlib_cfg.h"
#include "thread.hpp"
#include "Base.hpp"
#include "MQHeadDefine.h"
#include "define.h"

#include "LockGuard.hpp"

#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/file.h>
#include <time.h>
#include <dlfcn.h>
#include <sys/resource.h>

#include <sched.h>
#include <ctype.h>
#include "Statistic.hpp"

#define INFO(x...) do { \
		  Log(0,DEBUGLOG, ## x ); \
		  Log(0,DEBUGLOG, "[file:%s:%d %s]\r\n", __FILE__,__LINE__,__FUNCTION__); \
} while ( 0 )

#define WARN(x...) do { \
		  frequency_log(0,RUNLOG, ## x ); \
		  frequency_log(0,RUNLOG, "[file:%s:%d %s]\r\n", __FILE__,__LINE__,__FUNCTION__); \
} while ( 0 )

#define ERR(x...) do { \
		  frequency_log(0,ERRORLOG, ## x ); \
		  frequency_log(0,ERRORLOG, "[file:%s:%d %s]\r\n", __FILE__,__LINE__,__FUNCTION__); \
} while ( 0 )

class CAuxHandle  : public CThread
{
public:
	CAuxHandle();  
	~CAuxHandle(); 
	int  Initialize(void* pMainCrtl,char cAuxID);
	CCodeQueue* GetCodeQueue(){return &m_Main2MePipe;}; 
	virtual int Run();
	int LogInit(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum);
	void Log(int iKey,int iLevel,const char *sFormat, ...);
	void frequency_log(int iKey,int iLevel,const char *sFormat, ...);
public:		
	int CreateSocketNode(int iSocket,int iType,unsigned int unClientIP,
					unsigned short usClientPort,unsigned short usListenPort);
	int ClearSocketNode(int iSocketSuffix,int iCloseNotify=0); 

	int ProcessNotifyMsg(char* pCodeBuff,int iCodeLen);
	int ProcessAdminMsg(int iSocketSuffix,char *pMsg,int iMsgLen);

	int  CheckTimeOut();  
	int  CheckClientMessage();
	int  CheckMainMessage();
	int  CheckSvrMessage();
	int  DoSvrMessage(char* pMsgBuff,int iCodeLength);

	int FillMQHead(int iSuffix,TMQHeadInfo* pMQHeadInfo,
					unsigned char ucCmd,unsigned char ucDataType=TMQHeadInfo::DATA_TYPE_TCP);
	int RecvClientData(int iSockSuffix);
	
	int InitSocket();
	int NotifyAllThread(char* pCodeBuff,int iCodeLen);

public:

	CCodeQueue          m_Main2MePipe; 
	void *m_pMainCtrl;
	char m_cAuxID;

	//param from main
	TConfig* m_pMainConfig;
	CCodeQueue		*m_pMe2SvrPipe;                      /*输入队列*/ 
	CCodeQueue     *m_pSvr2MePipe;                      /*输出队列*/
	
	unsigned long long m_ullMemCost;
	
	CEPollFlow m_stEPollFlowUp;
	TSocketNode* m_pSocket;

	unsigned int m_unID;

	//Recv Buff
	TIdxObjMng m_stIdxObjMngRecv;
	CBuffMng m_stBuffMngRecv;

	//Send Buff
	TIdxObjMng m_stIdxObjMngSend;
	CBuffMng m_stBuffMngSend;		

	timeval		m_tNow;

	//负载控制
	CLoadGrid *m_pCLoadGrid;
	
	char m_szBuffRecv[MAX_MSG_LEN];
	char m_szBuffSend[MAX_MSG_LEN];

	//统计
	TStatStu	m_stStat;
	TStatStu m_stAllStat;
	unsigned int m_unTcpClientNum;
	unsigned int m_unTcpErrClientNum;

	bool m_bHandleRun;
	CStatistic* m_pStatistic;
	char m_szLogFileBase[256];
	char m_szLogFileName[256];
	int m_iMaxLogSize;
	int m_iMaxLogNum;	
};

#endif 


