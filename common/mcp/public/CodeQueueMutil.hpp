/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: 内存数据传输管道, -lpthread

CodeQueueMutil: 封装CodeQueue实现的多管道。

***********************************************************/

#ifndef _MEMQUEUEMUTIL_HPP
#define _MEMQUEUEMUTIL_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <assert.h>

#include "CodeQueue.hpp"

#define MAX_CODE_QUEUE_NUMBER	26

class CCodeQueueMutil
{
public:
	static int CreateMQByFile(char* szMQFile, CCodeQueueMutil* pCodeQueueMutil);
	
	CCodeQueueMutil();
	~CCodeQueueMutil();
	
	int GetCodeLength();
	int GetSize();
	int WaitData(struct timeval *pTimeval);
	
	int AddToSelectFD(fd_set *pReadFD);
	int ClearSelectFD(fd_set *pReadFD);

	bool IsQueueEmpty();
	inline bool IsQueueFull();
	void Print(FILE* fp = stdout);

	CCodeQueue* GetCodeQueue(short sCQID);
	int GetCodeQueueNum();

	int GetWLock(short sCQID=0){return m_apCodeQueue[sCQID]->GetWLock();};
	int GetRLock(short sCQID=0){return m_apCodeQueue[sCQID]->GetRLock();};
	

	int AppendOneCode(short sCQID,const char *pInCode, int iInLength,bool bAutoNotify=true);
	int GetHeadCode(short &sCQID,char *pOutCode, int *piOutLength);
	/*************************************************
	以下4个函数仅在1对1通信时可使用，不能用于竞争模式，
	其目的在于避免用户和CQ之间的数据拷贝，其基本思想在于只传递指针
	*************************************************/	
	int GetAppendPtr(short sCQID,char *&pInCode, int *iInLength);
	int AppendCodePtr(short sCQID,int iCodeLength);
	
	int GetHeadCodePtr(short &sCQID,char *&pOutCode, int *piOutLength);
	int SkipHeadCodePtr(short sCQID);

#if 0
	//单管道兼容接口
	int AppendOneCode(const char *pInCode, int iInLength,bool bAutoNotify=true)
	{return AppendOneCode(0,pInCode,iInLength,bAutoNotify);}
	int GetHeadCode(char *pOutCode, int *piOutLength)
	{return GetHeadCode(0,pOutCode,piOutLength);}
	int GetAppendPtr(char *&pInCode, int *iInLength)
	{return GetAppendPtr(0,pInCode,iInLength);}
	int AppendCodePtr(int iCodeLength)
	{return AppendCodePtr(0,iCodeLength);}
	int GetHeadCodePtr(char *&pOutCode, int *piOutLength)
	{return GetHeadCodePtr(0,pOutCode,piOutLength);}
	int SkipHeadCodePtr()
	{return SkipHeadCodePtr(0);}
#endif

private:
	CCodeQueue* m_apCodeQueue[MAX_CODE_QUEUE_NUMBER];

	//管道数量
	int	m_iCQNumber;
	
	//首选管道循环
	int m_iStartCQID;
};

#endif  /* _MEMQUEUE_HPP */


