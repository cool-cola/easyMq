/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: 内存数据传输管道, -lpthread

1写1读时，一个进程修改尾指针，一个进程修改头指针，
而对这两个指针的读写是原子操作,故不需要互斥


100w次测试:
fifo cost 684352 us
socketpair cost 1006577 us
pipe cost 714497 us

fifo性能远高于socketpair 和 pipe

CodeQueue: 实现基于指定内存空间的循环队列。支持读锁写锁，（spin_lock,mutex_lock)

***********************************************************/

#ifndef _MEMQUEUE_HPP
#define _MEMQUEUE_HPP

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

#include "spin_lock.h"

typedef struct 
{
	int iShmSize;
	int iWlock;
	int iRlock;
	int iNumber;
	unsigned short usUniqueID;
}TCQCfgStruct;

#define QUEUERESERVELENGTH		8	//预留部分的长度
#define MAX_LOCK_STRUCT_SIZE	1024

#define	MAX_CQCONF_NAME_SIZE	256
#define	MAX_CQCONF_VAL_SIZE	256

class CCodeQueue
{
public:
	static int GetMQCfgFromFile(char* szMQFile, TCQCfgStruct &stCQCfgStruct, char *pszFifoPath = NULL);
	static key_t FTOK(const char *pathname, char proj_id);
	static int CreateMQByFile(char* szMQFile, CCodeQueue* pCodeQueue);
	static char* CreateMQShm(key_t iShmKey,int iShmSize,int &iErrNo,bool &bNewCreate,int &iOutShmID);
	
	enum
	{
	    Init = 0,
	    Recover = 1
	};	

	typedef enum
	{
		emNoLock = 0,
		emSpinLock,
		emPthreadMutexLock,
	}EMLockType;
	
	CCodeQueue();
	~CCodeQueue();

	int AttachMem(int iFifoKey,char* pMem, int iMemSize,int iInitType=Init,
					int iWLock=emNoLock,int iRlock=emNoLock,void* pWLock=NULL,void* pRLock=NULL,char* szFifoPath=NULL);
	
	int GetCodeLength();
	int GetSize();
	int GetReadNotifyFD();
	int GetWriteNotifyFD();
	void SetID(int iID){m_iID = iID;}
	int GetID(){return m_iID;}
	
	int AddToSelectFD(fd_set *pReadFD);
	int ClearSelectFD(fd_set *pReadFD);
	int GetCodeQueueNum(){return 1;};
	int GetWLock(){return m_iWLock;};
	int GetRLock(){return m_iRLock;};
	
	int SendNotify();	
	int WaitData(struct timeval *pTimeval);

	bool IsQueueEmpty();
	bool IsQueueFull();
	void Print(FILE* fp = stdout);

	CCodeQueue* GetCodeQueue(short sCQID){return this;};
	
	int AppendOneCode(const char *pInCode, int iInLength,bool bAutoNotify=true);
	int GetHeadCode(char *pOutCode, int *piOutLength);

	/*************************************************
	以下4个函数仅在1对1通信时可使用，不能用于竞争模式，
	其目的在于避免用户和CQ之间的数据拷贝，其基本思想在于只传递指针
	*************************************************/	
	int GetAppendPtr(char *&pInCode, int *iInLength);
	int AppendCodePtr(int iCodeLength);
	
	int GetHeadCodePtr(char *&pOutCode, int *piOutLength);
	int SkipHeadCodePtr();

	//多管道兼容接口
	int AppendOneCode(short sCQID,const char *pInCode, int iInLength,bool bAutoNotify=true)
	{return AppendOneCode(pInCode,iInLength,bAutoNotify);}
	int GetHeadCode(short &sCQID,char *pOutCode, int *piOutLength)
	{return GetHeadCode(pOutCode,piOutLength);}
	int GetAppendPtr(short sCQID,char *&pInCode, int *iInLength)
	{return GetAppendPtr(pInCode,iInLength);}
	int AppendCodePtr(short sCQID,int iCodeLength)
	{return AppendCodePtr(iCodeLength);}
	int GetHeadCodePtr(short &sCQID,char *&pOutCode, int *piOutLength)
	{return GetHeadCodePtr(pOutCode,piOutLength);}
	int SkipHeadCodePtr(short sCQID)
	{return SkipHeadCodePtr();}
	
private:
	int _CreateFifo(int iFifoKey,char *szFifoPath);
	int _AppendOneCode(const char *pInCode, int iInLength,bool bAutoNotify);
	int _GetHeadCode(char *pOutCode, int *piOutLength);
	
	/****互斥使用*****/
	//锁类型
	int m_iWLock;
	int m_iRLock;

	//锁
	void* m_pWLock;
	void* m_pRLock;

	//内部锁空间
	char m_szWLockSpace[MAX_LOCK_STRUCT_SIZE];
	char m_szRLockSpace[MAX_LOCK_STRUCT_SIZE];

	//头部定义
	typedef struct
	{
		int m_nSize;
		volatile int m_nBegin;
		volatile int m_nEnd;
	}TCodeQueueHead;

	TCodeQueueHead *m_pCodeQueueHead;
	char * m_pCodeBuf;	

	//通知使用
	int m_iNotifyFiFoFD;
	int m_aiPipeFD[2];

	//自身ID
	int m_iID;
};

#endif  /* _MEMQUEUE_HPP */

