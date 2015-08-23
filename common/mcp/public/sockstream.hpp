/******************************************************************************
  文 件 名   : SockStream.hpp
  版 本 号   : V1.0
  功能描述   : 非阻塞的socket管道接收器，内部有缓冲，可用于                           
  依赖关系   : 独立无依赖
******************************************************************************/
#ifndef _SOCKSTREAM_H_
#define _SOCKSTREAM_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>

//可以挂接用户判包函数
/*
pData,unDataLen:包数据

iPkgTheoryLen: 包的理论长度,在只收到部分数据的情况下就可以知道
				包长度了，0为无法判断
				
return :完整包的实际长度
*/
typedef int (*check_complete)(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen);

class CSockStream
{
public:
	enum _enParas
	{
		STREAM_CLOSED = 0,
		STREAM_OPENED = 1,
		
		ERR_NOSOCK = -1,
		ERR_AGAIN	= -2,
		ERR_PKG_TOOBIG	= -3,
		ERR_FAILED	= -4,
		ERR_NODATA	= -5,
		ERR_SOCK_CLOSE = -6,
	};
	
	CSockStream(int iSendBuffSize=5*1024*1024,int iRecvBuffSize=5*1024*1024);
	~CSockStream();

	int SetUpStream(int iFD);
	int GetSocketFD();
	int GetStatus();
	int IsFull();
	int GetLeftLen();	

	int AddToCheckSet(fd_set *pCheckSet);
	int IsFDSetted(fd_set *pCheckSet);
	int RecvData();

	bool GetOneCode(int &iCodeLength, char *pCode);
	int SendOneCode(int iCodeLength, char *pCode);
	
	int Mutex_GetOneCode(int &iCodeLength, char *pCode)
	{
		int iRet = 0;
		pthread_mutex_lock(&stMutex);
		iRet = GetOneCode(iCodeLength,pCode);
		pthread_mutex_unlock(&stMutex);
		return iRet;
	}
	
	int Mutex_SendOneCode(int iCodeLength, char *pCode)
	{
		int iRet = 0;
		pthread_mutex_lock(&stMutex);
		iRet = SendOneCode(iCodeLength,pCode);
		pthread_mutex_unlock(&stMutex);
		return iRet;
	}
	int CloseStream();
	
	static check_complete net_complete_func;
protected:
	int m_iSocketFD;					//Socket描述子
	int m_iStatus;					//流状态

private:
	pthread_mutex_t stMutex;   
	
	char* m_pszRecvBuffer;
	int RECV_BUF_SIZE;

	char* m_pszSendBuffer;
	int SEND_BUF_SIZE;
	
	int  m_iReadBegin;
	int  m_iReadEnd;

	int  m_iSendBegin;
	int  m_iSendEnd;
};
#endif
