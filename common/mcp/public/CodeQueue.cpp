/******************************************************************************
 * CodeQueue.cpp
 * 
 * Copyright 2000-2008 TENCENT.Ltd.
 * 
 * DESCRIPTION: - 
 *     码流队列
 * --------------------
 * 2006年8月25日, nekeyzhong 

 100字节 162w tps (app+get)
 ******************************************************************************/
#include "CodeQueue.hpp"
#include <sys/stat.h>

/*************************************************
Description:	
从CQ配置文件读取配置

Input:	szMQFile 管道配置文件
Output:	
		Size 管道共享内存字节大小
		Wlock 写锁类型 EMLockType
		Rlock 读锁类型 EMLockType
		Number 管道数量默认1个
		UniqueID 本文件唯一ID默认0
*************************************************/
int CCodeQueue::GetMQCfgFromFile(char* szMQFile, TCQCfgStruct &stCQCfgStruct, char *pszFifoPath)
{
	memset(&stCQCfgStruct,0,sizeof(stCQCfgStruct));
	stCQCfgStruct.iNumber = 1;
	
	if (!szMQFile || (szMQFile[0]==0))
		return -1;

	FILE *pFileFp = fopen(szMQFile,"r+");
	if(!pFileFp)
	{
		printf("Can not open file %s\n",szMQFile);
		return -2;
	}

	char szLine[1024];
	char szName[MAX_CQCONF_NAME_SIZE];
	char szVal[MAX_CQCONF_VAL_SIZE];
	while(NULL != fgets(szLine,sizeof(szLine)-1,pFileFp))	
	{
		//head
		int i=0;
		while(szLine[i] == ' ' || szLine[i] == '\t') 	i++;

		if(szLine[i] == '#' || szLine[i] == '\0' || szLine[i] == '\r' ||szLine[i] == '\n')
		{
			continue;
		}

		int iRet = sscanf(szLine,"%s %s",szName,szVal);
		if(iRet != 2)
		{
			printf("Bad line %s\n",szLine);
			return -3;
		}
		
		if(0 == strcmp(szName,"Size"))
			stCQCfgStruct.iShmSize = atoi(szVal);
		else if(0 == strcmp(szName,"WLock"))
			stCQCfgStruct.iWlock = atoi(szVal);
		else if(0 == strcmp(szName,"RLock"))
			stCQCfgStruct.iRlock = atoi(szVal);
		else if(0 == strcmp(szName,"Number"))
			stCQCfgStruct.iNumber = atoi(szVal);		
		else if(0 == strcmp(szName,"UniqueID"))
			stCQCfgStruct.usUniqueID = (unsigned short)atoi(szVal);
		else if(0 == strcmp(szName,"FifoPath") && NULL != pszFifoPath)
		{
			strncpy(pszFifoPath,szVal,MAX_CQCONF_VAL_SIZE);
			pszFifoPath[MAX_CQCONF_VAL_SIZE-1] = 0;
		}
	}
	fclose(pFileFp);
	return 0;
}

key_t CCodeQueue::FTOK(const char *pathname, char proj_id)
{
	//get cofig from mq file
	TCQCfgStruct stCQCfgStruct;
	int iRet = CCodeQueue::GetMQCfgFromFile((char*)pathname,stCQCfgStruct);
	if (iRet < 0)
	{
		printf("CCodeQueue::FTOK %s failed! Ret %d\n",pathname,iRet);
		return -1;	
	}	
	
	/*
	key = ((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((proj_id & 0xff) << 24));	
	原始ftok的算法存在冲突，并不唯一，这里考虑兼容和规避
	*/
	key_t key = ftok(pathname,proj_id);
	if(stCQCfgStruct.usUniqueID == 0)
	{
		return key;
	}
	else
	{
		//proj_id + FF + UniqueID
		return 0x00FF0000 | ((proj_id & 0xff) << 24) | (int)stCQCfgStruct.usUniqueID;
	}
}

/*************************************************
Description:	
根据key,size创建共享内存

Input:	key,size
Output:	iErrNo,bNewCreate是否是新建的
*************************************************/
char* CCodeQueue::CreateMQShm(key_t iShmKey,int iShmSize,int &iErrNo,bool &bNewCreate,int &iOutShmID)
{
	int iShmID = shmget( iShmKey, iShmSize, IPC_CREAT|IPC_EXCL|0666);
	if( iShmID < 0 )
	{
		bNewCreate = false;
		if(errno != EEXIST )
		{
			iErrNo = errno;
			return NULL;
		}

		iShmID = shmget( iShmKey, iShmSize, 0666);
		if( iShmID < 0 )
		{
			iShmID = shmget( iShmKey, 0, 0666);
			if( iShmID < 0 )
			{
				iErrNo = errno;
				return NULL;
			}
			else
			{
				struct shmid_ds s_ds;
				shmctl(iShmID,IPC_STAT,&s_ds);
				if(s_ds.shm_nattch > 0)
				{
					iErrNo = EEXIST;
					return NULL;
				}
			
				if( shmctl( iShmID, IPC_RMID, NULL ) )
				{
					iErrNo = errno;
					return NULL;
				}
				iShmID = shmget( iShmKey, iShmSize, IPC_CREAT|IPC_EXCL|0666);
				if( iShmID < 0 )
				{
					iErrNo = errno;
					return NULL;
				}
				bNewCreate = true;
			}
		}
	}
	else
	{
		bNewCreate = true;
	}

	iErrNo = 0;
	iOutShmID = iShmID;
	return (char * )shmat( iShmID, NULL, 0);
}

/*************************************************
Description:	
根据管道文件建立管道

Input:	szMQFile,pCodeQueue
Output:	无
Return:	<0错误,=0成功
*************************************************/
int CCodeQueue::CreateMQByFile(char* szMQFile, CCodeQueue* pCodeQueue)
{
	if (!pCodeQueue)
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
	bool bNewCreate = false;
	int iOutShmID;
	char *pShmMemQ = CreateMQShm(iShmKeyQ,iShmSize,iErrorNo,bNewCreate,iOutShmID);
	if (!pShmMemQ)
	{
		printf("Create Shm failed! ErrorNo=%d, ERR=%s\n",iErrorNo,strerror(iErrorNo));
		return -2;
	}

	//create w lock
	void* pWLock = NULL;
	if(iWlock == emSpinLock)
	{
		key_t iShmKeyW = CCodeQueue::FTOK(szMQFile,'S');
		if (iShmKeyW < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}		
		
		pWLock = (void*)shm_spin_lock_init(iShmKeyW);
		if(!pWLock)
		{
			printf("ERR: create w spin_lock failed.\n");
			return -4;
		}		
	}
	else if(iWlock == emPthreadMutexLock)
	{
		key_t iShmKeyW = CCodeQueue::FTOK(szMQFile,'M');
		if (iShmKeyW < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}	
		
	    pthread_mutexattr_t mattr;
	    pthread_mutexattr_init(&mattr);
	    pthread_mutexattr_setpshared(&mattr,PTHREAD_PROCESS_SHARED);
		
		int iErrno;
		bool bNewCreate;
		int iOutShmID;
		
		//w lock
		pthread_mutex_t *pstMutexW = (pthread_mutex_t*)CreateMQShm(iShmKeyW,sizeof(pthread_mutex_t),
									iErrno,bNewCreate,iOutShmID);
		if(bNewCreate)
		{
			 pthread_mutex_init(pstMutexW,&mattr);
		}
		else
		{
			struct shmid_ds s_ds;
			shmctl(iOutShmID,IPC_STAT,&s_ds);
			if(s_ds.shm_nattch == 1)
			{
				pthread_mutex_init(pstMutexW,&mattr);
			}
		}
		pWLock = (void*)pstMutexW;
		pthread_mutexattr_destroy(&mattr);
	}	
	//no w lock
	else
	{
		;
	}

	//create r lock
	void* pRLock = NULL;
	if(iRlock == emSpinLock)
	{
		key_t iShmKeyR = CCodeQueue::FTOK(szMQFile,'s');
		if (iShmKeyR < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}		
		
		pRLock = (void*)shm_spin_lock_init(iShmKeyR);
		if(!pRLock)
		{
			printf("ERR: create r spin_lock failed.\n");
			return -4;
		}		
	}
	else if(iRlock == emPthreadMutexLock)
	{
		key_t iShmKeyR = CCodeQueue::FTOK(szMQFile,'m');
		if (iShmKeyR < 0)
		{
			printf("[%s:%d]CCodeQueue::FTOK %s fro iShmKey failed!\n",__FILE__,__LINE__,szMQFile);
			return -6;
		}	
		
	    pthread_mutexattr_t mattr;
	    pthread_mutexattr_init(&mattr);
	    pthread_mutexattr_setpshared(&mattr,PTHREAD_PROCESS_SHARED);
		
		int iErrno;
		bool bNewCreate;
		int iOutShmID;
		
		//r lock
		pthread_mutex_t *pstMutexR = (pthread_mutex_t*)CreateMQShm(iShmKeyR,sizeof(pthread_mutex_t),
									iErrno,bNewCreate,iOutShmID);
		if(bNewCreate)
		{
			 pthread_mutex_init(pstMutexR,&mattr);
		}
		else
		{
			struct shmid_ds s_ds;
			shmctl(iOutShmID,IPC_STAT,&s_ds);
			if(s_ds.shm_nattch == 1)
			{
				pthread_mutex_init(pstMutexR,&mattr);
			}
		}
		pRLock = (void*)pstMutexR;
		pthread_mutexattr_destroy(&mattr);
	}	
	//no r lock
	else
	{
		;
	}
	
	if (pCodeQueue->AttachMem(iShmKeyQ,pShmMemQ,iShmSize,
				bNewCreate?CCodeQueue::Init:CCodeQueue::Recover,
				iWlock,iRlock,(void*)pWLock,(void*)pRLock,szFifoPath))
	{
		printf("Call AttachMem failed!\n");
		return -3;
	}

	if (bNewCreate)
		printf("Create New Pipe Success,Key:%p,Size:%d,WLock %d,RLock %d\n",
				(void*)iShmKeyQ,iShmSize,iWlock,iRlock);
	else
		printf("Attach To Pipe Success,Key:%p,Size:%d,WLock %d,RLock %d\n",
				(void*)iShmKeyQ,iShmSize,iWlock,iRlock);
	
	printf("%s\n",szMQFile);
	return 0;
}

CCodeQueue::CCodeQueue()
{
	m_pCodeQueueHead = NULL;
	m_pCodeBuf = NULL;
	m_iWLock = emNoLock;
	m_iRLock = emNoLock;
	m_pWLock = NULL;
	m_pRLock = NULL;

	m_aiPipeFD[0] = -1;
	m_aiPipeFD[1] = -1;
	m_iNotifyFiFoFD = -1;	

	m_iID = 0;
}

CCodeQueue::~CCodeQueue()
{
}

/*************************************************
Description:	
绑定内存空间给CQ使用,如果iFifoKey = 0 则表示是new出来的空间(不用于进程之间,
不必使用进程间通信方式)

Input:	iFifoKey 创建通知fifo的key
		pMem,iMemSize 管道内存
		iInitType 初始化还是只绑定
		iWLock,iRlock 写锁类型，读锁类型0:nolock 1:spinlock 2:mutex lock
		pWLock,pRLock 已初始化后的写锁和读锁,
		如果为空则其内部自己构造相应类型的锁(线程间使用)，
		如果不为空则使用外部提供的空间(共享内存中,进程间)
		
Output:	无
Return:	<0错误,=0成功
*************************************************/
int CCodeQueue::AttachMem(int iFifoKey,char* pMem, int iMemSize,
			int iInitType/*=Init*/,int iWLock/*=emNoLock*/,int iRlock/*=emNoLock*/,
			void* pWLock/*=NULL*/,void* pRLock/*=NULL*/,char* szFifoPath/*=NULL*/)
{
	if(_CreateFifo(iFifoKey,szFifoPath))
	{
		return -1;
	}
	
	if (iMemSize <= (int)sizeof(TCodeQueueHead))
	{
		printf("[CCodeQueue]MemSize %d must bigger than %d!\n",iMemSize,(int)sizeof(TCodeQueueHead));
		return -2;
	}
	
	m_pCodeQueueHead = (TCodeQueueHead *)pMem;
	m_pCodeBuf = pMem+sizeof(TCodeQueueHead);
	m_iWLock = iWLock;
	m_iRLock = iRlock;

	if(m_iWLock != emNoLock)
		m_pWLock = pWLock?pWLock:(void*)m_szWLockSpace;

	if(m_iRLock != emNoLock)
		m_pRLock = pRLock?pRLock:(void*)m_szRLockSpace;

	//w lock
	if((iWLock == emSpinLock) && (!pWLock))
	{
		spin_lock_init((spinlock_t*)m_pWLock);	
	}
	else if((iWLock == emPthreadMutexLock) && (!pWLock))
	{
		pthread_mutex_init((pthread_mutex_t*)m_pWLock,NULL);
	}

	//r lock
	if((iRlock == emSpinLock) && (!pRLock))
	{
		spin_lock_init((spinlock_t*)m_pRLock);	
	}
	else if((iRlock == emPthreadMutexLock) && (!pRLock))
	{
		pthread_mutex_init((pthread_mutex_t*)m_pRLock,NULL);
	}

	//init
	if (iInitType == Init)
	{
		m_pCodeQueueHead->m_nBegin = 0;
		m_pCodeQueueHead->m_nEnd = 0;
		m_pCodeQueueHead->m_nSize = iMemSize-sizeof(TCodeQueueHead);	
	}
	else
	{
		if (m_pCodeQueueHead->m_nBegin < 0 || m_pCodeQueueHead->m_nEnd < 0)
		{
			assert(false);
			printf("[CCodeQueue]mem must be crashed!\n");
			return -4;
		}
		
		if (m_pCodeQueueHead->m_nSize != iMemSize-(int)sizeof(TCodeQueueHead))
		{
			assert(false);
			printf("[CCodeQueue]mem must be crashed!!\n");
			return -5;
		}
	}
	return 0;
}
int CCodeQueue::GetReadNotifyFD()
{
	if (m_iNotifyFiFoFD > 0)
		return m_iNotifyFiFoFD;
	else
		return m_aiPipeFD[0];
}
int CCodeQueue::GetWriteNotifyFD()
{
	if (m_iNotifyFiFoFD > 0)
		return m_iNotifyFiFoFD;
	else
		return m_aiPipeFD[1];
}
int CCodeQueue::AddToSelectFD(fd_set *pReadFD)
{
	if(!pReadFD)
		return -1;
	
	int iFD = GetReadNotifyFD();
	if (iFD > 0)
	{
		FD_SET(iFD, pReadFD);
		return 0;
	}
	return -1;
}
int CCodeQueue::ClearSelectFD(fd_set *pReadFD)
{
	if(!pReadFD)
		return -1;
	
	int iFD = GetReadNotifyFD();
	if (iFD > 0)
	{
		if(FD_ISSET(iFD, pReadFD))
		{		
			char szTempBuf[512];
			read(iFD, szTempBuf, sizeof(szTempBuf));
		}
		return 0;
	}
	return -1;
}
/*************************************************
Return:	数据总长度
*************************************************/
int CCodeQueue::GetCodeLength()
{
	if (!m_pCodeQueueHead)
		return 0;

	int nTempBegin = m_pCodeQueueHead->m_nBegin;
	int nTempEnd = m_pCodeQueueHead->m_nEnd;

	if( nTempBegin == nTempEnd )
	{
		return 0;
	}
	else if( nTempBegin < nTempEnd )
	{
		return nTempEnd - nTempBegin;
	}
	else
	{
		return m_pCodeQueueHead->m_nSize - (nTempBegin - nTempEnd);
	}
}

/*************************************************
Return:	管道总长度
*************************************************/
int CCodeQueue::GetSize()
{
	if (!m_pCodeQueueHead)
		return 0;
	
	return m_pCodeQueueHead->m_nSize;
}
bool CCodeQueue::IsQueueEmpty()
{
	if(!m_pCodeQueueHead)
		return true;

	return (m_pCodeQueueHead->m_nBegin == m_pCodeQueueHead->m_nEnd)?true:false;
}
bool CCodeQueue::IsQueueFull()
{
	if (!m_pCodeQueueHead)
		return false;

	//重要：最大长度应该减去预留部分长度，保证首尾不会相接
	int nTempMaxLength = m_pCodeQueueHead->m_nSize - GetCodeLength() - QUEUERESERVELENGTH;
	return (nTempMaxLength > 0)?false:true;
}
int CCodeQueue::WaitData(struct timeval *pTimeval)
{
	fd_set ReadFD;
	FD_ZERO(&ReadFD);
	char szTempBuf[1024];	
	
	int iFD = GetReadNotifyFD();
	FD_SET(iFD, &ReadFD);

	int iRet = select(iFD+1, &ReadFD, NULL, NULL, pTimeval);
	if((iRet > 0) && FD_ISSET(iFD, &ReadFD))
	{
		read(iFD, szTempBuf,sizeof(szTempBuf));
	}	

	return iRet>=0?iRet:0;
}
/*************************************************
Description:	
发送消息通知

Input:	无
Output:	无
Return:	0 success ,-1 failed
*************************************************/
int CCodeQueue::SendNotify()
{
	int iRet= write(GetWriteNotifyFD(), "\0", 1);
	return (iRet==1)?0:-1;
}

/*************************************************
Description:	
追加数据，根据初始化选项决定互斥方式

Input:	pInCode,iInLength,bAutoNotify:是否自动发送消息通知,否则自行调用SendNotify()
Output:	无
Return:	!=0错误,=0成功
*************************************************/
int CCodeQueue::AppendOneCode(const char *pInCode, int iInLength,bool bAutoNotify/*=true*/)
{
	if(m_iWLock == emNoLock)
	{
		int iRet = _AppendOneCode(pInCode,iInLength,bAutoNotify);
		return iRet>0?0:iRet;
	}
	else if(m_iWLock == emSpinLock)
	{
		spin_lock((spinlock_t*)m_pWLock);
		int iRet = _AppendOneCode(pInCode,iInLength,bAutoNotify);
		spin_unlock((spinlock_t*)m_pWLock);
		return iRet>0?0:iRet;		
	}
	else if(m_iWLock == emPthreadMutexLock)
	{
		pthread_mutex_lock((pthread_mutex_t*)m_pWLock);
		int iRet = _AppendOneCode(pInCode,iInLength,bAutoNotify);
		pthread_mutex_unlock((pthread_mutex_t*)m_pWLock);
		return iRet>0?0:iRet;
	}	
	else
	{
		assert(0);
	}
}

/*************************************************
Description:	
获取数据，根据初始化选项决定互斥方式

Input:	给定缓冲区位置pOutCode和缓冲区最大长度piOutLength
Output:	实际数据长度piOutLength
Return:	!=0错误,=0成功
*************************************************/	
int CCodeQueue::GetHeadCode(char *pOutCode, int *piOutLength)
{
	if(m_iRLock == emNoLock)
	{
		return _GetHeadCode(pOutCode,piOutLength);
	}
	else if(m_iRLock == emSpinLock)
	{
		spin_lock((spinlock_t*)m_pRLock);
		int iRet = _GetHeadCode(pOutCode,piOutLength);
		spin_unlock((spinlock_t*)m_pRLock);
		return iRet;		
	}
	else if(m_iRLock == emPthreadMutexLock)
	{
		pthread_mutex_lock((pthread_mutex_t*)m_pRLock);
		int iRet = _GetHeadCode(pOutCode,piOutLength);
		pthread_mutex_unlock((pthread_mutex_t*)m_pRLock);
		return iRet;
	}	
	else
	{
		assert(0);		
	}
}

/*************************************************
Description:	
打印管道内容，调试时使用

Input:	输出流
Output:	无
*************************************************/
void CCodeQueue::Print(FILE* fp)
{
	fprintf(fp,"Begin %d, End %d, Length %d:",m_pCodeQueueHead->m_nBegin,m_pCodeQueueHead->m_nEnd,GetCodeLength());

	if (m_pCodeQueueHead->m_nBegin < m_pCodeQueueHead->m_nEnd)
	{
		int iPos = 0;
		for(int  i = m_pCodeQueueHead->m_nBegin; i < m_pCodeQueueHead->m_nEnd; i++ )
		{
	        if( !(iPos%16) )
	        {
				fprintf(fp,"\n%04d>    ", iPos/16+1);
	        }
			iPos++;
			fprintf(fp,"%02x ", (unsigned char)m_pCodeBuf[i]);

			if(iPos > 256)
				break;
		}	
	}
	else if (m_pCodeQueueHead->m_nBegin > m_pCodeQueueHead->m_nEnd)
	{
		int iPos = 0;
		for(int  i = m_pCodeQueueHead->m_nBegin; i < m_pCodeQueueHead->m_nSize; i++ )
		{
	        if( !(iPos%16) )
	        {
				fprintf(fp,"\n%04d>    ", iPos/16+1);
	        }
			iPos++;
			fprintf(fp,"%02x ", (unsigned char)m_pCodeBuf[i]);

			if(iPos > 256)
				break;			
		}	

		for(int  i = 0; i < m_pCodeQueueHead->m_nEnd; i++ )
		{
	        if( !(iPos%16) )
	        {
				fprintf(fp,"\n%04d>    ", iPos/16+1);
	        }
			iPos++;
			fprintf(fp,"%02x ", (unsigned char)m_pCodeBuf[i]);

			if(iPos > 256)
				break;			
		}		
	}

	fprintf(fp,"\n");
}

/*************************************************
Description:	
获取可直接追加数据的位置指针和长度，由用户直接拷入数据，
可避免一次数据拷贝，这个空间必须连续，否则使用上过于麻烦

Input:	无
Output:	返回位置指针和可用长度
Return:	<0错误，=0成功
*************************************************/
int CCodeQueue::GetAppendPtr(char *&pInCode, int *iInLength)
{
	if(m_iWLock != emNoLock)
	{
		return -1;		
	}
	
	if(!m_pCodeQueueHead || m_pCodeQueueHead->m_nSize <= 0 )
	{
		return -1;
	}

	int nTempBegin = m_pCodeQueueHead->m_nBegin;
	int nTempEnd = m_pCodeQueueHead->m_nEnd;

	if(nTempEnd < nTempBegin)
	{
		/*
		* case 2:
		*              |---------------------------------------------|
		*              |>>>>>>>>|    |>>>>>>>>>>Data>>>>>>>>>>>|
		*    Header|---------------------------------------------| Buffer end.
		*/

		pInCode = (char*)&m_pCodeBuf[nTempEnd] + sizeof(int);
		*iInLength = nTempBegin-nTempEnd-sizeof(int)-QUEUERESERVELENGTH;
	}
	else
	{
		/*
		* case 1:
		*              |--------------------------------------|
		*              |         |>>>>>>>>>>>>Data>>>>>>>|    |
		*    Header|--------------------------------------| Buffer end.
		*/
		pInCode = (char*)&m_pCodeBuf[nTempEnd] + sizeof(int);
		*iInLength = m_pCodeQueueHead->m_nSize-nTempEnd-sizeof(int)-QUEUERESERVELENGTH;
	}

	if(*iInLength <= 0)
		return -2;

	return 0;	
}

/*************************************************
Description:	
用户使用GetAppendPtr获取连续空间并拷贝数据后，调用此函数告诉CQ数据的长度

Input:	iCodeLength 数据长度
Output:	无
Return:	<0错误，=0成功
*************************************************/
int CCodeQueue::AppendCodePtr(int iCodeLength)
{
	if(m_iWLock != emNoLock)
	{
		return -1;		
	}
	
	if(!m_pCodeQueueHead || m_pCodeQueueHead->m_nSize <= 0 )
	{
		return -2;
	}

	int nTempBegin = m_pCodeQueueHead->m_nBegin;
	int nTempEnd = m_pCodeQueueHead->m_nEnd;

	if(nTempEnd < nTempBegin)
	{
		/*
		* case 2:
		*              |---------------------------------------------|
		*              |>>>>>>>>|    |>>>>>>>>>>Data>>>>>>>>>>>|
		*    Header|---------------------------------------------| Buffer end.
		*/

		if(iCodeLength > nTempBegin-nTempEnd-(int)sizeof(int)-QUEUERESERVELENGTH)
			return -3;
		
		memcpy((char*)&m_pCodeBuf[nTempEnd],&iCodeLength,sizeof(int));
		nTempEnd = nTempEnd + sizeof(int) + iCodeLength;
	}
	else
	{
		/*
		* case 1:
		*              |--------------------------------------|
		*              |         |>>>>>>>>>>>>Data>>>>>>>|    |
		*    Header|--------------------------------------| Buffer end.
		*/

		if(iCodeLength > m_pCodeQueueHead->m_nSize-nTempEnd-(int)sizeof(int)-QUEUERESERVELENGTH)
			return -4;
		
		memcpy((char*)&m_pCodeBuf[nTempEnd],&iCodeLength,sizeof(int));
		nTempEnd = nTempEnd + sizeof(int) + iCodeLength;
	}

	m_pCodeQueueHead->m_nEnd = nTempEnd;
	return 0;
}

/*************************************************
Description:	
获取下一个数据的位置指针和长度，之后可由用户直接解码数据，
可避免一次数据拷贝，这个空间必须连续，否则用户无法使用

Input:	无
Output:	pOutCode 本次数据位置，piOutLength 本次数据长度
Return:	重复piOutLength长度返回，<0错误，或者无法拿出，=0无数据
*************************************************/
int CCodeQueue::GetHeadCodePtr(char *&pOutCode, int *piOutLength)
{
	if(m_iRLock != emNoLock)
	{
		return -1;		
	}
	
	if(!m_pCodeQueueHead || m_pCodeQueueHead->m_nSize <= 0 )
	{
		return -2;
	}

	int nTempBegin = m_pCodeQueueHead->m_nBegin;
	int nTempEnd = m_pCodeQueueHead->m_nEnd;

	if(nTempEnd < nTempBegin)
	{
		/*
		* case 2:
		*              |---------------------------------------------|
		*              |>>>>>>>>|    |>>>>>>>>>>Data>>>>>>>>>>>|
		*    Header|---------------------------------------------| Buffer end.
		*/

		if(m_pCodeQueueHead->m_nSize - nTempBegin < (int)sizeof(int))
			return -3;

		memcpy(piOutLength,(char*)&m_pCodeBuf[nTempBegin],sizeof(int));
		nTempBegin += sizeof(int);

		if(nTempBegin == m_pCodeQueueHead->m_nSize)
			nTempBegin = 0;

		if(*piOutLength > m_pCodeQueueHead->m_nSize - nTempBegin)
			return -4;
		
		pOutCode = (char*)&m_pCodeBuf[nTempBegin];
	}
	else if(nTempEnd == nTempBegin)
	{
		*piOutLength = 0;
	}
	else
	{
		/*
		* case 1:
		*              |--------------------------------------|
		*              |         |>>>>>>>>>>>>Data>>>>>>>|    |
		*    Header|--------------------------------------| Buffer end.
		*/
		if(nTempEnd - nTempBegin <= (int)sizeof(int))
			return -5;

		memcpy(piOutLength,(char*)&m_pCodeBuf[nTempBegin],sizeof(int));
		nTempBegin += sizeof(int);
		
		pOutCode = (char*)&m_pCodeBuf[nTempBegin];
	}
	
	return *piOutLength;	
}

/*************************************************
Description:	
使用GetHeadCodePtr获得当前数据位置并处理完毕后，告诉CQ跳过本数据

Input:	无
Output:	无
Return:	<0错误，=0成功
*************************************************/
int CCodeQueue::SkipHeadCodePtr()
{
	if(m_iRLock != emNoLock)
	{
		return -1;		
	}
	
	int iOutLength;	
	
	if(!m_pCodeQueueHead || m_pCodeQueueHead->m_nSize <= 0 )
	{
		return -2;
	}
	
	int nTempBegin = m_pCodeQueueHead->m_nBegin;
	int nTempEnd = m_pCodeQueueHead->m_nEnd;
	if( nTempBegin == nTempEnd )
	{
		return -3;
	}

	//取出长度，未打断时,大部分情况走这个分支，考虑到性能拆开对待
	if(nTempBegin + (int)sizeof(int) < m_pCodeQueueHead->m_nSize)
	{
		memcpy(&iOutLength,&m_pCodeBuf[nTempBegin],sizeof(int));
		nTempBegin += sizeof(int);
	}
	//取出长度，打断时	
	else
	{
		char* pTempDst = (char *)&iOutLength;
		for(int i=0 ;i<4; i++)
		{
			pTempDst[i] = m_pCodeBuf[nTempBegin];
			nTempBegin = (nTempBegin+1) % m_pCodeQueueHead->m_nSize;
		}
	}

	nTempBegin = (nTempBegin + iOutLength) % m_pCodeQueueHead->m_nSize;

	m_pCodeQueueHead->m_nBegin = nTempBegin;
	return 0;
}

/*************************************************
Description:	
创建fifo通知管道和互斥信号量，进程间使用CQ时调用

Input:	本管道唯一的key
Output:	无
Return:	<0错误,=0成功
*************************************************/
int CCodeQueue::_CreateFifo(int iFifoKey, char* szFifoPath)
{    
	m_aiPipeFD[0] = -1;
	m_aiPipeFD[1] = -1;
	m_iNotifyFiFoFD = -1;	
	
	if(iFifoKey == 0)
	{
		if (pipe(m_aiPipeFD) < 0)
		{ 
			return -1;
		}
		//write
		fcntl(m_aiPipeFD[1], F_SETFL, fcntl(m_aiPipeFD[1], F_GETFL, 0) | O_NONBLOCK | O_NDELAY);
		//read
		fcntl(m_aiPipeFD[0], F_SETFL, fcntl(m_aiPipeFD[0], F_GETFL, 0) | O_NONBLOCK | O_NDELAY);	
	}
	else
	{
		//创建进程间通知管道fifo
		char pFifoFile[512];
		if(NULL != szFifoPath && 0 != strlen(szFifoPath))
		{
			sprintf(pFifoFile,"%s/%d_%d.fifo",szFifoPath,iFifoKey,m_iID);
		}
		else
		{
			sprintf(pFifoFile,"/tmp/%d_%d.fifo",iFifoKey,m_iID);
		}

		if ((mkfifo(pFifoFile, 0666 | O_NONBLOCK | O_NDELAY)) < 0)
		{
			if (errno != EEXIST)
			{
				printf("[CCodeQueue]mkfifo %s failed!\n",pFifoFile);
				return -2;
			}	
		}
		
		if ((m_iNotifyFiFoFD = open(pFifoFile, O_RDWR)) < 0)
		{
			printf("[CCodeQueue]open fifo %s failed!\n",pFifoFile);
			return -3;
		}
		
		fcntl(m_iNotifyFiFoFD, F_SETFL, fcntl(m_iNotifyFiFoFD, F_GETFL, 0) | O_NONBLOCK | O_NDELAY);
	}
	
	return 0;
}

/*************************************************
Description:	
追加数据到CQ，此函数只改变End 位置

Input:	给定数据位置pInCode和长度iInLength , bAutoNotify:是否自动发送消息通知,否则自行调用SendNotify()
Output:	无
Return:	
<0错误(-1 invalid para; -2 not enough; -3 data crashed)
>0成功(值表示尾部位置)
*************************************************/
int CCodeQueue::_AppendOneCode(const char *pInCode, int iInLength,bool bAutoNotify)
{
	if( !pInCode || iInLength <= 0 )
		return -1;

	if(!m_pCodeQueueHead || m_pCodeQueueHead->m_nSize <= 0 )
		return -2;

	int nTempBegin = m_pCodeQueueHead->m_nBegin;
	int nTempEnd = m_pCodeQueueHead->m_nEnd;
	int nTempRt = nTempEnd;	
	
	//重要：最大长度应该减去预留部分长度，保证首尾不会相接
	int nTempMaxLength = m_pCodeQueueHead->m_nSize - GetCodeLength() - QUEUERESERVELENGTH;
	if((int)(iInLength + sizeof(int)) > nTempMaxLength )
	{
		//printf("In CCodeQueue::AppendOneCode, queue %d bytes left,not enough!\n",nTempMaxLength);
		return -3;
	}

	//取出长度，未打断时,大部分情况走这个分支，考虑到性能拆开对待
	if(nTempEnd + (int)sizeof(int) < m_pCodeQueueHead->m_nSize)
	{
		memcpy(&m_pCodeBuf[nTempEnd],&iInLength,sizeof(int));
		nTempEnd += sizeof(int);
	}
	//取出长度，打断时	
	else
	{
		char* pTempSrc = (char *)&iInLength;
		for(int i=0 ;i<4; i++)
		{
			m_pCodeBuf[nTempEnd] = pTempSrc[i];
			nTempEnd = (nTempEnd+1) % m_pCodeQueueHead->m_nSize;
		}	
	}

	if( nTempBegin > nTempEnd )
	{
		/*
		* case 2:
		*              |---------------------------------------------|
		*              |>>>>>>>>|    |>>>>>>>>>>Data>>>>>>>>>>>|
		*    Header|---------------------------------------------| Buffer end.
		*/
		memcpy((void *)&m_pCodeBuf[nTempEnd], (const void *)pInCode, iInLength);
	}
	else
	{
		/*
		* case 1:
		*              |--------------------------------------|
		*              |         |>>>>>>>>>>>>Data>>>>>>>|    |
		*    Header|--------------------------------------| Buffer end.
		*/
		
		//如果出现分片，则分段拷贝
		int iRightEdgeLength = m_pCodeQueueHead->m_nSize - nTempEnd;
		if(iInLength > iRightEdgeLength)
		{
			memcpy((void *)&m_pCodeBuf[nTempEnd], (const void *)&pInCode[0], iRightEdgeLength );
			memcpy((void *)&m_pCodeBuf[0],(const void *)&pInCode[iRightEdgeLength], (int)(iInLength - iRightEdgeLength ) );
		}
		else
		{
			memcpy((void *)&m_pCodeBuf[nTempEnd], (const void *)&pInCode[0], (int)iInLength);
		}
	}
	
	nTempEnd = (nTempEnd + iInLength) % m_pCodeQueueHead->m_nSize;

	//这里必须原子赋值,防止流程中断造成错误数据
	m_pCodeQueueHead->m_nEnd = nTempEnd;

	//空到非空则发送通知
	if(bAutoNotify && (GetCodeLength() == (int)sizeof(int) + iInLength))
	{
		write(GetWriteNotifyFD(), "\0", 1);
	}

	return nTempRt;
}

/*************************************************
Description:	
得到CQ中第一个包，将数据拷贝给用户，此函数只改变Begin 位置

Input:	给定缓冲区位置pOutCode和缓冲区最大长度piOutLength
Output:	实际数据长度piOutLength
Return:	!=0错误,=0成功
*************************************************/
int CCodeQueue::_GetHeadCode(char *pOutCode, int *piOutLength)
{
	char *pTempDst;

	if( !pOutCode || !piOutLength )
		return -1;

	if(!m_pCodeQueueHead || m_pCodeQueueHead->m_nSize <= 0 )
		return -2;
	
	//获得输出缓冲区的大小
	int iOutBufSize = *piOutLength;
	int nTempBegin = m_pCodeQueueHead->m_nBegin;
	int nTempEnd = m_pCodeQueueHead->m_nEnd;
	
	if( nTempBegin == nTempEnd )
	{
		*piOutLength = 0;
		return -3;
	}

	//取出长度，未打断时,大部分情况走这个分支，考虑到性能拆开对待
	if(nTempBegin + (int)sizeof(int) < m_pCodeQueueHead->m_nSize)
	{
		memcpy(piOutLength,&m_pCodeBuf[nTempBegin],sizeof(int));
		nTempBegin += sizeof(int);
	}
	//取出长度，打断时	
	else
	{
		pTempDst = (char *)piOutLength;	
		for(int i=0 ;i<4; i++)
		{
			pTempDst[i] = m_pCodeBuf[nTempBegin];
			nTempBegin = (nTempBegin+1) % m_pCodeQueueHead->m_nSize;
		}
	}

	//输出缓冲区不够
	if(iOutBufSize < (int)*piOutLength)
	{
		printf("In CCodeQueue::GetHeadCode error, outbuffer size <%d> is less than msg len %d\n", iOutBufSize, *piOutLength);
		return -6;
	}

	pTempDst = (char *)&pOutCode[0];

	if( nTempBegin < nTempEnd )
	{
		/*
		* case 1:
		*              |--------------------------------------|
		*              |         |>>>>>>>>>>>>Data>>>>>>>|    |
		*    Header|--------------------------------------| Buffer end.
		*/
		memcpy((void *)pTempDst, (const void *)&m_pCodeBuf[nTempBegin], (int)(*piOutLength));
	}
	else
	{
		/*
		* case 2:
		*              |---------------------------------------------|
		*              |>>>>>>>>|    |>>>>>>>>>>Data>>>>>>>>>>>|
		*    Header|---------------------------------------------| Buffer end.
		*/

		//如果出现分片，则分段拷贝
		int iRightEdgeLength = m_pCodeQueueHead->m_nSize - nTempBegin;
		if( iRightEdgeLength < *piOutLength)
		{
			memcpy((void *)pTempDst, (const void *)&m_pCodeBuf[nTempBegin],iRightEdgeLength);
			pTempDst += iRightEdgeLength;
			memcpy((void *)pTempDst, (const void *)&m_pCodeBuf[0], (int)(*piOutLength-iRightEdgeLength));
		}
		//否则，直接拷贝
		else	
		{
			memcpy((void *)pTempDst, (const void *)&m_pCodeBuf[nTempBegin], (int)(*piOutLength));
		}
	}
	nTempBegin = (nTempBegin + (*piOutLength)) % m_pCodeQueueHead->m_nSize;
	
	//这里必须原子赋值,防止流程中断造成错误数据
	m_pCodeQueueHead->m_nBegin = nTempBegin;
	return 0;
}

#if 0
#include <stdio.h>
#include <sys/time.h>

int main(int argc ,char **argv)
{
	
	int iErrNo;
	bool bNewCreate = true;
	char *pmem = new char[1*1024*1024];// CreateMQShm(3242343,10000000,iErrNo,bNewCreate);
	CCodeQueue stQueue;
	stQueue.AttachMem(0, pmem,1*1024*1024,CCodeQueue::Init,1,1);
	//CCodeQueue::CreateMQByFile("mq.conf",&stQueue);
	
	timeval t1,t2;
	gettimeofday(&t1,NULL);
	char ss[2000];
	memset(ss,'c',2000);
	char ss2[2000];
	unsigned size = 2000;
	int datalen = 0;
	int perlen = 501;
	int cnt = 1000000;
	for (int i=0;; i++)
	{
		stQueue.AppendOneCode(ss,perlen);
		stQueue.AppendOneCode(ss,perlen);
		stQueue.AppendOneCode(ss,perlen);
		
		ss2[5] = 9;datalen = 2000;stQueue.GetHeadCode((char*)ss2,&datalen);
		ss2[5] = 9;datalen = 2000;stQueue.GetHeadCode((char*)ss2,&datalen);
		ss2[5] = 9;datalen = 2000;stQueue.GetHeadCode((char*)ss2,&datalen);

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
	}
	gettimeofday(&t2,NULL);
	unsigned sp = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("sp = %d ms,cnt %d\n",sp,cnt);
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

