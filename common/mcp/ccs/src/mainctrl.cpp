#include "mainctrl.h"

check_complete LoadCompleteSo(char* pSoFile)
{
	char szFile[256];
	sprintf(szFile,"./%s",pSoFile);
	
	void* pHandle = dlopen(szFile, RTLD_LAZY);
	if(!pHandle)
	{
		printf("dlopen %s failed!",szFile);
		assert(0);
	}

	dlerror();  	//Clear any existing error 
	check_complete net_complete_func = (check_complete)dlsym(pHandle, "net_complete_func");
	if (dlerror() != NULL)
	{
		printf("get net_complete_func from %s failed!\n",pSoFile);
		assert(0);
	}
	return net_complete_func;
}
msg_header_len LoadHeaderLenSo(char* pSoFile)
{
	char szFile[256];
	sprintf(szFile,"./%s",pSoFile);
	
	void* pHandle = dlopen(szFile, RTLD_LAZY);
	if(!pHandle)
	{
		printf("dlopen %s failed!",szFile);
		assert(0);
	}

	dlerror();  	//Clear any existing error 
	msg_header_len msg_header_len_func = (msg_header_len)dlsym(pHandle, "msg_header_len");
	if (dlerror() != NULL)
	{
		printf("get msg_header_len from %s failed!\n",pSoFile);
		assert(0);
	}
	return msg_header_len_func;
}

CMainCtrl::CMainCtrl()
{
	m_pListenSocket = NULL;
	memset(&m_stStat,0,sizeof(m_stStat));
	memset(&m_stAllStat,0,sizeof(m_stAllStat));
	memset(&m_stConfig,0,sizeof(m_stConfig));

	memset(m_pAuxHandle,0,sizeof(m_pAuxHandle));
	
	default_net_complete_func = NULL;
	i_default_msg_header_len = 0;
	
	m_iRunFlag = 0;

	memset(m_stConfig.m_stTcpIPPortStu,0,sizeof(m_stConfig.m_stTcpIPPortStu));
	memset(m_stConfig.m_stUdpIPPortStu,0,sizeof(m_stConfig.m_stUdpIPPortStu));

	memset(m_stConfig.m_stTcpUsedIPPort,0,sizeof(m_stConfig.m_stTcpUsedIPPort));
	memset(m_stConfig.m_stUdpUsedIPPort,0,sizeof(m_stConfig.m_stUdpUsedIPPort));	

	memset(m_stConfig.m_aiTcpPortAuxMask,0xFF,sizeof(m_stConfig.m_aiTcpPortAuxMask));

	m_pIPAcl = NULL;
}
int CMainCtrl::ReadCfgFile(char* szCfgFile)
{
	char szBindCpu[1024];
	char szIpAccess[64];
	TLib_Cfg_GetConfig(szCfgFile,		
		"NetCompleteSo", CFG_STRING, m_stConfig.m_szNetCompleteSo, "",sizeof(m_stConfig.m_szNetCompleteSo),
		
		"MeToSvrMQ", CFG_STRING, m_stConfig.m_szMeToSvrMQ, "",sizeof(m_stConfig.m_szMeToSvrMQ),
		"SvrToMeMQ", CFG_STRING, m_stConfig.m_szSvrToMeMQ, "",sizeof(m_stConfig.m_szSvrToMeMQ),


		//"AuxHandleNum", CFG_INT, &(m_stConfig.m_iAuxHandleNum), 1,
		
		"StatisticGap", CFG_INT, &(m_stConfig.m_iStatTimeGap), 300,
		"MaxLogSize", CFG_INT, &(m_stConfig.m_iMaxLogSize), 20000000,
		"MaxLogNum", CFG_INT, &(m_stConfig.m_iMaxLogNum), 10,
		"DisconnectNotify", CFG_INT, &(m_stConfig.m_iDisconnectNotify), 0,

		"CpuAffinity", CFG_STRING, szBindCpu, "",sizeof(szBindCpu),
		"AdminMsgSo", CFG_STRING, m_stConfig.m_szAdminSo, "",sizeof(m_stConfig.m_szAdminSo),

		"IpAccessOpen", CFG_INT, &(m_stConfig.m_iIpAccessOpen), 0,
		"IpAccessDefault", CFG_STRING, szIpAccess,"ACCESS",sizeof(szIpAccess),
		"IpAccessTable", CFG_STRING, m_stConfig.m_szIpAccessTable, "",sizeof(m_stConfig.m_szIpAccessTable),
		
		"SOCKET_RCVBUF",CFG_INT, &(m_stConfig.SOCKET_RCVBUF),-1,
		"SOCKET_SNDBUF",CFG_INT, &(m_stConfig.SOCKET_SNDBUF),-1,

		"MAX_SOCKET_NUM",CFG_INT, &(m_stConfig.MAX_SOCKET_NUM),50000,	
		"RCV_BLOCK_SIZE",CFG_INT, &(m_stConfig.RCV_BLOCK_SIZE),1024,
		"RCV_BLOCK_NUM",CFG_INT, &(m_stConfig.RCV_BLOCK_NUM),1024,
		
		"SND_BLOCK_SIZE",CFG_INT, &(m_stConfig.SND_BLOCK_SIZE),1024,
		"SND_BLOCK_NUM",CFG_INT, &(m_stConfig.SND_BLOCK_NUM),1024,

		"LISTEN_BACK_LOG",CFG_INT, &(m_stConfig.LISTEN_BACK_LOG),1024,

		"TimeOutCheckSecs", CFG_INT, &(m_stConfig.m_iTimeOutCheckSecs), 0,
		"TimeOutSecs", CFG_INT, &(m_stConfig.m_iTimeOutSecs), 0,
		
		"MaxQueueWaitus", CFG_INT, &(m_stConfig.m_iMaxQueueWaitus), 0,

		"LoadCheckOpen", CFG_INT, &(m_stConfig.m_iLoadCheckOpen), 0,
		"LoadCheckAllSpan", CFG_INT, &(m_stConfig.m_iLoadCheckAllSpan), 0,
		"LoadCheckEachSpan", CFG_INT, &(m_stConfig.m_iLoadCheckEachSpan), 0,
		NULL);

	strncpy(m_stConfig.m_szCfgFileName,szCfgFile,sizeof(m_stConfig.m_szCfgFileName)-1);

	m_stConfig.m_iIpAccessDefault = IPAccessStu::Deny;
	if (strcmp(szIpAccess,"ACCESS")==0)
	{
		m_stConfig.m_iIpAccessDefault = IPAccessStu::Access;
	}
	if (m_stConfig.m_iIpAccessOpen)
	{
		LoadIPAccess(m_stConfig.m_szIpAccessTable);
	}
	
	//bind cpu
	memset(m_stConfig.m_aiBindCpu,-1,sizeof(m_stConfig.m_aiBindCpu));
	if(szBindCpu[0])
	{
		vector < string > vecContent;
		SplitTag(szBindCpu,";",vecContent);
		for(int i=0;i<(int)vecContent.size();i++)
		{
			m_stConfig.m_aiBindCpu[i] = atoi(vecContent[i].c_str());
		}	
	}
	
	for (int i=-1; i<MAX_BIND_PORT_NUM; i++)
	{
		char szName[64];
		char szContent[1024] = {0};
		
		if(i == -1)
		{
			sprintf(szName,"AdminIp");
			TLib_Cfg_GetConfig(szCfgFile,
				szName, CFG_STRING, szContent, "", sizeof(szContent),
				NULL);
			
			if(strlen(szContent) == 0) 
				continue;
		}
		else
		{
			sprintf(szName,"TcpIPAddr%d",i+1);
			TLib_Cfg_GetConfig(szCfgFile,
							szName, CFG_STRING, szContent, "", sizeof(szContent),
							NULL);		
			
			if(strlen(szContent) == 0)
				break;			
		}
	
		vector<string> vecContent;
		SplitTag(szContent,":",vecContent);

		if(vecContent.size() < 2)
			continue;

		int iPort = atoi(vecContent[1].c_str());
		if(i == -1)
			m_stConfig.m_iAdminPort = iPort;
		
		m_stConfig.m_stTcpIPPortStu[iPort].m_unIP = inet_addr(vecContent[0].c_str());
		m_stConfig.m_stTcpIPPortStu[iPort].m_usPort = iPort;
		m_stConfig.m_stTcpIPPortStu[iPort].net_complete_func = NULL;

		printf("\nTcp Port %d ",iPort);
		if(vecContent.size() >= 3 && vecContent[2].length() > 0)
		{
			m_stConfig.m_stTcpIPPortStu[iPort].net_complete_func = LoadCompleteSo((char*)vecContent[2].c_str());
			msg_header_len msg_header_len_func = (msg_header_len)LoadHeaderLenSo((char*)vecContent[2].c_str());
			m_stConfig.m_stTcpIPPortStu[iPort].m_iMsgHeaderLen = msg_header_len_func();
			printf("so %s,header len %d ",(char*)vecContent[2].c_str(),m_stConfig.m_stTcpIPPortStu[iPort].m_iMsgHeaderLen);
		}	

		if(vecContent.size() >= 4)
		{
			m_stConfig.m_stTcpIPPortStu[iPort].m_iLimitTps = atoi(vecContent[3].c_str());
			printf("Limit Tps %d ",m_stConfig.m_stTcpIPPortStu[iPort].m_iLimitTps);
		}

		if(vecContent.size() >= 5)
		{
			m_stConfig.m_aiTcpPortAuxMask[iPort] = (unsigned short)atoi(vecContent[4].c_str());
			printf("Bind aux %d ",m_stConfig.m_aiTcpPortAuxMask[iPort]);
		}
		printf("\n");

		delete m_stConfig.m_stTcpIPPortStu[iPort].m_pCLoadGrid;
		m_stConfig.m_stTcpIPPortStu[iPort].m_pCLoadGrid = new CLoadGrid(
			m_stConfig.m_iLoadCheckAllSpan,
			m_stConfig.m_iLoadCheckEachSpan,
			m_stConfig.m_stTcpIPPortStu[iPort].m_iLimitTps*(m_stConfig.m_iLoadCheckAllSpan/1000));	

		m_stConfig.m_stTcpIPPortStu[iPort].m_pCLoadGrid->OpenLock();

		if(i != -1)
			memcpy(&m_stConfig.m_stTcpUsedIPPort[i],&m_stConfig.m_stTcpIPPortStu[iPort],sizeof(TConfig::TIpPortStu));
	}

	for (int i=0; i<MAX_BIND_PORT_NUM; i++)
	{
		m_stConfig.m_stUdpIPPortStu[i].m_unIP = 0;
		m_stConfig.m_stUdpIPPortStu[i].m_usPort = 0;
		
		char szName[64];
		sprintf(szName,"UdpIPAddr%d",i+1);
		char szContent[1024] = {0};
		
		TLib_Cfg_GetConfig(szCfgFile,
						szName, CFG_STRING, szContent, "", sizeof(szContent),
						NULL);
		if(strlen(szContent) == 0)
			break;
		
		vector<string> vecContent;
		SplitTag(szContent,":",vecContent);

		if(vecContent.size() < 2)
			continue;

		int iPort = atoi(vecContent[1].c_str());

		m_stConfig.m_stUdpIPPortStu[iPort].m_unIP = inet_addr(vecContent[0].c_str());
		m_stConfig.m_stUdpIPPortStu[iPort].m_usPort = iPort;

		printf("\nUdp Port %d ",iPort);
		if(vecContent.size() >= 4)
		{
			m_stConfig.m_stUdpIPPortStu[iPort].m_iLimitTps = atoi(vecContent[3].c_str());	
			printf("Limit Tps %d ",m_stConfig.m_stUdpIPPortStu[iPort].m_iLimitTps);			
		}
		printf("\n");
		
		delete m_stConfig.m_stUdpIPPortStu[iPort].m_pCLoadGrid;
		m_stConfig.m_stUdpIPPortStu[iPort].m_pCLoadGrid = new CLoadGrid(
			m_stConfig.m_iLoadCheckAllSpan,
			m_stConfig.m_iLoadCheckEachSpan,
			m_stConfig.m_stUdpIPPortStu[iPort].m_iLimitTps*(m_stConfig.m_iLoadCheckAllSpan/1000));	

		m_stConfig.m_stUdpIPPortStu[iPort].m_pCLoadGrid->OpenLock();
		
		if(i != -1)
			memcpy(&m_stConfig.m_stUdpUsedIPPort[i],&m_stConfig.m_stUdpIPPortStu[iPort],sizeof(TConfig::TIpPortStu));		
	}

	m_stConfig.Print();
	return 0;
}
int CMainCtrl::LoadIPAccess(char *pFileName)
{
	std::vector<std::vector<std::string> > vecCfgRow;
	if (LoadGridCfgFile(pFileName,vecCfgRow))
		return -1;

	if(vecCfgRow.size() == 0)
		return 0;

	if (m_pIPAcl)
		delete m_pIPAcl;
	
	m_pIPAcl = new IPAccessStu[vecCfgRow.size()];
	m_iIpAclNum = 0;

	for (int i=0; i<(int)vecCfgRow.size(); i++)
	{
		vector<std::string> &vecCfgCol = vecCfgRow[i];
		string strIP = vecCfgCol[0];
		string strWay = vecCfgCol[1];

		if(strIP.length()>15)
		{
			printf("Bad IP %s in file %s\n",strIP.c_str(),pFileName);
			return -2;
		}
		memset(&m_pIPAcl[m_iIpAclNum],0,sizeof(IPAccessStu));
		strcpy(m_pIPAcl[m_iIpAclNum].szIP,strIP.c_str());

		if (strWay == "ACCESS")
			m_pIPAcl[m_iIpAclNum].sAccessWay = IPAccessStu::Access;
		else
			m_pIPAcl[m_iIpAclNum].sAccessWay = IPAccessStu::Deny;

		printf("%s %s\n",strIP.c_str(),strWay.c_str());
		m_iIpAclNum++;	
	}
	return 0;
}
int CMainCtrl::CheckIPAccess(unsigned int unClientIP)
{
	if (!m_stConfig.m_iIpAccessOpen)
	{
		return IPAccessStu::Access;
	}
	
	char szClientIP[32]={0};
	struct in_addr in;
	in.s_addr = unClientIP;
	inet_ntop(AF_INET,&in,szClientIP,sizeof(szClientIP));
	
	for (int pos=0; pos<m_iIpAclNum;pos++)
	{
		if(IsIpMatch(szClientIP,m_pIPAcl[pos].szIP))
		{
			return m_pIPAcl[pos].sAccessWay;
		}
	}

	return m_stConfig.m_iIpAccessDefault ;
}
bool CMainCtrl::IsIpMatch(char* pIP1,char* pIP2)
{
	char szIP1[17];
	char szIP2[17];
	strcpy(szIP1,pIP1);
	strcat(szIP1,".");

	strcpy(szIP2,pIP2);
	strcat(szIP2,".");
	
	char *pTmpIP1 = szIP1;
	char *pTmpIP2 = szIP2;

	for (int i=0; i<4 ;i++)
	{
		char* pDot1 = strchr(pTmpIP1,'.');
		char* pDot2 = strchr(pTmpIP2,'.');	
		if(!pDot1)
		{
			printf("Bad IP %s\n",pIP1);
			return false;
		}
		if(!pDot2)
		{
			printf("Bad IP %s\n",pIP2);
			return false;
		}
		*pDot1 = 0;
		*pDot2 = 0;
	
		if(0 == strcmp(pTmpIP1,"*") || 0 == strcmp(pTmpIP2,"*") ||
			0 == strcmp(pTmpIP1,pTmpIP2)	)
		{
			;
		}
		else
		{
			return false;
		}
		pTmpIP1 = pDot1+1;
		pTmpIP2 = pDot2+1;		
	}

	return true;
}
int CMainCtrl::Initialize(char* pProName,char* pConfigFile)
{
	//读配置
	strcpy(m_stConfig.m_szSvrName,pProName);
	int iRet = ReadCfgFile(pConfigFile);
	if( 0 != iRet )
	{
		return iRet;
	}
	//日志开关
	char szTmp[256];
	sprintf(szTmp,"touch ./.%s_ctrllog",m_stConfig.m_szSvrName);
	system(szTmp);
	sprintf(szTmp,"./.%s_ctrllog",m_stConfig.m_szSvrName);

	int iErrNo = 0;
	bool bNewCreate = false;
	m_stConfig.m_pShmLog = (TShmLog*)CreateShm(ftok(szTmp, 'L'),sizeof(TShmLog),iErrNo,bNewCreate,true);
	if( !m_stConfig.m_pShmLog )
	{
		printf("Alloc shared memory for Log failed ErrorNo=%d, ERR=%s\n",iErrNo,strerror(iErrNo));
		return -4;
	}
	if (bNewCreate)
	{
		memset((char*)m_stConfig.m_pShmLog,0,sizeof(TShmLog));
		m_stConfig.m_pShmLog->m_iLogLevel = ERRORLOG;
	}

	sprintf(szTmp,"../log/%s",m_stConfig.m_szSvrName);
	LogInit(szTmp,m_stConfig.m_iMaxLogSize, m_stConfig.m_iMaxLogNum);
	//读so
	if(m_stConfig.m_szNetCompleteSo[0])
	{
		void* pHandle = dlopen(m_stConfig.m_szNetCompleteSo, RTLD_LAZY);
		if(pHandle)
		{
			dlerror();  	//Clear any existing error 
			default_net_complete_func = (check_complete)dlsym(pHandle, "net_complete_func");
			if (dlerror() != NULL)
			{
				printf("get net_complete_func from %s failed!\n",m_stConfig.m_szNetCompleteSo);
				return -1;
			}
			msg_header_len msg_header_len_func = (msg_header_len)dlsym(pHandle, "msg_header_len");
			if (dlerror() != NULL)
			{
				printf("get msg_header_len from %s failed!\n",m_stConfig.m_szNetCompleteSo);
				return -1;
			}
			i_default_msg_header_len = msg_header_len_func();
		}	
		else
		{
			printf("dlopen %s failed!\n",m_stConfig.m_szNetCompleteSo);
			return -1;	
		}
	}
	if(!default_net_complete_func)
	{
		for (int i=0; i<MAX_BIND_PORT_NUM; i++)
		{
			if(m_stConfig.m_stTcpUsedIPPort[i].m_unIP == 0)
				break;
			
			if(!m_stConfig.m_stTcpUsedIPPort[i].net_complete_func)
			{
				printf("Tcp Port %d complete func missing!\n",(int)m_stConfig.m_stTcpUsedIPPort[i].m_usPort);
				return -1;	
			}
		}

		for (int i=0; i<MAX_BIND_PORT_NUM; i++)
		{
			if(m_stConfig.m_stUdpUsedIPPort[i].m_unIP == 0)
				break;
			
			if(!m_stConfig.m_stUdpUsedIPPort[i].net_complete_func)
			{
				printf("Udp Port %d complete func missing!\n",(int)m_stConfig.m_stUdpUsedIPPort[i].m_usPort);
				return -1;	
			}
		}		
	}
	
	//admin so
	if(m_stConfig.m_szAdminSo[0] && m_stConfig.m_iAdminPort)
	{
		void* pHandle = dlopen(m_stConfig.m_szAdminSo, RTLD_LAZY);
		if(pHandle)
		{
			dlerror();  	//Clear any existing error 
			m_stConfig.m_admin_msg_func = (admin_msg_func)dlsym(pHandle, "do_msg");
			if (dlerror() != NULL)
			{
				printf("get do_msg from %s failed!\n",m_stConfig.m_szAdminSo);
				return -1;
			}
		}	
	}

	long long ullMemCost = 0;
	
	//初始化socket组
	m_pListenSocket = new TSocketNode[m_stConfig.MAX_SOCKET_NUM];
	for(int i = 0; i < m_stConfig.MAX_SOCKET_NUM; i++ )
	{
		memset(&m_pListenSocket[i],0,sizeof(TSocketNode));
		m_pListenSocket[i].m_iSocket  = -1;
	}	
	ullMemCost += (long long)sizeof(TSocketNode)*(long long)m_stConfig.MAX_SOCKET_NUM;

	//连接资源限制,root用户下有效
	rlimit rlim;
	rlim.rlim_cur = m_stConfig.MAX_SOCKET_NUM+10240;
	rlim.rlim_max = m_stConfig.MAX_SOCKET_NUM+10240;
	if(setrlimit(RLIMIT_NOFILE, &rlim))
	{
		printf("ERR:CCS setrlimit failed,please use root to start!\n");
		return -1;
	}	
	
	//创建管道
	if(CCodeQueueMutil::CreateMQByFile(m_stConfig.m_szMeToSvrMQ,&m_Me2SvrPipe))
	{
		printf("CreateCodeMQ %s failed!\n",m_stConfig.m_szMeToSvrMQ);
		return -1;	
	}
	
	if(CCodeQueueMutil::CreateMQByFile(m_stConfig.m_szSvrToMeMQ,&m_Svr2MePipe))
	{
		printf("CreateCodeMQ %s failed!\n",m_stConfig.m_szSvrToMeMQ);
		return -1;	
	}

	if(m_Me2SvrPipe.GetCodeQueueNum() != m_Svr2MePipe.GetCodeQueueNum())
	{
		printf("ERR:Me2SvrPipe is %d CQ, but Svr2MePipe is %d CQ!\n",m_Me2SvrPipe.GetCodeQueueNum(),m_Svr2MePipe.GetCodeQueueNum());
		return -1;	
	}
	
	m_stConfig.m_iAuxHandleNum = m_Me2SvrPipe.GetCodeQueueNum();

	//初始化线程
	unsigned long long llMemCost = 0;
	for (int i=0; i<m_stConfig.m_iAuxHandleNum; i++)
	{
		m_pAuxHandle[i] = new CAuxHandle;
		m_pAuxHandle[i]->Initialize(this,i);
		m_pAuxHandle[i]->CreateThread();	

		llMemCost += m_pAuxHandle[i]->m_ullMemCost;
		printf("Create Aux Handle %d\n",i);
	}
	
	//初始化端口
	if (InitSocket())
	{
		printf("InitSocket error!\n");
		return -1;
	}
	
	printf("Server Init Success! Cost Mem %llu bytes.\n",llMemCost);
	TLib_Log_LogMsg("Server Init Success!\n");
	return 0;
}

int CMainCtrl::WriteStat()
{	
	int tNow = m_tNow.tv_sec;
	static int siLastStatTime = time(NULL);
	if (tNow - siLastStatTime < m_stConfig.m_iStatTimeGap)
	{
		return 0;
	}
	siLastStatTime = tNow;
	
	unsigned int unTcpClientNum = 0;
	memset(&m_stStat,0,sizeof(m_stStat));
	memset(&m_stAllStat,0,sizeof(m_stAllStat));
	
	//all aux
	for (int i=0; i<m_stConfig.m_iAuxHandleNum; i++)
	{
		unTcpClientNum += m_pAuxHandle[i]->m_unTcpClientNum;
		
		m_stStat.m_llTcpCSPkgNum += m_pAuxHandle[i]->m_stStat.m_llTcpCSPkgNum;
		m_stStat.m_llTcpCSPkgLen += m_pAuxHandle[i]->m_stStat.m_llTcpCSPkgLen;
		m_stStat.m_llTcpSCPkgNum += m_pAuxHandle[i]->m_stStat.m_llTcpSCPkgNum;
		m_stStat.m_llTcpSCPkgLen += m_pAuxHandle[i]->m_stStat.m_llTcpSCPkgLen;

		m_stStat.m_llUdpCSPkgNum += m_pAuxHandle[i]->m_stStat.m_llUdpCSPkgNum;
		m_stStat.m_llUdpCSPkgLen += m_pAuxHandle[i]->m_stStat.m_llUdpCSPkgLen;
		m_stStat.m_llUdpSCSuccPkgNum += m_pAuxHandle[i]->m_stStat.m_llUdpSCSuccPkgNum;
		m_stStat.m_llUdpSCFailedPkgNum += m_pAuxHandle[i]->m_stStat.m_llUdpSCFailedPkgNum;
		m_stStat.m_llUdpSCPkgLen += m_pAuxHandle[i]->m_stStat.m_llUdpSCPkgLen;

		m_stAllStat.m_llTcpCSPkgNum += m_pAuxHandle[i]->m_stAllStat.m_llTcpCSPkgNum;
		m_stAllStat.m_llTcpCSPkgLen += m_pAuxHandle[i]->m_stAllStat.m_llTcpCSPkgLen;
		m_stAllStat.m_llTcpSCPkgNum += m_pAuxHandle[i]->m_stAllStat.m_llTcpSCPkgNum;
		m_stAllStat.m_llTcpSCPkgLen += m_pAuxHandle[i]->m_stAllStat.m_llTcpSCPkgLen;

		m_stAllStat.m_llUdpCSPkgNum += m_pAuxHandle[i]->m_stAllStat.m_llUdpCSPkgNum;
		m_stAllStat.m_llUdpCSPkgLen += m_pAuxHandle[i]->m_stAllStat.m_llUdpCSPkgLen;
		m_stAllStat.m_llUdpSCSuccPkgNum += m_pAuxHandle[i]->m_stAllStat.m_llUdpSCSuccPkgNum;
		m_stAllStat.m_llUdpSCFailedPkgNum += m_pAuxHandle[i]->m_stAllStat.m_llUdpSCFailedPkgNum;
		m_stAllStat.m_llUdpSCPkgLen += m_pAuxHandle[i]->m_stAllStat.m_llUdpSCPkgLen;
	}

	char szTmp[256];
	sprintf(szTmp,"../log/%s_stat",m_stConfig.m_szSvrName);
	TLib_Log_WriteLog(szTmp,20000000,10, "========Stat in %ds==========\n"
		"TcpClientNum		%u\n"
		"TcpCSPkgNum		%lld\n"
		"TcpCSPkgLen			%.3fk\n"
		"TcpSCPkgNum		%lld\n"
		"TcpSCPkgLen			%.3fk\n"
		"UdpCSPkgNum		%lld\n"
		"UdpCSPkgLen		%.3fk\n"
		"UdpSCSuccPkgNum	%lld\n"
		"UdpSCFailedPkgNum	%lld\n"
		"UdpSCPkgLen		%.3fk\n",
		m_stConfig.m_iStatTimeGap,
		unTcpClientNum,
		m_stStat.m_llTcpCSPkgNum,
		(float)(m_stStat.m_llTcpCSPkgLen/(float)1024),
		m_stStat.m_llTcpSCPkgNum,
		(float)(m_stStat.m_llTcpSCPkgLen/(float)1024),
		m_stStat.m_llUdpCSPkgNum,
		(float)(m_stStat.m_llUdpCSPkgLen/(float)1024),
		m_stStat.m_llUdpSCSuccPkgNum,
		m_stStat.m_llUdpSCFailedPkgNum,
		(float)(m_stStat.m_llUdpSCPkgLen/(float)1024)
		);

	//by aux
	for (int i=0; i<m_stConfig.m_iAuxHandleNum; i++)
	{
		ssize_t iBuffUsed,iBuffCount,iObjUsed,iObjCount;
		m_pAuxHandle[i]->m_stBuffMngRecv.GetBufferUsage(iBuffUsed,iBuffCount,iObjUsed,iObjCount);
		float fBuffMngRecvUasge = (iBuffUsed/(float)iBuffCount) > (iObjUsed/(float)iObjCount) ? 
									(iBuffUsed/(float)iBuffCount) : (iObjUsed/(float)iObjCount);

		ssize_t iBuffSndUsed,iBuffSndCount,iObjSndUsed,iObjSndCount;
		m_pAuxHandle[i]->m_stBuffMngSend.GetBufferUsage(iBuffSndUsed,iBuffSndCount,iObjSndUsed,iObjSndCount);
		float fBuffMngSendUasge = (iBuffSndUsed/(float)iBuffSndCount) > (iObjSndUsed/(float)iObjSndCount) ? 
									(iBuffSndUsed/(float)iBuffSndCount) : (iObjSndUsed/(float)iObjSndCount);	

		TLib_Log_WriteLog(szTmp,20000000,10, 
			"Aux %d:\n"
			"TcpClientNum	%d\n"
			"TcpCSPkgNum		%lld\n"
			"TcpCSPkgLen			%.3fk\n"
			"TcpSCPkgNum		%lld\n"
			"TcpSCPkgLen			%.3fk\n"	
			"UdpCSPkgNum		%lld\n"
			"UdpCSPkgLen		%.3fk\n"
			"UdpSCSuccPkgNum	%lld\n"
			"UdpSCFailedPkgNum	%lld\n"
			"UdpSCPkgLen		%.3fk\n"			
			"BuffRecv			%.2f		Hash %lld/%lld		Obj %lld/%lld\n"
			"BuffSend			%.2f		Hash %lld/%lld		Obj %lld/%lld\n\n",
			i,
			m_pAuxHandle[i]->m_unTcpClientNum,
			m_pAuxHandle[i]->m_stStat.m_llTcpCSPkgNum,
			(float)(m_pAuxHandle[i]->m_stStat.m_llTcpCSPkgLen/(float)1024),
			m_pAuxHandle[i]->m_stStat.m_llTcpSCPkgNum,
			(float)(m_pAuxHandle[i]->m_stStat.m_llTcpSCPkgLen/(float)1024),	
			m_pAuxHandle[i]->m_stStat.m_llUdpCSPkgNum,
			(float)(m_pAuxHandle[i]->m_stStat.m_llUdpCSPkgLen/(float)1024),
			m_pAuxHandle[i]->m_stStat.m_llUdpSCSuccPkgNum,
			m_pAuxHandle[i]->m_stStat.m_llUdpSCFailedPkgNum,
			(float)(m_pAuxHandle[i]->m_stStat.m_llUdpSCPkgLen/(float)1024),			
			fBuffMngRecvUasge*100,(long long)iBuffUsed,(long long)iBuffCount,(long long)iObjUsed,(long long)iObjCount,
			fBuffMngSendUasge*100,(long long)iBuffSndUsed,(long long)iBuffSndCount,(long long)iObjSndUsed,(long long)iObjSndCount
			);

		memset(&(m_pAuxHandle[i]->m_stStat),0,sizeof(m_pAuxHandle[i]->m_stStat));
	}

	return 0;
}

int CMainCtrl::SetRunFlag(int iRunFlag)
{
	m_iRunFlag = iRunFlag;
	return 0;
}

int CMainCtrl::CreateSocketNode(int iSocket,int iType,unsigned int unClientIP, 
			unsigned short usClientPort,unsigned short usListenPort)
{
	if (iSocket<0)
		return -1;
	
	//加入数组
	int iPos = (iSocket%m_stConfig.MAX_SOCKET_NUM);
	if (m_pListenSocket[iPos].m_iSocket > 0)
		return -2;

	m_pListenSocket[iPos].m_iSocket = iSocket;
	m_pListenSocket[iPos].m_sSocketType = iType;
	m_pListenSocket[iPos].m_unClientIP = unClientIP;
	m_pListenSocket[iPos].m_usClientPort = usClientPort;
	m_pListenSocket[iPos].m_usListenPort = usListenPort;
	m_pListenSocket[iPos].m_iActiveTime = m_tNow.tv_sec;
	m_pListenSocket[iPos].m_unID = 0;

	//add by nekeyzhong for accept can not inherit O_NONBLOCK 2010-01-19
	fcntl(iSocket,F_SETFL,fcntl(iSocket,F_GETFL) | O_NONBLOCK | O_NDELAY);
	m_stEPollFlowListen.Add(iSocket,iPos,EPOLLIN | EPOLLERR | EPOLLHUP);
	return iPos;
}

int CMainCtrl::CheckClientMessage()
{
	long long llKey;
	unsigned int unEvents;
	while(m_stEPollFlowListen.GetEvents(llKey,unEvents))
	{
		int iSocketSuffix = llKey;
		TSocketNode* pSocketNode = &m_pListenSocket[iSocketSuffix];  
		if (pSocketNode->m_iSocket < 0)
		{
			TLib_Log_LogMsg("ERR:GetEvents.fd = %d\n",pSocketNode->m_iSocket);
			continue;
		}	

		//有输入
		if (EPOLLIN & unEvents)
		{
			if (TSocketNode::TCP_LISTEN_SOCKET == pSocketNode->m_sSocketType)
			{
				struct sockaddr_in  stSockAddr;  
				int iSockAddrSize = sizeof(sockaddr_in);
	    		int iNewSocket = accept(pSocketNode->m_iSocket, (struct sockaddr *)&stSockAddr, (socklen_t*)&iSockAddrSize);

				if(0 > iNewSocket)
					continue;

				if (CheckIPAccess(stSockAddr.sin_addr.s_addr) != IPAccessStu::Access)
				{
					TLib_Log_LogMsg("ERR:CheckIPAccess %s return deny!\n",inet_ntoa(stSockAddr.sin_addr));
					close(iNewSocket);
					continue;
				}
				
				TMQHeadInfo stMQHeadInfo;
				stMQHeadInfo.m_unClientIP = stSockAddr.sin_addr.s_addr;
				stMQHeadInfo.m_usClientPort = stSockAddr.sin_port;
				stMQHeadInfo.m_ucCmd = TMQHeadInfo::CMD_NEW_TCP_SOCKET;
				stMQHeadInfo.m_iSuffix = iNewSocket;
				stMQHeadInfo.m_usSrcListenPort = pSocketNode->m_usListenPort;

				int iDispatchAuxId = SelectAuxThreadForClient(iNewSocket,pSocketNode->m_usListenPort);
				if(iDispatchAuxId < 0)
				{
					TLib_Log_LogMsg("ERR:SelectAuxThreadForClient return no thread for selected in port %d!\n",pSocketNode->m_usListenPort);
					close(iNewSocket);
					continue;
				}

				m_pAuxHandle[iDispatchAuxId]->GetCodeQueue()->AppendOneCode((char*)&stMQHeadInfo,sizeof(stMQHeadInfo));
			}								
			else
			{
				TLib_Log_LogMsg("ERR:No this socket type %d!\n",(int)pSocketNode->m_sSocketType);
			}

			pSocketNode->m_iActiveTime = m_tNow.tv_sec;
		}
	}
	return 0;
}

int CMainCtrl::InitSocket()
{
	if(m_stEPollFlowListen.Create(m_stConfig.MAX_SOCKET_NUM))
	{
		printf("epoll create error!");
		return -1;
	}

	int iSocketSndBuff = 0;
	int iSocketRcvBuff = 0;
	
	//tcp 监听端口
	for (int i = 0; i < MAX_BIND_PORT_NUM; i++)
	{
		if(m_stConfig.m_stTcpIPPortStu[i].m_usPort == 0)
			continue;
		
		unsigned short usListenPort = m_stConfig.m_stTcpIPPortStu[i].m_usPort;
		unsigned int unListenIP = m_stConfig.m_stTcpIPPortStu[i].m_unIP;
		
		int iListenSocket = CreateListenSocket(unListenIP,usListenPort,
						m_stConfig.SOCKET_RCVBUF,m_stConfig.SOCKET_SNDBUF,0,m_stConfig.LISTEN_BACK_LOG);
		if (iListenSocket < 0)
		{
			printf("CreateListenSocket %d.%d.%d.%d:%u failed!\n",NIPQUAD(unListenIP),(unsigned int)usListenPort);
			TLib_Log_LogMsg("CreateListenSocket %d.%d.%d.%d:%u failed!\n",NIPQUAD(unListenIP),(unsigned int)usListenPort);
			return -2;		
		}

		socklen_t iLen =  4;
		getsockopt(iListenSocket, SOL_SOCKET, SO_SNDBUF, (void*)&iSocketSndBuff, (socklen_t*)&iLen);
		getsockopt(iListenSocket, SOL_SOCKET, SO_RCVBUF, (void*)&iSocketRcvBuff, (socklen_t*)&iLen);

		//加入监听数组
		int iNewSuffix = -1;
		iNewSuffix = CreateSocketNode(iListenSocket,TSocketNode::TCP_LISTEN_SOCKET,unListenIP,0,usListenPort);
		if (iNewSuffix < 0)
		{
			close(iListenSocket);
			printf("add to socket array failed!\n");
			TLib_Log_LogMsg("add to socket array failed!\n");
			return -3;
		}		
		printf("\nTcp Listen on %d.%d.%d.%d:%u Success!\n", NIPQUAD(unListenIP), (unsigned int)usListenPort);
	}
	printf("TCP SO_SNDBUF:%d, SO_RCVBUF:%d\n", iSocketSndBuff, iSocketRcvBuff);

	//udp 绑定端口
	for (int i = 0; i < MAX_BIND_PORT_NUM; i++)
	{
		unsigned short usListenPort = m_stConfig.m_stUdpIPPortStu[i].m_usPort;
		unsigned int unListenIP = m_stConfig.m_stUdpIPPortStu[i].m_unIP;
		if (usListenPort == 0)
			continue;
		
		int iListenSocket = CreateListenSocket(unListenIP,usListenPort,300*1024,300*1024,1);
		if (iListenSocket < 0)
		{
			printf("CreateUdpSocket %d.%d.%d.%d:%u failed!\n",NIPQUAD(unListenIP),(unsigned int)usListenPort);
			TLib_Log_LogMsg("CreateUdpSocket %d.%d.%d.%d:%u failed!\n",NIPQUAD(unListenIP),(unsigned int)usListenPort);
			return -2;		
		}

		socklen_t iLen =  4;
		getsockopt(iListenSocket, SOL_SOCKET, SO_SNDBUF, (void*)&iSocketSndBuff, (socklen_t*)&iLen);
		getsockopt(iListenSocket, SOL_SOCKET, SO_RCVBUF, (void*)&iSocketRcvBuff, (socklen_t*)&iLen);

		TMQHeadInfo stMQHeadInfo;
		stMQHeadInfo.m_ucCmd = TMQHeadInfo::CMD_NEW_UDP_SOCKET;
		stMQHeadInfo.m_iSuffix = iListenSocket;
		stMQHeadInfo.m_usSrcListenPort = usListenPort;

		for(int i=0; i<(int)m_stConfig.m_iAuxHandleNum;i++)
		{
			m_pAuxHandle[i]->GetCodeQueue()->AppendOneCode((char*)&stMQHeadInfo,sizeof(stMQHeadInfo));
		}	
		printf("Udp Bind on %d.%d.%d.%d:%u Success! limit tps=%d\n",NIPQUAD(unListenIP),
						usListenPort,m_stConfig.m_stUdpIPPortStu[usListenPort].m_iLimitTps);
	}
	printf("UDP SO_SNDBUF:%d, SO_RCVBUF:%d\n", iSocketSndBuff, iSocketRcvBuff);
	return 0;
}

int CMainCtrl::SelectAuxThreadForClient(int iSocket, int iListenPort)
{
	int iValidAuxNum = 0;
	int aiValidAuxId[MAX_CODE_QUEUE_NUMBER];
	int iAuxMask = m_stConfig.m_aiTcpPortAuxMask[iListenPort];
	// check all valid aux
	for(int i = 0 ; i < m_stConfig.m_iAuxHandleNum ; i++)
	{
		if((iAuxMask>>i)&1)
		{
			aiValidAuxId[iValidAuxNum] = i;
			iValidAuxNum++;
		}
	}
	//select one valid aux
	unsigned short unMaxTcpClient = (unsigned short)(-1);
	int iSelectAuxId = -1;
	for(int i = 0 ; i < iValidAuxNum ; i++)
	{
		if(m_pAuxHandle[aiValidAuxId[i]]->m_unTcpClientNum < unMaxTcpClient)
		{
			iSelectAuxId = aiValidAuxId[i];
			unMaxTcpClient = m_pAuxHandle[aiValidAuxId[i]]->m_unTcpClientNum;
		}
	}
	return iSelectAuxId;
}

int CMainCtrl::LogInit(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum)
{
	sprintf(m_szLogFileBase,"%s",sLogBaseName);
	sprintf(m_szLogFileName,"%s.log",sLogBaseName);
	m_iMaxLogSize = lMaxLogSize;
	m_iMaxLogNum = iMaxLogNum;

	return 0;
}
void CMainCtrl::Log(int iKey,int iLevel,const char *sFormat, ...)
{
	int iWriteLog = 0;
	if((iKey!=0)&&(m_stConfig.m_pShmLog->m_iLogKey == iKey))
	{
		iWriteLog = 1;
	}

	//本次级别大于要求的级别
	if (iLevel <= m_stConfig.m_pShmLog->m_iLogLevel)
	{
		iWriteLog = 1;
	}

	if (!iWriteLog)
		return;


	char buf[1024];
	int len = 0;
	switch(iLevel)
	{
		case KEYLOG:
			len += snprintf(buf, sizeof(buf) - 1, "[KEYLOG]");
			break;
		case NOLOG:
			len += snprintf(buf, sizeof(buf) - 1, "[NOLOG]");
			break;
		case ERRORLOG:
			len += snprintf(buf, sizeof(buf) - 1, "[ERRORLOG]");
			break;
		case RUNLOG:
			len += snprintf(buf, sizeof(buf) - 1, "[RUNLOG]");
			break;
		case DEBUGLOG:
			len += snprintf(buf, sizeof(buf) - 1, "[DEBUGLOG]");
			break;
		default:
			len += snprintf(buf, sizeof(buf) - 1, "[UNKOWN%d]", iLevel);
			break;				
	}	
	va_list ap;
	va_start(ap, sFormat);
	len += vsnprintf(buf+len, sizeof(buf)-len - 1, sFormat, ap);
	va_end(ap);
	//如果没有以换行结束，追加一个换行
	if(len > 0 && buf[len - 1] != '\n')
	{
		strcat(buf, "\n");
	}
	TLib_Log_WriteLog(m_szLogFileBase, m_iMaxLogSize, m_iMaxLogNum, buf);
	return;
	
}

int CMainCtrl::Run()
{
	while(1)
	{
		if(unlikely(Flag_Exit == m_iRunFlag))
		{
			for (int i=0; i<m_stConfig.m_iAuxHandleNum; i++)
			{
				m_pAuxHandle[i]->StopThread();
			}
			for (int i=0; i<m_stConfig.m_iAuxHandleNum; i++)
			{
				while(m_pAuxHandle[i]->m_bHandleRun)	
					usleep(10);
			}				
			TLib_Log_LogMsg("Server Exit!\n");
			return 0;
		}
		if(unlikely(Flag_ReloadCfg == m_iRunFlag))
		{
			ReadCfgFile(m_stConfig.m_szCfgFileName);
			TLib_Log_LogMsg("reload config file ok!\n");
			m_iRunFlag = 0;
		}

		// 1ms
		m_stEPollFlowListen.Wait(1);

		//每次循环取一次
		gettimeofday(&m_tNow,NULL);
		
		//读取客户端包
		CheckClientMessage();       

		//写统计
		WriteStat();
	}
	
	return 0;
}

