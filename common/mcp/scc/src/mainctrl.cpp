#include "mainctrl.h"

int MakeKey(unsigned int unClientIP,unsigned short usClientPort,char szSrcMQ[MQ_NAME_LEN],
				char *pKey)
{
	memset(pKey,0,sizeof(int)+sizeof(short)+MQ_NAME_LEN);
	
	memcpy(pKey,&unClientIP,sizeof(int));
	memcpy(pKey+sizeof(int),&usClientPort,sizeof(short));
	strncpy(pKey+sizeof(int)+sizeof(short),szSrcMQ,MQ_NAME_LEN);
	return sizeof(int)+sizeof(short)+MQ_NAME_LEN;
}

void UnMakeKey(char *pKey,
		unsigned int &unClientIP,unsigned short &usClientPort,char szSrcMQ[MQ_NAME_LEN])
{
	memset(szSrcMQ,0,MQ_NAME_LEN);
	memcpy(&unClientIP,pKey,sizeof(int));
	memcpy(&usClientPort,pKey+sizeof(int),sizeof(short));
	strncpy(szSrcMQ,pKey+sizeof(int)+sizeof(short),MQ_NAME_LEN);
}

//设置TSocketNode的主键
ssize_t SetSocketNodeKey(void* pObj,void* pKey,ssize_t iKeyLen)
{
	TSocketNode* pSocketNode = (TSocketNode*)pObj;
	UnMakeKey((char*)pKey,pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
	return 0;
}
//获取TSocketNode的主键
ssize_t GetSocketNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen)
{
	TSocketNode* pSocketNode = (TSocketNode*)pObj;
	iKeyLen = (ssize_t)MakeKey(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ,(char*)pKey);
	return 0;
}
CMainCtrl::CMainCtrl()
{
	memset(&m_stStat,0,sizeof(m_stStat));
	memset(&m_stConfig,0,sizeof(m_stConfig));
	
	m_unTcpClientNum = 0;
	m_iRunFlag = 0;

	m_pCLoadGrid = NULL;
}

CMainCtrl::~CMainCtrl()
{
}

int CMainCtrl::ReadCfgFile(char* szCfgFile)
{
	TLib_Cfg_GetConfig(szCfgFile,
		"NetCompleteSo", CFG_STRING, m_stConfig.m_szNetCompleteSo, "",sizeof(m_stConfig.m_szNetCompleteSo),
		
		"MeToSvrMQ", CFG_STRING, m_stConfig.m_szMeToSvrMQ, "",sizeof(m_stConfig.m_szMeToSvrMQ),
		"SvrToMeMQ", CFG_STRING, m_stConfig.m_szSvrToMeMQ, "",sizeof(m_stConfig.m_szSvrToMeMQ),

		"StatisticGap", CFG_INT, &(m_stConfig.m_iStatTimeGap), 300,
		"DisconnectNotify", CFG_INT, &(m_stConfig.m_iDisconnectNotify), 0,

		"CpuAffinity", CFG_INT,&(m_stConfig.m_iBindCpu),-1,

		"AdminIp", CFG_STRING, m_stConfig.m_szAdminIp, "",sizeof(m_stConfig.m_szAdminIp),
		"AdminPort", CFG_INT, &(m_stConfig.m_iAdminPort), 0,
		
		"SOCKET_RCVBUF",CFG_INT, &(m_stConfig.SOCKET_RCVBUF),-1,
		"SOCKET_SNDBUF",CFG_INT, &(m_stConfig.SOCKET_SNDBUF),-1,
		
		"MAX_SOCKET_NUM",CFG_INT, &(m_stConfig.MAX_SOCKET_NUM),1024,	
		"RCV_BLOCK_SIZE",CFG_INT, &(m_stConfig.RCV_BLOCK_SIZE),1024,
		"RCV_BLOCK_NUM",CFG_INT, &(m_stConfig.RCV_BLOCK_NUM),1024,
		
		"SND_BLOCK_SIZE",CFG_INT, &(m_stConfig.SND_BLOCK_SIZE),1024,
		"SND_BLOCK_NUM",CFG_INT, &(m_stConfig.SND_BLOCK_NUM),1024,
		
		"MaxQueueWaitus", CFG_INT, &(m_stConfig.m_iMaxQueueWaitus),0,

		"TimeOutCheckSecs", CFG_INT, &(m_stConfig.m_iTimeOutCheckSecs), 0,
		"TimeOutSecs", CFG_INT, &(m_stConfig.m_iTimeOutSecs), 0,	
		
		"LoadCheckAllSpan", CFG_INT, &(m_stConfig.m_iLoadCheckAllSpan), 0,
		"LoadCheckEachSpan", CFG_INT, &(m_stConfig.m_iLoadCheckEachSpan), 0,		
		NULL);

	strncpy(m_stConfig.m_szCfgFileName,szCfgFile,sizeof(m_stConfig.m_szCfgFileName)-1);

	m_stConfig.Print();

	delete m_pCLoadGrid;
	m_pCLoadGrid = new CLoadGrid(m_stConfig.m_iLoadCheckAllSpan,
							m_stConfig.m_iLoadCheckEachSpan);
	
	return 0;
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

	//连接资源限制,root用户下有效
	rlimit rlim;
	rlim.rlim_cur = m_stConfig.MAX_SOCKET_NUM+10240;
	rlim.rlim_max = m_stConfig.MAX_SOCKET_NUM+10240;
	setrlimit(RLIMIT_NOFILE, &rlim);	

	//bind cpu
	if(m_stConfig.m_iBindCpu >= 0)
	{
		cpu_set_t mask;
		int iCpuId = m_stConfig.m_iBindCpu;
		printf("set cpu affinity %d.\n",iCpuId);
		
		CPU_ZERO(&mask);
		CPU_SET(iCpuId, &mask);
		sched_setaffinity(0, sizeof(mask), &mask);
	}
	
	//读so
	void* pHandle = dlopen(m_stConfig.m_szNetCompleteSo, RTLD_LAZY);
	if (!pHandle)
	{
		printf("open %s failed!\n",m_stConfig.m_szNetCompleteSo);
		printf("use ccs/src/make -f makefile.complete to make these so,and choose the one you want.\n");
		return -1;
	}	
	dlerror();  	//Clear any existing error 
	net_complete_func = (check_complete)dlsym(pHandle, "net_complete_func");
	if (dlerror() != NULL)
	{
		printf("get net_complete_func from %s failed!\n",m_stConfig.m_szNetCompleteSo);
		return -1;
	}
	msg_header_len_func = (msg_header_len)dlsym(pHandle, "msg_header_len");
	if (dlerror() != NULL)
	{
		printf("get msg_header_len from %s failed!\n",m_stConfig.m_szNetCompleteSo);
		return -1;
	}
	m_iMsgHeaderLen = msg_header_len_func();

	//初始化epoll端口
	if(m_stEPollFlow.Create(m_stConfig.MAX_SOCKET_NUM))
	{
		printf("epoll create error!");
		return -1;
	}

	//创建管道
	if(CCodeQueue::CreateMQByFile(m_stConfig.m_szMeToSvrMQ,&m_Me2SvrPipe))
	{
		printf("CreateCodeMQ %s failed!\n",m_stConfig.m_szMeToSvrMQ);
		return -1;	
	}
	
	if(CCodeQueue::CreateMQByFile(m_stConfig.m_szSvrToMeMQ,&m_Svr2MePipe))
	{
		printf("CreateCodeMQ %s failed!\n",m_stConfig.m_szSvrToMeMQ);
		return -1;	
	}
	long long ullMemCost = 0;
	
	//创建CS消息缓冲区
	int iMemSize = TIdxObjMng::CountMemSize(m_stConfig.RCV_BLOCK_SIZE,m_stConfig.RCV_BLOCK_NUM,1);
	char* pMem = new char[iMemSize];
	m_stIdxObjMngRecv.AttachMem(pMem,iMemSize,m_stConfig.RCV_BLOCK_SIZE,m_stConfig.RCV_BLOCK_NUM,emInit,1);
	ullMemCost += (long long)iMemSize;

	iMemSize = CBuffMng::CountMemSize(m_stConfig.MAX_SOCKET_NUM);
	pMem = new char[iMemSize];	
	m_stBuffMngRecv.AttachMem(pMem,iMemSize,m_stConfig.MAX_SOCKET_NUM);
	ullMemCost += (long long)iMemSize;

	m_stBuffMngRecv.AttachIdxObjMng(&m_stIdxObjMngRecv);
	
	//创建SC消息缓冲区
	iMemSize = TIdxObjMng::CountMemSize(m_stConfig.SND_BLOCK_SIZE,m_stConfig.SND_BLOCK_NUM,1);
	pMem = new char[iMemSize];
	m_stIdxObjMngSend.AttachMem(pMem,iMemSize,m_stConfig.SND_BLOCK_SIZE,m_stConfig.SND_BLOCK_NUM,emInit,1);
	ullMemCost += (long long)iMemSize;

	iMemSize = CBuffMng::CountMemSize(m_stConfig.MAX_SOCKET_NUM);
	pMem = new char[iMemSize];	
	m_stBuffMngSend.AttachMem(pMem,iMemSize,m_stConfig.MAX_SOCKET_NUM);
	ullMemCost += (long long)iMemSize;

	m_stBuffMngSend.AttachIdxObjMng(&m_stIdxObjMngSend);

	//创建socket管理区
	iMemSize = TIdxObjMng::CountMemSize(sizeof(TSocketNode),m_stConfig.MAX_SOCKET_NUM,1);
	pMem = new char[iMemSize];
	m_stSocketNodeMng.AttachMem(pMem,iMemSize,sizeof(TSocketNode),m_stConfig.MAX_SOCKET_NUM,emInit,1);
	ullMemCost += (long long)iMemSize;

	iMemSize = CHashTab::CountMemSize(m_stConfig.MAX_SOCKET_NUM);
	pMem = new char[iMemSize];
	m_stSocketNodeHash.AttachMem(pMem, iMemSize,m_stConfig.MAX_SOCKET_NUM);
	ullMemCost += (long long)iMemSize;

	m_stSocketNodeHash.AttachIdxObjMng(&m_stSocketNodeMng,SetSocketNodeKey,GetSocketNodeKey);

	//内存管道监听
	if (m_Svr2MePipe.GetReadNotifyFD() >= 0)
	{
		iRet = CreateSocketNode(m_Svr2MePipe.GetReadNotifyFD(),TSocketNode::STATUS_OK,
							0,0,"_PIPE_",TSocketNode::PIPE_SOCKET);
		if(iRet < 0)
		{
			printf("CreateSocketNode Failed!iRet=%d\n",iRet);
			return iRet;
		}		
	}

	//管理端口
	if(m_stConfig.m_iAdminPort > 0)
	{
		unsigned short usListenPort = (unsigned short)m_stConfig.m_iAdminPort;
		unsigned int unListenIP = inet_addr(m_stConfig.m_szAdminIp);
		
		int iListenSocket = CreateListenSocket(unListenIP,usListenPort,
					m_stConfig.SOCKET_RCVBUF,m_stConfig.SOCKET_SNDBUF);
		if (iListenSocket < 0)
		{
			printf("CreateListenSocket %u:%u failed!\n",unListenIP,usListenPort);
			TLib_Log_LogMsg("CreateListenSocket %u:%u failed!\n",unListenIP,usListenPort);
			return -2;		
		}
		
		//加入监听数组
		int iNewSuffix = CreateSocketNode(iListenSocket,TSocketNode::STATUS_OK,
						unListenIP,usListenPort,"_ADMIN_",TSocketNode::ADMIN_LISTEN_SOCKET);
		if (iNewSuffix < 0)
		{
			close(iListenSocket);		
			printf("add to socket array failed!\n");
			TLib_Log_LogMsg("add to socket array failed!\n");
			return -3;
		}		

		printf("Admin Listen on %s:%u Success!\n", m_stConfig.m_szAdminIp, m_stConfig.m_iAdminPort);		
	}

	printf("%s Cost Mem %llu bytes.\n",SVR_NAME,ullMemCost);
	TLib_Log_LogMsg("Cost Mem %llu bytes.\n",ullMemCost);	
	printf("Server Init Success!\n");
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

	ssize_t iBuffUsed,iBuffCount,iObjUsed,iObjCount;
	m_stBuffMngRecv.GetBufferUsage(iBuffUsed,iBuffCount,iObjUsed,iObjCount);
	float fBuffMngRecvUasge = (iBuffUsed/(float)iBuffCount) > (iObjUsed/(float)iObjCount) ? 
								(iBuffUsed/(float)iBuffCount) : (iObjUsed/(float)iObjCount);

	ssize_t iBuffSndUsed,iBuffSndCount,iObjSndUsed,iObjSndCount;
	m_stBuffMngSend.GetBufferUsage(iBuffSndUsed,iBuffSndCount,iObjSndUsed,iObjSndCount);
	float fBuffMngSendUasge = (iBuffSndUsed/(float)iBuffSndCount) > (iObjSndUsed/(float)iObjSndCount) ? 
								(iBuffSndUsed/(float)iBuffSndCount) : (iObjSndUsed/(float)iObjSndCount);	

	char szTmp[256];
	sprintf(szTmp,"../log/%s_stat",m_stConfig.m_szSvrName);
	TLib_Log_WriteLog(szTmp,20000000,10, "======== Stat in %ds==========\n"
		"TcpClientNum		%u\n"
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
		"BuffSend			%.2f		Hash %lld/%lld		Obj %lld/%lld\n"
		"Svr2MePipe			%d/%d\n"
		"Me2SvrPipe			%d/%d\n\n",
		m_stConfig.m_iStatTimeGap,
		m_unTcpClientNum,
		m_stStat.m_llTcpCSPkgNum,
		(float)(m_stStat.m_llTcpCSPkgLen/(float)1024),
		m_stStat.m_llTcpSCPkgNum,
		(float)(m_stStat.m_llTcpSCPkgLen/(float)1024),
		m_stStat.m_llUdpCSPkgNum,
		(float)(m_stStat.m_llUdpCSPkgLen/(float)1024),
		m_stStat.m_llUdpSCSuccPkgNum,
		m_stStat.m_llUdpSCFailedPkgNum,
		(float)(m_stStat.m_llUdpSCPkgLen/(float)1024),
		fBuffMngRecvUasge*100,(long long)iBuffUsed,(long long)iBuffCount,(long long)iObjUsed,(long long)iObjCount,
		fBuffMngSendUasge*100,(long long)iBuffSndUsed,(long long)iBuffSndCount,(long long)iObjSndUsed,(long long)iObjSndCount,
		m_Svr2MePipe.GetCodeLength(),m_Svr2MePipe.GetSize(),
		m_Me2SvrPipe.GetCodeLength(),m_Me2SvrPipe.GetSize()
		);

	memset(&m_stStat,0,sizeof(m_stStat));
	return 0;
}

int CMainCtrl::FillMQHead(TMQHeadInfo* pMQHeadInfo,TSocketNode* pSocketNode,
			unsigned char ucCmd,unsigned char ucDataType/*=TMQHeadInfo::DATA_TYPE_TCP*/)
{
	//gettimeofday(&m_tNow,NULL);
	
	memset(pMQHeadInfo,0,sizeof(TMQHeadInfo));
	pMQHeadInfo->m_ucCmd = ucCmd;
	pMQHeadInfo->m_unClientIP = pSocketNode->m_unClientIP;
	pMQHeadInfo->m_usClientPort = pSocketNode->m_usClientPort;
	memcpy(pMQHeadInfo->m_szSrcMQ,SCC_MQ,sizeof(SCC_MQ));	
	memcpy(pMQHeadInfo->m_szDstMQ,pSocketNode->m_szSrcMQ,MQ_NAME_LEN);
	
	pMQHeadInfo->m_tTimeStampSec = m_tNow.tv_sec;
	pMQHeadInfo->m_tTimeStampuSec = m_tNow.tv_usec;

	pMQHeadInfo->m_ucDataType = ucDataType;
	
	if(pSocketNode->m_sSocketType == TSocketNode::UDP_SOCKET)
		pMQHeadInfo->m_ucDataType = TMQHeadInfo::DATA_TYPE_UDP;
	else
		pMQHeadInfo->m_ucDataType = TMQHeadInfo::DATA_TYPE_TCP;
	
	return 0;
}


int CMainCtrl::CheckTimeOut()
{
	time_t tNow = m_tNow.tv_sec;
	if (m_stConfig.m_iTimeOutCheckSecs)
	{
		int iDelNum = 0;
		vector<TSocketNode> VecDelNode;
		
		static time_t sTimeLast = time(NULL);
		if (tNow - sTimeLast > m_stConfig.m_iTimeOutCheckSecs)
		{
			sTimeLast = tNow;
			for (int i=0; i<m_stSocketNodeHash.GetBucketNum();i++)
			{
				ssize_t iObjIdx = m_stSocketNodeHash.GetBucketNodeHead(i);
				TSocketNode* pSocketNode = (TSocketNode*)m_stSocketNodeHash.GetObjectByIdx(iObjIdx);

				while(pSocketNode)
				{
					if ((tNow - pSocketNode->m_iActiveTime > m_stConfig.m_iTimeOutSecs) && 
						(pSocketNode->m_unClientIP>0) && (pSocketNode->m_usClientPort>0))
					{
						VecDelNode.push_back(*pSocketNode);
						iDelNum++;
					}	

					ssize_t iNextObjIdx = m_stSocketNodeHash.GetBucketNodeNext(iObjIdx);
					pSocketNode = (TSocketNode*)m_stSocketNodeHash.GetObjectByIdx(iNextObjIdx);
				}			
			}

			int iDelSize = VecDelNode.size();
			for (int i=0; i<iDelSize; i++)
			{
				TSocketNode &stSocketNode = VecDelNode[i];
				ClearSocketNode(stSocketNode.m_unClientIP,stSocketNode.m_usClientPort,stSocketNode.m_szSrcMQ);
			}		
		}
	}

	return 0;
}

int CMainCtrl::SetRunFlag(int iRunFlag)
{
	m_iRunFlag = iRunFlag;
	return 0;
}

TSocketNode* CMainCtrl::GetSocketNode(unsigned int unClientIP, unsigned short usClientPort,char* szSrcMQ)
{
	char szKey[1024];
	ssize_t iKeyLen = (ssize_t)MakeKey(unClientIP,usClientPort,szSrcMQ,szKey);
	ssize_t iObjIdx = -1;
	return  (TSocketNode*)m_stSocketNodeHash.GetObjectByKey((void *)szKey,iKeyLen,iObjIdx);
}

//return iObjIdx
int CMainCtrl::CreateSocketNode(int iSocket,int iStatus,
			unsigned int unClientIP, unsigned short usClientPort,char* szSrcMQ,short sSocketType)
{
	if (iSocket<0)
		return -1;

	//分配空间,返回对象位置
	char szKey[1024];
	ssize_t iKeyLen = (ssize_t)MakeKey(unClientIP,usClientPort,szSrcMQ,szKey);
	ssize_t iObjIdx = 0;
	TSocketNode* pSocketNode = (TSocketNode*)m_stSocketNodeHash.CreateObjectByKey(
								(void *)szKey,iKeyLen,iObjIdx);
	if (!pSocketNode)
		return -2;

	pSocketNode->m_iSocket = iSocket;
	pSocketNode->m_sStatus = iStatus;
	pSocketNode->m_unClientIP = unClientIP;
	pSocketNode->m_usClientPort = usClientPort;
	if(szSrcMQ)
		strncpy(pSocketNode->m_szSrcMQ,szSrcMQ,MQ_NAME_LEN);
	
	pSocketNode->m_sSocketType = sSocketType;
	
	pSocketNode->m_iActiveTime = m_tNow.tv_sec;		
	fcntl(iSocket, F_SETFL, fcntl(iSocket, F_GETFL) | O_NONBLOCK | O_NDELAY);

	if (iStatus != TSocketNode::STATUS_OK)
		m_stEPollFlow.Add(iSocket,iObjIdx, EPOLLIN |EPOLLOUT| EPOLLERR | EPOLLHUP);
	else
		m_stEPollFlow.Add(iSocket,iObjIdx, EPOLLIN | EPOLLERR | EPOLLHUP);

	if (sSocketType == TSocketNode::TCP_SOCKET)
		m_unTcpClientNum++;
	
	return iObjIdx;
}


int CMainCtrl::ClearSocketNode(unsigned int unClientIP, unsigned short usClientPort,char szSrcMQ[MQ_NAME_LEN])
{
	char szKey[1024];
	ssize_t iKeyLen = (ssize_t)MakeKey(unClientIP,usClientPort,szSrcMQ,szKey);
	ssize_t iObjIdx = -1;
	TSocketNode* pSocketNode =  (TSocketNode*)m_stSocketNodeHash.GetObjectByKey((void *)szKey,iKeyLen,iObjIdx);
	if (!pSocketNode)
		return -1;

	if (pSocketNode->m_iSocket < 0)
		return -2;

	if((pSocketNode->m_sSocketType != TSocketNode::TCP_SOCKET) &&
		(pSocketNode->m_sSocketType != TSocketNode::ADMIN_CLIENT_SOCKET))
		return -3;

	//just TCP socket can close;
	if (pSocketNode->m_sSocketType == TSocketNode::TCP_SOCKET)
	{
		//通知服务器关闭
		TMQHeadInfo stMQHeadInfo;
		if (pSocketNode->m_sStatus == TSocketNode::STATUS_CONNECTING)
		{
			FillMQHead(&stMQHeadInfo,pSocketNode,TMQHeadInfo::CMD_SCC_RSP_CONNFAIL);
			m_Me2SvrPipe.AppendOneCode((const char *)&stMQHeadInfo, sizeof(TMQHeadInfo));	
		}	
		else
		{
			FillMQHead(&stMQHeadInfo,pSocketNode,TMQHeadInfo::CMD_SCC_RSP_DISCONN);
			if (m_stConfig.m_iDisconnectNotify)
			{
				m_Me2SvrPipe.AppendOneCode((const char *)&stMQHeadInfo, sizeof(TMQHeadInfo));	
			}
		}	
		m_unTcpClientNum--;	
	}
	
	m_stEPollFlow.Del(pSocketNode->m_iSocket);
	close(pSocketNode->m_iSocket);
	pSocketNode->m_iSocket = -1;

	m_stBuffMngRecv.FreeBuffer(iObjIdx);
	m_stBuffMngSend.FreeBuffer(iObjIdx);
	m_stSocketNodeHash.DeleteObjectByKey((void *)szKey,iKeyLen);
	return 0;
}

int CMainCtrl::CheckClientMessage()
{
	long long llKey;
	unsigned int unEvents;
	while(m_stEPollFlow.GetEvents(llKey,unEvents))
	{
		int iNodeObjIdx = llKey;
		TSocketNode* pSocketNode = (TSocketNode*)m_stSocketNodeMng.GetAttachObj(iNodeObjIdx);
		if (!pSocketNode ||(pSocketNode->m_iSocket<0))
			continue;

		if (!((EPOLLIN | EPOLLOUT) & unEvents))
		{
			ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
			continue;
		}

		//有输入
		if (EPOLLIN & unEvents)
		{
			//ADMIN
			if (pSocketNode->m_sSocketType == TSocketNode::ADMIN_LISTEN_SOCKET)
			{
				struct sockaddr_in  stSockAddr;  
				int iSockAddrSize = sizeof(sockaddr_in);
	    		int iNewSocket = accept(pSocketNode->m_iSocket, (struct sockaddr *)&stSockAddr, (socklen_t*)&iSockAddrSize);

				if(0 > iNewSocket)	
					continue;

				int iNodeObjIdx = CreateSocketNode(iNewSocket,TSocketNode::STATUS_OK,
						stSockAddr.sin_addr.s_addr,ntohs(stSockAddr.sin_port),
						"_ADMIN_",TSocketNode::ADMIN_CLIENT_SOCKET);
		
				if (iNodeObjIdx < 0)
				{
					TLib_Log_LogMsg("ERR:ADMIN SOCKET Add to socket array failed!TcpClientNum %d\n",m_unTcpClientNum);
					close(iNewSocket);
				}
				continue;
			}	
			else if (TSocketNode::ADMIN_CLIENT_SOCKET == pSocketNode->m_sSocketType)
			{
				ProcessAdminMsg(iNodeObjIdx);
				continue;
			}	
			//MQ
			else if (pSocketNode->m_sSocketType == TSocketNode::PIPE_SOCKET)
			{
				//清除通知
				timeval tv;
				tv.tv_sec=0;
				tv.tv_usec=0;			
				m_Svr2MePipe.WaitData(&tv);
				
				CheckSvrMessage();
				continue;
			}
			//UDP
			else if(pSocketNode->m_sSocketType == TSocketNode::UDP_SOCKET)
			{
				for(int i=0; i<500;i++)
				{
					struct sockaddr_in addrClient;
					int iClientLen = sizeof(struct sockaddr_in);
					int iRecvBytes = recvfrom(pSocketNode->m_iSocket,
											m_szCodeBuff+sizeof(TMQHeadInfo),
											sizeof(m_szCodeBuff)-sizeof(TMQHeadInfo),
											0,(struct sockaddr *)&addrClient,(socklen_t*)&iClientLen);

					if(iRecvBytes <= 0)
						break;
					
					TMQHeadInfo* pMQHeadInfo = (TMQHeadInfo*)m_szCodeBuff;
					FillMQHead(pMQHeadInfo,pSocketNode,TMQHeadInfo::CMD_DATA_TRANS,
									TMQHeadInfo::DATA_TYPE_UDP);
					pMQHeadInfo->m_unClientIP = addrClient.sin_addr.s_addr;
					pMQHeadInfo->m_usClientPort = ntohs(addrClient.sin_port);
					
					int iRet = m_Me2SvrPipe.AppendOneCode((const char *)m_szCodeBuff, iRecvBytes+sizeof(TMQHeadInfo));
					if(0 > iRet)
					{
						TLib_Log_LogMsg("ERR:AppendOneCode() to Me2Svr Pipe failed! ret=%d, UDP![%s:%d]\n", iRet,__FILE__,__LINE__);
						break;
					}
					
					m_stStat.m_llUdpCSPkgNum++;
					m_stStat.m_llUdpCSPkgLen += iRecvBytes;				
				}
			}
			else
			{
				//TCP
				RecvClientData(iNodeObjIdx);			
			}
		}
		
		//有输出
		if (EPOLLOUT & unEvents)
		{
			if(pSocketNode->m_sStatus != TSocketNode::STATUS_OK)
			{
				//getscokopt check
				int iSockErr = 0;
				int iSockErrLen = sizeof(iSockErr);
				if (getsockopt(pSocketNode->m_iSocket, SOL_SOCKET, SO_ERROR,
							(char*)&iSockErr, (socklen_t*)&iSockErrLen))
				{		
					char szIPAddr[32]={0};
					NtoP(pSocketNode->m_unClientIP,szIPAddr);				
					TLib_Log_LogMsg("ERR:connect to %s:%d failed! errno=%d[%s:%d]\n",
									szIPAddr,pSocketNode->m_usClientPort,errno,__FILE__,__LINE__);
					ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
					continue;
				}			
			}
			
			//主动连接完毕则通知
			if (pSocketNode->m_sStatus == TSocketNode::STATUS_CONNECTING)
			{
				pSocketNode->m_sStatus = TSocketNode::STATUS_OK;
				
				TMQHeadInfo stMQHeadInfo;
				FillMQHead(&stMQHeadInfo,pSocketNode,TMQHeadInfo::CMD_SCC_RSP_CONNSUCC);
				m_Me2SvrPipe.AppendOneCode((const char *)&stMQHeadInfo, sizeof(TMQHeadInfo));

				//去掉OUT监控
				m_stEPollFlow.Modify(pSocketNode->m_iSocket,iNodeObjIdx,EPOLLIN|EPOLLERR|EPOLLHUP);				
			}
			else if (pSocketNode->m_sStatus == TSocketNode::STATUS_SENDING)
			{
				pSocketNode->m_sStatus = TSocketNode::STATUS_OK;
			}

			//缓存有东西则发送
			int iSendBytes = m_stBuffMngSend.SendBufferToSocket(pSocketNode->m_iSocket,iNodeObjIdx);
			 if ((iSendBytes<0)&&(errno!=EAGAIN))
			{
				TLib_Log_LogMsg("ERR:send() to socket error,ret %d, close link!\n",iSendBytes);
				ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
				return -1;
			}				

			//发送完毕,去掉OUT监控
			if (m_stBuffMngSend.GetBufferSize(iNodeObjIdx) <= 0)
			{
				m_stEPollFlow.Modify(pSocketNode->m_iSocket, iNodeObjIdx, EPOLLIN | EPOLLERR|EPOLLHUP);
			}			
		}
		
		pSocketNode->m_iActiveTime = m_tNow.tv_sec;
	}
	return 0;
}
	
int CMainCtrl::CheckSvrMessage()
{
	int iDoMsgCnt = 0;
	int iDoMsgLen = 0;
	int iCodeLength;
	char *pMsgBuff;	
	int iGetPtrLen = -1;
	while((iDoMsgLen < 500*1024) && (iDoMsgCnt < 500))
	{
		ssize_t iBuffUsed,iBuffCount,iObjUsed,iObjCount;
		m_stBuffMngRecv.GetBufferUsage(iBuffUsed,iBuffCount,iObjUsed,iObjCount);

		//空间已满
		if(iObjUsed/(float)iObjCount > 0.9)
		{
			//少数连接比较慢
			if(iBuffUsed/(float)iBuffCount > 0.5)
				break;
		}
			
		if(iGetPtrLen > 0)
		{
			m_Svr2MePipe.SkipHeadCodePtr();
			iGetPtrLen = -1;
		}

		iGetPtrLen = m_Svr2MePipe.GetHeadCodePtr(pMsgBuff, &iCodeLength);
		if(iGetPtrLen < 0)
		{
			iCodeLength = sizeof(m_szCodeBuff);
			int iRet = m_Svr2MePipe.GetHeadCode(m_szCodeBuff, &iCodeLength);
			if(iRet < 0 || iCodeLength <= 0)
			{
				break;
			}
			pMsgBuff = m_szCodeBuff;
		}
		else if(iGetPtrLen == 0)
		{
			break;
		}
		
		iDoMsgLen += iCodeLength;
		iDoMsgCnt++;

		char *pClientMsg = pMsgBuff + sizeof(TMQHeadInfo);
		int iClientMsgLen = iCodeLength - sizeof(TMQHeadInfo);
			
		TMQHeadInfo* pMQHeadInfo = (TMQHeadInfo*)pMsgBuff;
		//请求关闭	
		if(pMQHeadInfo->m_ucCmd == TMQHeadInfo::CMD_REQ_SCC_CLOSE)
		{
			ClearSocketNode(pMQHeadInfo->m_unClientIP,pMQHeadInfo->m_usClientPort,pMQHeadInfo->m_szSrcMQ);
			continue;
		}	
		//必须是连接或者传输包
		if ((pMQHeadInfo->m_ucCmd != TMQHeadInfo::CMD_REQ_SCC_CONN) &&
			(pMQHeadInfo->m_ucCmd != TMQHeadInfo::CMD_DATA_TRANS))
		{
			TLib_Log_LogMsg("ERR:Bad MQ cmd id %d from server!\n",pMQHeadInfo->m_ucCmd);
			continue;
		}
		//检查接收队列中的超时包
		if(m_stConfig.m_iMaxQueueWaitus)
		{	
			gettimeofday(&m_tNow,NULL);
			int iQueueWaitus = (m_tNow.tv_sec - pMQHeadInfo->m_tTimeStampSec)*1000000+
							(m_tNow.tv_usec - pMQHeadInfo->m_tTimeStampuSec);
			
			if(iQueueWaitus>m_stConfig.m_iMaxQueueWaitus)
			{
				TLib_Log_LogMsgFrequency(1,"WARN::Expire Msg From Mem Queue, QueueWait=%dus\n",iQueueWaitus);
			}
		}

		char szKey[1024];
		ssize_t iKeyLen = 0;
		if(pMQHeadInfo->m_ucDataType == TMQHeadInfo::DATA_TYPE_UDP)
		{
			iKeyLen = (ssize_t)MakeKey(0,0,pMQHeadInfo->m_szSrcMQ,szKey);
			ssize_t iNodeObjIdx = -1;
			TSocketNode* pSocketNode = (TSocketNode*)m_stSocketNodeHash.GetObjectByKey((void *)szKey,iKeyLen,iNodeObjIdx);
			if (!pSocketNode)
			{
				int iSocket = CreateConnectSocket(0,0,
							m_stConfig.SOCKET_RCVBUF,m_stConfig.SOCKET_SNDBUF,1);
				if (iSocket < 0)
				{
					char szIPAddr[32]={0};
					NtoP(pMQHeadInfo->m_unClientIP,szIPAddr);
					TLib_Log_LogMsg("ERR:UDP connect to %s:%d failed![%s:%d]\n",
									szIPAddr,pMQHeadInfo->m_usClientPort,__FILE__,__LINE__);
					continue;
				}

				iNodeObjIdx = CreateSocketNode(iSocket,TSocketNode::STATUS_OK,
							0,0,pMQHeadInfo->m_szSrcMQ,TSocketNode::UDP_SOCKET);
				if (iNodeObjIdx < 0)
				{
					close(iSocket);
					TLib_Log_LogMsg("ERR:UDP Add to socket array failed!SocketNodeMng used %d, free %d\n",
									m_stSocketNodeMng.GetUsedCount(),m_stSocketNodeMng.GetFreeCount());
					continue;
				}
				pSocketNode = (TSocketNode*)m_stSocketNodeHash.GetObjectByIdx(iNodeObjIdx);				
			}
			else if(pSocketNode->m_sSocketType != TSocketNode::UDP_SOCKET)
			{
				TLib_Log_LogMsg("ERR:MQ name %d alread exist! and its type is %d, not UDP socket.\n",
									pSocketNode->m_szSrcMQ,(int)pSocketNode->m_sSocketType);
				continue;	
			}

			struct sockaddr_in addrClient;	
			addrClient.sin_family = AF_INET;
			addrClient.sin_addr.s_addr = pMQHeadInfo->m_unClientIP;
			addrClient.sin_port = htons(pMQHeadInfo->m_usClientPort);

			int iSendBytes = sendto(pSocketNode->m_iSocket,pClientMsg,iClientMsgLen,
							0,(struct sockaddr*)&addrClient,sizeof(struct sockaddr_in));
			
			pSocketNode->m_iActiveTime = m_tNow.tv_sec;
			if(iSendBytes > 0)
			{
				m_stStat.m_llUdpSCSuccPkgNum++;
				m_stStat.m_llUdpSCPkgLen += iClientMsgLen;	
			}
			else
			{
				TLib_Log_LogMsg("ERR:Udp sendto failed ret=%d.[%s:%d]\n",iSendBytes,__FILE__,__LINE__);
				m_stStat.m_llUdpSCFailedPkgNum++;
			}
		}
		else
		{
			iKeyLen = (ssize_t)MakeKey(pMQHeadInfo->m_unClientIP,pMQHeadInfo->m_usClientPort,pMQHeadInfo->m_szSrcMQ,szKey);
			ssize_t iNodeObjIdx = -1;
			TSocketNode* pSocketNode = (TSocketNode*)m_stSocketNodeHash.GetObjectByKey((void *)szKey,iKeyLen,iNodeObjIdx);
			if (!pSocketNode)
			{
				//连接不存在
				int iSocket = CreateConnectSocket(pMQHeadInfo->m_unClientIP,pMQHeadInfo->m_usClientPort,
								m_stConfig.SOCKET_RCVBUF,m_stConfig.SOCKET_SNDBUF,0);
				if (iSocket < 0)
				{
					char szIPAddr[32]={0};
					NtoP(pMQHeadInfo->m_unClientIP,szIPAddr);
					TLib_Log_LogMsg("ERR:connect to %s:%d failed![%s:%d]\n",
									szIPAddr,pMQHeadInfo->m_usClientPort,__FILE__,__LINE__);
					continue;
				}

				int iSocketStatus = TSocketNode::STATUS_CONNECTING;
				if (pMQHeadInfo->m_ucCmd == TMQHeadInfo::CMD_DATA_TRANS)
					iSocketStatus = TSocketNode::STATUS_SENDING;

				iNodeObjIdx = CreateSocketNode(iSocket,iSocketStatus,
						pMQHeadInfo->m_unClientIP, pMQHeadInfo->m_usClientPort,
						pMQHeadInfo->m_szSrcMQ,TSocketNode::TCP_SOCKET);
				if (iNodeObjIdx < 0)
				{
					close(iSocket);
					TLib_Log_LogMsg("ERR:Add to socket array failed!SocketNodeMng used %d, free %d\n",
									m_stSocketNodeMng.GetUsedCount(),m_stSocketNodeMng.GetFreeCount());
					continue;
				}
				pSocketNode = (TSocketNode*)m_stSocketNodeHash.GetObjectByIdx(iNodeObjIdx);
			}
			else if (pMQHeadInfo->m_ucCmd == TMQHeadInfo::CMD_REQ_SCC_CONN)
			{
				//连接存在
				TMQHeadInfo stMQHeadInfo;
				FillMQHead(&stMQHeadInfo,pSocketNode,TMQHeadInfo::CMD_SCC_RSP_CONNSUCC);
				m_Me2SvrPipe.AppendOneCode((const char *)&stMQHeadInfo, sizeof(TMQHeadInfo));
				continue;
			}

			//试图发送
			int iSendBytes = m_stBuffMngSend.SendBufferToSocket(pSocketNode->m_iSocket,iNodeObjIdx);
			 if ((iSendBytes<0)&&(errno!=EAGAIN))
			{
					TLib_Log_LogMsg("ERR:write() to socket failed,ret %d,errno=%d,socket %d,clientmsglen %d[%s:%d].\n",
						iSendBytes,errno,pSocketNode->m_iSocket,iClientMsgLen,__FILE__,__LINE__);
				ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
				continue;
			}
			 
			//数据传输,缓存没有则直接发送,有则放入缓存	
			if (m_stBuffMngSend.GetBufferSize(iNodeObjIdx) <= 0)
			{
				int iSendBytes = write(pSocketNode->m_iSocket, pClientMsg, iClientMsgLen);
				if ((iSendBytes<0)&&(errno!=EAGAIN))
				{
					TLib_Log_LogMsg("ERR:write() to socket failed,ret %d,errno=%d,socket %d,clientmsglen %d[%s:%d]\n",
						iSendBytes,errno,pSocketNode->m_iSocket,iClientMsgLen,__FILE__,__LINE__);
					ClearSocketNode(pMQHeadInfo->m_unClientIP,pMQHeadInfo->m_usClientPort,pSocketNode->m_szSrcMQ);
					continue;
				}			
				else if (iSendBytes < iClientMsgLen)
				{
					iSendBytes = iSendBytes>0 ? iSendBytes : 0;
					if (0==m_stBuffMngSend.AppendBuffer(iNodeObjIdx, 
								pClientMsg+iSendBytes,iClientMsgLen-iSendBytes))
					{
						//增加EPOLLOUT监控
						m_stEPollFlow.Modify(pSocketNode->m_iSocket, iNodeObjIdx, EPOLLIN |EPOLLOUT| EPOLLERR|EPOLLHUP);		
					}
					else
					{
						//已经发了一部分,防止数据错乱,关闭
						ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);			
						TLib_Log_LogMsg("ERR:AppendBuffer() failed,BuffMngSend may be full,close link![%s:%d]\n",__FILE__,__LINE__);
						continue;
					}
				}	
			}
			else
			{
				if(m_stBuffMngSend.AppendBuffer(iNodeObjIdx, pClientMsg,iClientMsgLen))
				{
					//不能秘密丢包,关闭连接,使外界感知
					ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
					TLib_Log_LogMsgFrequency(1,"ERR:AppendBuffer() failed,BuffMngSend may be full, close link![%s:%d]\n",__FILE__,__LINE__);
					continue;
				}	
			}
			pSocketNode->m_iActiveTime = m_tNow.tv_sec;
			m_stStat.m_llTcpSCPkgNum++;
			m_stStat.m_llTcpSCPkgLen += (unsigned int)iClientMsgLen;
		}
		
		m_pCLoadGrid->CheckLoad(m_tNow);
	}	

	if(iGetPtrLen > 0)
	{
		m_Svr2MePipe.SkipHeadCodePtr();
		iGetPtrLen = -1;
	}
		
	return 0;
}

//接收对应socket的数据
int CMainCtrl::RecvClientData(int iNodeObjIdx)
{
	//得到当前socket
	TSocketNode* pSocketNode = (TSocketNode*)m_stSocketNodeMng.GetAttachObj(iNodeObjIdx);
	if (!pSocketNode || pSocketNode->m_iSocket<0)
	{
		return -1;
	}

	//计算至少缺少的数据
	int iMissDataLen = 0;
	int iOldDataSize = m_stBuffMngRecv.GetBufferSize(iNodeObjIdx);
	if(iOldDataSize > 0)
	{
		int iGetSize = m_stBuffMngRecv.GetBuffer(iNodeObjIdx,m_szCodeBuff,m_iMsgHeaderLen);
		if (iGetSize == m_iMsgHeaderLen)
		{
			//根据头部数据判断本包应该多长
			int iPkgTheoryLen = 0;
			int iMsgLen = net_complete_func(m_szCodeBuff,m_iMsgHeaderLen,iPkgTheoryLen);
			if (iMsgLen < 0)
			{
				ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
				return -2;
			}
			iMissDataLen = iPkgTheoryLen - iOldDataSize;
		}	
	}

	//读取数据
	int iRecvBytes = read(pSocketNode->m_iSocket, m_szCodeBuff+sizeof(TMQHeadInfo)+iOldDataSize, 
								sizeof(m_szCodeBuff)-sizeof(TMQHeadInfo)-iOldDataSize);
	if ((iRecvBytes == 0)||((iRecvBytes < 0) && (errno != EAGAIN)))
	{
		ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
		return 0;
	}
	else if(iRecvBytes < 0)
	{
		return -3;
	}

	char *pCodeStartPos = m_szCodeBuff+sizeof(TMQHeadInfo)+iOldDataSize;
	int iCodeLeftLen = iRecvBytes;

	//足够至少一个包
	if(iRecvBytes >= iMissDataLen)
	{
		m_stBuffMngRecv.GetBuffer(iNodeObjIdx,m_szCodeBuff+sizeof(TMQHeadInfo),iOldDataSize);
		m_stBuffMngRecv.FreeBuffer(iNodeObjIdx);
		
		pCodeStartPos = m_szCodeBuff+sizeof(TMQHeadInfo);
		iCodeLeftLen = iRecvBytes+iOldDataSize;
		
		//循环拿出完整的包
		int iMsgLen = 0;	
		int iPkgTheoryLen = 0;
		while ((iMsgLen=net_complete_func(pCodeStartPos,iCodeLeftLen,iPkgTheoryLen)) > 0)
		{			
			if (iPkgTheoryLen < 0 ||iPkgTheoryLen >= MAX_MSG_LEN)
			{
				ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
				TLib_Log_LogMsg("ERR:Bad PkgTheoryLen %d ,close link![%s:%d]\n",iPkgTheoryLen,__FILE__,__LINE__);
				return -4;
			}
			
			//通知服务器			
			TMQHeadInfo* pMQHeadInfo = (TMQHeadInfo*)(pCodeStartPos-sizeof(TMQHeadInfo));
			FillMQHead(pMQHeadInfo,pSocketNode,TMQHeadInfo::CMD_DATA_TRANS);

			//通知服务器
			int iRet = m_Me2SvrPipe.AppendOneCode((const char *)pCodeStartPos-sizeof(TMQHeadInfo), iMsgLen+sizeof(TMQHeadInfo));
			if(0 > iRet)
			{
				//不能秘密丢包,关闭连接,使外界感知
				//ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
				TLib_Log_LogMsgFrequency(1,"ERR:AppendOneCode() failed,Me2Svr pipe May be full![%s:%d]\n",__FILE__,__LINE__);
				return -5;
			}
			
			m_stStat.m_llTcpCSPkgNum++;
			m_stStat.m_llTcpCSPkgLen += iMsgLen;

			pCodeStartPos += iMsgLen;
			iCodeLeftLen -= iMsgLen;
		}
		
		if (iMsgLen < 0)
		{
			ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
			TLib_Log_LogMsg("ERR:net_complete_func return: Bad Msg,close link!Ret=%d[%s:%d]\n",iMsgLen,__FILE__,__LINE__);
			return -6;
		}	
		
		if (iCodeLeftLen <= 0)
			return 0;
		
		//残渣数据存入
		if (0==m_stBuffMngRecv.AppendBuffer(iNodeObjIdx,pCodeStartPos,iCodeLeftLen))
		{
			return 0;
		}
		else
		{
			ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
			TLib_Log_LogMsg("ERR:AppendBuffer() failed,BuffMngRecv may be full, close link![%s:%d]\n",__FILE__,__LINE__);
		}		
	}
	//不足一个包,直接存入
	else
	{
		if (0==m_stBuffMngRecv.AppendBuffer(iNodeObjIdx,pCodeStartPos,iCodeLeftLen))
		{
			return 0;
		}
		else
		{
			char szIPAddr[16]={0};
			NtoP(pSocketNode->m_unClientIP,szIPAddr);
			TLib_Log_LogMsg("ERR:AppendBuffer() failed! BuffMngRecv may be full, close link %s:%d.[%s:%d]\n",
					szIPAddr,(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);		
			ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,pSocketNode->m_szSrcMQ);
		}
	}

	return -7;
}

int CMainCtrl::Run()
{
	while(1)
	{
		if(Flag_Exit == m_iRunFlag)
		{
			TLib_Log_LogMsg("Server Exit!\n");
			return 0;
		}
		
		if(Flag_ReloadCfg == m_iRunFlag)
		{
			ReadCfgFile(m_stConfig.m_szCfgFileName);
			TLib_Log_LogMsg("reload config file ok!\n");
			m_iRunFlag = 0;
		}

		// 1ms
		if(m_Svr2MePipe.GetCodeLength() == 0)
			m_stEPollFlow.Wait(1);
		else
			m_stEPollFlow.Wait(0);		

		//每次循环取一次
		gettimeofday(&m_tNow,NULL);
		
		//读取客户端包
		CheckClientMessage();    
		
		//读取服务器端包
		CheckSvrMessage(); 

		//检测通讯超时
		CheckTimeOut();    

		//写统计
		WriteStat();
	}
	
	return 0;
}

//实行管理和实时状态查询
int CMainCtrl::ProcessAdminMsg(int iNodeObjIdx)
{
	TSocketNode* pSocketNode = (TSocketNode*)m_stSocketNodeMng.GetAttachObj(iNodeObjIdx);
	
	TAdminMsg stAdminMsg;
	memset(&stAdminMsg,0,sizeof(stAdminMsg));
	int iRecvBytes = read(pSocketNode->m_iSocket,&stAdminMsg, sizeof(TAdminMsg));
	if (iRecvBytes <= 0)
	{
		ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,"_ADMIN_");
		return -1;
	}
	stAdminMsg.Decode();
			
	if (stAdminMsg.m_iAdminCmd == TAdminMsg::CMD_FETCH_LOAD)
	{
		memset(&stAdminMsg,0,sizeof(stAdminMsg));
		
		int iTimeAllSpan=0;
		int iReqNum = 0;
		m_pCLoadGrid->FetchLoad(iTimeAllSpan,iReqNum);
			
		stAdminMsg.m_iVal[0] = m_unTcpClientNum;	
		stAdminMsg.m_iVal[1] = m_stConfig.MAX_SOCKET_NUM;
		stAdminMsg.m_iVal[2] = iReqNum;
		stAdminMsg.m_iVal[3] = iTimeAllSpan;
		
		stAdminMsg.m_iVal[4] = m_Svr2MePipe.GetCodeLength();
		stAdminMsg.m_iVal[5] = m_Svr2MePipe.GetSize();

		stAdminMsg.m_iVal[6] = m_Me2SvrPipe.GetCodeLength();
		stAdminMsg.m_iVal[7] = m_Me2SvrPipe.GetSize();		

		ssize_t iBuffUsed,iBuffCount,iObjUsed,iObjCount;
		m_stBuffMngRecv.GetBufferUsage(iBuffUsed,iBuffCount,iObjUsed,iObjCount);
		stAdminMsg.m_iVal[8] = iBuffUsed;
		stAdminMsg.m_iVal[9] = iBuffCount;
		stAdminMsg.m_iVal[10] = iObjUsed;
		stAdminMsg.m_iVal[11] = iObjCount;
		stAdminMsg.m_iVal[12] = m_stBuffMngRecv.GetBlockSize();
		
		
		m_stBuffMngSend.GetBufferUsage(iBuffUsed,iBuffCount,iObjUsed,iObjCount);
		stAdminMsg.m_iVal[13] = iBuffUsed;
		stAdminMsg.m_iVal[14] = iBuffCount;
		stAdminMsg.m_iVal[15] = iObjUsed;
		stAdminMsg.m_iVal[16] = iObjCount;
		stAdminMsg.m_iVal[17] = m_stBuffMngSend.GetBlockSize();

		stAdminMsg.Encode();
	}
	else
	{
		ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,"_ADMIN_");	
		TLib_Log_LogMsg("ERR:Bad admin msg id %d!\n",stAdminMsg.m_iAdminCmd);
		return -2;
	}

	int iSendBytes = write(pSocketNode->m_iSocket, &stAdminMsg, sizeof(TAdminMsg));
	if ((iSendBytes<0)&&(errno!=EAGAIN))
	{
		ClearSocketNode(pSocketNode->m_unClientIP,pSocketNode->m_usClientPort,"_ADMIN_");
		TLib_Log_LogMsg("ERR:Admin msg write() to socket failed! id %d\n",
					stAdminMsg.m_iAdminCmd);
		return -3;
	}
	return 0;
}

