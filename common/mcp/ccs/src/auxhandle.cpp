#include "mainctrl.h"

CAuxHandle::CAuxHandle()
{
	m_pSocket = NULL;
	memset(&m_stStat,0,sizeof(m_stStat));
	memset(&m_stAllStat,0,sizeof(m_stAllStat));
	m_unID = 0;
	m_cAuxID = -1;
	m_ullMemCost = 0;	
	m_pCLoadGrid = NULL;
	m_unTcpClientNum = 0;
	m_unTcpErrClientNum = 0;
}
CAuxHandle::~CAuxHandle()
{
	delete m_pCLoadGrid;
}; 
int CAuxHandle::Initialize(void* pMainCrtl,char cAuxID)
{
	m_pMainCtrl = pMainCrtl;
	m_cAuxID = cAuxID;
	
	m_pMainConfig = &(((CMainCtrl*)m_pMainCtrl)->m_stConfig);
	m_pMe2SvrPipe = ((CMainCtrl*)m_pMainCtrl)->m_Me2SvrPipe.GetCodeQueue((short)m_cAuxID);
	m_pSvr2MePipe = ((CMainCtrl*)m_pMainCtrl)->m_Svr2MePipe.GetCodeQueue((short)m_cAuxID);

	assert(m_pMe2SvrPipe);
	assert(m_pSvr2MePipe);
	
	//log
	char szTmp[512];
	sprintf(szTmp,"../log/%s_aux%d", ((CMainCtrl*)m_pMainCtrl)->m_stConfig.m_szSvrName,(int)m_cAuxID);
	LogInit(szTmp,20000000, 10);

	//stat
	sprintf(szTmp,"../log/%s_stat_aux%d", ((CMainCtrl*)m_pMainCtrl)->m_stConfig.m_szSvrName,(int)m_cAuxID);
	m_pStatistic = new CStatistic();
	m_pStatistic->Inittialize(szTmp);
	
	//初始化socket组
	m_pSocket = new TSocketNode[m_pMainConfig->MAX_SOCKET_NUM];
	for(int i = 0; i < m_pMainConfig->MAX_SOCKET_NUM; i++ )
	{
		memset(&m_pSocket[i],0,sizeof(TSocketNode));
		m_pSocket[i].m_iSocket  = -1;
	}	
	m_ullMemCost += (long long)sizeof(TSocketNode)*(long long)m_pMainConfig->MAX_SOCKET_NUM;

	//连接资源限制,root用户下有效
	rlimit rlim;
	rlim.rlim_cur = m_pMainConfig->MAX_SOCKET_NUM+10240;
	rlim.rlim_max = m_pMainConfig->MAX_SOCKET_NUM+10240;
	setrlimit(RLIMIT_NOFILE, &rlim);
	
	//创建管道
	char *pMem = new char[MAX_MSG_LEN];
	m_Main2MePipe.AttachMem(0,pMem,MAX_MSG_LEN,CCodeQueue::Init);

	//创建CS消息缓冲区
	int iMemSize = TIdxObjMng::CountMemSize(m_pMainConfig->RCV_BLOCK_SIZE,m_pMainConfig->RCV_BLOCK_NUM,1);
	pMem = new char[iMemSize];
	m_stIdxObjMngRecv.AttachMem(pMem,iMemSize,
				m_pMainConfig->RCV_BLOCK_SIZE,m_pMainConfig->RCV_BLOCK_NUM,emInit,1);
	m_ullMemCost += (long long)iMemSize;

	iMemSize = CBuffMng::CountMemSize(m_pMainConfig->MAX_SOCKET_NUM);
	pMem = new char[iMemSize];	
	m_stBuffMngRecv.AttachMem(pMem,iMemSize,m_pMainConfig->MAX_SOCKET_NUM);
	m_ullMemCost += (long long)iMemSize;

	if (m_stBuffMngRecv.AttachIdxObjMng(&m_stIdxObjMngRecv))
	{
		printf("BuffMngRecv AttachIdxObjMng  failed!\n");
		return -1;	
	}
	
	//创建SC消息缓冲区
	iMemSize = TIdxObjMng::CountMemSize(m_pMainConfig->SND_BLOCK_SIZE,m_pMainConfig->SND_BLOCK_NUM,1);
	pMem = new char[iMemSize];
	m_stIdxObjMngSend.AttachMem(pMem,iMemSize,
			m_pMainConfig->SND_BLOCK_SIZE,m_pMainConfig->SND_BLOCK_NUM,emInit,1);
	m_ullMemCost += (long long)iMemSize;

	iMemSize = CBuffMng::CountMemSize(m_pMainConfig->MAX_SOCKET_NUM);
	pMem = new char[iMemSize];	
	m_stBuffMngSend.AttachMem(pMem,iMemSize,m_pMainConfig->MAX_SOCKET_NUM);
	m_ullMemCost += (long long)iMemSize;

	if (m_stBuffMngSend.AttachIdxObjMng(&m_stIdxObjMngSend))
	{
		printf("BuffMngRecv AttachIdxObjMng  failed!\n");
		return -1;	
	}
	
	//初始化端口
	if (InitSocket())
	{
		printf("InitSocket error!\n");
		return -1;
	}
	
	//bind cpu
	if(m_pMainConfig->m_aiBindCpu[(int)m_cAuxID] >= 0)
	{
		cpu_set_t mask;
		int iCpuId = m_pMainConfig->m_aiBindCpu[(int)m_cAuxID];
		WARN("Aux %d set cpu affinity %d, use CQ %d|%d\n",(int)m_cAuxID,iCpuId,(int)m_pMe2SvrPipe->GetID(),(int)m_pSvr2MePipe->GetID());
		printf("Aux %d set cpu affinity %d,use CQ %d|%d\n",(int)m_cAuxID,iCpuId,(int)m_pMe2SvrPipe->GetID(),(int)m_pSvr2MePipe->GetID());
		
		CPU_ZERO(&mask);
		CPU_SET(iCpuId, &mask);
		sched_setaffinity(0, sizeof(mask), &mask);
	}

	m_pCLoadGrid = new CLoadGrid(
		m_pMainConfig->m_iLoadCheckAllSpan,
		m_pMainConfig->m_iLoadCheckEachSpan,
		m_pMainConfig->m_iLoadCheckMaxPkgNum);
	
	WARN("Aux %d Init Success! Cost Mem %llu bytes.\n",(int)m_cAuxID,m_ullMemCost);
	printf("Aux %d Init Success! Cost Mem %llu bytes.\n",(int)m_cAuxID,m_ullMemCost);
	return 0;
}

int CAuxHandle::FillMQHead(int iSuffix,TMQHeadInfo* pMQHeadInfo,
		unsigned char ucCmd,unsigned char ucDataType/*=TMQHeadInfo::DATA_TYPE_TCP*/)
{
	TSocketNode* pSocketNode = &m_pSocket[iSuffix];    
	if (pSocketNode->m_iSocket<0)
		return -1;
	
	memset(pMQHeadInfo,0,sizeof(TMQHeadInfo));
	pMQHeadInfo->m_iSuffix = iSuffix;
	pMQHeadInfo->m_ucCmd = ucCmd;
	pMQHeadInfo->m_ucDataType = ucDataType;
	pMQHeadInfo->m_usClientPort = pSocketNode->m_usClientPort;		
	pMQHeadInfo->m_unClientIP = pSocketNode->m_unClientIP;
	memcpy(pMQHeadInfo->m_szSrcMQ,CCS_MQ,sizeof(CCS_MQ));
	pMQHeadInfo->m_usSrcListenPort = pSocketNode->m_usListenPort;
	pMQHeadInfo->m_usFromCQID = (u_int16_t)m_pMe2SvrPipe->GetID();	/* Add for CodeQueueMutil */

	pMQHeadInfo->m_tTimeStampSec = m_tNow.tv_sec;
	pMQHeadInfo->m_tTimeStampuSec = m_tNow.tv_usec;

	//pMQHeadInfo->m_usFromCQID = (unsigned short)m_cAuxID;
		
	memcpy(pMQHeadInfo->m_szEchoData,&(pSocketNode->m_unID),sizeof(int));
	return 0;
}

int CAuxHandle::CheckTimeOut()
{
	if (m_pMainConfig->m_iTimeOutCheckSecs)
	{
		static time_t sTimeLast = time(NULL);
		time_t tNow = m_tNow.tv_sec;
		if (tNow - sTimeLast >= m_pMainConfig->m_iTimeOutCheckSecs)
		{
			sTimeLast = tNow;
			for (int i=0; i<m_pMainConfig->MAX_SOCKET_NUM; i++)
			{
				if ((m_pSocket[i].m_iSocket < 0)||(m_pSocket[i].m_sSocketType != TSocketNode::TCP_CLIENT_SOCKET))
					continue;
				
				if (tNow - m_pSocket[i].m_iActiveTime > m_pMainConfig->m_iTimeOutSecs)
				{
					WARN("iSocketSuffix %d closed. fd %d, timeout\n", i, m_pSocket[i].m_iSocket);
					ClearSocketNode(i,m_pMainConfig->m_iDisconnectNotify);
				}
			}			
		}	
	}

	return 0;
}
int CAuxHandle::CreateSocketNode(int iSocket,int iType,unsigned int unClientIP,
	unsigned short usClientPort,unsigned short usListenPort)
{
	if (iSocket<0)
		return -1;

	//加入数组
	int iPos = (iSocket%m_pMainConfig->MAX_SOCKET_NUM);
	if (m_pSocket[iPos].m_iSocket > 0)
	{
		close(iSocket);
		return -2;
	}

	m_pSocket[iPos].m_iSocket = iSocket;
	m_pSocket[iPos].m_sSocketType = iType;
	m_pSocket[iPos].m_unClientIP = unClientIP;
	m_pSocket[iPos].m_usClientPort = usClientPort;
	m_pSocket[iPos].m_usListenPort = usListenPort;
	m_pSocket[iPos].m_iActiveTime = m_tNow.tv_sec;
	m_pSocket[iPos].m_unID = m_unID++;

	//add by nekeyzhong for accept can not inherit O_NONBLOCK 2010-01-19
	fcntl(iSocket,F_SETFL,fcntl(iSocket,F_GETFL) | O_NONBLOCK | O_NDELAY);

	m_stEPollFlowUp.Add(iSocket,iPos,EPOLLIN | EPOLLERR | EPOLLHUP);

	if (iType == TSocketNode::TCP_CLIENT_SOCKET)
		m_unTcpClientNum++;
	WARN("CreateSocketNode:iSocketSuffix %d fd %d [%d.%d.%d.%d:%d] listen port %d\n", iPos, iSocket,  NIPQUAD(unClientIP), usClientPort, usListenPort);
	return iPos;
}

int CAuxHandle::ClearSocketNode(int iSocketSuffix,int iCloseNotify/*=0*/)
{
	if (iSocketSuffix<0 ||iSocketSuffix>=m_pMainConfig->MAX_SOCKET_NUM)
		return -1;
	
	TSocketNode* pSocketNode = &m_pSocket[iSocketSuffix];    
	if (pSocketNode->m_iSocket < 0)
		return -2;
	
	if(pSocketNode->m_sSocketType == TSocketNode::TCP_CLIENT_SOCKET)
	{
		//通知
		if (iCloseNotify)
		{
			TMQHeadInfo stMQHeadInfo;		
			FillMQHead(iSocketSuffix,&stMQHeadInfo,TMQHeadInfo::CMD_CCS_NOTIFY_DISCONN);
			int iRet = m_pMe2SvrPipe->AppendOneCode((const char *)&stMQHeadInfo, sizeof(TMQHeadInfo));
			if(0 > iRet)
			{
				ERR("AppendOneCode to Me2SvrPipe failed! ret=%d, NOTIFY![%s:%d]\n",iRet,__FILE__,__LINE__);
			}	
		}
		m_unTcpClientNum--;

		m_stEPollFlowUp.Del(pSocketNode->m_iSocket);
		close(pSocketNode->m_iSocket);
		memset(pSocketNode,0,sizeof(TSocketNode));
		pSocketNode->m_iSocket = -1;

		m_stBuffMngRecv.FreeBuffer(iSocketSuffix);
		m_stBuffMngSend.FreeBuffer(iSocketSuffix);		
	}	
	return 0;
}
int CAuxHandle::DoSvrMessage(char* pMsgBuff,int iCodeLength)
{
	TMQHeadInfo* pMQHeadInfo = (TMQHeadInfo*)pMsgBuff;
	if (pMQHeadInfo->m_ucCmd != TMQHeadInfo::CMD_DATA_TRANS)
	{
		ProcessNotifyMsg(pMsgBuff,iCodeLength);
		return -1;
	}	

	char *pClientMsg = pMsgBuff + sizeof(TMQHeadInfo);
	int iClientMsgLen = iCodeLength - sizeof(TMQHeadInfo);
	if (iClientMsgLen <= 0)
	{
		return -1;
	}

	if(m_pMainConfig->m_iMaxQueueWaitus)
	{
		int iQueueWaitus = (m_tNow.tv_sec - pMQHeadInfo->m_tTimeStampSec)*1000000+
						(m_tNow.tv_usec - pMQHeadInfo->m_tTimeStampuSec);
		
		if(iQueueWaitus > m_pMainConfig->m_iMaxQueueWaitus)
		{
			ERR("Expire Msg From Svr2Me pipe, QueueWait=%dus\n",iQueueWaitus);
		}
	}

	//check
	int iSocketSuffix = pMQHeadInfo->m_iSuffix;
	if(iSocketSuffix < 0 || iSocketSuffix >= m_pMainConfig->MAX_SOCKET_NUM)
	{
		ERR("Bad Suffix %d From Svr2Me pipe.\n",iSocketSuffix);
		return -1;
	}
	TSocketNode* pSocketNode = &m_pSocket[iSocketSuffix];
	if (pSocketNode->m_iSocket < 0)
	{
		ERR("Bad Suffix %d From Svr2Me pipe,socket invalid.\n",iSocketSuffix);
		return -1;
	}
	
	if(pSocketNode->m_sSocketType == TSocketNode::TCP_CLIENT_SOCKET)
	{
		unsigned int unID = *(unsigned int*)(pMQHeadInfo->m_szEchoData);
		if(unID != pSocketNode->m_unID)
		{
			ERR("Msg from Svr2Me pipe expired because socket have been changed recv id[%d] sockid [%d] iSocketSuffix[%d] fd[%d]![%s:%d]\n",
				unID, pSocketNode->m_unID, iSocketSuffix, pSocketNode->m_iSocket, __FILE__,__LINE__);
			return -1;
		}

		if (m_stBuffMngSend.GetBufferSize(iSocketSuffix) <= 0)
		{
			int iSendBytes = send(pSocketNode->m_iSocket,pClientMsg,iClientMsgLen,0);
			if ((iSendBytes<0)&&(errno!=EAGAIN))
			{	
				WARN("write() to socket error,ret=%d![%s:%d]\n",iSendBytes,__FILE__,__LINE__);
				return -1;
			}
			else if (iSendBytes < iClientMsgLen)
			{
				iSendBytes = iSendBytes>0 ? iSendBytes : 0;
				if (0==m_stBuffMngSend.AppendBuffer(iSocketSuffix, 
								pClientMsg+iSendBytes,iClientMsgLen-iSendBytes))
				{
					//增加EPOLLOUT监控
					m_stEPollFlowUp.Modify(pSocketNode->m_iSocket,iSocketSuffix,EPOLLIN |EPOLLOUT| EPOLLERR|EPOLLHUP);		
				}
				else
				{
					ERR("AppendBuffer() failed,BuffMngSend may be full, iSocketSuffix %d fd %d close link %d.%d.%d.%d:%d [%s:%d]\n",
						iSocketSuffix, pSocketNode->m_iSocket, NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);

					//已经发了一部分,防止数据错乱,关闭
					ClearSocketNode(iSocketSuffix);
					return -1;
				}
			}	
		}
		else
		{
			if (m_stBuffMngSend.AppendBuffer(iSocketSuffix, pClientMsg,iClientMsgLen))
			{
				ERR("AppendBuffer() failed,iSocketSuffix %d fd %d  BuffMngSend may be full, close link %d.%d.%d.%d:%d [%s:%d]\n",
					iSocketSuffix, pSocketNode->m_iSocket, NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);

				//不能秘密丢包,关闭连接,使外界感知
				ClearSocketNode(iSocketSuffix);					
				return -1;
			}				
		}		
		pSocketNode->m_iActiveTime = m_tNow.tv_sec;		
		m_stStat.m_llTcpSCPkgNum++;
		m_stStat.m_llTcpSCPkgLen += iClientMsgLen;	

		m_stAllStat.m_llTcpSCPkgNum++;
		m_stAllStat.m_llTcpSCPkgLen += iClientMsgLen;	
	}
	else if(pSocketNode->m_sSocketType == TSocketNode::UDP_SOCKET)
	{
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

			m_stAllStat.m_llUdpSCSuccPkgNum++;
			m_stAllStat.m_llUdpSCPkgLen += iClientMsgLen;		
		}
		else
		{
			WARN("Udp sendto failed ret=%d.[%s:%d]\n",iSendBytes,__FILE__,__LINE__);
			m_stStat.m_llUdpSCFailedPkgNum++;
			m_stAllStat.m_llUdpSCFailedPkgNum++;
		}			
	}
	else
	{
		ERR("Msg from Svr2Me pipe, can not send data to socket type %d.[%s:%d]\n",(int)pSocketNode->m_sSocketType,__FILE__,__LINE__);
		return -1;
	}	

	return 0;
}
int CAuxHandle::CheckClientMessage()
{
	long long llKey;
	unsigned int unEvents;
	while(m_stEPollFlowUp.GetEvents(llKey,unEvents))
	{
		int iSocketSuffix = llKey;
		TSocketNode* pSocketNode = &m_pSocket[iSocketSuffix];  
		if (pSocketNode->m_iSocket < 0)
		{
			ERR("GetEvents.fd = %d\n",pSocketNode->m_iSocket);
			continue;
		}	

		if (!((EPOLLIN | EPOLLOUT) & unEvents))
		{
			WARN("epoll events %u invalid,close socket, iSocketSuffix %d fd %d\n", unEvents, iSocketSuffix, pSocketNode->m_iSocket);
			ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);
			continue;
		}

		//有输入
		if (EPOLLIN & unEvents)
		{
			if (TSocketNode::MQ_PIPE_SOCKET == pSocketNode->m_sSocketType)
			{
				if(pSocketNode->m_iSocket == m_Main2MePipe.GetReadNotifyFD())
				{
					//清除通知
					timeval tv;
					tv.tv_sec=0;
					tv.tv_usec=0;			
					m_Main2MePipe.WaitData(&tv);
					CheckMainMessage();
				}			
				else if(pSocketNode->m_iSocket == m_pSvr2MePipe->GetReadNotifyFD())
				{
					//清除通知
					timeval tv;
					tv.tv_sec=0;
					tv.tv_usec=0;			
					m_pSvr2MePipe->WaitData(&tv);
					CheckSvrMessage();
				}
			}					
			else if (TSocketNode::TCP_CLIENT_SOCKET == pSocketNode->m_sSocketType)
			{
				RecvClientData(iSocketSuffix);
								
			}
			else if (TSocketNode::UDP_SOCKET == pSocketNode->m_sSocketType)
			{
				struct sockaddr_in addrClient;
				int iClientLen = sizeof(struct sockaddr_in);
				int iRecvBytes = recvfrom(pSocketNode->m_iSocket,
										m_szBuffRecv+sizeof(TMQHeadInfo),
										sizeof(m_szBuffRecv)-sizeof(TMQHeadInfo),
										0,(struct sockaddr *)&addrClient,(socklen_t*)&iClientLen);

				if(iRecvBytes <= 0)
					continue;

				if (((CMainCtrl*)m_pMainCtrl)->CheckIPAccess(addrClient.sin_addr.s_addr) != IPAccessStu::Access)
				{
					WARN("CheckIPAccess %s return deny!\n",inet_ntoa(addrClient.sin_addr));
					break;
				}
					
				TMQHeadInfo* pMQHeadInfo = (TMQHeadInfo*)m_szBuffRecv;
				FillMQHead(iSocketSuffix,pMQHeadInfo,
					TMQHeadInfo::CMD_DATA_TRANS,TMQHeadInfo::DATA_TYPE_UDP);
				pMQHeadInfo->m_unClientIP = addrClient.sin_addr.s_addr;
				pMQHeadInfo->m_usClientPort = ntohs(addrClient.sin_port);
				
				int iRet = m_pMe2SvrPipe->AppendOneCode(
								(const char *)m_szBuffRecv, iRecvBytes+sizeof(TMQHeadInfo));
				if(0 > iRet)
				{
					ERR("AppendOneCode() to Me2Svr Pipe failed! ret=%d, UDP![%s:%d]\n", iRet,__FILE__,__LINE__);
					continue;
				}
				
				m_stStat.m_llUdpCSPkgNum++;
				m_stStat.m_llUdpCSPkgLen += iRecvBytes;	

				m_stAllStat.m_llUdpCSPkgNum++;
				m_stAllStat.m_llUdpCSPkgLen += iRecvBytes;	
			}			
			else
			{
				ERR("No this socket type %d!\n",(int)pSocketNode->m_sSocketType);
			}
		}
	
		//有输出
		if (EPOLLOUT & unEvents)
		{
			//缓存有东西则发送
			int iSendBytes = m_stBuffMngSend.SendBufferToSocket(pSocketNode->m_iSocket,iSocketSuffix);
			 if ((iSendBytes<0)&&(errno!=EAGAIN))
			{
				ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);
				ERR("send() to socket error,iSocketSuffix %d fd %d, ret=%d,close link![%s:%d]\n",  iSocketSuffix, pSocketNode->m_iSocket, iSendBytes,__FILE__,__LINE__);
				return -1;
			}				

			//发送完毕,去掉OUT监控
			if (m_stBuffMngSend.GetBufferSize(iSocketSuffix) <= 0)
			{
				m_stEPollFlowUp.Modify(pSocketNode->m_iSocket,iSocketSuffix,EPOLLIN | EPOLLERR|EPOLLHUP);
			}			
		}
		pSocketNode->m_iActiveTime = m_tNow.tv_sec;
	}
	return 0;
}
int CAuxHandle::CheckMainMessage()
{
	ssize_t iDoMsgCnt = 0;
	ssize_t iDoMsgLen = 0;
	int iCodeLength;
	char* pMsgBuff;
	int iGetPtrLen = -1;

	while((iDoMsgLen < 500*1024) && (iDoMsgCnt < 500))
	{
		if(iGetPtrLen > 0)
		{
			m_Main2MePipe.SkipHeadCodePtr();
			iGetPtrLen = -1;
		}
		
		iGetPtrLen = m_Main2MePipe.GetHeadCodePtr(pMsgBuff, &iCodeLength);
		if(iGetPtrLen < 0)
		{
			iCodeLength = sizeof(m_szBuffSend);
			int iRet = m_Main2MePipe.GetHeadCode(m_szBuffSend, &iCodeLength);
			if(iRet < 0 || iCodeLength <= 0)
			{
				break;
			}
			pMsgBuff = m_szBuffSend;
		}
		else if(iGetPtrLen == 0)
		{
			break;
		}
		
		iDoMsgLen += iCodeLength;
		iDoMsgCnt++;

		DoSvrMessage(pMsgBuff,iCodeLength);
	}

	if(iGetPtrLen > 0)
	{
		m_Main2MePipe.SkipHeadCodePtr();
		iGetPtrLen = -1;
	}
		
	return 0;
}

int CAuxHandle::CheckSvrMessage()
{
	ssize_t iDoMsgCnt = 0;
	ssize_t iDoMsgLen = 0;
	int iCodeLength;
	char* pMsgBuff;
	int iGetPtrLen = -1;
	
	while((iDoMsgLen < 500*1024) && (iDoMsgCnt < 500))
	{
		if(iGetPtrLen > 0)
		{
			m_pSvr2MePipe->SkipHeadCodePtr();
			iGetPtrLen = -1;
		}
		
		iGetPtrLen = m_pSvr2MePipe->GetHeadCodePtr(pMsgBuff, &iCodeLength);
		if(iGetPtrLen < 0)
		{
			iCodeLength = sizeof(m_szBuffSend);
			int iRet = m_pSvr2MePipe->GetHeadCode(m_szBuffSend, &iCodeLength);
			if(iRet < 0 || iCodeLength <= 0)
			{
				break;
			}
			pMsgBuff = m_szBuffSend;
		}
		else if(iGetPtrLen == 0)
		{
			break;
		}
		
		iDoMsgLen += iCodeLength;
		iDoMsgCnt++;
		
		DoSvrMessage(pMsgBuff,iCodeLength);
	}

	if(iGetPtrLen > 0)
	{
		m_pSvr2MePipe->SkipHeadCodePtr();
		iGetPtrLen = -1;
	}
		
	return 0;
}
//接收对应socket的数据,return recv len
int CAuxHandle::RecvClientData(int iSocketSuffix)
{
	if (iSocketSuffix<0 ||iSocketSuffix>=m_pMainConfig->MAX_SOCKET_NUM)
		return -1;
	
	TSocketNode* pSocketNode = &m_pSocket[iSocketSuffix];    
	if (pSocketNode->m_iSocket < 0)
		return -2;

	//本端口的包完整性函数
	check_complete my_net_complete_func = m_pMainConfig->m_stTcpIPPortStu[pSocketNode->m_usListenPort].net_complete_func;
	int iMsgHeaderLen = m_pMainConfig->m_stTcpIPPortStu[pSocketNode->m_usListenPort].m_iMsgHeaderLen;
	CLoadGrid *pMyCLoadGrid = m_pMainConfig->m_stTcpIPPortStu[pSocketNode->m_usListenPort].m_pCLoadGrid;
	if(!my_net_complete_func)
	{
		my_net_complete_func = ((CMainCtrl*)m_pMainCtrl)->default_net_complete_func;
		iMsgHeaderLen = ((CMainCtrl*)m_pMainCtrl)->i_default_msg_header_len;
	}
	assert(my_net_complete_func);
	
	int iMissDataLen = 0;
	int iOldDataSize = m_stBuffMngRecv.GetBufferSize(iSocketSuffix);
	if(iOldDataSize > 0)
	{
		int iGetSize = m_stBuffMngRecv.GetBuffer(iSocketSuffix,m_szBuffRecv,iMsgHeaderLen);
		if (iGetSize == iMsgHeaderLen)
		{
			//根据头部数据判断本包应该多长
			int iPkgTheoryLen = 0;
			int iMsgLen = my_net_complete_func(m_szBuffRecv,iMsgHeaderLen,iPkgTheoryLen);
			if (iMsgLen < 0)
			{	
				ERR("iSocketSuffix %d closed. fd %d, my_net_complete_func iMsgLen %d\n", iSocketSuffix, pSocketNode->m_iSocket, iMsgLen);
				ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);
				m_unTcpErrClientNum++;
				return -3;
			}
			iMissDataLen = iPkgTheoryLen - iOldDataSize;
		}	
	}

	//预留sizeof(TMQHeadInfo)长度用来构造shm头部
	int iRecvBytes = read(pSocketNode->m_iSocket, m_szBuffRecv+sizeof(TMQHeadInfo)+iOldDataSize, 
								sizeof(m_szBuffRecv)-sizeof(TMQHeadInfo)-iOldDataSize);
	if ((iRecvBytes == 0)||((iRecvBytes < 0) && (errno != EAGAIN)))
	{
		WARN("iSocketSuffix %d closed. fd %d, read error iRecvBytes %d, errno %d\n", iSocketSuffix, pSocketNode->m_iSocket, iRecvBytes, errno);
		ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);
		if(iRecvBytes != 0)
			m_unTcpErrClientNum++;
		return 0;
	}
	else if(iRecvBytes < 0)
	{
		return -4;
	}

	char *pCodeStartPos = m_szBuffRecv+sizeof(TMQHeadInfo)+iOldDataSize;
	int iCodeLeftLen = iRecvBytes;

	if(iRecvBytes >= iMissDataLen)
	{
		m_stBuffMngRecv.GetBuffer(iSocketSuffix,m_szBuffRecv+sizeof(TMQHeadInfo),iOldDataSize);
		m_stBuffMngRecv.FreeBuffer(iSocketSuffix);
		
		pCodeStartPos = m_szBuffRecv+sizeof(TMQHeadInfo);
		iCodeLeftLen = iRecvBytes+iOldDataSize;
		
		//判断是否一个完整的包
		int iMsgLen = 0;
		int iPkgTheoryLen = 0;
		while ((iMsgLen=my_net_complete_func(pCodeStartPos,iCodeLeftLen,iPkgTheoryLen)) > 0)
		{		
			if (iPkgTheoryLen < 0 ||iPkgTheoryLen >= MAX_MSG_LEN)
			{
				ERR("iSocketSuffix %d closed. fd %d Bad PkgTheoryLen %d ,close link![%s:%d]\n",iSocketSuffix, pSocketNode->m_iSocket,iPkgTheoryLen,__FILE__,__LINE__);
				ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);				
				m_unTcpErrClientNum++;
				return -3;
			}

			if (pMyCLoadGrid->CheckLoad(m_tNow)==CLoadGrid::LR_FULL)
			{
				if (m_pMainConfig->m_iLoadCheckOpen)
				{
					ERR("iSocketSuffix %d closed. fd %d  port %d LoadGrid full!close link!\n",iSocketSuffix, pSocketNode->m_iSocket, (int)pSocketNode->m_usListenPort);
					ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);					
					m_unTcpErrClientNum++;
					return -5;				
				}
			}

			TMQHeadInfo* pMQHeadInfo = (TMQHeadInfo*)(pCodeStartPos-sizeof(TMQHeadInfo));
			FillMQHead(iSocketSuffix,pMQHeadInfo,TMQHeadInfo::CMD_DATA_TRANS);

			if(m_pMainConfig->m_iAdminPort == pSocketNode->m_usListenPort)
			{
				ProcessAdminMsg(iSocketSuffix,pCodeStartPos,iMsgLen);
			}
			else
			{
				//通知服务器
				int iRet = m_pMe2SvrPipe->AppendOneCode((const char *)pCodeStartPos-sizeof(TMQHeadInfo),
									iMsgLen+sizeof(TMQHeadInfo));
				if(0 > iRet)
				{		
					ERR("AppendOneCode() to Me2Svr Pipe failed!iSocketSuffix %d fd %d ret=%d, close link %d.%d.%d.%d:%d [%s:%d]\n", iSocketSuffix, pSocketNode->m_iSocket, iRet,
								NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);

					//不能秘密丢包,关闭连接,使外界感知
					ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);	
					m_unTcpErrClientNum++;
					return -6;
				}

				m_stStat.m_llTcpCSPkgNum++;
				m_stStat.m_llTcpCSPkgLen += iMsgLen;	

				m_stAllStat.m_llTcpCSPkgNum++;
				m_stAllStat.m_llTcpCSPkgLen += iMsgLen;				
			}

			pCodeStartPos += iMsgLen;
			iCodeLeftLen -= iMsgLen;
		}

		if (iMsgLen == -1)
		{
			ERR("ERR: iSocketSuffix %d fd %d my_net_complete_func ret=%d, close link %d.%d.%d.%d:%d [%s:%d]\n",iSocketSuffix, pSocketNode->m_iSocket, iMsgLen,
				NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);

			ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);
			m_unTcpErrClientNum++;
			return 0;
		}
		else if (iMsgLen < 0)
		{
			ERR("iSocketSuffix %d fd %d port %d net_complete_func return %d: Bad Msg,close link![%s:%d]\n",
				iSocketSuffix, pSocketNode->m_iSocket, (int)pSocketNode->m_usListenPort,iMsgLen,__FILE__,__LINE__);

			char szTmpBuff[1024]={0};
			if(iCodeLeftLen < (int)sizeof(szTmpBuff))
			{
			    for(int i = 0; i < iCodeLeftLen; i++ )
					sprintf(szTmpBuff+strlen(szTmpBuff),"%02x", (unsigned char)pCodeStartPos[i]);
				
			    TLib_Log_LogMsg("%s\n",szTmpBuff);				
			}

			WARN("DUMP:\n");	
			WARN("iRecvBytes:%d iOldDataSize:%d iMsgHeaderLen:%d iMissDataLen:%d iCodeLeftLen:%d\n",
				iRecvBytes,iOldDataSize,iMsgHeaderLen,iMissDataLen,iCodeLeftLen);	
			
			ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);
			m_unTcpErrClientNum++;
			return -7;
		}

		if (iCodeLeftLen <= 0)
			return 0;
		
		//残渣数据存入
		if (0==m_stBuffMngRecv.AppendBuffer(iSocketSuffix,pCodeStartPos,iCodeLeftLen))
		{
			//告诉外面不用在检查了
			return 0;
		}
		else
		{
			ERR("iSocketSuffix %d fd %d AppendBuffer() failed! BuffMngRecv may be full, close link %d.%d.%d.%d:%d.[%s:%d]\n",
					iSocketSuffix, pSocketNode->m_iSocket, NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);

			ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);	
			m_unTcpErrClientNum++;
		}			
	}
	else
	{
		//后续数据存入
		if (0==m_stBuffMngRecv.AppendBuffer(iSocketSuffix,pCodeStartPos,iCodeLeftLen))
		{
			return iCodeLeftLen;
		}
		else
		{
			ERR("iSocketSuffix %d fd %d AppendBuffer() failed! BuffMngRecv may be full, close link %d.%d.%d.%d:%d.[%s:%d]\n",
					iSocketSuffix, pSocketNode->m_iSocket, NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);	
			
			ClearSocketNode(iSocketSuffix,m_pMainConfig->m_iDisconnectNotify);			
			m_unTcpErrClientNum++;
		}
	}
	
	return -8;
}

int CAuxHandle::InitSocket()
{
	if(m_stEPollFlowUp.Create(m_pMainConfig->MAX_SOCKET_NUM))
	{
		printf("epoll create error!");
		return -1;
	}

	//shm管道监听机制
	assert (m_Main2MePipe.GetReadNotifyFD() >= 0);
	int iNewSuffix = CreateSocketNode(m_Main2MePipe.GetReadNotifyFD(),TSocketNode::MQ_PIPE_SOCKET,0,0,0);
	if (iNewSuffix < 0)
	{
		printf("add MQ_PIPE_SOCKET to socket array failed!\n");
		ERR("add MQ_PIPE_SOCKET to socket array failed!\n");
		return -3;
	}	


	//shm管道监听机制
	assert (m_pSvr2MePipe->GetReadNotifyFD() >= 0);
	iNewSuffix = CreateSocketNode(m_pSvr2MePipe->GetReadNotifyFD(),TSocketNode::MQ_PIPE_SOCKET,0,0,0);
	if (iNewSuffix < 0)
	{
		printf("add MQ_PIPE_SOCKET to socket array failed!\n");
		ERR("add MQ_PIPE_SOCKET to socket array failed!\n");
		return -3;
	}	

	return 0;
}

int CAuxHandle::Run()
{
	m_bHandleRun = true;
	while(1)
	{
		if (unlikely(m_iRunStatus == rt_stopped))
			break;

		// 1ms
		if((m_Main2MePipe.GetCodeLength() == 0) && (m_pSvr2MePipe->GetCodeLength() == 0))
			m_stEPollFlowUp.Wait(1);
		else
			m_stEPollFlowUp.Wait(0);

		//每次循环取一次
		gettimeofday(&m_tNow,NULL);
		
		//读取客户端包
		CheckClientMessage();    

		//读取主线程包
		CheckMainMessage(); 
		
		//读取服务器端包
		CheckSvrMessage(); 

		//检测通讯超时
		CheckTimeOut();    
	}
	m_bHandleRun = false;
	return 0;
}
int CAuxHandle::LogInit(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum)
{
	sprintf(m_szLogFileBase,"%s",sLogBaseName);
	sprintf(m_szLogFileName,"%s.log",sLogBaseName);
	m_iMaxLogSize = lMaxLogSize;
	m_iMaxLogNum = iMaxLogNum;	
	return 0;
};

void CAuxHandle::Log(int iKey,int iLevel,const char *sFormat, ...)
{
	int iWriteLog = 0;
	if((iKey!=0)&&(((CMainCtrl*)m_pMainCtrl)->m_stConfig.m_pShmLog->m_iLogKey == iKey))
	{
		iWriteLog = 1;
	}
	
	//本次级别大于要求的级别   
	if (iLevel <= ((CMainCtrl*)m_pMainCtrl)->m_stConfig.m_pShmLog->m_iLogLevel)
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
void CAuxHandle::frequency_log(int iKey,int iLevel,const char *sFormat, ...)
{
	int iWriteLog = 0;
	if((iKey!=0)&&(m_pMainConfig->m_pShmLog->m_iLogKey == iKey))
	{
		iWriteLog = 1;
	}

	//本次级别大于要求的级别
	if (iLevel <= m_pMainConfig->m_pShmLog->m_iLogLevel)
	{
		iWriteLog = 1;
	}

	if (!iWriteLog)
		return;
	static time_t last = ((CMainCtrl*) m_pMainCtrl)->m_tNow.tv_sec;
	static int frq = 0;
	time_t now = ((CMainCtrl*) m_pMainCtrl)->m_tNow.tv_sec;
	if (now - last >= 5)
	{
		last = now;
		frq = 0;	
	}
	if (frq > 10)
		return;
	frq++;
	
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

int CAuxHandle::ProcessNotifyMsg(char* pCodeBuff,int iCodeLen)
{
	TMQHeadInfo* pMQHeadInfo = (TMQHeadInfo*)pCodeBuff;
	if(pMQHeadInfo->m_ucCmd == TMQHeadInfo::CMD_NEW_TCP_SOCKET)
	{
		int iSocket = pMQHeadInfo->m_iSuffix;
		if (iSocket < 0)
			return -1;

		int iListenPort = pMQHeadInfo->m_usSrcListenPort;
				
		int iNewSuffix = CreateSocketNode(iSocket,TSocketNode::TCP_CLIENT_SOCKET,
									pMQHeadInfo->m_unClientIP,pMQHeadInfo->m_usClientPort,iListenPort);
		if(iNewSuffix < 0)
			return -2;
	}
	else if(pMQHeadInfo->m_ucCmd == TMQHeadInfo::CMD_NEW_UDP_SOCKET)
	{
		int iSocket = pMQHeadInfo->m_iSuffix;
		if (iSocket < 0)
			return -1;

		int iListenPort = pMQHeadInfo->m_usSrcListenPort;

		int iNewSuffix = CreateSocketNode(iSocket,TSocketNode::UDP_SOCKET,0,0,iListenPort);
		if(iNewSuffix < 0)
			return -2;
	}	
	else if(pMQHeadInfo->m_ucCmd == TMQHeadInfo::CMD_CCS_NOTIFY_DISCONN)
	{
		TSocketNode* pSocketNode = &m_pSocket[pMQHeadInfo->m_iSuffix];
		if (pSocketNode->m_iSocket < 0)
			return -1;

		if(pSocketNode->m_sSocketType == TSocketNode::TCP_CLIENT_SOCKET)
		{
			if((pSocketNode->m_unClientIP != pMQHeadInfo->m_unClientIP) || 
				(pSocketNode->m_usClientPort != pMQHeadInfo->m_usClientPort))
			{
				return -2;	
			}				
			char szIpClient[32];
			HtoP(pSocketNode->m_unClientIP, szIpClient);	
			WARN("iSocketSuffix %d fd %d CMD_CCS_NOTIFY_DISCONN %s:%d.[%s:%d]\n",
					pMQHeadInfo->m_iSuffix, pSocketNode->m_iSocket, szIpClient,(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);
			ClearSocketNode(pMQHeadInfo->m_iSuffix);	
		}
	}
	else if (TMQHeadInfo::CMD_CCS_MCP_NOTIFY_DISCONN == pMQHeadInfo->m_ucCmd) {		
		TSocketNode* pSocketNode = &m_pSocket[pMQHeadInfo->m_iSuffix];
		char szIpCmd[32];
		HtoP(ntohl(pMQHeadInfo->m_unClientIP), szIpCmd);		
		ERR("iSocketSuffix %d fd %d CMD_CCS_MCP_NOTIFY_DISCONN close port %d %s:%d.[%s:%d]\n",
					(int)pMQHeadInfo->m_iSuffix, (int)pSocketNode->m_iSocket, (int)pMQHeadInfo->m_usClosePort,
					szIpCmd,(int)pMQHeadInfo->m_usClientPort,__FILE__,__LINE__);
		pMQHeadInfo->m_ucCmd = TMQHeadInfo::CMD_CCS_THREAD_NOTIFY_DISCONN;
		NotifyAllThread(pCodeBuff, iCodeLen);
	}
	else if (TMQHeadInfo::CMD_CCS_THREAD_NOTIFY_DISCONN == pMQHeadInfo->m_ucCmd) {		
		for (int i=0; i<m_pMainConfig->MAX_SOCKET_NUM; ++i) {
			TSocketNode* pSocketNode = &m_pSocket[i];
			if (pSocketNode->m_usListenPort == pMQHeadInfo->m_usClosePort) {
				char szIpClient[32], szIpCmd[32];
				HtoP(ntohl(pSocketNode->m_unClientIP), szIpClient);
				HtoP(ntohl(pMQHeadInfo->m_unClientIP), szIpCmd);
				ERR("iSocketSuffix %d fd %d CMD_CCS_THREAD_NOTIFY_DISCONN cmd[%s:%d] close port %d client [%s:%d].[%s:%d]\n",
					i, (int)pSocketNode->m_iSocket, szIpCmd,(int)pMQHeadInfo->m_usClientPort, (int)pMQHeadInfo->m_usClosePort,
					szIpClient,(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);
				ClearSocketNode(i);	
			}
		}
	}
	return 0;
}
/*
* 2013-03-14
* patxiao 
* send msg to all thread
*/
int CAuxHandle::NotifyAllThread(char* pCodeBuff,int iCodeLen)
{
	int iRet = 0;
	for(int i = 0 ; i < m_pMainConfig->m_iAuxHandleNum ; i++)
	{
		iRet = ((CMainCtrl*)m_pMainCtrl)->m_pAuxHandle[i]->GetCodeQueue()->AppendOneCode(pCodeBuff, iCodeLen);
		if (iRet < 0) {
			ERR("AppendOneCode to Aux %d failed, ret %d[%s:%d]\n",
				i, iRet, __FILE__,__LINE__);				
		}
	}
	return 0;
}

//实行管理和实时状态查询
int CAuxHandle::ProcessAdminMsg(int iSocketSuffix,char *pMsg,int iMsgLen)
{
	TSocketNode* pSocketNode = &m_pSocket[iSocketSuffix];  

	int iOutLen;
	int iRet = m_pMainConfig->m_admin_msg_func((void*)m_pMainCtrl,pMsg,iMsgLen,m_szBuffSend,iOutLen);
	if(iRet != 0)
	{
		ERR("m_pMainConfig->m_admin_msg_func failed! ret=%d\n",iRet);
		return -1;
	}
	char *pClientMsg = m_szBuffSend;
	int iClientMsgLen = iOutLen;
	
	if (m_stBuffMngSend.GetBufferSize(iSocketSuffix) <= 0)
	{
		int iSendBytes = send(pSocketNode->m_iSocket,pClientMsg,iClientMsgLen,0);
		if ((iSendBytes<0)&&(errno!=EAGAIN))
		{		
			WARN("write() to socket error,ret=%d![%s:%d]\n",iSendBytes,__FILE__,__LINE__);
			m_unTcpErrClientNum++;
			return -1;
		}
		else if (iSendBytes < iClientMsgLen)
		{
			iSendBytes = iSendBytes>0 ? iSendBytes : 0;
			if (0==m_stBuffMngSend.AppendBuffer(iSocketSuffix, 
							pClientMsg+iSendBytes,iClientMsgLen-iSendBytes))
			{
				//增加EPOLLOUT监控
				m_stEPollFlowUp.Modify(pSocketNode->m_iSocket,iSocketSuffix,EPOLLIN |EPOLLOUT| EPOLLERR|EPOLLHUP);		
			}
			else
			{
				ERR("iSocketSuffix %d fd %d AppendBuffer() failed,BuffMngSend may be full, close link %d.%d.%d.%d:%d [%s:%d]\n",
					iSocketSuffix, pSocketNode->m_iSocket, NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);

				//已经发了一部分,防止数据错乱,关闭
				ClearSocketNode(iSocketSuffix);
				m_unTcpErrClientNum++;
				return -1;
			}
		}	
	}
	else
	{
		if (m_stBuffMngSend.AppendBuffer(iSocketSuffix, pClientMsg,iClientMsgLen))
		{
			ERR("iSocketSuffix %d fd %dAppendBuffer() failed,BuffMngSend may be full, close link %d.%d.%d.%d:%d [%s:%d]\n",
				iSocketSuffix, pSocketNode->m_iSocket, NIPQUAD(pSocketNode->m_unClientIP),(int)pSocketNode->m_usClientPort,__FILE__,__LINE__);

			//不能秘密丢包,关闭连接,使外界感知
			ClearSocketNode(iSocketSuffix);	
			m_unTcpErrClientNum++;
			return -1;
		}				
	}		
	return 0;
}
