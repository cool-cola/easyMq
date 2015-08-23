#include "MemKeyCache.hpp"

#include <assert.h>
#ifndef ASSERT
#define ASSERT(COND){\
	assert(#COND);\
}
#endif

extern u_int32_t SuperFastHash(char *data, u_int32_t len);

#define MemMakeHashKey(pKey,iLen)	SuperFastHash((char*)pKey,(u_int32_t)iLen)

enum
{
	op_set = 1,
	op_mark,
	op_del,
	op_append
};

#define MIN_DATE_LEN		(sizeof(int)+sizeof(int)+sizeof(int))

//在独立的连续空间中定位指针
#define DecodeBufferKeyFormat(pBuffer,BUFFLEN,piDataFlag,piKeyLen,pKey) {\
	piDataFlag = (int*)(pBuffer);\
	piKeyLen = (int*)((pBuffer)+sizeof(int));\
	pKey = (pBuffer)+sizeof(int)+sizeof(int);\
}

//int val = ({int ret=999;ret;});
#define EncodeBufferKeyFormat(pBuffer,BUFFLEN,iDataFlag,iKeyLen,pKey) \
({	\
	ssize_t iMakeBuffLen = 2*sizeof(int)+(iKeyLen);\
	if(BUFFLEN < iMakeBuffLen)\
		iMakeBuffLen = -1;\
	else\
	{\
		*(int*)(pBuffer) = (iDataFlag);\
		*(int*)((pBuffer)+sizeof(int)) = (iKeyLen);\
		memmove((pBuffer)+2*sizeof(int),(pKey),(iKeyLen));\
	}\
iMakeBuffLen;})

/*
标准编码格式
flag,keylen,key,reserveLen,reserve,data
*/
#define DecodeBufferFormat(pBuffer,BUFFLEN,piDataFlag,piKeyLen,pKey,piReserveLen,pReserve,iDataLen,pData) {\
	piDataFlag = (int*)(pBuffer);\
	piKeyLen = (int*)((pBuffer)+sizeof(int));\
	pKey = (pBuffer)+sizeof(int)+sizeof(int);\
	piReserveLen = (int*)((pBuffer)+2*sizeof(int)+*(piKeyLen));\
	pReserve = (pBuffer)+2*sizeof(int)+*(piKeyLen)+sizeof(int);\
	pData = (pBuffer)+2*sizeof(int)+*(piKeyLen)+sizeof(int)+*(piReserveLen);\
	iDataLen = (BUFFLEN) - (2*sizeof(int)+*(piKeyLen)+sizeof(int)+*(piReserveLen));\
}

#define EncodeBufferFormat(pBuffer,BUFFLEN,iDataFlag,iKeyLen,pKey,iReserveLen,pReserve,iDataLen,pData) \
({	\
	int iMakeBuffLen = 2*sizeof(int)+(iKeyLen)+sizeof(int)+(iReserveLen)+(iDataLen);\
	if((BUFFLEN) < iMakeBuffLen)\
		iMakeBuffLen = -1;\
	else\
	{\
		*(int*)(pBuffer) = (iDataFlag);\
		*(int*)((pBuffer)+sizeof(int)) = (iKeyLen);\
		memmove((pBuffer)+2*sizeof(int),(pKey),(iKeyLen));\
		*(int*)((pBuffer)+2*sizeof(int)+(iKeyLen)) = (iReserveLen);\
		memmove((pBuffer)+2*sizeof(int)+(iKeyLen)+sizeof(int),(pReserve),(iReserveLen));\
		memmove((pBuffer)+2*sizeof(int)+(iKeyLen)+sizeof(int)+(iReserveLen),(pData),(iDataLen));\
	}\
iMakeBuffLen;})

/*
标准编码格式
flag,keylen,key,reserveLen,reserve,data
*/
#define EncodeSize(iKeyLen,iReserveLen,iDataLen) \
({	\
	ssize_t iEncodeBytes = sizeof(int)+sizeof(int)+iKeyLen+sizeof(int)+iReserveLen+iDataLen;\
iEncodeBytes;})

//按1比1
ssize_t CalcBucketNum(ssize_t iNodeNum)
{
	return iNodeNum;
}

MemKeyCache::MemKeyCache()
{
	m_pMemCacheHead = NULL;
	m_iDumpMin = 0;
	memset(m_szDumpFile,0,sizeof(m_szDumpFile));
	m_iInitType = 0;

	m_iBinLogOpen = 0;
	m_iTotalBinLogSize = 0;
	m_pBuffer = new char[BUFFSIZE];

	pthread_mutex_init(&m_MutexLock, NULL);
}

MemKeyCache::~MemKeyCache()
{
	delete []m_pBuffer;
}

ssize_t MemKeyCache::CountBaseSize(ssize_t iNodeNum,ssize_t iBlockSize/*=512*/)
{
	ssize_t iBucketNum = iNodeNum;
	
	ssize_t iSize = sizeof(TMemCacheHead)+sizeof(ssize_t)*iBucketNum +
		TIdxObjMng::CountMemSize(0,iNodeNum,3) +
		CObjQueue::CountMemSize();

	iSize+=(CBuffMng::CountMemSize(iNodeNum)+TIdxObjMng::CountMemSize(iBlockSize,iNodeNum,1));
	
	return iSize;
}

//绑定内存,NodeNum=0 表示自动计算
ssize_t MemKeyCache::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,
		ssize_t	 iNodeNum/*=0*/,ssize_t iInitType/*=emInit*/,ssize_t iBlockSize/*=512*/)
{
	if (!pMemPtr || (MEMSIZE<=0) ||(iNodeNum<0) || iBlockSize<=0)
		return -1;

	ASSERT(iBlockSize >= MIN_DATE_LEN);
	
	//自动计算NodeNum
	if(iNodeNum == 0)
	{
		ssize_t iLeftSize = MEMSIZE - sizeof(TMemCacheHead) 
						- sizeof(TIdxObjMng::TIdxObjHead) 
						- sizeof(CObjQueue::TObjQueueHead)
						- sizeof(CBuffMng::TBuffMngHead)
						- sizeof(TIdxObjMng::TIdxObjHead);

		/*
		记录长度正太分布在平均记录两边，所以大于平均记录的占50%
		block数量为node数量1.5倍的方程式为:
		
		iLeftSize = X*sizeof(ssize_t)								//bucket
				+X*(TIdxObjMng::GetIdxSize(3)+ 0)
				+0
				+sizeof(CBuffMng::TBuffItem)*X
				+1.5*X*(TIdxObjMng::GetIdxSize(1)+ iBlockSize);
		*/
		double X = iLeftSize/(double)(sizeof(ssize_t)
						+(TIdxObjMng::GetIdxSize(3)+ 0)
						+sizeof(CBuffMng::TBuffItem)
						+1.5*(double)(TIdxObjMng::GetIdxSize(1)+ iBlockSize));

		iNodeNum = (ssize_t)X;
	}
	
	if (MEMSIZE < CountBaseSize(iNodeNum,iBlockSize))
		return -2;

	ssize_t iBucketNum = CalcBucketNum(iNodeNum);
	ssize_t iAttachBytes=0,iAllocBytes = 0;

	//-------TMemCacheHead 头部分配---------------------------
	m_pMemCacheHead = (TMemCacheHead*)pMemPtr;
	iAllocBytes = sizeof(TMemCacheHead);

	if (iInitType==emRecover && !m_pMemCacheHead->m_iDataOK )
		iInitType = emInit;

	m_iInitType = iInitType;
	
	//-------Bucket 分配---------------------------
	m_piBucket = (ssize_t*)(pMemPtr+iAllocBytes);
	iAllocBytes += iBucketNum*sizeof(ssize_t);

	//-------Node 分配--3个下标链,1个bucket主链,2个给rwlru链
	iAttachBytes = m_NodeObjMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,
										0,iNodeNum,iInitType,3);
	if (iAttachBytes < 0)
		return -4;
	iAllocBytes += iAttachBytes;

	ssize_t iNodeDS = m_NodeObjMng.GetOneFreeDS();
	ssize_t iLRUDS1 = m_NodeObjMng.GetOneFreeDS();
	ssize_t iLRUDS2 = m_NodeObjMng.GetOneFreeDS();
	
	//RLRU 需要2条链
	iAttachBytes = m_NodeRLruQueue.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes, iInitType);
	if (iAttachBytes < 0)
		return -6;
	iAllocBytes += iAttachBytes;

	if(m_NodeRLruQueue.AttachIdxObjMng(&m_NodeObjMng,iLRUDS1,iLRUDS2))
	{
		return -7;
	}

	//WLRU 需要2条链
	iAttachBytes = m_NodeWLruQueue.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes, iInitType);
	if (iAttachBytes < 0)
		return -6;
	iAllocBytes += iAttachBytes;

	if(m_NodeWLruQueue.AttachIdxObjMng(&m_NodeObjMng,iLRUDS1,iLRUDS2))
	{
		return -7;
	}	
	
	if (iInitType == emInit)
	{
		m_pMemCacheHead->m_iDataOK = 0;
		m_pMemCacheHead->m_tLastDumpTime = 0;
		m_pMemCacheHead->m_iDSSuffix = iNodeDS;
		m_pMemCacheHead->m_iBucketUsed = 0;		
		m_pMemCacheHead->m_iBucketNum = iBucketNum;
		memset(m_piBucket,-1,iBucketNum*sizeof(ssize_t));
	}
	else
	{
		if(m_pMemCacheHead->m_iBucketNum != iBucketNum)
		{
			return -3;
		}
		if(m_pMemCacheHead->m_iDSSuffix != iNodeDS)
		{
			return -4;
		}		
	}	
		
	//-------Block 分配---------------------------
	iAttachBytes = m_BuffMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iNodeNum,iInitType);
	if (iAttachBytes < 0)
		return -8;
	iAllocBytes += iAttachBytes;

	//剩下的分给数据块, 需要1条链
	iAttachBytes = m_BuffObjMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iBlockSize,0,iInitType,1);
	if (iAttachBytes < 0)
		return -9;
	iAllocBytes += iAttachBytes;
	
	if(m_BuffMng.AttachIdxObjMng(&m_BuffObjMng))
	{
		return -7;
	}	

	m_pMemPtr = pMemPtr;
	m_iMemSize = iAllocBytes;	

	m_pMemCacheHead->m_iDataOK = 1;
	return iAllocBytes;
}

ssize_t MemKeyCache::DumpInit(ssize_t iDumpMin,
	char *DumpFile/*=cache.dump*/,
	ssize_t iDumpType/*=DUMP_TYPE_NODE*/,
	ssize_t iBinLogOpen/*=0*/,
	char * pBinLogBaseName/*=binlog*/,
	ssize_t iMaxBinLogSize/*=20000000*/,
	ssize_t iMaxBinLogNum/*=20*/,
	ssize_t iBinlogCommitSecs/*=0*/)
{
	m_iDumpMin = iDumpMin;
	strcpy(m_szDumpFile,DumpFile);
	m_iDumpType = iDumpType;
	m_iTotalBinLogSize = iMaxBinLogSize*iMaxBinLogNum;
	m_iBinlogCommitSecs = iBinlogCommitSecs;
	m_tLastRefreshTime = time(0);
	
	m_iBinLogOpen = iBinLogOpen;
	if (m_iBinLogOpen)
	{
		m_stBinLog.Init(pBinLogBaseName,iMaxBinLogSize,iMaxBinLogNum,!m_iBinlogCommitSecs);
	}
	return 0;
}

ssize_t MemKeyCache::StartRecover()
{
	//非新建的共享内存,不用恢复
	if (m_iInitType!=emInit)
	{
		return 0;
	}

	m_pMemCacheHead->m_iDataOK = 0;
	
	//关闭binlog
	ssize_t iOldBinLogOpen = m_iBinLogOpen;
	m_iBinLogOpen = 0;
			
	if (access(m_szDumpFile, F_OK) == 0)
	{
		//恢复core
		ssize_t iRet = _CoreRecover(m_iDumpType);
		if (iRet > 0)
		{
			if (m_iDumpType == DUMP_TYPE_MIRROR)
			{
				printf("Recover from dumpfile(%s) %lld bytes.\n",m_szDumpFile,(long long)iRet);
			}
			else
			{
				printf("Recover from dumpfile(%s) %lld records.\n",m_szDumpFile,(long long)iRet);
			}		
		}
		else
		{
			printf("not recover from dumpfile\n");
		}
	}
	else
	{
		printf("no dumpfile to recover.\n");
	}

	ssize_t iOp = 0;	
	ssize_t iCount = 0;
	ssize_t iLogLen = 0;

	char* pTmpBuffer = new char[BUFFSIZE];
	u_int64_t tLogTime;
	//恢复日志流水,均为脏数据
	m_stBinLog.ReadRecordStart();	
	while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(pTmpBuffer,BUFFSIZE,tLogTime)))
	{
		memcpy(&iOp,pTmpBuffer,sizeof(ssize_t));
		
		char *pBuffer = pTmpBuffer+sizeof(ssize_t);
		ssize_t iBufferLen = iLogLen - sizeof(ssize_t);
		
		if (iOp == op_set)
		{		
			int *piNodeKeyLen,*piNodeReserveLen,*piDataFlag;
			int iNodeDataLen;
			char *pNodeKey,*pNodeReserve,*pNodeData;
			DecodeBufferFormat(pBuffer,iBufferLen,
				piDataFlag,
				piNodeKeyLen,pNodeKey,
				piNodeReserveLen,pNodeReserve,
				iNodeDataLen,pNodeData);
		
			Set(pNodeKey,*piNodeKeyLen,pNodeData,iNodeDataLen,*piDataFlag,pNodeReserve,*piNodeReserveLen);
		}	
		else if  (iOp == op_mark)
		{	
			int* piDataFlag;
			int* piNodeKeyLen;
			char *pNodeKey;
			DecodeBufferKeyFormat(pBuffer,iBufferLen,piDataFlag,piNodeKeyLen,pNodeKey);
			
			MarkFlag(pNodeKey,*piNodeKeyLen,*piDataFlag);
		}	
		else if  (iOp == op_del)
		{	
			int* piDataFlag;
			int* piNodeKeyLen;
			char *pNodeKey;
			DecodeBufferKeyFormat(pBuffer,iBufferLen,piDataFlag,piNodeKeyLen,pNodeKey);
			
			Del(pNodeKey,*piNodeKeyLen);
		}	
		else if  (iOp == op_append)
		{	
			int *piNodeKeyLen,*piNodeReserveLen,*piDataFlag;
			int iNodeDataLen;
			char *pNodeKey,*pNodeReserve,*pNodeData;
			DecodeBufferFormat(pBuffer,iBufferLen,
				piDataFlag,
				piNodeKeyLen,pNodeKey,
				piNodeReserveLen,pNodeReserve,
				iNodeDataLen,pNodeData);
		
			Append(pNodeKey,*piNodeKeyLen,pNodeData,iNodeDataLen,*piDataFlag);
		}			
		else
		{
			printf("bad binlog op %lld.\n",(long long)iOp);
		}
		
		iCount++;
	}

	printf("recover from binlog %lld records.\n",(long long)iCount);
	delete []pTmpBuffer;

	//恢复binlog开关
	m_iBinLogOpen = iOldBinLogOpen;

	m_pMemCacheHead->m_iDataOK = 1;
	return 0;
}

ssize_t MemKeyCache::TimeTick(time_t iNow)
{
	if(iNow == 0)
		iNow = time(0);

	//binlog刷盘
	if(m_iBinLogOpen && 
		(m_iBinlogCommitSecs > 0) &&
		(iNow -m_tLastRefreshTime >= m_iBinlogCommitSecs))
	{
		m_stBinLog.Flush();
		m_tLastRefreshTime = iNow;
	}
	
	//数据量标准
	bool bNeedCoreDump = false;
	if(m_iTotalBinLogSize > 0)
	{
		if(m_stBinLog.GetBinLogTotalSize() >= m_iTotalBinLogSize)
		{
			bNeedCoreDump = true;
		}
	}

	//时间标准
	if (m_iDumpMin > 0)
	{
		if(iNow -m_pMemCacheHead->m_tLastDumpTime >=  (m_iDumpMin*60))
		{
			bNeedCoreDump = true;
		}
	}

	if(bNeedCoreDump)
	{
		CoreDump();
	}	

	return 0;
}

ssize_t MemKeyCache::GetNodeIdx(const char* pKey,const ssize_t KEYSIZE)
{
	if (!pKey || KEYSIZE<=0)
		return -1;

	size_t unHashIdx = MemMakeHashKey((char*)pKey,(u_int32_t)KEYSIZE);
	unHashIdx %= m_pMemCacheHead->m_iBucketNum;
	
	//查找节点
	ssize_t iMinGetLen = 2*sizeof(ssize_t)+KEYSIZE;
	ssize_t iNodeIdx = m_piBucket[unHashIdx];
	while( iNodeIdx >= 0 )
	{
		ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,iMinGetLen);
		if(iAllLen >= iMinGetLen)
		{
			int* piDataFlag;
			int* piNodeKeyLen;
			char *pNodeKey;
			DecodeBufferKeyFormat(m_pBuffer,iAllLen,piDataFlag,piNodeKeyLen,pNodeKey);
			
			if((*piNodeKeyLen == KEYSIZE) && (0==memcmp(pNodeKey,pKey,KEYSIZE)))
			{
				break;
			}
		}
		iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);
	}
		
	return iNodeIdx;
}
ssize_t MemKeyCache::GetBufferSize(ssize_t iNodeIdx)
{
	ssize_t iSize = m_BuffMng.GetBufferSize(iNodeIdx);
	return iSize>=0?iSize:0;
}
ssize_t MemKeyCache::GetFlag(ssize_t iNodeIdx)
{
	if(iNodeIdx < 0)
		return 0;
	
	char szGetBuf[MIN_DATE_LEN];

	ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,szGetBuf,MIN_DATE_LEN);
	if(iAllLen > 0)
	{
		//key
		int* piDataFlag;
		int* piNodeKeyLen;
		char *pNodeKey;
		DecodeBufferKeyFormat(szGetBuf,iAllLen,piDataFlag,piNodeKeyLen,pNodeKey);
		
		return *piDataFlag;
	}
	return 0;
}
ssize_t MemKeyCache::MarkFlag(const char* pKey,const ssize_t KEYSIZE,ssize_t iDataFlag)
{
	ssize_t iNodeIdx = GetNodeIdx(pKey,KEYSIZE);
	if(iNodeIdx < 0)
		return E_NO_DATA;

	ssize_t iOldDataFlag = GetFlag(iNodeIdx);
	
	if(iOldDataFlag == iDataFlag)
		return 0;

	//dirty,del -> clean
	if(iDataFlag == F_DATA_CLEAN)
	{
		m_NodeWLruQueue.DeleteItem(iNodeIdx);
		m_NodeRLruQueue.AppendToTail(iNodeIdx);
	}
	// clean -> dirty,del
	else
	{
		m_NodeRLruQueue.DeleteItem(iNodeIdx);
		m_NodeWLruQueue.AppendToTail(iNodeIdx);
	}	

	//new flag copy in
	int* piDataFlag;
	int* piNodeKeyLen;
	char *pNodeKey;
	DecodeBufferKeyFormat(m_BuffMng.GetFirstBlockPtr(iNodeIdx),m_BuffMng.GetBlockSize(),
					piDataFlag,piNodeKeyLen,pNodeKey);
	*piDataFlag = (int)iDataFlag;
	
	if (m_iBinLogOpen)
	{
		//dirty,del -> clean	
		if(iOldDataFlag != F_DATA_CLEAN)
		{
			*(int*)m_pBuffer = op_mark;
			ssize_t iEncodeLen = EncodeBufferKeyFormat(m_pBuffer+sizeof(int),BUFFSIZE-sizeof(int),
									iDataFlag,KEYSIZE,pKey);

			ASSERT(iEncodeLen > 0);	

			ssize_t iRet = m_stBinLog.WriteToBinLog(m_pBuffer,sizeof(int)+iEncodeLen);
			if(iRet != 0)
				return E_WRITE_BINLOG;
		}
		//clean -> dirty,del
		else
		{
			ssize_t iBuffLen = GetBufferSize(GetNodeIdx(pKey,KEYSIZE));
			if(iBuffLen < 10240)
			{
				char pTmpData[10240];
				char pReserve[10240];
				ssize_t iReserveLen = 0;
				ssize_t iDataFlag;
				ssize_t iDataLen = Get(pKey,KEYSIZE,
								pTmpData,10240,iDataFlag,
								pReserve,10240,iReserveLen);

				ssize_t iRet = _WriteDataLog(op_set,pKey,KEYSIZE,pTmpData,iDataLen,iDataFlag,pReserve,iReserveLen);
				if(iRet != 0)
					return E_WRITE_BINLOG;				
			}
			else
			{
				char *pTmpData = new char[iBuffLen];
				char *pReserve = new char[iBuffLen];
				ssize_t iReserveLen = 0;
				ssize_t iDataFlag;
				ssize_t iDataLen = Get(pKey,KEYSIZE,
								pTmpData,iBuffLen,iDataFlag,
								pReserve,iBuffLen,iReserveLen);

				ssize_t iRet = _WriteDataLog(op_set,pKey,KEYSIZE,pTmpData,iDataLen,iDataFlag,pReserve,iReserveLen);
				delete []pTmpData;
				delete []pReserve;
				
				if(iRet != 0)
					return E_WRITE_BINLOG;			
			}
		}
	}
	return 0;
}

//0 success
ssize_t MemKeyCache::Set(const char* pKey,const ssize_t KEYSIZE,
			char* szData,ssize_t iDataSize,ssize_t iDataFlag/*=F_DATA_DIRTY*/,
			char* pReserve/*=NULL*/,ssize_t iReserveLen/*=0*/)
{
	ssize_t iOldDataFlag = 0;
	ssize_t iRet = _Set(pKey,KEYSIZE,szData,iDataSize,iDataFlag,iOldDataFlag,pReserve,iReserveLen);
	if(iRet != 0)
	{
		return iRet;
	}

	//写binlog
	if (m_iBinLogOpen)
	{
		if((iDataFlag != F_DATA_CLEAN) || (iOldDataFlag != F_DATA_CLEAN))
		{
			iRet = _WriteDataLog(op_set,pKey,KEYSIZE,szData,iDataSize,iDataFlag,pReserve,iReserveLen);	
			if(iRet != 0)
				return E_WRITE_BINLOG;
		}
	}
	
	return 0;
}

//返回数据长度
ssize_t MemKeyCache::Get(const char* pKey,const ssize_t KEYSIZE,
				char* pData,const ssize_t DATASIZE,ssize_t &iDataFlag,
				char* pReserve,const ssize_t RESERVESIZE,ssize_t &iReserveLen)
{
	if (!pKey || !pData || DATASIZE<=0 || KEYSIZE<=0)
		return E_PARAM;

	ssize_t iNodeIdx = GetNodeIdx(pKey,KEYSIZE);
	if( iNodeIdx < 0 )
		return E_NO_DATA;

	ssize_t iRet = GetDataByIdx(iNodeIdx,
							pData,DATASIZE,iDataFlag,
							pReserve,RESERVESIZE,iReserveLen);

	if(iDataFlag == F_DATA_CLEAN)
	{
		m_NodeRLruQueue.DeleteItem(iNodeIdx);
		m_NodeRLruQueue.AppendToTail(iNodeIdx);	
	}
	else
	{
		m_NodeWLruQueue.DeleteItem(iNodeIdx);
		m_NodeWLruQueue.AppendToTail(iNodeIdx);
	}

	return iRet;
}

//返回数据长度
ssize_t MemKeyCache::GetNoLRU(const char* pKey,const ssize_t KEYSIZE,
				char* pData,const ssize_t DATASIZE,ssize_t &iDataFlag,
				char* pReserve,const ssize_t RESERVESIZE,ssize_t &iReserveLen)
{
	if (!pKey || !pData || DATASIZE<=0 || KEYSIZE<=0)
		return E_PARAM;

	ssize_t iNodeIdx = GetNodeIdx(pKey,KEYSIZE);
	if( iNodeIdx < 0 )
		return E_NO_DATA;

	ssize_t iRet = GetDataByIdx(iNodeIdx,
							pData,DATASIZE,iDataFlag,
							pReserve,RESERVESIZE,iReserveLen);

	return iRet;
}

ssize_t MemKeyCache::Del(const char* pKey,const ssize_t KEYSIZE)
{
	ssize_t iOldDataFlag = 0;
	ssize_t iRet = _Del(pKey,KEYSIZE,iOldDataFlag);
	if(iRet != 0)
	{
		return iRet;
	}
	
	if (m_iBinLogOpen)
	{
		if(iOldDataFlag != F_DATA_CLEAN)
		{
			*(int*)m_pBuffer = op_del;
			ssize_t iEncodeLen = EncodeBufferKeyFormat(m_pBuffer+sizeof(int),BUFFSIZE-sizeof(int),
									iOldDataFlag,KEYSIZE,pKey);

			ASSERT(iEncodeLen > 0)		

			ssize_t iRet = m_stBinLog.WriteToBinLog(m_pBuffer,sizeof(int)+iEncodeLen);
			if(iRet != 0)
				return E_WRITE_BINLOG;
		}
	}
	return 0;
}


//datalen是计算出来的，所以可以直接append
//return 0
ssize_t MemKeyCache::Append(const char* pKey,const ssize_t KEYSIZE,char* szData,ssize_t iDataSize,ssize_t iDataFlag)
{
#ifdef _BUFFMNG_APPEND_SKIP

	if (!pKey || !szData)
		return E_PARAM;

	ssize_t iNodeIdx = GetNodeIdx(pKey,KEYSIZE);
	if( iNodeIdx < 0 )
		return E_NO_DATA;

	ssize_t iRet = m_BuffMng.AppendBuffer(iNodeIdx,szData,iDataSize);
	if(iRet != 0)
	{
		return iRet;
	}

	//写binlog
	if (m_iBinLogOpen)
	{
		ssize_t iOldDataFlag = GetFlag(iNodeIdx);
		
		if((iDataFlag != F_DATA_CLEAN) || (iOldDataFlag != F_DATA_CLEAN))
		{
			iRet = _WriteDataLog(op_append,pKey,KEYSIZE,szData,iDataSize,iDataFlag);	
			if(iRet != 0)
				return E_WRITE_BINLOG;
		}
	}
#else
	ASSERT(1);
#endif

	return 0;
}


ssize_t MemKeyCache::GetUsage(ssize_t &iBucketUsed,ssize_t &iBucketNum,
					ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,
					ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount,
					ssize_t &iDirtyNodeCnt)
{
	if(!m_pMemCacheHead)
		return -1;

	iBucketUsed = m_pMemCacheHead->m_iBucketUsed;
	iBucketNum = m_pMemCacheHead->m_iBucketNum;
	iDirtyNodeCnt = m_NodeWLruQueue.GetLength();
	
	//哈希节点和buffmng的节点一一对应的
	return m_BuffMng.GetBufferUsage(iHashNodeUsed,iHashNodeCount,iObjNodeUsed,iObjNodeCount);
}
//返回key长度
ssize_t MemKeyCache::GetKeyByIdx(ssize_t iNodeIdx,char* pKey/*=NULL*/,const ssize_t KEYSIZE/*=0*/)
{
	if(iNodeIdx < 0)
		return -1;
	
	ssize_t iBlockSize = m_BuffMng.GetBlockSize();
	ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,iBlockSize);
	if(iAllLen <= 0)
		return -1;

	//key
	int* piDataFlag;
	int* piNodeKeyLen;
	char *pNodeKey;	
	if(iAllLen == iBlockSize)
	{
		iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,BUFFSIZE);
	}
	
	DecodeBufferKeyFormat(m_pBuffer,iAllLen,piDataFlag,piNodeKeyLen,pNodeKey);

	if(pKey == NULL)
		return *piNodeKeyLen;
	
	if(KEYSIZE < *piNodeKeyLen)
		return E_DATA_BUFF_SMALL;

	memcpy(pKey,pNodeKey,*piNodeKeyLen);
	return *piNodeKeyLen;
}

//return datalen
ssize_t MemKeyCache::GetDataByIdx(ssize_t iNodeIdx,
					char* pData,const ssize_t DATASIZE,ssize_t &iDataFlag,
					char* pReserve/*=NULL*/,const ssize_t RESERVESIZE,ssize_t &iReserveLen)
{
	if(iNodeIdx < 0)
		return E_NO_DATA;
	
	ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,BUFFSIZE);
	if(iAllLen > 0)
	{
		int *piNodeKeyLen,*piNodeReserveLen,*piDataFlag;
		int iNodeDataLen;
		char *pNodeKey,*pNodeReserve,*pNodeData;
		DecodeBufferFormat(m_pBuffer,iAllLen,
			piDataFlag,
			piNodeKeyLen,pNodeKey,
			piNodeReserveLen,pNodeReserve,
			iNodeDataLen,pNodeData);
		
		if(pReserve)
		{
			if(RESERVESIZE < *piNodeReserveLen)
				return E_RESERVE_BUFF_SMALL;
			
			memcpy(pReserve,pNodeReserve,*piNodeReserveLen);
			iReserveLen = *piNodeReserveLen;		
		}
		
		if(DATASIZE < iNodeDataLen)
			return E_DATA_BUFF_SMALL;

		iDataFlag = *piDataFlag;

		if(pData)
			memcpy(pData,pNodeData,iNodeDataLen);
		
		return iNodeDataLen;
	}
	return E_NO_DATA;
}
//返回key长度
ssize_t MemKeyCache::GetOldestKey(char *pKey,const ssize_t KEYSIZE,ssize_t iLRUType)
{
	return GetKeyByIdx(GetOldestNodeIdx(iLRUType),pKey,KEYSIZE);
}
//返回nodeidx
ssize_t MemKeyCache::GetOldestNodeIdx(ssize_t iLRUType)
{
	if(iLRUType == em_r_list)
		return m_NodeRLruQueue.GetHeadItem();
	else
		return m_NodeWLruQueue.GetHeadItem();
}
ssize_t MemKeyCache::GetNextLruNodeIdx(ssize_t iNodeIdx)
{
	//R list ,W list使用相同的指针随便一个GetNextItem结果一样的
	ssize_t iNextItemIdx;
	m_NodeRLruQueue.GetNextItem(iNodeIdx,iNextItemIdx);
	return iNextItemIdx;
}

void MemKeyCache::Print(FILE *fpOut)
{
	if( !fpOut )
		return;
	
	fprintf(fpOut,"\n[MemKeyCache LastDumpTime %lld, DS %lld, BucketNum %lld]\n",
		(long long)m_pMemCacheHead->m_tLastDumpTime,
		(long long)m_pMemCacheHead->m_iDSSuffix,
		(long long)m_pMemCacheHead->m_iBucketNum);

	for(ssize_t iBucketIdx = 0; iBucketIdx < m_pMemCacheHead->m_iBucketNum; iBucketIdx++ )
	{ 
		if(m_piBucket[iBucketIdx] < 0 )
		{
			continue;
		}

		fprintf(fpOut, "BUCKET[%06lld]->", (long long)iBucketIdx);

		ssize_t iNodeIdx = m_piBucket[iBucketIdx];
		while( iNodeIdx >= 0 )
		{
			fprintf(fpOut, "OBJ[%lld]->", (long long)iNodeIdx);
			iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);
		}
		fprintf(fpOut, "NULL\n");
	}
	m_NodeObjMng.Print(fpOut);
	fprintf(fpOut, "\n");fprintf(fpOut, "\n");
	
	fprintf(fpOut, "r_lru_list:\n");m_NodeRLruQueue.Print(fpOut);fprintf(fpOut, "\n");
	fprintf(fpOut, "w_lru_list:\n");m_NodeWLruQueue.Print(fpOut);fprintf(fpOut, "\n");
	m_BuffMng.Print(fpOut);fprintf(fpOut, "\n");
	
	fprintf(fpOut, "\n");
	for (ssize_t iBucketIdx=0; iBucketIdx<m_pMemCacheHead->m_iBucketNum; iBucketIdx++)
	{
		ssize_t iNodeIdx = m_piBucket[iBucketIdx];
		if(iNodeIdx < 0)
			continue;
		
		fprintf(fpOut,"BUCKET[%lld] =>\n",(long long)iBucketIdx);
		while( iNodeIdx >= 0 )
		{
			ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,BUFFSIZE);
			if(iAllLen > 0)
			{
				int *piNodeKeyLen,*piNodeReserveLen,*piDataFlag;
				int iNodeDataLen;
				char *pNodeKey,*pNodeReserve,*pNodeData;
				DecodeBufferFormat(m_pBuffer,iAllLen,
					piDataFlag,
					piNodeKeyLen,pNodeKey,
					piNodeReserveLen,pNodeReserve,
					iNodeDataLen,pNodeData);

				fprintf(fpOut,"FLAG %lld KEY[%lld] ",(long long)*piDataFlag,(long long)*piNodeKeyLen);
				for(ssize_t i=0; i<*piNodeKeyLen; i++)
					fprintf(fpOut,"%02x", (unsigned char)pNodeKey[i]);
	
				fprintf(fpOut," RESERVE[%lld] ",(long long)*piNodeReserveLen);
				for(ssize_t i=0; i<*piNodeReserveLen; i++)
					fprintf(fpOut,"%02x", (unsigned char)pNodeReserve[i]);
				
				fprintf(fpOut," DATA[%lld] ",(long long)iNodeDataLen);
				for(ssize_t i=0; i<iNodeDataLen; i++)
					fprintf(fpOut,"%02x", (unsigned char)pNodeData[i]);
				
				fprintf(fpOut,"\n");
			}

			iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);
		}
	}	
}
void MemKeyCache::PrintBinLog(FILE *fpOut)
{
	if( !fpOut )
		return;

	ssize_t iLogLen,iCount=0;
	char* m_pTmpBuffer = new char[BUFFSIZE];
	u_int64_t tLogTime;
	//恢复日志流水,均为脏数据
	m_stBinLog.ReadRecordStart();	
	while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(m_pTmpBuffer,BUFFSIZE,tLogTime)))
	{
		char szTimeStr[128];
		struct tm curr;
		
		timeval tvLogTime;
		memcpy(&tvLogTime,&tLogTime,sizeof(u_int64_t));
		
		time_t tNow = tvLogTime.tv_sec;
		localtime_r(&tNow,&curr);

		if (curr.tm_year > 50)
		{
			sprintf(szTimeStr, "%04d-%02d-%02d %02d:%02d:%02d.%d", 
				curr.tm_year+1900, curr.tm_mon+1, curr.tm_mday,
				curr.tm_hour, curr.tm_min, curr.tm_sec,(int) tvLogTime.tv_usec);
		}
		else
		{
			sprintf(szTimeStr, "%04d-%02d-%02d %02d:%02d:%02d.%d",
		        curr.tm_year+2000, curr.tm_mon+1, curr.tm_mday,
		        curr.tm_hour, curr.tm_min, curr.tm_sec,(int) tvLogTime.tv_usec);
		}
	
		ssize_t iOp = 0;
		memcpy(&iOp,m_pTmpBuffer,sizeof(ssize_t));
		
		char *pCurrBuffer = m_pTmpBuffer+sizeof(ssize_t);
		ssize_t iAllLen = iLogLen - sizeof(ssize_t);

		fprintf(fpOut,"[%s] ",szTimeStr);
		
		if (iOp == op_set)
		{		
			int *piNodeKeyLen,*piNodeReserveLen,*piDataFlag;
			int iNodeDataLen;
			char *pNodeKey,*pNodeReserve,*pNodeData;
			DecodeBufferFormat(pCurrBuffer,iAllLen,
				piDataFlag,
				piNodeKeyLen,pNodeKey,
				piNodeReserveLen,pNodeReserve,
				iNodeDataLen,pNodeData);
				
			fprintf(fpOut,"Set  FLAG %lld KEY[%04lld] ",(long long)*piDataFlag,(long long)*piNodeKeyLen);
			for(ssize_t i=0; i<*piNodeKeyLen; i++)
				fprintf(fpOut,"%02x", (unsigned char)pNodeKey[i]);
			
			fprintf(fpOut," RESERVE[%04lld] ",(long long)*piNodeReserveLen);
			for(ssize_t i=0; i<*piNodeReserveLen; i++)
				fprintf(fpOut,"%02x", (unsigned char)pNodeReserve[i]);
			
			fprintf(fpOut," DATA[%04lld] ",(long long)iNodeDataLen);
			for(ssize_t i=0; i<iNodeDataLen; i++)
				fprintf(fpOut,"%02x", (unsigned char)pNodeData[i]);
			
			fprintf(fpOut,"\n");
		}
		else if  (iOp == op_mark)
		{	
			int* piDataFlag;
			int* piNodeKeyLen;
			char *pNodeKey;
			DecodeBufferKeyFormat(pCurrBuffer,iAllLen,piDataFlag,piNodeKeyLen,pNodeKey);
			
			fprintf(fpOut,"Mark FLAG %lld KEY[%04lld] ",(long long)*piDataFlag,(long long)*piNodeKeyLen);
			for(ssize_t i=0; i<*piNodeKeyLen; i++)
				fprintf(fpOut,"%02x", (unsigned char)pNodeKey[i]);

			fprintf(fpOut,"\n");
		}		
		else if  (iOp == op_del)
		{	
			int* piDataFlag;
			int* piNodeKeyLen;
			char *pNodeKey;
			DecodeBufferKeyFormat(pCurrBuffer,iAllLen,piDataFlag,piNodeKeyLen,pNodeKey);
			
			fprintf(fpOut,"Del  FLAG %lld KEY[%04lld] ",(long long)*piDataFlag,(long long)*piNodeKeyLen);
			for(ssize_t i=0; i<*piNodeKeyLen; i++)
				fprintf(fpOut,"%02x", (unsigned char)pNodeKey[i]);

			fprintf(fpOut,"\n");
		}
		else
		{
			printf("bad binlog op %lld.\n",(long long)iOp);
		}
		
		iCount++;
	}

	printf("recover from binlog %lld records.\n",(long long)iCount);
	delete []m_pTmpBuffer;
}
//返回dump的字节数
ssize_t MemKeyCache::CoreDumpMem(char* pBuffer,const ssize_t BUFFSIZE,DUMP_MATCH_FUNC fDumpMatchFunc,void* pArg)
{
	ssize_t iBufferPos = 0;
	char* pTmpBuffer = new char[BUFFSIZE];
	
	for (ssize_t iBucketIdx=0; iBucketIdx<m_pMemCacheHead->m_iBucketNum; iBucketIdx++)
	{
		ssize_t iNodeIdx = m_piBucket[iBucketIdx];
		while( iNodeIdx >= 0 )
		{
			ssize_t iKeyLen = GetKeyByIdx(iNodeIdx,pTmpBuffer,BUFFSIZE);
			ssize_t iDataFlag = GetFlag(iNodeIdx);
			char *pKey = pTmpBuffer;
			
			//符合条件的
			if(fDumpMatchFunc(pKey,iKeyLen,iDataFlag,pArg) == 0)
			{
				ssize_t iBufferLen = m_BuffMng.GetBuffer(iNodeIdx,pTmpBuffer,BUFFSIZE);
				
				//存入总长度
				memcpy(pBuffer+iBufferPos,&iBufferLen,sizeof(ssize_t));
				iBufferPos += sizeof(ssize_t);

				//存入数据
				memcpy(pBuffer+iBufferPos,pTmpBuffer,iBufferLen);
				iBufferPos += iBufferLen;
			}	
			iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);
		}
	}

	delete []pTmpBuffer;
	return iBufferPos;
}

//返回恢复的记录数
ssize_t MemKeyCache::CoreRecoverMem(char* pBuffer,ssize_t iBufferSize)
{
	ssize_t iRecordLen=0;
	ssize_t iCount = 0;
	ssize_t iBufferPos = 0;
	
	while(iBufferPos < iBufferSize)
	{
		memcpy(&iRecordLen,pBuffer+iBufferPos,sizeof(ssize_t));
		iBufferPos += sizeof(ssize_t);

		int *piNodeKeyLen,*piNodeReserveLen,*piDataFlag;
		int iNodeDataLen;
		char *pNodeKey,*pNodeReserve,*pNodeData;
		DecodeBufferFormat(pBuffer+iBufferPos,iRecordLen,
			piDataFlag,
			piNodeKeyLen,pNodeKey,
			piNodeReserveLen,pNodeReserve,
			iNodeDataLen,pNodeData);
		
		Set(pNodeKey,*piNodeKeyLen,pNodeData,iNodeDataLen,*piDataFlag,pNodeReserve,*piNodeReserveLen);
		
		iBufferPos += iRecordLen;
		iCount++;
	}
	return iCount;
}

//返回清除的记录数,写log
ssize_t MemKeyCache::CleanNode(DUMP_MATCH_FUNC fMatchFunc,void* pArg)
{
	ssize_t iCnt = 0;
	for (ssize_t iBucketIdx=0; iBucketIdx<m_pMemCacheHead->m_iBucketNum; iBucketIdx++)
	{
		ssize_t iNodeIdx = m_piBucket[iBucketIdx];
		while( iNodeIdx >= 0 )
		{
			ssize_t iKeyLen = GetKeyByIdx(iNodeIdx,m_pBuffer,BUFFSIZE);
			ssize_t iDataFlag = GetFlag(iNodeIdx);
			char *pKey = m_pBuffer;

			//先指向下一个，然后删除
			iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);
			
			//符合条件的
			if(fMatchFunc(pKey,iKeyLen,iDataFlag,pArg) == 0)
			{
				Del(pKey,iKeyLen);
				iCnt++;
			}
		}
	}

	return iCnt;
}

ssize_t MemKeyCache::CoreDump()
{
	if (0 <= _CoreDump(m_iDumpType))
	{			
		//delete binlog
		m_stBinLog.ClearAllBinLog();
		m_pMemCacheHead->m_tLastDumpTime = time(0);
		return 0;
	}	
	return -1;
}

ssize_t MemKeyCache::_CoreDump(ssize_t iType)
{	
	FILE *pFile =fopen(m_szDumpFile,"w+");
	if (!pFile)
	{
		return -1;
	}

	if (iType == DUMP_TYPE_MIRROR)
	{
		fwrite(m_pMemPtr,1,m_iMemSize,pFile);
		ssize_t iLen = ftell(pFile);
		fclose(pFile);	
		return iLen;
	}

	ssize_t iCnt = 0;
	for (ssize_t iBucketIdx=0; iBucketIdx<m_pMemCacheHead->m_iBucketNum; iBucketIdx++)
	{
		ssize_t iNodeIdx = m_piBucket[iBucketIdx];
		while(iNodeIdx>=0)
		{
			ssize_t iDataFlag = GetFlag(iNodeIdx);
			
			if ((iType == DUMP_TYPE_DIRTY_NODE)&&(iDataFlag == F_DATA_CLEAN))
			{
				//指向下一个
				iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);	
				continue;
			}		

			ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,BUFFSIZE);
			if(iAllLen > 0)
			{
				fwrite(&iAllLen,1,sizeof(ssize_t),pFile);
				fwrite(m_pBuffer,1,iAllLen,pFile);
				iCnt++;
			}

			iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);	
		}
	}

	fclose(pFile);	
	return iCnt;
}

ssize_t MemKeyCache::_CoreRecover(ssize_t iType)
{	
	FILE *pFile =fopen(m_szDumpFile,"rb+");
	if (!pFile)
	{
		return -1;
	}

	if (iType == DUMP_TYPE_MIRROR)
	{ 
		ssize_t iReadLen = fread(m_pMemPtr,1,m_iMemSize,pFile); 
		fclose(pFile);	

		if (iReadLen != m_iMemSize)
		{
			printf("ERR:%s just %lld bytes read!\n",m_szDumpFile,(long long)iReadLen);
			return -2;
		}
		return iReadLen;
	}

	ASSERT(m_iBinLogOpen == 0);
	
	char* pTmpBuffer = new char[BUFFSIZE];
	
	ssize_t iReadLen = 0;
	ssize_t iCount = 0;
	while(!feof(pFile))
	{
		ssize_t iRecordLen = 0;
		if(sizeof(ssize_t) != fread(&iRecordLen,1,sizeof(ssize_t),pFile))
		{
			break;
		}
		if (iRecordLen > BUFFSIZE)
		{
			printf("ERR:%s may be crashed!RecordLen=%lld[%s:%d]\n",
						m_szDumpFile,(long long)iRecordLen,__FILE__,__LINE__);
			fclose(pFile);
			delete []pTmpBuffer;
			return -1;
		}

		iReadLen = fread(pTmpBuffer,1,iRecordLen,pFile);
		if (iReadLen != iRecordLen)
		{
			printf("ERR:_CoreRecover Data is missing![%s:%d]\n",__FILE__,__LINE__);
			break;
		}
		
		int *piNodeKeyLen,*piNodeReserveLen,*piDataFlag;
		int iNodeDataLen;
		char *pNodeKey,*pNodeReserve,*pNodeData;
		DecodeBufferFormat(pTmpBuffer,iRecordLen,
			piDataFlag,
			piNodeKeyLen,pNodeKey,
			piNodeReserveLen,pNodeReserve,
			iNodeDataLen,pNodeData);
		
		Set(pNodeKey,*piNodeKeyLen,pNodeData,iNodeDataLen,*piDataFlag,pNodeReserve,*piNodeReserveLen);
		
		iCount++;
	}

	fclose(pFile);

	delete []pTmpBuffer;
	return iCount;
}

//return 0
ssize_t MemKeyCache::_Set(const char* pKey,const ssize_t KEYSIZE,
				char* szData,ssize_t iDataSize,ssize_t iDataFlag,ssize_t &iOldDataFlag,
				char* pReserve,ssize_t iReserveLen)
{
	if (!pKey || !szData || KEYSIZE<=0)
		return -1;

	ssize_t iEncodeBytes = EncodeSize(KEYSIZE,iReserveLen,iDataSize);
	
	if(!m_BuffMng.HaveFreeSpace(iEncodeBytes))
		return E_NO_SPACE;
		
	iOldDataFlag = 0;
	ssize_t iNodeIdx = GetNodeIdx(pKey,KEYSIZE);
	if( iNodeIdx < 0 )
	{
		size_t unHashIdx = MemMakeHashKey((char*)pKey,(u_int32_t)KEYSIZE);
		unHashIdx %= m_pMemCacheHead->m_iBucketNum;
	
		ssize_t iNewNodeIdx = m_NodeObjMng.CreateObject();
		if( iNewNodeIdx < 0 )
		{
			printf("ERR:%s NodeObjMng Create Object failed![%s:%d]\n",__FUNCTION__,__FILE__,__LINE__);
			return E_NO_SPACE;
		}
		m_NodeObjMng.SetDsIdx(iNewNodeIdx,m_pMemCacheHead->m_iDSSuffix,m_piBucket[unHashIdx]);	

		if(m_piBucket[unHashIdx] == -1)
			m_pMemCacheHead->m_iBucketUsed++;
		
		m_piBucket[unHashIdx] = iNewNodeIdx;
		
		iNodeIdx = iNewNodeIdx;
	}
	else
	{
		iOldDataFlag = GetFlag(iNodeIdx);
	}
	
	if(iOldDataFlag != F_DATA_CLEAN)
	{
		m_NodeWLruQueue.DeleteItem(iNodeIdx);
	}
	else
	{
		m_NodeRLruQueue.DeleteItem(iNodeIdx);
	}

	if(iDataFlag != F_DATA_CLEAN)
	{
		m_NodeWLruQueue.AppendToTail(iNodeIdx);
	}
	else
	{
		m_NodeRLruQueue.AppendToTail(iNodeIdx);
	}

	EncodeBufferFormat(m_pBuffer,BUFFSIZE,iDataFlag,KEYSIZE,pKey,iReserveLen,pReserve,iDataSize,szData);

	/*
	char *pTmp = m_pBuffer;
	memcpy(pTmp,(char*)&iDataFlag,sizeof(iDataFlag));
	pTmp+=sizeof(iDataFlag);
	memcpy(pTmp,(char*)&KEYSIZE,sizeof(KEYSIZE));
	pTmp+=sizeof(KEYSIZE);
	memcpy(pTmp,(char*)pKey,KEYSIZE);
	pTmp+=KEYSIZE;
	memcpy(pTmp,(char*)&iReserveLen,sizeof(iReserveLen));
	pTmp+=sizeof(iReserveLen);
	memcpy(pTmp,(char*)pReserve,iReserveLen);
	pTmp+=iReserveLen;

	memcpy(pTmp,(char*)szData,iDataSize);
	pTmp+=iDataSize;

	*/
	
	return m_BuffMng.SetBuffer(iNodeIdx,m_pBuffer,iEncodeBytes);
}

//0 success
ssize_t MemKeyCache::_Del(const char* pKey,const ssize_t KEYSIZE,ssize_t &iOldDataFlag)
{
	if (!pKey || KEYSIZE<=0)
		return E_PARAM;

	iOldDataFlag = 0;
	size_t unHashIdx = MemMakeHashKey((char*)pKey, (u_int32_t)KEYSIZE);
	unHashIdx %= m_pMemCacheHead->m_iBucketNum;
	
	//查找节点
	ssize_t iPrevSuffix = -1;
	ssize_t iMinGetLen = 2*sizeof(ssize_t)+KEYSIZE;
	ssize_t iNodeIdx = m_piBucket[unHashIdx];
	while( iNodeIdx >= 0 )
	{
		ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,iMinGetLen);
		if(iAllLen >= iMinGetLen)
		{
			int* piDataFlag;
			int* piNodeKeyLen;
			char *pNodeKey;
			DecodeBufferKeyFormat(m_pBuffer,iAllLen,piDataFlag,piNodeKeyLen,pNodeKey);
			
			//compare key
			if((*piNodeKeyLen == KEYSIZE) && (0==memcmp(pNodeKey,pKey,KEYSIZE)))
			{
				iOldDataFlag = *piDataFlag;
				break;
			}
		}
		iPrevSuffix = iNodeIdx;
		iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);
	}

	if( iNodeIdx < 0 )
		return E_NO_DATA;

	if(iOldDataFlag != F_DATA_CLEAN)
	{
		m_NodeWLruQueue.DeleteItem(iNodeIdx);
	}
	else
	{
		m_NodeRLruQueue.DeleteItem(iNodeIdx);
	}

	//释放空间
	m_BuffMng.FreeBuffer(iNodeIdx);

	//变更使用链
	ssize_t iNextIdx =  m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);
	if (iPrevSuffix >= 0)
		m_NodeObjMng.SetDsIdx(iPrevSuffix,m_pMemCacheHead->m_iDSSuffix,iNextIdx);

	m_NodeObjMng.SetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix,-1);

	if( iNodeIdx == m_piBucket[unHashIdx])
	{
		m_piBucket[unHashIdx] = iNextIdx;
		
		if(m_piBucket[unHashIdx] == -1)
			m_pMemCacheHead->m_iBucketUsed--;
	}	

	m_NodeObjMng.DestroyObject(iNodeIdx);
	return 0;
}

//数据拍照
ssize_t MemKeyCache::_WriteDataLog(ssize_t iOp,const char* pKey,const ssize_t KEYSIZE,char* pData,ssize_t iDataSize,ssize_t iDataFlag,
			char* pReserve/*=NULL*/,ssize_t iReserveLen/*=0*/)
{
	*(ssize_t*)m_pBuffer = iOp;
	ssize_t iMkBufLen = EncodeBufferFormat(m_pBuffer+sizeof(ssize_t),BUFFSIZE-sizeof(ssize_t),
									iDataFlag,
									KEYSIZE,pKey,
									iReserveLen,pReserve,
									iDataSize,pData);
	if(iMkBufLen < 0)
		return iMkBufLen;

	return m_stBinLog.WriteToBinLog(m_pBuffer,sizeof(ssize_t)+iMkBufLen);	
}

#if 1
#include <sys/time.h>

int main(int argc, char* argv[])
{

	if(argc < 2)
	{
		printf("%s: cachelen\n",argv[0]);
		return 0;
	}

	int MEMSIZE = atoi(argv[1]);
	char *pMem = new char[MEMSIZE];
	memset(pMem,0,MEMSIZE);

	MemKeyCache *pMemCache = NULL;
	pMemCache = new MemKeyCache;
	ssize_t len = pMemCache->AttachMem(pMem, MEMSIZE,0,emInit,13);

	char dd[10240];
	memset(dd,'c',sizeof(dd));
	
	int sss = time(0);
	srand(sss);
	
	timeval t1,t2;
	gettimeofday(&t1,NULL);
	int addinfo = 999;
	int cnt = 0;
	while(1)
	{
		int u_key = rand();
		int ikeylen = rand()%4;  if(ikeylen <= 0)ikeylen = 1;
		int datalen = rand()%1024; if(datalen <= 0)datalen = 1;
		
		int addinfo = rand();
		int addinfolen = rand()%4;	 if(addinfolen <= 0)addinfolen = 1;

		int dataflag = rand()%1;
		int ret = pMemCache->Set((const char*)&u_key, (const ssize_t)ikeylen, 
			(char*)dd, datalen, dataflag, (char*)&addinfo, addinfolen);

		if(ret)
		{
			delete pMemCache;
			pMemCache = new MemKeyCache;
			
			int bsize = rand()%50;  if(bsize <= 0)bsize = 1;
			
			pMemCache->AttachMem(pMem, MEMSIZE,0,emInit,bsize);
			
			printf("Set %d ReAttachMem bsize %d,cnt=%d\n",ret,bsize,cnt);

			cnt = 0;
			continue;
			
			//break;
		}
		

		char dd2[10240];
		memset(dd2,0,10240);

		int dataflag2 = 0;
		int addinfo2;
		int addinfolen2;
		
		ret = pMemCache->Get((const char*)&u_key, (const ssize_t)ikeylen,
							 (char*)dd2,10240,dataflag2,(char*)&addinfo2,4,addinfolen2);

		if(dataflag != dataflag2)
		{
			printf("err dataflag cnt %d\n",cnt);
			return -1;
		}
		
		if(addinfolen2 != addinfolen)
		{
			printf("err addinfolen cnt %d\n",cnt);;
			return -1;
		}

		if(0!=memcmp(&addinfo,&addinfo2,addinfolen))
		{
			printf("err addinfo cnt %d\n",cnt);
			return -1;
		}	
		
		if(datalen != ret)
		{
			printf("err datalen cnt %d\n",cnt);
			return -1;
		}
		
		if(0!=memcmp(dd,dd2,datalen))
		{
			printf("err memcmp cnt %d\n",cnt);
			return -1;
		}	
		
		
		cnt++;
	}

	printf("success cnt %d\n",cnt);
	return 0;

/*
	
	gettimeofday(&t2,NULL);
	int span = (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/(float)1000;
	printf("cost %d ms, %d record, datalen %d\n",span,cnt,datalen);

	long long secnum = (long long)(cnt/(float)span) * (long long)1000;
	printf("secnum %lld\n",secnum);
	
	//---------------
	srand(sss);
	gettimeofday(&t1,NULL);
	int cnt2 = cnt;
	unsigned dlen;
	ssize_t fflag;
	const ssize_t  size = 1024;
	while(1)
	{	
		int u_key = rand();
		int ret = stMemCache.Get((const char*)&u_key, (const ssize_t)sizeof(unsigned),
							 (char*)dd, size,fflag);
		if(cnt2 <= 0)
			break;

		cnt2--;
	}
	gettimeofday(&t2,NULL);
	span = (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/(float)1000;
	printf("get cost %d ms, %d record, datalen %d\n",span,cnt,datalen);

	secnum = (long long)(cnt/(float)span) * (long long)1000;
	printf("get secnum %lld\n",secnum);
	
	return 0;
	*/
#if 1
printf("-----------\n");
	#define MEMSIZE   10000000
	char *pMem = new char[MEMSIZE];

	MemKeyCache stMemCache;
	
	int len = stMemCache.AttachMem(pMem, MEMSIZE,1000,emInit,6);
	printf("len =%d\n",len);

	ssize_t iBucketUsed,iBucketNum,iHashNodeUsed, iHashNodeCount, iObjNodeUsed,  iObjNodeCount,  iDirtyNodeCnt;
	stMemCache.GetUsage( iBucketUsed,iBucketNum,iHashNodeUsed,  iHashNodeCount, iObjNodeUsed,  iObjNodeCount,  iDirtyNodeCnt);
	printf("iHashNodeUsed %lld\n",(long long)iHashNodeUsed);
	printf("iHashNodeCount %lld\n",(long long)iHashNodeCount);
	printf("iObjNodeUsed %lld\n",(long long)iObjNodeUsed);
	printf("iObjNodeCount %lld\n",(long long)iObjNodeCount);
	printf("iDirtyNodeCnt %lld\n",(long long)iDirtyNodeCnt);
	


	//stMemCache.PrintBinLog(stdout);

	


	#define DATALEN	20550
	
	char key[DATALEN];

	
	char key2[8];
	strcpy(key2,"01234567");

	char* szData = new char[DATALEN];
	char* szData2 = new char[DATALEN];
	char* szData3 = new char[DATALEN];
	
	memset(szData,'c',DATALEN);
	ssize_t ret=0;

	stMemCache.DumpInit();
	stMemCache.StartRecover();
	stMemCache.PrintBinLog(stdout);

	len = 100;
	key2[0] = 'a';
	stMemCache.Set((char*)key2,8,szData,len,MemKeyCache::F_DATA_CLEAN,"rrrrrrrr",0);

	key2[0] = 'b';
	stMemCache.Set((char*)key2,8,szData,len,MemKeyCache::F_DATA_CLEAN,"rrrrrrrr",0);

	key2[0] = 'c';
	stMemCache.Set((char*)key2,8,szData,len,1,"rrrrrrrr",0);


	key2[0] = 'd';
	stMemCache.Set((char*)key2,8,szData,len,1,"rrrrrrrr",0);
	
	//key2[0] = 'y';
	//stMemCache.Del((char*)key2,8);
	
	//stMemCache.Print(stdout);
	//return 0;
	timeval ttt,ttt2;
		
	

	for(ssize_t len=1;len<=DATALEN;len++)
	{
		ssize_t kkk = 1;
		ret = stMemCache.Set((char*)&kkk,4,szData,len,MemKeyCache::F_DATA_CLEAN,szData,512);
		if(ret)
		{
			printf("set ret==%d",ret);
			return 0;
		}
		if(len%100==0)
			printf("set len =%d\n",len);
	}
	gettimeofday(&ttt,NULL);
	for(ssize_t len=1;len<=DATALEN;len++)
	{
		ssize_t kkk = 1;
		ssize_t iii;
		ssize_t iDataFlag;
		ret = stMemCache.Get((char*)&kkk,4,szData,DATALEN,iDataFlag,szData2,512,iii);
		if(ret<=0)
		{
			printf("get ret=%d",ret);
			return 0;
		}
		//if//(len%100==0)
			//printf("set len =%d\n",len);
	}
		gettimeofday(&ttt2,NULL);


	ssize_t iMillSecsSpan = (ttt2.tv_sec-ttt.tv_sec)*1000000+(ttt2.tv_usec-ttt.tv_usec);
	printf("time %d us ,avg %d us\n",iMillSecsSpan,iMillSecsSpan/1000000);

	

	stMemCache.Print(stdout);
	return 0;
	
	strcpy(key,"123456788");
	stMemCache.Del(key,9);
	strcpy(key,"0123456787");

	//stMemCache.CoreDump();

	return 0;
	#endif
}

#endif

