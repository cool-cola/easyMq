#include "MemKeyCache.hpp"

#include <assert.h>
#ifndef ASSERT
#define ASSERT(COND){\
	assert(#COND);\
}
#endif
/*
memcache每条记录消耗:51字节

其中哈希节点sizeof(item) 34字节。
cas 8字节
suffix 8字节（用户flag和数据长度）
1字节空洞,原因不详。

*/

/*
总消耗内存 = node*28 + obj_num * obj_size

其中每个块可用空间=obj_size-18-key_size

------------------------------------------
每用户索引耗费:
Bucket  4字节(桶入口)
Node   4x3字节(一个用来串node，2个用来lru)
BuffObj 4x2 (1个标记起点obj,一个标记数据长度)
Obj	4x1  (一个链拼块)

一共28字节。

------------------------------------
每用户内容耗费 18字节
flag,keylen,key,reserveLen,reserve,data
2   ,   2     ,      ,       2     ,    4x3,
*/
extern u_int32_t SuperFastHash(char *data, u_int32_t len);

#define MemMakeHashKey(pKey,iLen)	SuperFastHash((char*)pKey,(u_int32_t)iLen)

enum
{
	op_set = 1,
	op_mark,
	op_del,
	op_append
};

#define GUESS_DATA_LEN		256
#define MIN_DATA_LEN		(sizeof(int16_t)+sizeof(int16_t)+sizeof(int16_t))

/*
标准编码格式
flag,keylen,key,reserveLen,reserve,data
*/
#define DecodeBufferKeyFormat(pBuffer,BUFFLEN) \
	int16_t *piDataFlag = (int16_t*)(pBuffer);\
	int16_t *piNodeKeyLen = (int16_t*)((pBuffer)+sizeof(int16_t));\
	char *pNodeKey = (pBuffer)+sizeof(int16_t)+sizeof(int16_t);\
	piDataFlag=piDataFlag;piNodeKeyLen=piNodeKeyLen;pNodeKey=pNodeKey;	/*just for warning: unused variable*/


//int val = ({int ret=999;ret;});
#define EncodeBufferKeyFormat(pBuffer,BUFFLEN,iDataFlag,iKeyLen,pKey) \
({	\
	ssize_t iMakeBuffLen = 2*sizeof(int16_t)+(iKeyLen);\
	if((ssize_t)(BUFFLEN) < iMakeBuffLen)\
		iMakeBuffLen = -1;\
	else\
	{\
		*(int16_t*)(pBuffer) = (iDataFlag);\
		*(int16_t*)((pBuffer)+sizeof(int16_t)) = (iKeyLen);\
		memmove((pBuffer)+2*sizeof(int16_t),(pKey),(iKeyLen));\
	}\
iMakeBuffLen;})

/*
标准编码格式
flag,keylen,key,reserveLen,reserve,data
*/
#define KeyEnough(pBuffer,BUFFLEN) \
({	\
	bool bDataEnough=true;\
	if((size_t)(BUFFLEN) <= 2*sizeof(int16_t))\
		bDataEnough = false;\
	else\
	{\
		do{\
			int16_t *piKeyLen = (int16_t*)((pBuffer)+sizeof(int16_t));\
			if(((size_t)BUFFLEN) < sizeof(int16_t)+sizeof(int16_t)+*piKeyLen)\
			{\
				bDataEnough = false;\
				break;\
			}\
		}while(0);\
	}\
bDataEnough;})

/*
标准编码格式
flag,keylen,key,reserveLen,reserve,data
*/
#define KeyReserveEnough(pBuffer,BUFFLEN) \
({	\
	bool bDataEnough=true;\
	if((size_t)(BUFFLEN) < MIN_DATA_LEN)\
		bDataEnough = false;\
	else\
	{\
		do{\
			int16_t *piKeyLen = (int16_t*)((pBuffer)+sizeof(int16_t));\
			if((size_t)(BUFFLEN) < sizeof(int16_t)+sizeof(int16_t)+*piKeyLen+sizeof(int16_t))\
			{\
				bDataEnough = false;\
				break;\
			}\
			int16_t *piReserveLen = (int16_t*)((pBuffer)+2*sizeof(int16_t)+*(piKeyLen));\
			if((size_t)(BUFFLEN) < sizeof(int16_t)+sizeof(int16_t)+*piKeyLen+sizeof(int16_t)+*piReserveLen)\
			{\
				bDataEnough = false;\
				break;\
			}\
		}while(0);\
	}\
bDataEnough;})

#define DecodeBufferFormat(pBuffer,BUFFLEN) \
	int16_t *piDataFlag = (int16_t*)(pBuffer);\
	int16_t *piNodeKeyLen = (int16_t*)((pBuffer)+sizeof(int16_t));\
	char *pNodeKey = (pBuffer)+sizeof(int16_t)+sizeof(int16_t);\
	int16_t *piNodeReserveLen = (int16_t*)((pBuffer)+2*sizeof(int16_t)+*(piNodeKeyLen));\
	char *pNodeReserve = (pBuffer)+2*sizeof(int16_t)+*(piNodeKeyLen)+sizeof(int16_t);\
	char *pNodeData = (pBuffer)+2*sizeof(int16_t)+*(piNodeKeyLen)+sizeof(int16_t)+*(piNodeReserveLen);\
	ssize_t iNodeDataLen = (BUFFLEN) - (2*sizeof(int16_t)+*(piNodeKeyLen)+sizeof(int16_t)+*(piNodeReserveLen));\
	\
	piDataFlag=piDataFlag;piNodeKeyLen=piNodeKeyLen;pNodeKey=pNodeKey;\
	piNodeReserveLen = piNodeReserveLen;pNodeReserve = pNodeReserve;\
	pNodeData=pNodeData;iNodeDataLen=iNodeDataLen;	/*just for warning: unused variable*/

#define EncodeBufferFormat(pBuffer,BUFFLEN,iDataFlag,iKeyLen,pKey,iReserveLen,pReserve,iDataLen,pData) \
({	\
	ssize_t iMakeBuffLen = 2*sizeof(int16_t)+(iKeyLen)+sizeof(int16_t)+(iReserveLen)+(iDataLen);\
	if((ssize_t)(BUFFLEN) < iMakeBuffLen)\
		iMakeBuffLen = -1;\
	else\
	{\
		*(int16_t*)(pBuffer) = (iDataFlag);\
		*(int16_t*)((pBuffer)+sizeof(int16_t)) = (iKeyLen);\
		memmove((pBuffer)+2*sizeof(int16_t),(pKey),(iKeyLen));\
		*(int16_t*)((pBuffer)+2*sizeof(int16_t)+(iKeyLen)) = (iReserveLen);\
		memmove((pBuffer)+2*sizeof(int16_t)+(iKeyLen)+sizeof(int16_t),(pReserve),(iReserveLen));\
		memmove((pBuffer)+2*sizeof(int16_t)+(iKeyLen)+sizeof(int16_t)+(iReserveLen),(pData),(iDataLen));\
	}\
iMakeBuffLen;})
/*
标准编码格式
flag,keylen,key,reserveLen,reserve,data
*/
#define EncodeSize(iKeyLen,iReserveLen,iDataLen) \
({	\
	ssize_t iEncodeBytes = sizeof(int16_t)+sizeof(int16_t)+iKeyLen+sizeof(int16_t)+iReserveLen+iDataLen;\
iEncodeBytes;})

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

//至少需要的空间
ssize_t MemKeyCache::CountBaseSize(ssize_t iBucketNum,ssize_t iNodeNum,ssize_t iBlockSize/*=512*/)
{
	ssize_t iSize = sizeof(TMemCacheHead)+sizeof(idx_t)*iBucketNum +
		TIdxObjMng::CountMemSize(0,iNodeNum,3) +
		CObjQueue::CountMemSize()+CObjQueue::CountMemSize();

	iSize+=(CBuffMng::CountMemSize(iNodeNum)+TIdxObjMng::CountMemSize(iBlockSize,iNodeNum,1));

	return iSize;
}
//自动计算NodeNum, bucket:node 按最大的1:1  ,fBlockDivNode block:node数量比值
ssize_t MemKeyCache::CalcNodeNum(const ssize_t MEMSIZE,ssize_t iBlockSize,double fBlockDivNode/*=1.1*/)
{
	ssize_t iLeftSize = MEMSIZE - sizeof(TMemCacheHead)
					- sizeof(TIdxObjMng::TIdxObjHead)
					- sizeof(CObjQueue::TObjQueueHead)
					- sizeof(CObjQueue::TObjQueueHead)
					- sizeof(CBuffMng::TBuffMngHead)
					- sizeof(TIdxObjMng::TIdxObjHead);

	/*
	记录长度正太分布在平均记录两边，所以大于平均记录的占50%
	block数量为node数量1.1倍的方程式为:
	bucket:node按最大的1:1计算

	iLeftSize = X*sizeof(idx_t)								//bucket
			+X*(TIdxObjMng::GetIdxSize(3)+ 0)
			+0
			+sizeof(CBuffMng::TBuffItem)*X
			+1.1*X*(TIdxObjMng::GetIdxSize(1)+ iBlockSize);
	*/
	double X = iLeftSize/(double)(sizeof(idx_t)
					+(TIdxObjMng::GetIdxSize(3)+ 0)
					+sizeof(CBuffMng::TBuffItem)
					+fBlockDivNode*(double)(TIdxObjMng::GetIdxSize(1)+ iBlockSize));

	ssize_t iNodeNum = (ssize_t)X;
	//printf("Auto Calc NodeNum = %lld\n",(long long)iNodeNum);
	return iNodeNum;
}

//自动计算NodeNum, bucket:node 按最大的1:1  ,fBlockDivNode block:node数量比值
ssize_t MemKeyCache::CalcObjNum(const ssize_t MEMSIZE,ssize_t iNodeNum,ssize_t iBlockSize)
{
	ssize_t iAllocBytes = 0;
	ssize_t iBucketNum = iNodeNum;

	//-------TMemCacheHead 头部分配---------------------------
	iAllocBytes = sizeof(TMemCacheHead);

	//-------Bucket 分配---------------------------
	iAllocBytes += iBucketNum*sizeof(idx_t);

	//-------Node 分配--3个下标链:1个bucket主链,2个给rwlru链
	iAllocBytes += TIdxObjMng::CountMemSize(0,iNodeNum,3);

	//RLRU 需要2条链
	iAllocBytes += CObjQueue::CountMemSize();

	//WLRU 需要2条链
	iAllocBytes += CObjQueue::CountMemSize();

	//-------Block 分配---------------------------
	iAllocBytes += CBuffMng::CountMemSize(iNodeNum);

	//剩下的分给数据块, 需要1条链
	return TIdxObjMng::CountIdxObjNum(MEMSIZE-iAllocBytes,iBlockSize,1);
}

//绑定内存
ssize_t MemKeyCache::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,
		ssize_t	 iBucketNum,ssize_t iNodeNum,ssize_t iInitType,ssize_t iBlockSize)
{
	if (!pMemPtr || (MEMSIZE<=0) ||(iNodeNum<=0) || (iBucketNum<=0) || iBlockSize<=0)
		return -1;

	ASSERT(iBlockSize >= MIN_DATA_LEN);

	if(iBucketNum > iNodeNum)
		iBucketNum = iNodeNum;

	if (MEMSIZE < CountBaseSize(iBucketNum,iNodeNum,iBlockSize))
		return -2;

	ssize_t iAttachBytes=0,iAllocBytes = 0;

	//-------TMemCacheHead 头部分配---------------------------
	m_pMemCacheHead = (TMemCacheHead*)pMemPtr;
	iAllocBytes = sizeof(TMemCacheHead);

	if (iInitType==emRecover && !m_pMemCacheHead->m_iDataOK )
		iInitType = emInit;

	m_iInitType = iInitType;

	//-------Bucket 分配---------------------------
	m_piBucket = (idx_t*)(pMemPtr+iAllocBytes);
	iAllocBytes += iBucketNum*sizeof(idx_t);

	//-------Node 分配--3个下标链:1个bucket主链,2个给rwlru链
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
		memset(m_piBucket,-1,iBucketNum*sizeof(idx_t));
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

//绑定内存,NodeNum=0 表示自动计算
ssize_t MemKeyCache::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,
		ssize_t	 iNodeNum/*=0*/,ssize_t iInitType/*=emInit*/,ssize_t iBlockSize/*=512*/)
{
	if(iNodeNum == 0)
	{
		iNodeNum = CalcNodeNum(MEMSIZE,iBlockSize);
	}	

	ssize_t iBucketNum = iNodeNum;
	return AttachMem(pMemPtr,MEMSIZE,iBucketNum,iNodeNum,iInitType,iBlockSize);
}

ssize_t MemKeyCache::DumpInit(ssize_t iDumpMin,
	char *DumpFile/*=cache.dump*/,
	ssize_t iDumpType/*=DUMP_TYPE_DIRTY_NODE*/,
	ssize_t iBinLogOpen/*=1*/,
	char * pBinLogBaseName/*=binlog*/,
	ssize_t iMaxBinLogSize/*=20000000*/,
	ssize_t iMaxBinLogNum/*=20*/,
	ssize_t iBinlogCommitSecs/*=2*/)
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
		m_stBinLog.Init(pBinLogBaseName,iMaxBinLogSize,!m_iBinlogCommitSecs);
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

	char cOp = 0;
	ssize_t iCount = 0;
	ssize_t iLogLen = 0;

	char* pTmpBuffer = new char[BUFFSIZE];
	u_int64_t tLogTime;
	//恢复日志流水,均为脏数据
	m_stBinLog.ReadRecordStart();
	while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(pTmpBuffer,BUFFSIZE,tLogTime)))
	{
		memcpy(&cOp,pTmpBuffer,sizeof(cOp));

		char *pBuffer = pTmpBuffer+sizeof(cOp);
		ssize_t iBufferLen = iLogLen - sizeof(cOp);

		if (cOp == op_set)
		{
			DecodeBufferFormat(pBuffer,iBufferLen);
			Set(pNodeKey,*piNodeKeyLen,pNodeData,iNodeDataLen,*piDataFlag,pNodeReserve,*piNodeReserveLen);
		}
		else if  (cOp == op_mark)
		{
			DecodeBufferKeyFormat(pBuffer,iBufferLen);
			MarkFlag(pNodeKey,*piNodeKeyLen,*piDataFlag);
		}
		else if  (cOp == op_del)
		{
			DecodeBufferKeyFormat(pBuffer,iBufferLen);
			Del(pNodeKey,*piNodeKeyLen);
		}
		else if  (cOp == op_append)
		{
			DecodeBufferFormat(pBuffer,iBufferLen);
			Append(pNodeKey,*piNodeKeyLen,pNodeData,iNodeDataLen,*piDataFlag);
		}
		else
		{
			printf("bad binlog op %lld.\n",(long long)cOp);
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

bool MemKeyCache::IsNeedDump(time_t iNow/*=0*/)
{
	if(iNow == 0)
		iNow = time(0);

	//数据量标准
	bool bNeedCoreDump = false;
	if(m_iTotalBinLogSize > 0)
	{
		if((ssize_t)m_stBinLog.GetBinLogTotalSize() >= m_iTotalBinLogSize)
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
	return bNeedCoreDump;
}
ssize_t MemKeyCache::TimeTick(time_t iNow/*=0*/)
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

	return 0;
}

ssize_t MemKeyCache::GetNodeIdx(const char* pKey,const ssize_t KEYSIZE)
{
	if (!pKey || KEYSIZE<=0 || KEYSIZE>=MAX_KEY_LEN)
		return -1;

	size_t unHashIdx = MemMakeHashKey((char*)pKey,(u_int32_t)KEYSIZE);
	unHashIdx %= m_pMemCacheHead->m_iBucketNum;


	//查找节点
	char szTmpBuffer[MIN_DATA_LEN+MAX_KEY_LEN];
	ssize_t iMinGetLen = MIN_DATA_LEN+KEYSIZE;
	ssize_t iNodeIdx = m_piBucket[unHashIdx];
	while( iNodeIdx >= 0 )
	{
		ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,szTmpBuffer,iMinGetLen);
		if(iAllLen >= iMinGetLen)
		{
			DecodeBufferKeyFormat(szTmpBuffer,iAllLen);
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
ssize_t MemKeyCache::GetDataSize(const char* pKey,const ssize_t KEYSIZE)
{
	ssize_t iNodeIdx = GetNodeIdx(pKey, KEYSIZE);
	if (iNodeIdx <0)
		return E_NO_DATA;

	char szTmpBuffer[MIN_DATA_LEN+MAX_KEY_LEN];
	ssize_t iMinGetLen = MIN_DATA_LEN+KEYSIZE;
	ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,szTmpBuffer,iMinGetLen);
	if(iAllLen < iMinGetLen)
		return E_NO_DATA;

	iAllLen = m_BuffMng.GetBufferSize(iNodeIdx);

	DecodeBufferFormat(szTmpBuffer,iAllLen);
	return iNodeDataLen;
}
ssize_t MemKeyCache::GetFlag(ssize_t iNodeIdx)
{
	if(iNodeIdx < 0)
		return 0;

	char szGetBuf[MIN_DATA_LEN];

	ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,szGetBuf,MIN_DATA_LEN);
	if(iAllLen == MIN_DATA_LEN)
	{
		//key
		DecodeBufferKeyFormat(szGetBuf,iAllLen);
		return (ssize_t)*piDataFlag;
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
	DecodeBufferKeyFormat(m_BuffMng.GetFirstBlockPtr(iNodeIdx),m_BuffMng.GetBlockSize());
	*piDataFlag = iDataFlag;


	if (m_iBinLogOpen)
	{
		//dirty,del -> clean
		if(iOldDataFlag != F_DATA_CLEAN)
		{
			char szTmpBuffer[MIN_DATA_LEN + MAX_KEY_LEN];
			*(char*)szTmpBuffer = op_mark;
			ssize_t iEncodeLen = EncodeBufferKeyFormat(szTmpBuffer+sizeof(char),MIN_DATA_LEN + MAX_KEY_LEN-sizeof(char),
									iDataFlag,KEYSIZE,pKey);

			ASSERT(iEncodeLen > 0);

			ssize_t iRet = m_stBinLog.WriteToBinLog(szTmpBuffer,sizeof(char)+iEncodeLen);
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
	if (!pKey || !pData || DATASIZE<=0 || KEYSIZE<=0 || KEYSIZE>=MAX_KEY_LEN)
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
	if (!pKey || !pData || DATASIZE<=0 || KEYSIZE<=0 || KEYSIZE>=MAX_KEY_LEN)
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
			char szTmpBuffer[MIN_DATA_LEN+MAX_KEY_LEN];
			*(char*)szTmpBuffer = op_del;
			ssize_t iEncodeLen = EncodeBufferKeyFormat(szTmpBuffer+sizeof(char),MIN_DATA_LEN+MAX_KEY_LEN-sizeof(char),
									iOldDataFlag,KEYSIZE,pKey);

			ASSERT(iEncodeLen > 0)

			ssize_t iRet = m_stBinLog.WriteToBinLog(szTmpBuffer,sizeof(char)+iEncodeLen);
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

void MemKeyCache::IgnoreInvalidIdx(ssize_t iNodeIdx)
{
	ssize_t iOldDataFlag = GetFlag(iNodeIdx);
	if(iOldDataFlag != F_DATA_CLEAN)
	{
		m_NodeWLruQueue.DeleteItem(iNodeIdx);
		m_NodeWLruQueue.AppendToTail(iNodeIdx);
	}
	else
	{
		m_NodeRLruQueue.DeleteItem(iNodeIdx);
		m_NodeRLruQueue.AppendToTail(iNodeIdx);
	}
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

ssize_t MemKeyCache::GetUsage(ssize_t &iBucketUsed,ssize_t &iBucketNum,
					ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,
					ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount,
					ssize_t &iDirtyNodeCnt, ssize_t &iCleanNodeCnt)
{
	if(!m_pMemCacheHead)
		return -1;

	iBucketUsed = m_pMemCacheHead->m_iBucketUsed;
	iBucketNum = m_pMemCacheHead->m_iBucketNum;
	iDirtyNodeCnt = m_NodeWLruQueue.GetLength();
	iCleanNodeCnt = m_NodeRLruQueue.GetLength();

	//哈希节点和buffmng的节点一一对应的
	return m_BuffMng.GetBufferUsage(iHashNodeUsed,iHashNodeCount,iObjNodeUsed,iObjNodeCount);

}
/*
return keylen
pKey=NULL 不拷贝key,只要长度
*/
ssize_t MemKeyCache::GetKeyByIdx(ssize_t iNodeIdx,char* pKey/*=NULL*/,const ssize_t KEYSIZE/*=0*/)
{
	if(iNodeIdx < 0)
		return -1;

	char szTmpBuffer[MIN_DATA_LEN+MAX_KEY_LEN];
	ssize_t iGetSize = m_BuffMng.GetBlockSize()>GUESS_DATA_LEN?GUESS_DATA_LEN:m_BuffMng.GetBlockSize();
	ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,szTmpBuffer,iGetSize);
	if(iAllLen <= 0)
		return E_NO_DATA;

	//数据是否足够
	bool bKeyEnough = KeyEnough(szTmpBuffer,iAllLen);
	if(!bKeyEnough)
	{
		iAllLen = m_BuffMng.GetBuffer(iNodeIdx,szTmpBuffer,MIN_DATA_LEN+MAX_KEY_LEN);
	}

	bKeyEnough = KeyEnough(szTmpBuffer,iAllLen);
	if(!bKeyEnough)
		return E_DATA_CRASH;

	//key
	DecodeBufferKeyFormat(szTmpBuffer,iAllLen);
	ssize_t iKeyLen = *piNodeKeyLen;

	if(pKey == NULL)
		return iKeyLen;

	if(KEYSIZE < iKeyLen)
		return E_DATA_BUFF_SMALL;

	memmove(pKey,pNodeKey,iKeyLen);
	return iKeyLen;
}

/*
return datalen
pData=NULL 不拷贝data
pReserve=NULL 不拷贝reserve
*/
ssize_t MemKeyCache::GetDataByIdx(ssize_t iNodeIdx,
					char* pData/*=NULL*/,const ssize_t DATASIZE/*=0*/,ssize_t &iDataFlag,
					char* pReserve/*=NULL*/,const ssize_t RESERVESIZE/*=0*/,ssize_t &iReserveLen)
{
	if(iNodeIdx < 0)
		return E_NO_DATA;

	ssize_t iAllLen = 0;
	if(pData == NULL)
	{
		//guess 一个长度256
		ssize_t iGetSize = m_BuffMng.GetBlockSize()>GUESS_DATA_LEN?GUESS_DATA_LEN:m_BuffMng.GetBlockSize();
		iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,iGetSize);
		if(iAllLen <= 0)
			return E_NO_DATA;

		//数据是否足够
		bool bKeyReserveEnough = KeyReserveEnough(m_pBuffer,iAllLen);
		if(!bKeyReserveEnough)
		{
			iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,BUFFSIZE);
		}
	}
	else
	{
		iAllLen = m_BuffMng.GetBuffer(iNodeIdx,m_pBuffer,BUFFSIZE);
		if(iAllLen <= 0)
			return E_NO_DATA;
	}

	//数据是否足够
	bool bKeyReserveEnough = KeyReserveEnough(m_pBuffer,iAllLen);
	if(!bKeyReserveEnough)
		return E_DATA_CRASH;

	ssize_t iBuffLen = m_BuffMng.GetBufferSize(iNodeIdx);

	DecodeBufferFormat(m_pBuffer,iBuffLen);
	if(pReserve)
	{
		if(RESERVESIZE < *piNodeReserveLen)
			return E_RESERVE_BUFF_SMALL;

		memcpy(pReserve,pNodeReserve,*piNodeReserveLen);
		iReserveLen = *piNodeReserveLen;
	}

	iDataFlag = *piDataFlag;

	if(pData)
	{
		if(DATASIZE < iNodeDataLen)
			return E_DATA_BUFF_SMALL;
		memcpy(pData,pNodeData,iNodeDataLen);
	}

	return iNodeDataLen;
}

//返回nodeidx
ssize_t MemKeyCache::GetOldestNodeIdx(ssize_t iLRUType/*=em_r_list*/)
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
				DecodeBufferFormat(m_pBuffer,iAllLen);

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

		char cOp = 0;
		memcpy(&cOp,m_pTmpBuffer,sizeof(cOp));

		char *pCurrBuffer = m_pTmpBuffer+sizeof(cOp);
		ssize_t iAllLen = iLogLen - sizeof(cOp);

		fprintf(fpOut,"[%s] ",szTimeStr);

		if (cOp == op_set)
		{
			DecodeBufferFormat(pCurrBuffer,iAllLen);

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
		else if  (cOp == op_mark)
		{
			DecodeBufferKeyFormat(pCurrBuffer,iAllLen);

			fprintf(fpOut,"Mark FLAG %lld KEY[%04lld] ",(long long)*piDataFlag,(long long)*piNodeKeyLen);
			for(ssize_t i=0; i<*piNodeKeyLen; i++)
				fprintf(fpOut,"%02x", (unsigned char)pNodeKey[i]);

			fprintf(fpOut,"\n");
		}
		else if  (cOp == op_del)
		{
			DecodeBufferKeyFormat(pCurrBuffer,iAllLen);

			fprintf(fpOut,"Del  FLAG %lld KEY[%04lld] ",(long long)*piDataFlag,(long long)*piNodeKeyLen);
			for(ssize_t i=0; i<*piNodeKeyLen; i++)
				fprintf(fpOut,"%02x", (unsigned char)pNodeKey[i]);

			fprintf(fpOut,"\n");
		}
		else
		{
			printf("bad binlog op %lld.\n",(long long)cOp);
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

		DecodeBufferFormat(pBuffer+iBufferPos,iRecordLen);
		Set(pNodeKey,*piNodeKeyLen,pNodeData,iNodeDataLen,*piDataFlag,pNodeReserve,*piNodeReserveLen);

		iBufferPos += iRecordLen;
		iCount++;
	}
	return iCount;
}

//返回清除的记录数,写log
ssize_t MemKeyCache::CleanNode(DUMP_MATCH_FUNC fMatchFunc,void* pArg)
{
	char szKey[MAX_KEY_LEN];

	ssize_t iCnt = 0;
	for (ssize_t iBucketIdx=0; iBucketIdx<m_pMemCacheHead->m_iBucketNum; iBucketIdx++)
	{
		ssize_t iNodeIdx = m_piBucket[iBucketIdx];
		while( iNodeIdx >= 0 )
		{
			ssize_t iKeyLen = GetKeyByIdx(iNodeIdx,szKey,MAX_KEY_LEN);
			ssize_t iDataFlag = GetFlag(iNodeIdx);

			//先指向下一个，然后删除
			iNodeIdx = m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);

			//符合条件的
			if(fMatchFunc(szKey,iKeyLen,iDataFlag,pArg) == 0)
			{
				Del(szKey,iKeyLen);
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

		DecodeBufferFormat(pTmpBuffer,iRecordLen);
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
	if (!pKey || !szData || KEYSIZE<=0 || KEYSIZE>=MAX_KEY_LEN)
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

	return m_BuffMng.SetBuffer(iNodeIdx,m_pBuffer,iEncodeBytes);
}

//0 success
ssize_t MemKeyCache::_Del(const char* pKey,const ssize_t KEYSIZE,ssize_t &iOldDataFlag)
{
	if (!pKey || KEYSIZE<=0 || KEYSIZE>=MAX_KEY_LEN)
		return E_PARAM;

	iOldDataFlag = 0;
	size_t unHashIdx = MemMakeHashKey((char*)pKey, (u_int32_t)KEYSIZE);
	unHashIdx %= m_pMemCacheHead->m_iBucketNum;

	//查找节点
	char szTmpBuffer[MIN_DATA_LEN+MAX_KEY_LEN];
	ssize_t iMinGetLen = MIN_DATA_LEN+KEYSIZE;
	ssize_t iPrevSuffix = -1;
	ssize_t iNodeIdx = m_piBucket[unHashIdx];
	while( iNodeIdx >= 0 )
	{
		ssize_t iAllLen = m_BuffMng.GetBuffer(iNodeIdx,szTmpBuffer,iMinGetLen);
		if(iAllLen >= iMinGetLen)
		{
			DecodeBufferKeyFormat(szTmpBuffer,iAllLen);

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
	*(char*)m_pBuffer = iOp;
	ssize_t iMkBufLen = EncodeBufferFormat(m_pBuffer+sizeof(char),BUFFSIZE-sizeof(char),
									iDataFlag,
									KEYSIZE,pKey,
									iReserveLen,pReserve,
									iDataSize,pData);
	if(iMkBufLen < 0)
		return iMkBufLen;

	return m_stBinLog.WriteToBinLog(m_pBuffer,sizeof(char)+iMkBufLen);
}

#if 0
ssize_t dumpfunc(void* pKey,ssize_t iKeyLen,ssize_t iFlag,void* pArg)
{
	int ikey = *(int*)pKey;
	int modres = *(int*)pArg;
	if(ikey %10000 == modres)
		return 0;

	return -1;
}

#include <sys/time.h>

int main(int argc, char* argv[])
{
	int MEMSIZE = 1*1024*1024*1024;
	char *pMem = new char[MEMSIZE];
	memset(pMem,0,MEMSIZE);

	MemKeyCache *pMemCache = NULL;
	pMemCache = new MemKeyCache;

	ssize_t len = pMemCache->AttachMem(pMem, MEMSIZE,0,emInit,1024);
	//printf("attache len %d, count len %d\n",len,(int)MemKeyCache::CountBaseSize(100000,13));

	char dd[10240];
	memset(dd,'c',sizeof(dd));

	int sss = time(0);
	srand(sss);

	//pMemCache->DumpInit();

	//pMemCache->StartRecover();
	//pMemCache->Print(stdout);

	timeval t1,t2;

	int addinfo = 999;
	int cnt = 0;
	while(1)
	{
		int u_key = rand();
		int ikeylen = rand()%4;  if(ikeylen <= 0)ikeylen = 1;
		int datalen = rand()%10; if(datalen <= 0)datalen = 1;

		int addinfo = rand();
		int addinfolen = rand()%4;	 if(addinfolen <= 0)addinfolen = 1;

		int dataflag = 1;//rand()%1;
		int ret = pMemCache->Set((const char*)&u_key, (const ssize_t)ikeylen,
			(char*)dd, datalen, dataflag, (char*)&addinfo, addinfolen);

		cnt++;

		if(cnt == 3)
			break;

		if(ret)
		{
			break;
		}
	}
pMemCache->Print(stdout);

	printf("set cnt=%d\n",cnt);

	ssize_t iBucketUsed,iBucketNum,iHashNodeUsed, iHashNodeCount, iObjNodeUsed,  iObjNodeCount,  iDirtyNodeCnt;
	pMemCache->GetUsage( iBucketUsed,iBucketNum,iHashNodeUsed,  iHashNodeCount, iObjNodeUsed,  iObjNodeCount,  iDirtyNodeCnt);

	                    printf("iBucketUsed %lld\n",(long long)iBucketUsed);
						                                    printf("iBucketCount %lld\n",(long long)iBucketNum);
	printf("iHashNodeUsed %lld\n",(long long)iHashNodeUsed);
	printf("iHashNodeCount %lld\n",(long long)iHashNodeCount);
	printf("iObjNodeUsed %lld\n",(long long)iObjNodeUsed);
	printf("iObjNodeCount %lld\n",(long long)iObjNodeCount);
	printf("iDirtyNodeCnt %lld\n",(long long)iDirtyNodeCnt);

	gettimeofday(&t1,NULL);

	int modres = 8;
	pMemCache->CleanNode(dumpfunc,(void *)&modres);

	gettimeofday(&t2,NULL);

	int cost = (t2.tv_sec-t1.tv_sec)*1000000 + (t2.tv_usec-t1.tv_usec);
	printf("clean cost %d us\n",cost);

pMemCache->Print(stdout);

	    pMemCache->GetUsage( iBucketUsed,iBucketNum,iHashNodeUsed,  iHashNodeCount, iObjNodeUsed,  iObjNodeCount,  iDirtyNodeCnt);

		            printf("iBucketUsed %lld\n",(long long)iBucketUsed);
					                printf("iBucketCount %lld\n",(long long)iBucketNum);

		    printf("iHashNodeUsed %lld\n",(long long)iHashNodeUsed);
			    printf("iHashNodeCount %lld\n",(long long)iHashNodeCount);
				    printf("iObjNodeUsed %lld\n",(long long)iObjNodeUsed);
					    printf("iObjNodeCount %lld\n",(long long)iObjNodeCount);
						    printf("iDirtyNodeCnt %lld\n",(long long)iDirtyNodeCnt);

}

#endif

#if 0
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

	ssize_t len = pMemCache->AttachMem(pMem, MEMSIZE,0,emInit,1024);
	//printf("attache len %d, count len %d\n",len,(int)MemKeyCache::CountBaseSize(100000,13));
	// pMemCache->Print(stdout);
/*
	ssize_t iBucketUsed,iBucketNum,iHashNodeUsed,iHashNodeCount,iObjNodeUsed,iObjNodeCount,iDirtyNodeCnt;
	pMemCache->GetUsage(iBucketUsed,iBucketNum,
					iHashNodeUsed,iHashNodeCount,
					iObjNodeUsed,iObjNodeCount,
					iDirtyNodeCnt);
	
	int iNodeNum = iHashNodeCount;
	int size = MemKeyCache::CalcObjNum(MEMSIZE,iNodeNum,1024);
	printf("objnum %d, objnum = %d\n",(int)iObjNodeCount,size);
	return 0;
	*/

	char dd[10240];
	memset(dd,'c',sizeof(dd));

	int sss = time(0);
	srand(sss);

	//pMemCache->DumpInit();

	//pMemCache->StartRecover();
	//pMemCache->Print(stdout);

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

		int dataflag = 1;//rand()%1;
		int ret = pMemCache->Set((const char*)&u_key, (const ssize_t)ikeylen,
			(char*)dd, datalen, dataflag, (char*)&addinfo, addinfolen);

		//pMemCache->Print(stdout);
		//return 0;

		//int ret = pMemCache->Set((const char*)dd, 1024,
			//(char*)dd, datalen, dataflag, (char*)&addinfo, addinfolen);

		if(ret)
		{
			delete pMemCache;
			pMemCache = new MemKeyCache;

			int bsize = rand()%1024;  if(bsize <= 0)bsize = 1;

			len = pMemCache->AttachMem(pMem, MEMSIZE,0,emInit,bsize);

			printf("Set %d ReAttachMem bsize %d,cnt=%d ,",ret,bsize,cnt);
			printf("attache len %d, count len %d\n",len,(int)MemKeyCache::CountBaseSize(0,bsize));

			cnt = 0;
			continue;

			//break;
		}
/*
		// mid test
		//ssize_t iNodeIdx = pMemCache->GetNodeIdx((const char*)&u_key, (const ssize_t)ikeylen);
		ssize_t iNodeIdx = pMemCache->GetNodeIdx((const char*)dd, (const ssize_t)1024);

		char szTkey[1024];
		int iTkeyLen = pMemCache->GetKeyByIdx(iNodeIdx,NULL,0);
		//if(iTkeyLen != ikeylen)
			//return -1;

		//if(0 != memcmp(szTkey,&u_key,ikeylen))
		//	return -1;

		char szTData[1024];
		ssize_t iTDataFlag;
		char szTRes[1024];
		ssize_t iTReslen;
		int iTDataLen = pMemCache->GetDataByIdx(iNodeIdx,NULL,0,iTDataFlag,szTRes,1024,iTReslen);
		if(iTDataLen != datalen)
			return -1;

		//if(0 != memcmp(szTData,dd,datalen))
			//return -1;

		// mid test end
*/
	/*
		char dd2[10240];
		memset(dd2,0,10240);

		int dataflag2 = 0;
		int addinfo2;
		int addinfolen2;

		ret = pMemCache->Get((const char*)&u_key, (const ssize_t)ikeylen,
							 (char*)dd2,10240,dataflag2,(char*)&addinfo2,4,addinfolen2);

		if(dataflag != dataflag2)
		{
			printf("err dataflag cnt %d!=%d\n",dataflag,dataflag2);
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

		*/
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
#if 0
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

