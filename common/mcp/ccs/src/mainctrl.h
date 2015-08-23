/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2010
Description: 主控类
***********************************************************/

#ifndef __MAINCTRL_H__
#define __MAINCTRL_H__

#include "auxhandle.hpp"

#define SVR_NAME	"CCS"
#define SVR_VER	1.0

typedef int (*check_complete)(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen);
typedef int (*msg_header_len)();
typedef int (*admin_msg_func)(void* pMainCtrl,char* pIn, int iInLen,char* pOut, int &iOutLen);
#define MAX_AUX_HANDLE_NUM		(1024)


class CMainCtrl
{
public:
	CMainCtrl();  
	~CMainCtrl(){}; 
	int  Initialize(char* pProName,char* pConfigFile);
	int  Run();
	int  SetRunFlag(int iRunFlag);
	int LogInit(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum);
	void Log(int iKey,int iLevel,const char *sFormat, ...);
public:		
	int CreateSocketNode(int iSocket,int iType,unsigned int unClientIP, 
			unsigned short usClientPort,unsigned short usListenPort);
	int  CheckClientMessage();
	int  ReadCfgFile(char*);

	int LoadIPAccess(char *pFileName);
	int CheckIPAccess(unsigned int unClientIP);
	bool IsIpMatch(char* pIP1,char* pIP2);
	
	int  WriteStat();
	int InitSocket();
	int SelectAuxThreadForClient(int iSocket, int iListenPort);

public:

	CCodeQueueMutil		m_Me2SvrPipe;                      /*输入队列*/ 
	CCodeQueueMutil     m_Svr2MePipe;                      /*输出队列*/
	
	CEPollFlow m_stEPollFlowListen;
	TSocketNode* m_pListenSocket;

	CAuxHandle* m_pAuxHandle[MAX_AUX_HANDLE_NUM];		

	timeval		m_tNow;
	TConfig		m_stConfig;  

	//判整包函数,返回包长
	check_complete default_net_complete_func;
	int i_default_msg_header_len;

	//最后5min 统计
	TStatStu	m_stStat;
	//全部统计
	TStatStu	m_stAllStat;

	int 	m_iIpAclNum;
	IPAccessStu	*m_pIPAcl;
	
	int m_iRunFlag;

	char m_szLogFileBase[256];
	char m_szLogFileName[256];
	int m_iMaxLogSize;
	int m_iMaxLogNum;
};

#endif 

