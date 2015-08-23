/******************************************************************************
 * CodeQueue.cpp
 * 
 * Copyright 2000-2008 TENCENT.Ltd.
 * 
 * DESCRIPTION: - 
 *     码流队列
 * --------------------
 * 2006年8月25日, nekeyzhong 

 ******************************************************************************/
#include "CodeQueueMutil.hpp"

/*************************************************
Description:	
根据管道文件建立管道

Input:	szMQFile,pCodeQueueMutil
Output:	无
Return:	<0错误,=0成功
*************************************************/
int CCodeQueueMutil::CreateMQByFile(char* szMQFile, CCodeQueueMutil* pCodeQueueMutil)
{
	if (!pCodeQueueMutil)
	{
		return -1;
	}

	//get cofig from mq file
	TCQCfgStruct stCQCfgStruct;
	char szFifoPath[MAX_CQCONF_VAL_SIZE] = {0};
	int iRet = CCodeQueue::GetMQCfgFromFile(szMQFile,stCQCfgStruct,szFifoPath);
	if (iRet < 0)
	{
		printf("GetMQFromFile %s failed!\n",szMQFile);
		return -2;	
	}

	int iShmSize = stCQCfgStruct.iShmSize;
	int iWlock = stCQCfgStruct.iWlock;
	int iRlock = stCQCfgStruct.iRlock;
	int iNumber = stCQCfgStruct.iNumber;
	
	if(iNumber <= 0 || iNumber > MAX_CODE_QUEUE_NUMBER)
	{
		printf("[%s:%d]%s iNumber %d value error!\n",__FILE__,__LINE__,szMQFile,iNumber);
		return -4;
	}
	
	if(iShmSize <= 0)
	{
		printf("[%s:%d]%s Size %d value error!\n",__FILE__,__LINE__,szMQFile,iShmSize);
		return -4;
	}

	//create shm for mq
	key_t iShmKeyQ = CCodeQueue::FTOK(szMQFile,'Q');
	if (iShmKeyQ < 0)
	{
		printf("[%s:%d]CCodeQueue::FTOK %s for iShmKeyQ failed!\n",__FILE__,__LINE__,szMQFile);
		return -5;
	}
	
	int iErrorNo = 0;
	bool bShmNewCreate = false;
	int iOutShmID;
	char *pShmMemQ = CCodeQueue::CreateMQShm(iShmKeyQ,iShmSize*iNumber,iErrorNo,bShmNewCreate,iOutShmID);
	if (!pShmMemQ)
	{
		printf("Create Shm failed! ErrorNo=%d, ERR=%s\n",iErrorNo,strerror(iErrorNo));
		return -2;
	}

	char *pWLockMem = NULL;
	bool bLockNewCreate;
	int iWLockSize = 0;
	if(iWlock == CCodeQueue::emSpinLock)
	{
		iWLockSize = sizeof(spinlock_t);
			
		key_t iShmKeyW = CCodeQueue::FTOK(szMQFile,'S');
		if (iShmKeyW < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}	
		
		pWLockMem = (char*)shm_spin_lock_init(iShmKeyW,iNumber);
		if(!pWLockMem)
		{
			printf("ERR: create w spin_lock failed.\n");
			return -4;
		}	
	}
	else if(iWlock == CCodeQueue::emPthreadMutexLock)
	{
		iWLockSize = sizeof(pthread_mutex_t);
		
		key_t iShmKeyW = CCodeQueue::FTOK(szMQFile,'M');
		if (iShmKeyW < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}	
		
		pWLockMem = CCodeQueue::CreateMQShm(iShmKeyW,sizeof(pthread_mutex_t)*iNumber,iErrorNo,bLockNewCreate,iOutShmID);
		if(!pWLockMem)
		{
			printf("Create Shm for PthreadMutexLock failed! ErrorNo=%d, ERR=%s\n",iErrorNo,strerror(iErrorNo));
			return -2;
		}

	    pthread_mutexattr_t mattr;
	    pthread_mutexattr_init(&mattr);
	    pthread_mutexattr_setpshared(&mattr,PTHREAD_PROCESS_SHARED);
			
		struct shmid_ds s_ds;
		shmctl(iOutShmID,IPC_STAT,&s_ds);
		int iAttach = s_ds.shm_nattch;

		if((iAttach==1) || bLockNewCreate)
		{
			for(int i=0; i<iNumber; i++)
			{
				pthread_mutex_t *pstMutexW = (pthread_mutex_t*)(pWLockMem+sizeof(pthread_mutex_t)*i);
				pthread_mutex_init(pstMutexW,&mattr);
			}		
		}	
	}
	//no w lock
	else
	{
		;
	}

	//r lock
	char *pRLockMem = NULL;
	int iRLockSize = 0;
	if(iRlock == CCodeQueue::emSpinLock)
	{
		iRLockSize = sizeof(spinlock_t);
		
		key_t iShmKeyR = CCodeQueue::FTOK(szMQFile,'s');
		if (iShmKeyR < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}	

		pRLockMem = (char*)shm_spin_lock_init(iShmKeyR,iNumber);
		if(!pRLockMem)
		{
			printf("ERR: create r spin_lock failed.\n");
			return -4;
		}
	}
	else if(iRlock == CCodeQueue::emPthreadMutexLock)
	{
		iRLockSize = sizeof(pthread_mutex_t);
		
		key_t iShmKeyR = CCodeQueue::FTOK(szMQFile,'m');
		if (iShmKeyR < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}	
		
		pRLockMem = CCodeQueue::CreateMQShm(iShmKeyR,sizeof(pthread_mutex_t)*iNumber,iErrorNo,bLockNewCreate,iOutShmID);
		if(!pRLockMem)
		{
			printf("Create Shm for PthreadMutexLock failed! ErrorNo=%d, ERR=%s\n",iErrorNo,strerror(iErrorNo));
			return -2;
		}

	    pthread_mutexattr_t mattr;
	    pthread_mutexattr_init(&mattr);
	    pthread_mutexattr_setpshared(&mattr,PTHREAD_PROCESS_SHARED);
			
		struct shmid_ds s_ds;
		shmctl(iOutShmID,IPC_STAT,&s_ds);
		int iAttach = s_ds.shm_nattch;

		if((iAttach==1) || bLockNewCreate)
		{
			for(int i=0; i<iNumber; i++)
			{
				pthread_mutex_t *pstMutexR = (pthread_mutex_t*)(pRLockMem+sizeof(pthread_mutex_t)*i);
				pthread_mutex_init(pstMutexR,&mattr);
			}		
		}	
	}
	//no r lock
	else
	{
		;
	}

	for(int i=0; i<iNumber; i++)
	{
		pCodeQueueMutil->m_apCodeQueue[i] = new CCodeQueue();
		pCodeQueueMutil->m_apCodeQueue[i]->SetID(i);

		char *pWLock = (char*)pWLockMem + iWLockSize*i;
		char *pRLock = (char*)pRLockMem + iRLockSize*i;
		if (pCodeQueueMutil->m_apCodeQueue[i]->AttachMem(iShmKeyQ,pShmMemQ+i*iShmSize,iShmSize,
					bShmNewCreate?CCodeQueue::Init:CCodeQueue::Recover,
					iWlock,iRlock,(void*)pWLock,(void*)pRLock,szFifoPath))
		{
			printf("Call AttachMem failed!\n");
			return -3;
		}
	}

	pCodeQueueMutil->m_iCQNumber = iNumber;

	if (bShmNewCreate)
		printf("Create New Pipe Success,Key:%p,Size:%d x %d,WLock %d,RLock %d\n",
				(void*)iShmKeyQ,iShmSize,iNumber,iWlock,iRlock);
	else
		printf("Attach To Pipe Success,Key:%p,Size:%d x %d,WLock %d,RLock %d\n",
				(void*)iShmKeyQ,iShmSize,iNumber,iWlock,iRlock);	
	printf("%s ",szMQFile);	
	return 0;
}

CCodeQueueMutil::CCodeQueueMutil()
{
	memset(m_apCodeQueue,0,sizeof(m_apCodeQueue));
	m_iStartCQID = 0;
	m_iCQNumber = 0;
}

CCodeQueueMutil::~CCodeQueueMutil()
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		if(!m_apCodeQueue[i])
			break;

		delete m_apCodeQueue[i];
		m_apCodeQueue[i] = NULL;
	}
}


/*************************************************
Return:	数据总长度
*************************************************/
int CCodeQueueMutil::GetCodeLength()
{
	int iAllLen = 0;
	for(int i=0; i<m_iCQNumber; i++)
	{
		iAllLen += m_apCodeQueue[i]->GetCodeLength();
	}
	
	return iAllLen;
}

/*************************************************
Return:	管道总长度
*************************************************/
int CCodeQueueMutil::GetSize()
{
	int iAllSize = 0;
	for(int i=0; i<m_iCQNumber; i++)
	{
		iAllSize += m_apCodeQueue[i]->GetSize();
	}
	
	return iAllSize;
}
bool CCodeQueueMutil::IsQueueEmpty()
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		if(!m_apCodeQueue[i]->IsQueueEmpty())
			return false;
	}
	return true;
}
bool CCodeQueueMutil::IsQueueFull()
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		if(!m_apCodeQueue[i]->IsQueueFull())
			return false;
	}
	return true;
}

CCodeQueue* CCodeQueueMutil::GetCodeQueue(short sCQID)
{
	if(sCQID >= m_iCQNumber || sCQID < 0)
		return NULL;
	
	if(!m_apCodeQueue[sCQID])
		return NULL;
	
	return m_apCodeQueue[sCQID];
}
int CCodeQueueMutil::GetCodeQueueNum()
{
	return m_iCQNumber;
}
int CCodeQueueMutil::AddToSelectFD(fd_set *pReadFD)
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		m_apCodeQueue[i]->AddToSelectFD(pReadFD);
	}	
	return 0;
}
int CCodeQueueMutil::ClearSelectFD(fd_set *pReadFD)
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		m_apCodeQueue[i]->ClearSelectFD(pReadFD);
	}	
	return 0;
}
int CCodeQueueMutil::WaitData(struct timeval *pTimeval)
{
	fd_set ReadFD;
	FD_ZERO(&ReadFD);
	char szTempBuf[1024];	

	for(int i=0; i<m_iCQNumber; i++)
	{
		int iFD = m_apCodeQueue[i]->GetReadNotifyFD();
		if(iFD >= 0)
			FD_SET(iFD, &ReadFD);
	}

	int iRet = select(FD_SETSIZE, &ReadFD, NULL, NULL, pTimeval);
	if(iRet > 0)
	{
		for(int i=0; i<m_iCQNumber; i++)
		{
			int iFD = m_apCodeQueue[i]->GetReadNotifyFD();
			if(FD_ISSET(iFD, &ReadFD))
			{
				read(iFD, szTempBuf,sizeof(szTempBuf));
			}
		}	
	}

	return iRet>=0?iRet:0;
}

/*************************************************
Description:	
追加数据，根据初始化选项决定互斥方式

Input:	pInCode,iInLength,bAutoNotify:是否自动发送消息通知,否则自行调用SendNotify()
Output:	无
Return:	!=0错误,=0成功
*************************************************/
int CCodeQueueMutil::AppendOneCode(short sCQID,const char *pInCode, int iInLength,bool bAutoNotify/*=true*/)
{
	if(!m_apCodeQueue[sCQID])
		return -1;
	
	int iRet = m_apCodeQueue[sCQID]->AppendOneCode(pInCode,iInLength,bAutoNotify);
	return iRet>0?0:iRet;
}

/*************************************************
Description:	
获取数据，根据初始化选项决定互斥方式

Input:	给定缓冲区位置pOutCode和缓冲区最大长度piOutLength
Output:	实际数据长度piOutLength
Return:	!=0错误,=0成功
*************************************************/	
int CCodeQueueMutil::GetHeadCode(short &sCQID,char *pOutCode, int *piOutLength)
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		m_iStartCQID++;
		m_iStartCQID = m_iStartCQID%m_iCQNumber;	
		
		int iOutLength = *piOutLength;
		int iRet = m_apCodeQueue[m_iStartCQID]->GetHeadCode(pOutCode,&iOutLength);
		if(iRet == 0)
		{
			sCQID = (short)m_iStartCQID;
			*piOutLength = iOutLength;
			return iRet;
		}
	}
	return -1;
}

/*************************************************
Description:	
打印管道内容，调试时使用

Input:	输出流
Output:	无
*************************************************/
void CCodeQueueMutil::Print(FILE* fp)
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		if(!m_apCodeQueue[i])
			break;

		fprintf(fp,"\n\nCQ %d:",i);
		m_apCodeQueue[i]->Print(fp);
	}
}

/*************************************************
Description:	
获取可直接追加数据的位置指针和长度，由用户直接拷入数据，
可避免一次数据拷贝，这个空间必须连续，否则使用上过于麻烦

Input:	无
Output:	返回位置指针和可用长度
Return:	<0错误，=0成功
*************************************************/
int CCodeQueueMutil::GetAppendPtr(short sCQID,char *&pInCode, int *iInLength)
{
	if(!m_apCodeQueue[sCQID])
		return -1;
	
	return m_apCodeQueue[sCQID]->GetAppendPtr(pInCode,iInLength);
}

/*************************************************
Description:	
用户使用GetAppendPtr获取连续空间并拷贝数据后，调用此函数告诉CQ数据的长度

Input:	iCodeLength 数据长度
Output:	无
Return:	<0错误，=0成功
*************************************************/
int CCodeQueueMutil::AppendCodePtr(short sCQID,int iCodeLength)
{
	if(!m_apCodeQueue[sCQID])
		return -1;

	return m_apCodeQueue[sCQID]->AppendCodePtr(iCodeLength);
}

/*************************************************
Description:	
获取下一个数据的位置指针和长度，之后可由用户直接解码数据，
可避免一次数据拷贝，这个空间必须连续，否则用户无法使用

Input:	无
Output:	pOutCode 本次数据位置，piOutLength 本次数据长度
Return:	重复piOutLength长度返回，<0有数据但无法拿出，=0无数据
*************************************************/
int CCodeQueueMutil::GetHeadCodePtr(short &sCQID,char *&pOutCode, int *piOutLength)
{
	for(int i=0; i<m_iCQNumber; i++)
	{
		m_iStartCQID++;
		m_iStartCQID = m_iStartCQID%m_iCQNumber;	
		
		int iRet = m_apCodeQueue[m_iStartCQID]->GetHeadCodePtr(pOutCode,piOutLength);
		if(iRet > 0)
		{
			sCQID = (short)m_iStartCQID;
			return iRet;
		}	
		else if(iRet < 0)
		{
			m_iStartCQID--;
			if(m_iStartCQID < 0 )
				m_iStartCQID = m_iCQNumber-1;
			
			return iRet;
		}	
	}
	return 0;
}

/*************************************************
Description:	
使用GetHeadCodePtr获得当前数据位置并处理完毕后，告诉CQ跳过本数据

Input:	无
Output:	无
Return:	<0错误，=0成功
*************************************************/
int CCodeQueueMutil::SkipHeadCodePtr(short sCQID)
{
	if(!m_apCodeQueue[sCQID])
		return -1;

	return m_apCodeQueue[sCQID]->SkipHeadCodePtr();
}

#if 0
#include <stdio.h>
#include <sys/time.h>

int main(int argc ,char **argv)
{
	CCodeQueueMutil stQueue;
	CCodeQueueMutil::CreateMQByFile("mq_test.conf",&stQueue);
	
	timeval t1,t2;
	gettimeofday(&t1,NULL);
	char ss[2000];
	memset(ss,'c',2000);
	char ss2[2000];
	unsigned size = 2000;
	int datalen = 0;
	int perlen = 5;
	int cnt = 1000000;
	for (int i=0;; i++)
	{
		stQueue.AppendOneCode(0,ss,perlen);
		stQueue.AppendOneCode(0,ss,perlen);
		stQueue.AppendOneCode(0,ss,perlen);
		stQueue.AppendOneCode(1,ss,perlen);
		stQueue.AppendOneCode(2,ss,perlen);

		//stQueue.Print(stdout);
		printf("emtpy %d,",(int)stQueue.IsQueueEmpty());
		printf("full %d,",(int)stQueue.IsQueueFull());
		printf("size %d,",(int)stQueue.GetSize());
		printf("len %d,",(int)stQueue.GetCodeLength());
		
		while(1)
		{
			short cq;
			datalen = 2000;
			int iRet = stQueue.GetHeadCode(cq,(char*)ss2,&datalen);
			if(iRet != 0)
				break;
			
			if(perlen != datalen)
			{
				printf("bad len\n");
				return -1;
			}
			if( 0 != memcmp(ss,ss2,perlen))
			{
				printf("bad data\n");
				return -1;
			}
			printf("get from %d",(int)cq);
		}

		break;
	}
		printf("emtpy %d,",(int)stQueue.IsQueueEmpty());
		printf("full %d,",(int)stQueue.IsQueueFull());
		printf("size %d,",(int)stQueue.GetSize());
		printf("len %d,",(int)stQueue.GetCodeLength());

	stQueue.Print(stdout);
	
/*

	gettimeofday(&t1,NULL);
	for (int i=0; i<1000000; i++)
	{
		char* pAppPtr;
		int iAppLen = 0;
		if(stQueue.GetAppendPtr(pAppPtr,&iAppLen))
		{
			if(stQueue.AppendOneCode(ss,200))
				printf("ffffffff\n");		
		}
		else if(iAppLen < 2000)
		{
			if(stQueue.AppendOneCode(ss,200))
				printf("ffffffff\n");			
		}
		else
		{
			memcpy(pAppPtr,ss,datalen);
			stQueue.AppendCodePtr(datalen);
		}

		//-----
		if(stQueue.GetHeadCodePtr(pAppPtr,&iAppLen))
		{
			datalen = 2000;
			if(stQueue.GetHeadCode((char*)ss,&datalen))	
				printf("bbbbb\n");
		}
		else
		{
			memcpy(ss,pAppPtr,iAppLen);
			stQueue.SkipHeadCodePtr();
		}
			
	}
	gettimeofday(&t2,NULL);
	sp = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("sp = %d,cnt 1000000\n",sp);
*/	
	return 0;
}

#endif


