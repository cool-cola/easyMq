/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2010
Description:  主控类
***********************************************************/
#ifndef __TCPCTRL_H__
#define __TCPCTRL_H__

#include <sys/resource.h>
 
#include "CodeQueue.hpp"
#include "IdxObjMng.hpp"
#include "EpollFlow.hpp"
#include "tlib_log.h"
#include "tlib_cfg.h"
#include "Base.hpp"
#include "MQHeadDefine.h"
#include "admin.h"

#define SVR_NAME	"SCC"
typedef struct  
{
	//基本
	char	m_szSvrName[256];
	char	m_szCfgFileName[256];

	char	m_szNetCompleteSo[256];
	char	m_szMeToSvrMQ[256];
	char	m_szSvrToMeMQ[256];

	char m_szAdminIp[32];
	int	m_iAdminPort;
	
	int	m_iStatTimeGap;
	int 	m_iDisconnectNotify;
	int	m_iMaxQueueWaitus;

	int m_iBindCpu;

	int 	m_iLoadCheckAllSpan;
	int 	m_iLoadCheckEachSpan;

	int	m_iTimeOutCheckSecs;
	int	m_iTimeOutSecs;	

	int	SOCKET_RCVBUF;
	int 	SOCKET_SNDBUF;
	
	int	MAX_SOCKET_NUM;
	
	int 	RCV_BLOCK_SIZE;
	int 	RCV_BLOCK_NUM;
	
	int 	SND_BLOCK_SIZE;
	int 	SND_BLOCK_NUM;
	
	void Print()
	{
		TLib_Log_LogMsg("[config]\n");
		TLib_Log_LogMsg("m_szSvrName %s\n",m_szSvrName);
		TLib_Log_LogMsg("m_szCfgFileName %s\n",m_szCfgFileName);
		TLib_Log_LogMsg("m_szNetCompleteSo %s\n",m_szNetCompleteSo);
		TLib_Log_LogMsg("m_szMeToSvrMQ %s\n",m_szMeToSvrMQ);
		TLib_Log_LogMsg("m_szSvrToMeMQ %s\n",m_szSvrToMeMQ);
		TLib_Log_LogMsg("m_iStatTimeGap %d\n",m_iStatTimeGap);
		TLib_Log_LogMsg("m_iDisconnectNotify %d\n",m_iDisconnectNotify);
		TLib_Log_LogMsg("m_iTimeOutCheckSecs %d\n",m_iTimeOutCheckSecs);
		TLib_Log_LogMsg("m_iTimeOutSecs %d\n",m_iTimeOutSecs);
		TLib_Log_LogMsg("m_iMaxQueueWaitus %d\n",m_iMaxQueueWaitus);
		TLib_Log_LogMsg("SOCKET_RCVBUF %d\n",SOCKET_RCVBUF);
		TLib_Log_LogMsg("SOCKET_SNDBUF %d\n",SOCKET_SNDBUF);
		TLib_Log_LogMsg("RCV_BLOCK_SIZE %d\n",RCV_BLOCK_SIZE);
		TLib_Log_LogMsg("SND_BLOCK_SIZE %d\n",SND_BLOCK_SIZE);
		TLib_Log_LogMsg("RCV_BLOCK_NUM %d\n",RCV_BLOCK_NUM);
		TLib_Log_LogMsg("SND_BLOCK_NUM %d\n",SND_BLOCK_NUM);		
		TLib_Log_LogMsg("MAX_SOCKET_NUM %d\n",MAX_SOCKET_NUM);
		TLib_Log_LogMsg("\n");
	}		
}TConfig;

typedef struct
{
	unsigned int m_unClientIP;				/*客户端IP地址*/
	unsigned short m_usClientPort;				/*客户端端口*/
	char m_szSrcMQ[MQ_NAME_LEN];
	
	int m_iSocket;                    //socket句柄
	int  m_iActiveTime;                       /*最后活动时间*/
	
	short  m_sStatus;
	enum
	{
		STATUS_OK = 0,				//正常
		STATUS_CONNECTING = 1,	//正在连接
		STATUS_SENDING = 2,	//正在发送
	};
	
	short m_sSocketType;
	enum
	{
		PIPE_SOCKET = 0,
		TCP_SOCKET = 1,
		UDP_SOCKET = 2,
		ADMIN_LISTEN_SOCKET,
		ADMIN_CLIENT_SOCKET,		//管理端口	
	};	
} TSocketNode;

enum
{
	Flag_ReloadCfg = 1,
	Flag_Exit = 2
};

typedef struct
{
	long long m_llTcpCSPkgNum;
	long long m_llTcpCSPkgLen;
	long long m_llTcpSCPkgNum;
	long long m_llTcpSCPkgLen;

	long long m_llUdpCSPkgNum;
	long long m_llUdpCSPkgLen;
	long long m_llUdpSCSuccPkgNum;
	long long m_llUdpSCFailedPkgNum;
	long long m_llUdpSCPkgLen;	
}TStatStu;

//设置TSocketNode的主键
ssize_t SetSocketNodeKey(void* pObj,void* pKey,ssize_t iKeyLen);
//获取TSocketNode的主键
ssize_t GetSocketNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen);

//判包完整函数
typedef int (*check_complete)(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen);
typedef int (*msg_header_len)();
class CMainCtrl
{
public:
	CMainCtrl();  
	~CMainCtrl(); 
	int  Initialize(char* pProName,char* pConfigFile);
	int  Run();
	int  SetRunFlag(int iRunFlag);
private:		
	int CreateSocketNode(int iSocket,int iStatus,
			unsigned int unClientIP, unsigned short usClientPort,char* szSrcMQ,short sSocketType);
	int ClearSocketNode(unsigned int unClientIP, unsigned short usClientPort,char szSrcMQ[MQ_NAME_LEN]); 
	TSocketNode* GetSocketNode(unsigned int unClientIP, unsigned short usClientPort,char szSrcMQ[MQ_NAME_LEN]);
	int FillMQHead(TMQHeadInfo* pMQHeadInfo,TSocketNode* pSocketNode,
			unsigned char ucCmd,unsigned char ucDataType=TMQHeadInfo::DATA_TYPE_TCP);

	int  CheckTimeOut();  
	int  CheckClientMessage();
	int  CheckSvrMessage();

	int RecvClientData(int iNodeObjIdx);

	int  ReadCfgFile(char*);
	int  WriteStat();

	int ProcessAdminMsg(int iNodeObjIdx);

private:
	timeval		m_tNow;
	TConfig             m_stConfig;  

	//客户通过ip port找到SocketNode,epoll通过objidx找到SocketNode
	//每个SocketNode 通过唯一对应一个send buffer ,一个recv buffer
	TIdxObjMng m_stSocketNodeMng;
	CHashTab m_stSocketNodeHash;
	
	CEPollFlow m_stEPollFlow;

	//接收缓存
	TIdxObjMng m_stIdxObjMngRecv;
	CBuffMng m_stBuffMngRecv;

	//发送缓存
	TIdxObjMng m_stIdxObjMngSend;
	CBuffMng m_stBuffMngSend;		

	CCodeQueue          m_Me2SvrPipe;                      /*输入队列*/ 
	CCodeQueue          m_Svr2MePipe;                      /*输出队列*/

	//判整包函数,返回包长
	check_complete net_complete_func;
	msg_header_len msg_header_len_func;
	int m_iMsgHeaderLen;
	
	char m_szCodeBuff[MAX_MSG_LEN];

	//负载统计
	CLoadGrid *m_pCLoadGrid;
	
	//统计
	TStatStu	m_stStat;
	unsigned int m_unTcpClientNum;
	
	int m_iRunFlag;
};

#endif 

