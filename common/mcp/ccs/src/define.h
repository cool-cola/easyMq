#ifndef _DEFINE_H_
#define _DEFINE_H_

#if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define __builtin_expect(x, expected_value) (x)
#endif
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define MAX_BIND_PORT_NUM	65535

typedef int (*check_complete)(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen);
typedef int (*msg_header_len)();
typedef int (*admin_msg_func)(void* pMainCtrl,char* pIn, int iInLen,char* pOut, int &iOutLen);

typedef struct
{
	char szIP[16];
	short sAccessWay;
	enum
	{
		Access = 0,
		Deny,
	};
	
}IPAccessStu;
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
	int m_iLogLevel;
	int m_iLogKey;
}TShmLog;
typedef struct  
{
	typedef struct
	{
		unsigned int m_unIP;
		unsigned short m_usPort;
		check_complete net_complete_func;
		int 	m_iMsgHeaderLen;
		int m_iLimitTps;

		CLoadGrid* m_pCLoadGrid;
	}TIpPortStu;

	TIpPortStu m_stTcpIPPortStu[MAX_BIND_PORT_NUM];
	TIpPortStu m_stUdpIPPortStu[MAX_BIND_PORT_NUM];

	//依次排列的数据，用来遍历
	TIpPortStu m_stTcpUsedIPPort[MAX_BIND_PORT_NUM];
	TIpPortStu m_stUdpUsedIPPort[MAX_BIND_PORT_NUM];

	//设定各个port的tcp连接允许的处理线程
	int		m_aiTcpPortAuxMask[MAX_BIND_PORT_NUM];//指定port的对应的aux线程
	
	//基本
	char	m_szSvrName[256];
	char	m_szCfgFileName[256];

	char	m_szNetCompleteSo[256];

	char	m_szMeToSvrMQ[128];
	char	m_szSvrToMeMQ[128];

	int		m_iAuxHandleNum;

	int		m_iAdminPort;
	char	m_szAdminSo[256];
	admin_msg_func m_admin_msg_func;

	int 	m_iStatTimeGap;
	int 	m_iMaxLogSize;
	int 	m_iMaxLogNum;
	int 	m_iDisconnectNotify;

	int		m_aiBindCpu[1024];

	int 	m_iIpAccessOpen;
	int 	m_iIpAccessDefault;
	char	m_szIpAccessTable[128];
	
	int 	m_iTimeOutCheckSecs;
	int 	m_iTimeOutSecs;

	int 	m_iMaxQueueWaitus;

	int 	m_iLoadCheckOpen;
	int 	m_iLoadCheckAllSpan;
	int 	m_iLoadCheckEachSpan;
	int 	m_iLoadCheckMaxPkgNum;

	int		SOCKET_RCVBUF;
	int 	SOCKET_SNDBUF;

	int		MAX_SOCKET_NUM;
	
	int 	RCV_BLOCK_SIZE;
	int 	RCV_BLOCK_NUM;
	
	int 	SND_BLOCK_SIZE;
	int 	SND_BLOCK_NUM;

	int		LISTEN_BACK_LOG;
	//日志开关
	TShmLog* m_pShmLog;
	void Print()
	{
		TLib_Log_LogMsg("[config]\n");
		TLib_Log_LogMsg("m_szSvrName %s\n",m_szSvrName);
		TLib_Log_LogMsg("m_szCfgFileName %s\n",m_szCfgFileName);
		TLib_Log_LogMsg("m_szNetCompleteSo %s\n",m_szNetCompleteSo);
		TLib_Log_LogMsg("m_szMeToSvrMQ %s\n",m_szMeToSvrMQ);
		TLib_Log_LogMsg("m_szSvrToMeMQ %s\n",m_szSvrToMeMQ);
		TLib_Log_LogMsg("m_iAuxHandleNum %d\n",m_iAuxHandleNum);
		TLib_Log_LogMsg("m_szAdminSo %s\n",m_szAdminSo);
		TLib_Log_LogMsg("m_iAdminPort %d\n",m_iAdminPort);
		TLib_Log_LogMsg("m_iStatTimeGap %d\n",m_iStatTimeGap);
		TLib_Log_LogMsg("m_iDisconnectNotify %d\n",m_iDisconnectNotify);
		for(int i=0;m_aiBindCpu[i]>=0;i++)
			TLib_Log_LogMsg("BindCpu %d\n",m_aiBindCpu[i]);
		TLib_Log_LogMsg("m_iTimeOutCheckSecs %d\n",m_iTimeOutCheckSecs);
		TLib_Log_LogMsg("m_iTimeOutSecs %d\n",m_iTimeOutSecs);
		TLib_Log_LogMsg("m_iMaxQueueWaitus %d\n",m_iMaxQueueWaitus);
		TLib_Log_LogMsg("m_iLoadCheckOpen %d\n",m_iLoadCheckOpen);
		TLib_Log_LogMsg("m_iLoadCheckAllSpan %d\n",m_iLoadCheckAllSpan);
		TLib_Log_LogMsg("m_iLoadCheckEachSpan %d\n",m_iLoadCheckEachSpan);
		TLib_Log_LogMsg("m_iLoadCheckMaxPkgNum %d\n",m_iLoadCheckMaxPkgNum);
		TLib_Log_LogMsg("SOCKET_RCVBUF %d\n",SOCKET_RCVBUF);
		TLib_Log_LogMsg("SOCKET_SNDBUF %d\n",SOCKET_SNDBUF);
		TLib_Log_LogMsg("RCV_BLOCK_SIZE %d\n",RCV_BLOCK_SIZE);
		TLib_Log_LogMsg("SND_BLOCK_SIZE %d\n",SND_BLOCK_SIZE);
		TLib_Log_LogMsg("RCV_BLOCK_NUM %d\n",RCV_BLOCK_NUM);
		TLib_Log_LogMsg("SND_BLOCK_NUM %d\n",SND_BLOCK_NUM);
		TLib_Log_LogMsg("MAX_SOCKET_NUM %d\n",MAX_SOCKET_NUM);
		TLib_Log_LogMsg("LISTEN_BACK_LOG %d\n",LISTEN_BACK_LOG);
		TLib_Log_LogMsg("\n");
	}	
}TConfig;

typedef struct
{
	int m_iSocket;                    		//socket句柄
	
	short m_sSocketType;
	enum
	{
		TCP_LISTEN_SOCKET = 0,		//监听口
		TCP_CLIENT_SOCKET = 1,		//客户端主动连接

		UDP_SOCKET,				//udp socket

		//sys
		MQ_PIPE_SOCKET,			//shm管道通知口
		ADMIN_LISTEN_SOCKET,
		ADMIN_CLIENT_SOCKET,		//管理端口		
	};
	unsigned short m_usClientPort;		/*客户端端口*/	
	unsigned int m_unClientIP;			/*客户端IP地址*/
	int  m_iActiveTime;                      	/*最后活动时间*/

	unsigned short m_usListenPort;		/*原生的监听端口*/	
	unsigned int m_unID;				/*唯一ID*/
} TSocketNode;

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

enum
{
	Flag_ReloadCfg = 1,
	Flag_Exit = 2
};


#endif

