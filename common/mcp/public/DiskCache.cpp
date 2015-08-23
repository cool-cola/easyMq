#include <vector>
using namespace std;

#include "DiskCache.hpp"

enum
{
	op_set = 1,
	op_del,
};

//设置TBucketNode的主键
ssize_t Mem_SetNodeKey(void* pObj,void* pKey,ssize_t iKeyLen)
{
	DiskCache::THashNode* pHashNode = (DiskCache::THashNode*)pObj;
	if (iKeyLen < (ssize_t)sizeof(pHashNode->m_szKey))
	{
		return -1;
	}
	memset(pHashNode,0,sizeof(DiskCache::THashNode));
	memcpy(pHashNode->m_szKey,pKey,sizeof(pHashNode->m_szKey));
	return 0;
}

//获取TBucketNode的主键
ssize_t Mem_GetNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen)
{
	DiskCache::THashNode* pHashNode = (DiskCache::THashNode*)pObj;
	memcpy(pKey,pHashNode->m_szKey,sizeof(pHashNode->m_szKey));
	iKeyLen = sizeof(pHashNode->m_szKey);
	return 0;
}

DiskCache::DiskCache()
{
	m_pMemCacheHead = NULL;
	memset(m_szDumpFile,0,sizeof(m_szDumpFile));
	m_iInitType = 0;
	m_pBuffer = new char[BUFFSIZE];
	m_iTimeNow = 0;

	INIT_MUTEXLOCK(m_MutexLock);
}

DiskCache::~DiskCache()
{
	delete []m_pBuffer;
}
/*
大于iBlockSize的最多占50%,所以block 最多比node多0.5倍???
*/
size_t DiskCache::CountMemSize(ssize_t iBucketNum/*=0*/,size_t iHashNum/*=0*/,size_t FILESIZE,size_t iBlockSize/*=512*/)
{
	if(iHashNum == 0)
	{
		size_t iMaxObjNum = TDiskIdxObjMng::CountObjNumByFile(iBlockSize,FILESIZE);
		iHashNum = (ssize_t)(iMaxObjNum/(float)1.5);
	}

	if(iBucketNum == 0)
		iBucketNum = iHashNum;
		
	size_t iSize = sizeof(TMemCacheHead)+CHashTab::CountMemSize((ssize_t)iBucketNum) +
		TIdxObjMng::CountMemSize(sizeof(THashNode),iHashNum,3) +
		CObjQueue::CountMemSize()+CBuffMng::CountMemSize(iHashNum);

	size_t iObjNum = TDiskIdxObjMng::CountObjNumByFile(iBlockSize,FILESIZE);

	iSize+=TDiskIdxObjMng::CountMemSize(iObjNum,1);

	printf("DiskCache::CountMemSize BucketNum %lld,HashNum %lld,FILESIZE %lld,BlockSize %lld,Cost %lld bytes.\n",
		(long long)iBucketNum,(long long)iHashNum,(long long )FILESIZE,(long long )iBlockSize,(long long)iSize);
	return iSize;
}

ssize_t DiskCache::AttachMemFile(char* pMemPtr,const size_t MEMSIZE, ssize_t iInitType,
			ssize_t iBucketNum/*=0*/,size_t iHashNum/*=0*/,
			char* pDiskFile,size_t iDiskFileStartPos/*=0*/,const size_t FILESIZE,size_t iBlockSize/*=512*/)
{
	if (!pMemPtr || (MEMSIZE<=0) ||(iHashNum<0))
		return -1;

	ASSERT(iBlockSize >= sizeof(ssize_t));

	//自动计算iHashNum
	if(iHashNum == 0)
	{
		ssize_t iMaxObjNum = TDiskIdxObjMng::CountObjNumByFile(iBlockSize,FILESIZE);
		iHashNum = (ssize_t)(iMaxObjNum/(float)1.5);
	}
	if(iBucketNum == 0)
		iBucketNum = iHashNum;
	
	size_t iNeedMemSize = CountMemSize(iBucketNum,iHashNum,FILESIZE,iBlockSize);
	
	if (MEMSIZE < iNeedMemSize)
	{
		printf("MEMSIZE %lld not enough! need %lld bytes\n",(long long)MEMSIZE,(long long)iNeedMemSize);
		return -2;
	}
	
	ssize_t iAttachBytes=0,iAllocBytes = 0;

	//头部
	m_pMemCacheHead = (TMemCacheHead*)pMemPtr;
	iAllocBytes = sizeof(TMemCacheHead);

	/* 新创建的cache置为不完整，整个cache初始化后再置为完整 */
	if ( iInitType == emInit )
		m_pMemCacheHead->m_iDataOK = 0;

	if ( iInitType==emRecover && !m_pMemCacheHead->m_iDataOK )
		iInitType = emInit;

	m_iInitType = iInitType;

	//索引部分-----------------------------------
	iAttachBytes = m_stBucketHashTab.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,(ssize_t)iBucketNum,iInitType);
	if (iAttachBytes < 0)
		return -3;
	iAllocBytes += iAttachBytes;

	//hash需要1条链
	iAttachBytes = m_stHashObjMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,sizeof(THashNode),iHashNum,iInitType,3);
	if (iAttachBytes < 0)
		return -4;
	iAllocBytes += iAttachBytes;
	
	if(m_stBucketHashTab.AttachIdxObjMng(&m_stHashObjMng,Mem_SetNodeKey,Mem_GetNodeKey))
	{
		return -5;
	}

	//LRU 需要2条链
	iAttachBytes = m_stLRUQueue.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes, iInitType);
	if (iAttachBytes < 0)
		return -6;
	iAllocBytes += iAttachBytes;

	if(m_stLRUQueue.AttachIdxObjMng(&m_stHashObjMng))
	{
		return -7;
	}
	
	//数据部分------------------------------------

	//m_stBuffMng 需要1条链
	iAttachBytes = m_stBuffMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iHashNum,iInitType);
	if (iAttachBytes < 0)
		return -8;
	iAllocBytes += iAttachBytes;

	//剩下的分给数据块
	iAttachBytes = m_stBuffObjMng.AttachMemFile(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iInitType,
									pDiskFile,iDiskFileStartPos,FILESIZE,iBlockSize,0,1);
	if (iAttachBytes < 0)
		return -9;
	iAllocBytes += iAttachBytes;
	
	if(m_stBuffMng.AttachIdxObjMng(&m_stBuffObjMng))
	{
		return -7;
	}		

	m_pMemCacheHead->m_iDataOK = 1;
	m_pMemPtr = pMemPtr;
	m_iMemSize = iAllocBytes;	
	return iAllocBytes;
}

ssize_t DiskCache::DumpInit(char *DumpFile/*=cache.dump*/,
	char * pBinLogBaseName/*=binlog_*/,
	ssize_t iMaxBinLogSize/*=20000000*/,
	ssize_t iMaxBinLogNum/*=20*/)
{
	strcpy(m_szDumpFile,DumpFile);
	
	m_stBinLog.Init(pBinLogBaseName,iMaxBinLogSize,iMaxBinLogNum,true);
	return 0;
}

ssize_t DiskCache::StartUp()
{
	//非新建的共享内存,不用恢复
	if (m_iInitType==emInit)
	{
		/* 恢复前，置cache为不完整，恢复后再置为完整 */
		m_pMemCacheHead->m_iDataOK = 0;
		if (access(m_szDumpFile, F_OK) == 0)
		{
			//恢复core
			ssize_t iRet = _CoreRecover();
			if (iRet < 0)
			{
				printf("Recover from dumpfile(%s) failed, ret %lld.\n",m_szDumpFile,(long long)iRet);
				return iRet;
			}
			printf("Recover from dumpfile(%s) %lld bytes.\n",m_szDumpFile,(long long)iRet);	
		}
		else
		{
			printf("no dumpfile to recover.\n");
		}
		
		ssize_t iOp = 0;	
		ssize_t iCount = 0;
		ssize_t iLogLen = 0;
		
		u_int64_t tLogTime;
		//恢复日志流水,均为脏数据
		m_stBinLog.ReadRecordStart();	
		while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(m_pBuffer,(int32_t)BUFFSIZE,tLogTime)))
		{
			ssize_t iBuffPos = 0;
			
			memcpy(&iOp,m_pBuffer,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);

			ssize_t iDataSize = 0;	
			memcpy(&iDataSize,m_pBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);

			THashNode stHashNode;
			memcpy(&stHashNode,m_pBuffer+iBuffPos,sizeof(THashNode));
			iBuffPos += sizeof(THashNode);

			if (iOp == op_set)
			{
				_Set(&stHashNode,iDataSize);
			}		
			else if (iOp == op_del)
			{	
				_Del(&stHashNode);
			}
			else 
			{
				printf("bad binlog op %lld.\n",(long long)iOp);
			}
			
			iCount++;
		}

		m_pMemCacheHead->m_iDataOK	= 1;
		printf("recover from binlog %lld records.\n",(long long)iCount);	
	}

	/* scan invalid key */
	for(ssize_t iBucket = 0;iBucket<m_stBucketHashTab.GetBucketNum();iBucket++)
	{
		ssize_t iCurrIdx = m_stBucketHashTab.GetBucketNodeHead(iBucket);
		if(iCurrIdx < 0)
			continue;

		do{
			THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByIdx(iCurrIdx);

			if(pHashNode->m_cFlag > NODE_WFLAG_VALID)
			{
				pHashNode->m_cFlag = NODE_WFLAG_VALID;
			}
			else if ( pHashNode->m_cFlag == NODE_WFLAG_WRITING )
			{
				pHashNode->m_cFlag = NODE_WFLAG_INVALID;
			}

			iCurrIdx = m_stBucketHashTab.GetBucketNodeNext(iCurrIdx);
		}while(iCurrIdx >= 0);

	}

	return 0;
}
//return 0 success
ssize_t DiskCache::GetNodeReserve(char szKey[HASH_KEY_LEN],char* pReserve,const ssize_t RESERVELEN/*=NODE_RESERVE_LEN*/)
{
	MutexGuard MyLock(&m_MutexLock);
	
	ssize_t iObjIdx = -1;
	THashNode* pOldHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pOldHashNode)	
		return E_NO_DATA;

	if(pOldHashNode->m_cFlag == NODE_WFLAG_WRITING)
		return E_WRITTING;

	if(pOldHashNode->m_cFlag == NODE_WFLAG_INVALID)
		return E_INVALID;
	
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);

	pOldHashNode->m_iLastUpdate = m_iTimeNow?m_iTimeNow:time(NULL);

	ssize_t iCopyLen = (RESERVELEN>NODE_RESERVE_LEN) ? NODE_RESERVE_LEN:RESERVELEN;
	memmove(pReserve,pOldHashNode->m_szReserve,iCopyLen);
	return 0;
}
//return 0 success
ssize_t DiskCache::SetNodeReserve(char szKey[HASH_KEY_LEN],char* pReserve,const ssize_t RESERVELEN/*=NODE_RESERVE_LEN*/)
{
	if(RESERVELEN > NODE_RESERVE_LEN)
		return E_BUFF_TOO_SMALL;
	
	MutexGuard MyLock(&m_MutexLock);

	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pHashNode)
	{
		pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
		if (!pHashNode)
			return E_NO_HASH_SPACE;		
	}

	if(pHashNode->m_cFlag == NODE_WFLAG_WRITING)
		return E_WRITTING;

	if(pHashNode->m_cFlag == NODE_WFLAG_INVALID)
		return E_INVALID;
	
	memcpy(pHashNode->m_szReserve,pReserve,RESERVELEN);

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);

	pHashNode->m_iLastUpdate = m_iTimeNow?m_iTimeNow:time(NULL);

	if ( _WriteNodeLog(op_set,pHashNode,0) )
		return E_ERR_WRITE_BINLOG;

	return 0;
}

//return 0 success
ssize_t DiskCache::Set(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,char* pReserve/*=NULL*/,ssize_t iReserveLen/*=0*/)
{
	ssize_t ret = 0;
	
	if (!szKey)
		return -1;
	
	if(iReserveLen > NODE_RESERVE_LEN)
		return E_RESERVE_BUFF_SMALL;

	ssize_t iObjIdx = -1;
	THashNode* pHashNode = NULL;
	
	//内存部分加锁
	{
		MutexGuard MyLock(&m_MutexLock);
		pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
		if (pHashNode)
		{
			if(pHashNode->m_cFlag == NODE_WFLAG_WRITING)
				return E_WRITTING;

			if(pHashNode->m_cFlag > NODE_WFLAG_VALID)
				return E_READING;
			
			if(m_stBuffMng.SetBufferSpace(iObjIdx,iDataSize))
				return E_NO_OBJ_SPACE;			
		}
		else
		{
			pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
			if (!pHashNode)
				return E_NO_HASH_SPACE;	

			if(m_stBuffMng.SetBufferSpace(iObjIdx,iDataSize))
			{
				m_stBucketHashTab.DeleteObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN);
				return E_NO_OBJ_SPACE;
			}			
		}

		memcpy(pHashNode->m_szReserve,pReserve,iReserveLen);
		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stLRUQueue.AppendToTail(iObjIdx);	

		pHashNode->m_iLastUpdate = m_iTimeNow?m_iTimeNow:time(NULL);
		
		//binlog 中设置为无效
		pHashNode->m_cFlag = NODE_WFLAG_INVALID;
		ret = _WriteNodeLog(op_set,pHashNode,iDataSize);
			
		//mem中设置为正在写
		pHashNode->m_cFlag = NODE_WFLAG_WRITING;
	}
	
	//write disk
	ssize_t iRet = m_stBuffMng.SetDataToSpace(iObjIdx,szData,iDataSize);
		
	{
		MutexGuard MyLockDisk(&m_MutexLock);
		if(iRet != 0)
			pHashNode->m_cFlag = NODE_WFLAG_INVALID;
		else 
			pHashNode->m_cFlag = NODE_WFLAG_VALID;

		ret = _WriteNodeLog(op_set,pHashNode,0);
	}

	if ( ret )
		return E_ERR_WRITE_BINLOG;
	
	return 0;
}


//return 0 success
ssize_t DiskCache::SetNoLock(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,char* pReserve/*=NULL*/,ssize_t iReserveLen/*=0*/)
{
	ssize_t ret = 0;
	
	if (!szKey)
		return -1;
	
	if(iReserveLen > NODE_RESERVE_LEN)
		return E_RESERVE_BUFF_SMALL;

	ssize_t iObjIdx = -1;
	THashNode* pHashNode = NULL;
	
	//内存部分加锁
	{
		pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
		if (pHashNode)
		{
			if(pHashNode->m_cFlag == NODE_WFLAG_WRITING)
				return E_WRITTING;

			if(pHashNode->m_cFlag > NODE_WFLAG_VALID)
				return E_READING;
			
			if(m_stBuffMng.SetBufferSpace(iObjIdx,iDataSize))
				return E_NO_OBJ_SPACE;			
		}
		else
		{
			pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
			if (!pHashNode)
				return E_NO_HASH_SPACE;	

			if(m_stBuffMng.SetBufferSpace(iObjIdx,iDataSize))
			{
				m_stBucketHashTab.DeleteObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN);
				return E_NO_OBJ_SPACE;
			}			
		}

		memcpy(pHashNode->m_szReserve,pReserve,iReserveLen);
		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stLRUQueue.AppendToTail(iObjIdx);	

		pHashNode->m_iLastUpdate = m_iTimeNow?m_iTimeNow:time(NULL);
		
		//binlog 中设置为无效
		pHashNode->m_cFlag = NODE_WFLAG_INVALID;
		ret = _WriteNodeLog(op_set,pHashNode,iDataSize);
			
		//mem中设置为正在写
		pHashNode->m_cFlag = NODE_WFLAG_WRITING;
	}
	
	//write disk
	ssize_t iRet = m_stBuffMng.SetDataToSpace(iObjIdx,szData,iDataSize);
		
	{
		if(iRet != 0)
			pHashNode->m_cFlag = NODE_WFLAG_INVALID;
		else 
			pHashNode->m_cFlag = NODE_WFLAG_VALID;

		ret = _WriteNodeLog(op_set,pHashNode,0);
	}

	if ( ret )
		return E_ERR_WRITE_BINLOG;
	
	return 0;
}


//返回长度
ssize_t DiskCache::Get(char szKey[HASH_KEY_LEN],char* szData,const ssize_t DATASIZE,
					char* pReserve/*=NULL*/,const ssize_t RESERVELEN/*=NODE_RESERVE_LEN*/)
{
	if (!szKey || !szData || DATASIZE<=0)
	{
		return -1;
	}
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = NULL;
	
	//内存部分加读锁
	{
		MutexGuard MyLock(&m_MutexLock);
		pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
		if (!pHashNode)
			return E_NO_DATA;

		if(pHashNode->m_cFlag == NODE_WFLAG_WRITING)
			return E_WRITTING;
			
		if(pHashNode->m_cFlag < NODE_WFLAG_VALID)
			return E_INVALID;

		if (DATASIZE < m_stBuffMng.GetBufferSize(iObjIdx))
		{
			return E_BUFF_TOO_SMALL;
		}	
		
		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stLRUQueue.AppendToTail(iObjIdx);

		pHashNode->m_iLastUpdate = m_iTimeNow?m_iTimeNow:time(NULL);

		if(pReserve)
		{
			ssize_t iCopyLen = (RESERVELEN>NODE_RESERVE_LEN) ? NODE_RESERVE_LEN:RESERVELEN;
			memmove(pReserve,pHashNode->m_szReserve,iCopyLen);		
		}

		// 设置节点状态为正在读，即不能对其进行删除
		// 因为删除会重新分配，数据被重写
		pHashNode->m_cFlag++;
	}
	
	//read disk
	ssize_t iDataLen = m_stBuffMng.GetBuffer(iObjIdx,szData,DATASIZE);

	{
		MutexGuard MyLock(&m_MutexLock);
		pHashNode->m_cFlag--;
	}	
	return iDataLen;
}

//返回长度
/* ONLY called in dumping status */
ssize_t DiskCache::GetNoLockNoLRU(char szKey[HASH_KEY_LEN],char* szData,const ssize_t DATASIZE,
					char* pReserve/*=NULL*/,const ssize_t RESERVELEN/*=NODE_RESERVE_LEN*/)
{
	if (!szKey || !szData || DATASIZE<=0)
	{
		return -1;
	}
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = NULL;
	
	{
		pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
		if (!pHashNode)
			return E_NO_DATA;

		if(pHashNode->m_cFlag == NODE_WFLAG_WRITING)
			return E_WRITTING;
			
		if(pHashNode->m_cFlag < NODE_WFLAG_VALID)
			return E_INVALID;

		if (DATASIZE < m_stBuffMng.GetBufferSize(iObjIdx))
		{
			return E_BUFF_TOO_SMALL;
		}	
		
		if(pReserve)
		{
			ssize_t iCopyLen = (RESERVELEN>NODE_RESERVE_LEN) ? NODE_RESERVE_LEN:RESERVELEN;
			memmove(pReserve,pHashNode->m_szReserve,iCopyLen);		
		}

	}
	
	//read disk
	ssize_t iDataLen = m_stBuffMng.GetBuffer(iObjIdx,szData,DATASIZE);

	return iDataLen;
}

//return 0 success
ssize_t DiskCache::Del(char szKey[HASH_KEY_LEN])
{
	ssize_t ret = 0;

	MutexGuard MyLock(&m_MutexLock);
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
		return E_NO_DATA;

	// 正在进行读或者写的时候节点不能被删除，防止被重新使用，导致数据错乱
	if(pHashNode->m_cFlag == NODE_WFLAG_WRITING)
		return E_WRITTING;

	if(pHashNode->m_cFlag > NODE_WFLAG_VALID)
		return E_READING;
			
	ret = _WriteNodeLog(op_del,pHashNode,0);

	m_stBuffMng.FreeBuffer(iObjIdx);
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);

	if ( ret )
		return E_ERR_WRITE_BINLOG;
	
	return 0;
}

/* Called by dumping process for recovering */
ssize_t DiskCache::DelNoLock(char szKey[HASH_KEY_LEN])
{
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
		return E_NO_DATA;

	// 正在进行读或者写的时候节点不能被删除，防止被重新使用，导致数据错乱
	if(pHashNode->m_cFlag == NODE_WFLAG_WRITING)
		return E_WRITTING;

	if(pHashNode->m_cFlag > NODE_WFLAG_VALID)
		return E_READING;
			
	ssize_t ret = _WriteNodeLog(op_del,pHashNode,0);

	m_stBuffMng.FreeBuffer(iObjIdx);
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);

	if ( ret )
		return E_ERR_WRITE_BINLOG;
	
	return 0;
}

ssize_t DiskCache::GetDataSize(char szKey[HASH_KEY_LEN])
{
	MutexGuard MyLock(&m_MutexLock);
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
		return E_NO_DATA;

	return m_stBuffMng.GetBufferSize(iObjIdx);	
}
//return 0 success
ssize_t DiskCache::GetNode(char szKey[HASH_KEY_LEN],THashNode *pHashNode)
{
	MutexGuard MyLock(&m_MutexLock);
	
	ssize_t iObjIdx = -1;
	THashNode* pOldHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pOldHashNode)	
		return E_NO_DATA;

	if(pOldHashNode->m_cFlag == NODE_WFLAG_WRITING)
		return E_WRITTING;

	if(pOldHashNode->m_cFlag == NODE_WFLAG_INVALID)
		return E_INVALID;
	
	memcpy(pHashNode,pOldHashNode,sizeof(THashNode));
	return 0;
}
#ifdef NODE_EXPIRE
ssize_t DiskCache::SetExpireTime(char szKey[HASH_KEY_LEN],int32_t iExpireTime)
{
	MutexGuard MyLock(&m_MutexLock);
	
	ssize_t iObjIdx = -1;
	THashNode* pOldHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pOldHashNode)	
		return E_NO_DATA;

	if(pOldHashNode->m_cFlag == NODE_WFLAG_INVALID)
		return E_INVALID;	

	pOldHashNode->m_iExpireTime = iExpireTime;
}

#endif

void DiskCache::LockCache()
{
	pthread_mutex_lock(&m_MutexLock);
}

void DiskCache::UnLockCache()
{
	pthread_mutex_unlock(&m_MutexLock);
}

ssize_t DiskCache::GetUsage(ssize_t &iBucketNum,ssize_t &iUsedBucket,
							ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,
							ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount)
{
	MutexGuard MyLock(&m_MutexLock);

	m_stBucketHashTab.GetUsage(iBucketNum,iUsedBucket);
	
	//哈希节点和buffmng的节点一一对应的
	return m_stBuffMng.GetBufferUsage(iHashNodeUsed,iHashNodeCount,iObjNodeUsed,iObjNodeCount);
}

//淘汰return 0 success
ssize_t DiskCache::GetOldestNode(THashNode* pHashNode)
{
	MutexGuard MyLock(&m_MutexLock);
	if ( sizeof(THashNode) == m_stHashObjMng.CopyAttachObj((size_t)m_stLRUQueue.GetHeadItem(), 0, (char *)pHashNode, sizeof(THashNode)) )
		return 0;
	else 
		return -1;
}

void DiskCache::Print(FILE *fpOut)
{
	char *pTmpBuffer = new char[BUFFSIZE];
	
	for(ssize_t iBucket = 0; iBucket < m_stBucketHashTab.GetBucketNum(); iBucket++ )
	{ 
		ssize_t iCurrIdx = m_stBucketHashTab.GetBucketNodeHead(iBucket);
		if(iCurrIdx < 0)
			continue;

		fprintf(fpOut, "BUCKET[%06lld]->", (long long)iBucket);
		
		do{
			THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByIdx(iCurrIdx);
			
			fprintf(fpOut, "KEY[");
		    for(ssize_t i = 0; i < HASH_KEY_LEN; i++ )
		    {
				fprintf(fpOut,"%02x", (unsigned char)(pHashNode->m_szKey[i]));
		    }
			fprintf(fpOut, "] Flag %lld ",(long long)pHashNode->m_cFlag);

			ssize_t unDataLen = m_stBuffMng.GetBuffer(iCurrIdx,pTmpBuffer,BUFFSIZE);
			 
			fprintf(fpOut, "Data [%lld]:",(long long)unDataLen);
		    for(ssize_t i = 0; i < unDataLen; i++ )
		    {
				fprintf(fpOut,"%02x", (unsigned char)(pTmpBuffer[i]));
		    }
			
			fprintf(fpOut,"\n");
			
			iCurrIdx = m_stBucketHashTab.GetBucketNodeNext(iCurrIdx);
		}while(iCurrIdx >= 0);
		
	}

	ssize_t iBucketNum,iUsedBucket,iHashNodeUsed,iHashNodeCount,iObjNodeUsed,iObjNodeCount;
	
	GetUsage(iBucketNum,iUsedBucket,iHashNodeUsed,iHashNodeCount,iObjNodeUsed,iObjNodeCount);
	fprintf(fpOut, "iBucketNum %lld,iUsedBucket %lld,iHashNodeUsed %lld,iHashNodeCount %lld,iObjNodeUsed %lld,iObjNodeCount %lld\n",
		(long long)iBucketNum,(long long)iUsedBucket,
		(long long)iHashNodeUsed,(long long)iHashNodeCount,(long long)iObjNodeUsed,(long long)iObjNodeCount);

	fprintf(fpOut, "BlockSize %lld\n",(long long)m_stBuffObjMng.GetObjSize());
	
	fprintf(fpOut, "##############\n");
	
	m_stHashObjMng.Print(fpOut);
	m_stBucketHashTab.Print(fpOut);

	m_stLRUQueue.Print(fpOut);

	m_stBuffMng.Print(fpOut);

	delete []pTmpBuffer;
}

ssize_t DiskCache::CoreDump()
{
	MutexGuard MyLock(&m_MutexLock);
	
	if (0 <= _CoreDump())
	{			
		//delete binlog
		m_stBinLog.ClearAllBinLog();
		return 0;
	}	
	return -1;
}

ssize_t DiskCache::CoreDumpNoLock()
{	
	if (0 <= _CoreDump())
	{			
		//delete binlog
		m_stBinLog.ClearAllBinLog();
		return 0;
	}
	return -1;
}


ssize_t DiskCache::_CoreDump()
{	
	char szLast[256], szTmp[256];
	sprintf(szLast,"%s.last", m_szDumpFile);
	sprintf(szTmp,"%s.tmp", m_szDumpFile);
	
	FILE *pFile =fopen(szTmp,"w+");
	if (!pFile)
	{
		return -1;
	}

	fwrite(m_pMemPtr,1,m_iMemSize,pFile);
	ssize_t iLen = ftell(pFile);
	fclose(pFile);

	rename(m_szDumpFile,szLast);
	rename(szTmp,m_szDumpFile);
	
	return iLen;
}

ssize_t DiskCache::_CoreRecover()
{

	char DumpName[256];

	struct stat stat_buf;


	strcpy(DumpName, m_szDumpFile);
	
	memset(&stat_buf, 0, sizeof(stat_buf));
	stat(DumpName, &stat_buf);
	if ( stat_buf.st_size != m_iMemSize )
	{
		memset(&stat_buf, 0, sizeof(stat_buf));		
		sprintf(DumpName, "%s.last", m_szDumpFile);
		stat(DumpName, &stat_buf);
		if ( stat_buf.st_size != m_iMemSize )
		{
			printf("ERR:dump file[%s] is conflict with memory len[%ld]!!\n",DumpName,m_iMemSize);
			return -3;
		}
	}

	FILE *pFile =fopen(DumpName,"rb+");
	if (!pFile)
	{
		return -1;
	}

	ssize_t iReadLen = fread(m_pMemPtr,1,m_iMemSize,pFile); 
	fclose(pFile);	

	if (iReadLen != m_iMemSize)
	{
		printf("ERR:%s just %lld bytes read!\n",DumpName,(long long)iReadLen);
		return -2;
	}
	return iReadLen;

}

ssize_t DiskCache::_Set(THashNode* pHashNode,ssize_t iDataSize)
{
	ssize_t iObjIdx = -1;
	THashNode* pOldHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN,iObjIdx);
	if(!pOldHashNode)
	{
		assert(iDataSize > 0);
		pOldHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN,iObjIdx);
		assert(pOldHashNode != NULL);		
	}

	memcpy(pOldHashNode,pHashNode,sizeof(THashNode));

	//有数据
	if(iDataSize > 0)
	{
		if(m_stBuffMng.SetBufferSpace(iObjIdx,iDataSize))
			return E_NO_OBJ_SPACE;

		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stLRUQueue.AppendToTail(iObjIdx);		
	}
	return 0;
}

ssize_t DiskCache::_Del(THashNode* pHashNode)
{
	ssize_t iObjIdx = -1;
	THashNode* pOldHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN,iObjIdx);
	if (!pOldHashNode)
		return E_NO_HASH_SPACE;
	
	m_stBuffMng.FreeBuffer(iObjIdx);
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stBucketHashTab.DeleteObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN);
	return 0;
}

//节点拍照
ssize_t DiskCache::_WriteNodeLog(ssize_t iOp,THashNode* pHashNode,ssize_t iDataSize)
{
	ssize_t iBuffLen = 0;
	
	memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
	iBuffLen += sizeof(ssize_t);

	memcpy(m_pBuffer+iBuffLen,&iDataSize,sizeof(ssize_t));
	iBuffLen += sizeof(ssize_t);
	
	memcpy(m_pBuffer+iBuffLen,pHashNode,sizeof(THashNode));
	iBuffLen += sizeof(THashNode);
	
	return m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);		
}

#ifdef 0

#include <sys/time.h>

DiskCache *pMemCache = NULL;

int iok = 1;
void* dododo(void* arg)
{
	char szKey[HASH_KEY_LEN];
	memset(szKey,0,sizeof(szKey));

	char data[1024];
	memset(data,'c',1024);
	char data2[1024];
	memset(data2,0,1024);

	char res[NODE_RESERVE_LEN];
	memset(res,0,NODE_RESERVE_LEN);
	
	char res2[NODE_RESERVE_LEN];
	memset(res2,0,NODE_RESERVE_LEN);
	
	unsigned int cnt = 0;
	while(1)
	{
		int key = rand();
		memcpy(szKey,&key,4);

		key = rand();
		memcpy(szKey+4,&key,4);

		key = rand();
		memcpy(szKey+8,&key,4);

		key = rand();
		memcpy(szKey+12,&key,4);
		
		int datasize = rand()%1024;
		if(datasize <= 0)datasize = 1;

		int ressize = rand()%NODE_RESERVE_LEN;
		if(ressize <= 0)ressize = 1;

		int pos = rand()%datasize;
		data[pos] = rand();
		
		int iRet = pMemCache->Set((char*)szKey,data,datasize,res,ressize);
		if(iRet)
		{
			sleep(2);
			while(iok==0)
				sleep(1);
			
			printf("renew blocksize\n");
			continue;
		}

		memset(data2,0,sizeof(data2));
		memset(res2,0,sizeof(res2));
		
		size_t iDataLen = pMemCache->Get((char*)szKey,(char*)data2,1024,res2,ressize);
		if(iDataLen <= 0)
		{
			printf("bad get ret %d\n",iDataLen);
			return NULL;
		}

		if(iDataLen != datasize)
		{
			printf("bad get datasize %d!=%d\n",iDataLen,datasize);
			return NULL;	
		}	
		if(0 != memcmp(data,data2,iDataLen))
		{
			printf("bad get val\n");
			return NULL;	
		}
	
		if(0 != memcmp(res,res2,ressize))
		{
			printf("bad getres val\n");
			return NULL;	
		}		

		if(datasize %2 == 0)
			 pMemCache->Del((char*)szKey);

		cnt++;
		
	}
	return NULL;
}
#include <pthread.h>
int main()
{
	int MEMSIZE = 100483647;//DiskCache::CountMemSizeByFile(100000000,512);
	char *pMem = new char[MEMSIZE];
		
	char szKey[HASH_KEY_LEN];
	memset(szKey,0,sizeof(szKey));

	char data[1024];
	memset(data,'c',1024);
	char data2[1024];
	memset(data2,0,1024);

	char res[NODE_RESERVE_LEN];
	memset(res,0,NODE_RESERVE_LEN);
	
	char res2[NODE_RESERVE_LEN];
	memset(res2,0,NODE_RESERVE_LEN);
	
	timeval ttt,ttt2;

	//stMemCache.Print(stdout);

	ssize_t FILESIZE = 100000000;
	
	ssize_t nodenum = 1000;
	size_t hashnum = 500;
	
	//printf("CountMemSizeByFile %d\n",DiskCache::CountMemSizeByFile(FILESIZE,4096));
	printf("CountMemSize %d\n",DiskCache::CountMemSize(nodenum,FILESIZE,1));

	pMemCache = new DiskCache();
	int len = pMemCache->AttachMemFile(pMem, MEMSIZE,emInit,hashnum,nodenum,"./disk2",0,FILESIZE,1);
	printf("alloc %d\n",len);
	
	pMemCache->DumpInit();
	pMemCache->StartUp();

	pMemCache->Print(stdout);

/*
	timeval t1,t2;
	gettimeofday(&t1,NULL);

	memset(szKey,'a',sizeof(szKey));
	for(int i=0;i<100000;i++)
	{
		int key = 999;
		pMemCache->Set((char*)szKey,data,200);
	}
	gettimeofday(&t2,NULL);
	int lll = (t2.tv_sec-t1.tv_sec)*1000000 + (t2.tv_usec-t1.tv_usec);
	printf("cost %dus\n",lll);
	return 0;
	*/
		
	
	memset(szKey,'a',sizeof(szKey));
	memset(data,'a',1024);
	pMemCache->Set((char*)szKey,data,43);
	
	memset(szKey,'b',sizeof(szKey));
	memset(data,'b',1024);
	pMemCache->Set((char*)szKey,data,41);

	memset(szKey,'c',sizeof(szKey));
	memset(data,'c',1024);
	pMemCache->Set((char*)szKey,data,41);

	memset(szKey,'d',sizeof(szKey));
	memset(data,'d',1024);
	pMemCache->Set((char*)szKey,data,41);

	memset(szKey,'c',sizeof(szKey));
	pMemCache->Del((char*)szKey);


	//pMemCache->CoreDump();

	return 0;

	

	pthread_t tid[10];
	for (int i=0; i<10;i++)
		pthread_create ( &tid[i], NULL,dododo,NULL); // 3:NULL is ok


	unsigned int cnt = 0;
	while(1)
	{
		int key = rand();
		memcpy(szKey,&key,4);

		key = rand();
		memcpy(szKey+4,&key,4);

		key = rand();
		memcpy(szKey+8,&key,4);

		key = rand();
		memcpy(szKey+12,&key,4);
		
		int datasize = rand()%1024;
		if(datasize <= 0)datasize = 1;

		int ressize = rand()%NODE_RESERVE_LEN;
		if(ressize <= 0)ressize = 1;

		int pos = rand()%datasize;
		data[pos] = rand();
		
		int iRet = pMemCache->Set((char*)szKey,data,datasize);
		if(iRet)
		{
			iok = 0;
			return 0;
			
			sleep(1);
			
			int blocksize = rand()%1024;
			if(blocksize <= 10)blocksize = 10;

			delete pMem;
			int MEMSIZE = DiskCache::CountMemSize(0,100000000,blocksize);
			
			if(MEMSIZE <= 0)
				MEMSIZE = 100483647;
			
			pMem = new char[MEMSIZE];
			
			delete pMemCache;
			pMemCache = new DiskCache();
			pMemCache->AttachMemFile(pMem, MEMSIZE,emInit,0,"./disk",0,100000000,blocksize);

			printf("renew blocksize =%d\n",blocksize);

			iok = 1;
			continue;
		}

		memset(data2,0,sizeof(data2));
		memset(res2,0,sizeof(res2));
		
		size_t iDataLen = pMemCache->Get((char*)szKey,(char*)data2,1024);
		if(iDataLen <= 0)
		{
			printf("bad get ret %d\n",iDataLen);
			return -1;
		}

		if(iDataLen != datasize)
		{
			printf("bad get datasize %d!=%d\n",iDataLen,datasize);
			return -1;	
		}	
		if(0 != memcmp(data,data2,iDataLen))
		{
			printf("bad get val\n");
			return -1;	
		}	

		if(datasize %2 == 0)
			 pMemCache->Del((char*)szKey);

		cnt++;
/*
		if(cnt % 100000 == 0)
		{
			pMemCache->DumpInit();
			
			pMemCache->CoreDump();

			DiskCache::THashNode stNode;
			ssize_t iRet = pMemCache->GetOldestNode(&stNode);
			while(iRet == 0)
			{
				pMemCache->Del(stNode.m_szKey);
				iRet = pMemCache->GetOldestNode(&stNode);	
			}
			
			pMemCache->StartUp();		
		}
		*/
	}	

			/*
	gettimeofday(&ttt,NULL);
	
	for (int i=0; i<1000000;i++)
		{
			int kk = 999;
			size_t ll;
			//stMemCache.Set((char*)&kk, szbb, 1024,ll);
			stMemCache.Get((char*)&kk, szbb, 1024,ll);
		}
	gettimeofday(&ttt2,NULL);
	int iMillSecsSpan = (ttt2.tv_sec-ttt.tv_sec)*1000000+(ttt2.tv_usec-ttt.tv_usec);
	printf("time %d us ,avg %d us\n",iMillSecsSpan,iMillSecsSpan/1000000);
*/
	return 0;
}
#endif

