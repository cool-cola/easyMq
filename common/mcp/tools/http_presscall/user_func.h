#ifndef _USERFUNC_H_
#define _USERFUNC_H_

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h> 
#include "presscall.h"
#include "TSocket.h"

/*
用户实现DoNone接口即可
*/
class CUserFunc
{
public:
	CUserFunc(int iMyID,int iDestIp,int iDestPort,char *szConfigFile,
			pthread_mutex_t *pMutex);
	int DoOnce(struct TUsrResult *pResult);
private:
	pthread_mutex_t *m_pMutex;	
public:
	int m_iMyID;
	int m_iDestIp;
	int m_iDestPort;
	TcpCltSocket m_TcpCltSocket;

	char* m_pszBuff;
	int m_iLen;

	char *m_pszBuff2;
public:

};

#endif

