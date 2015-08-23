#include "user_func.h"

CUserFunc::CUserFunc(int iMyID,int iDestIp,int iDestPort,char *szConfigFile,
	pthread_mutex_t *pMutex)
{
	m_iMyID = iMyID;
	m_iDestIp = iDestIp;
	m_iDestPort = iDestPort;
	if (m_TcpCltSocket.ConnectServer(iDestIp, iDestPort) != true)
	{
		printf("connect to %d:%d failed!\n",iDestIp,iDestPort);
		throw "";
	}	
	m_pMutex = pMutex;
	pthread_mutex_lock(m_pMutex);


	TLib_Cfg_GetConfig(szConfigFile,
			"MsgLen", CFG_INT, &m_iLen,0,
			NULL);		


	pthread_mutex_unlock(m_pMutex);

	if(m_iLen <= 0)
		assert(0);

	m_pszBuff = new char[m_iLen+1024];
	m_pszBuff2  = new char[m_iLen+1024];
	
	sprintf(m_pszBuff,"GET / HTTP/2.0\r\n\r\n");
}

/*
//呼叫结果元素
struct  TUsrResult
{
	//总呼量
	int iAllReqNum;
	int iOkResponseNum;
	int iNoResponseNum;
	int iBadResponseNum;
};
return: 非0则sleep(1),0则继续
*/

int net_complete_func(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen)
{
	iPkgTheoryLen = 0;
	char *pMsgData = (char*)pData;

	//no head
	char *pHeadEnd = strstr(pMsgData,"\r\n\r\n");
	if((pHeadEnd == NULL) || (pHeadEnd+4-pMsgData > (int)unDataLen))
		return 0;

	pHeadEnd += 4;	

	//no body,return head
	char *pContentLength = strcasestr(pMsgData,"Content-Length:");
	if((pContentLength == NULL) || (pContentLength > pHeadEnd))
		return pHeadEnd - pMsgData;

	int iHeadLen = pHeadEnd - pMsgData;
	int iBodyLen = unDataLen - iHeadLen;

	int iContentLength = atol(pContentLength+15);
	
	if(iBodyLen < iContentLength)
	{
		iPkgTheoryLen = iHeadLen + iContentLength;
		return 0;
	}
	else
	{
		//多余的也带上
		return iHeadLen+iBodyLen;
	}
}

int CUserFunc::DoOnce(struct TUsrResult *pResult)
{
	
	pResult->iAllReqNum++;

	m_TcpCltSocket.TcpWrite(m_pszBuff,m_iLen);

	unsigned int iBuffLen = 0;
	int iRet = 0;
	do{
		int iReadLen = m_TcpCltSocket.TcpRead(m_pszBuff2+iBuffLen,m_iLen-iBuffLen);
		if(iReadLen <= 0)
			break;
		iBuffLen += iReadLen;

		int iTheoryLen;
		iRet = net_complete_func(m_pszBuff2,iBuffLen,iTheoryLen);
	}while(iRet<=0);
	
    if(iBuffLen == 0)
    {
    	pResult->iNoResponseNum++;
    }
	else
    {
    	pResult->iOkResponseNum++;
    }		
	
	return 0;
	
}
