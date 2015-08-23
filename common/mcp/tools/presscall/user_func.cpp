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


	char szDataFile[512]={0};
	TLib_Cfg_GetConfig(szConfigFile,
			"MsgLen", CFG_INT, &m_iLen,0,
			"DataFile", CFG_STRING, szDataFile, "",sizeof(szDataFile),
			NULL);		


	pthread_mutex_unlock(m_pMutex);

	if(m_iLen <= 0)
		assert(0);

	FILE* fin = fopen(szDataFile, "rb");
	if (fin != NULL)
	{
		fseek(fin, 0, SEEK_END);
		m_iLen = ftell(fin);
		fseek(fin, 0, SEEK_SET);
		m_pszBuff = new char[m_iLen+1024];
		m_pszBuff2  = new char[m_iLen+1024];
		fread(m_pszBuff, m_iLen, 1, fin);
		fclose(fin);
	}
	else
	{
		if(szDataFile[0] != 0)
		{
			printf("read data file error!\n");
			exit(0);
		}
		m_pszBuff = new char[m_iLen+1024];
		m_pszBuff2  = new char[m_iLen+1024];
		memset(m_pszBuff,'a',m_iLen+1024);
	}
	
	int iLen = htonl(m_iLen);
	memcpy(m_pszBuff,&iLen,4);
	
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
	if (unDataLen < sizeof(int))
		return 0;

	int iMsgLen = ntohl(*(int*)pData);
	iPkgTheoryLen = iMsgLen;

	if (iPkgTheoryLen <= (int)unDataLen)
	{
		return iPkgTheoryLen;
	}

	return 0;
}

int CUserFunc::DoOnce(struct TUsrResult *pResult)
{
	
	pResult->iAllReqNum++;

	m_TcpCltSocket.TcpWrite(m_pszBuff,m_iLen);
	int iTheoryLen = 0;
	unsigned int iBuffLen = 0;
	int iRet = 0;
	do{
		int iReadLen = m_TcpCltSocket.TcpRead(m_pszBuff2+iBuffLen,m_iLen-iBuffLen);
		if(iReadLen <= 0)
			break;
		iBuffLen += iReadLen;

		
		iRet = net_complete_func(m_pszBuff2,iBuffLen,iTheoryLen);
	}while(iRet<=0);
	if(m_iLen != iTheoryLen)
	{
		printf("send len %d != iTheoryLen %d\n", m_iLen, iTheoryLen);
		sleep(10);
		exit(-1);
	}
	if(m_iLen != iBuffLen)
	{
		printf("send len %d != iTheoryLen %d\n", m_iLen, iBuffLen);
		sleep(10);
		exit(-1);
	}
	if(memcmp(m_pszBuff, m_pszBuff2, m_iLen))
	{
		printf("send data != return data\n");
		sleep(10);
		exit(-1);
	}
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
