#include <assert.h>

#include "NodeCache.hpp"

enum
{
	op_append = 1,
	op_insert,
	op_del_block,
	op_del,
	op_trim_tail,
	op_set,	
	op_set_node
};

//设置THashNode的主键
ssize_t SetBucketNodeKey(void* pObj,void* pKey,ssize_t iKeyLen)
{
	NodeCache::THashNode* pHashNode = (NodeCache::THashNode*)pObj;
	if (iKeyLen < (ssize_t)sizeof(pHashNode->m_szKey))
	{
		return -1;
	}
	memset(pHashNode,0,sizeof(NodeCache::THashNode));
	memcpy(pHashNode->m_szKey,pKey,sizeof(pHashNode->m_szKey));
	return 0;
}

//获取TBucketNode的主键
ssize_t GetBucketNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen)
{
	NodeCache::THashNode* pHashNode = (NodeCache::THashNode*)pObj;
	memcpy(pKey,pHashNode->m_szKey,sizeof(pHashNode->m_szKey));
	iKeyLen = sizeof(pHashNode->m_szKey);
	return 0;
}

ssize_t NodeCache::CountBaseSize(ssize_t iHashNum,ssize_t iBlockSize/*=512*/)
{
	ssize_t iSize = sizeof(TNodeCacheHead)+CHashTab::CountMemSize((ssize_t)(iHashNum/(float)2)) +
		TIdxObjMng::CountMemSize(sizeof(THashNode),iHashNum,3) +
		CObjQueue::CountMemSize();
	
	if(iBlockSize > 0)
	{
		iSize+=(CBlockMng::CountMemSize(iHashNum)+TIdxObjMng::CountMemSize(iBlockSize,iHashNum,1));
	}
	return iSize;	
}

NodeCache::NodeCache()
{
	m_pNodeCacheHead = NULL;
	m_iDumpMin = 0;
	memset(m_szDumpFile,0,sizeof(m_szDumpFile));
	m_iInitType = 0;

	m_iBinLogOpen = 0;
	m_iTotalBinLogSize = 0;
	m_pBuffer = new char[BUFFSIZE];
}

NodeCache::~NodeCache()
{
	delete []m_pBuffer;
}

//绑定内存
ssize_t NodeCache::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iHashNum,ssize_t iInitType/*=emInit*/,ssize_t iBlockSize/*=512*/)
{
	if (!pMemPtr || (MEMSIZE<=0) ||(iHashNum<=0))
	{
		return -1;
	}

	if (MEMSIZE < CountBaseSize(iHashNum,iBlockSize))
	{
		return -2;
	}
 
	ssize_t iAttachBytes=0,iAllocBytes = 0;

	//头部
	m_pNodeCacheHead = (TNodeCacheHead*)pMemPtr;
	iAllocBytes = sizeof(TNodeCacheHead);

	m_iInitType = iInitType;
	if (iInitType == emInit)
	{
		m_pNodeCacheHead->m_tLastDumpTime = 0;
	}

	//索引部分-----------------------------------
	iAttachBytes = m_stBucketHashTab.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,(ssize_t)(iHashNum/(float)2),iInitType);
	if (iAttachBytes < 0)
		return -3;
	iAllocBytes += iAttachBytes;

	//hash需要1条链
	iAttachBytes = m_stHashObjMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,sizeof(THashNode),iHashNum,iInitType,3);
	if (iAttachBytes < 0)
		return -4;
	iAllocBytes += iAttachBytes;
	
	if(m_stBucketHashTab.AttachIdxObjMng(&m_stHashObjMng,SetBucketNodeKey,GetBucketNodeKey))
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
	if(iBlockSize > 0)
	{
		//m_stBlockMng 需要1条链
		iAttachBytes = m_stBlockMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iHashNum,iInitType);
		if (iAttachBytes < 0)
			return -8;
		iAllocBytes += iAttachBytes;

		//剩下的分给数据块
		iAttachBytes = m_stBlockObjMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iBlockSize,0,iInitType,1);
		if (iAttachBytes < 0)
			return -9;
		iAllocBytes += iAttachBytes;
		
		if(m_stBlockMng.AttachIdxObjMng(&m_stBlockObjMng))
		{
			return -7;
		}	
	}

	m_pMemPtr = pMemPtr;
	m_iMemSize = iAllocBytes;
	return iAllocBytes;
}

ssize_t NodeCache::StartUp()
{
	//非新建的共享内存,不用恢复
	if (m_iInitType!=emInit)
	{
		return 0;
	}

	if (access(m_szDumpFile, F_OK) == 0)
	{
		//恢复core
		ssize_t iRet = _CoreRecover(m_iDumpType);
		if (iRet < 0)
		{
			printf("Recover from dumpfile(%s) failed, ret %lld.\n",m_szDumpFile,(long long)iRet);
			return iRet;
		}
		else if (m_iDumpType == DUMP_TYPE_MIRROR)
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
		printf("no dumpfile to recover.\n");
	}

	//恢复日志流水
	m_stBinLog.ReadRecordStart();

	ssize_t iOp = 0;	
	char szHashKey[HASH_KEY_LEN];
	ssize_t iDataLen = 0;
	char* pData = NULL;
	ssize_t iCount = 0;
	ssize_t iLogLen = 0;
	
	time_t tLogTime;
	while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(m_pBuffer,BUFFSIZE,tLogTime)))
	{
		ssize_t iBuffPos = 0;
		
		memcpy(&iOp,m_pBuffer,sizeof(ssize_t));
		iBuffPos += sizeof(ssize_t);

		if  (iOp == op_del)
		{
			memcpy(szHashKey,m_pBuffer+iBuffPos,HASH_KEY_LEN);
			iBuffPos += HASH_KEY_LEN;
		
			_Del(szHashKey);			
		}
		else if  (iOp == op_append)
		{
			memcpy(szHashKey,m_pBuffer+iBuffPos,HASH_KEY_LEN);
			iBuffPos += HASH_KEY_LEN;
			
			pData = m_pBuffer+iBuffPos;
			_AppendBlock(szHashKey,pData);
		}	
		else if  (iOp == op_insert)
		{
			memcpy(szHashKey,m_pBuffer+iBuffPos,HASH_KEY_LEN);
			iBuffPos += HASH_KEY_LEN;
			
			ssize_t iPos = 0;
			memcpy(&iPos,m_pBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);
			
			pData = m_pBuffer+iBuffPos;
			_InsertBlock(szHashKey,pData,iPos);
		}		
		else if  (iOp == op_del_block)
		{
			memcpy(szHashKey,m_pBuffer+iBuffPos,HASH_KEY_LEN);
			iBuffPos += HASH_KEY_LEN;
			
			ssize_t iPos = 0;
			memcpy(&iPos,m_pBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);
			
			_DeleteBlock(szHashKey,iPos);
		}	
		else if  (iOp == op_trim_tail)
		{
			memcpy(szHashKey,m_pBuffer+iBuffPos,HASH_KEY_LEN);
			iBuffPos += HASH_KEY_LEN;
			
			ssize_t iBlockNum = 0;
			memcpy(&iBlockNum,m_pBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);
			
			_TrimTail(szHashKey,iBlockNum);
		}	
		else if  (iOp == op_set)
		{
			THashNode stHashNode;
			memcpy(&stHashNode,m_pBuffer+iBuffPos,sizeof(THashNode));
			iBuffPos += sizeof(THashNode);
		
			memcpy(&iDataLen,m_pBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);
			
			pData = m_pBuffer+iBuffPos;
			
			_Set(&stHashNode,pData,iDataLen);
		}		
		else if (iOp == op_set_node)
		{
			THashNode stHashNode;
			memcpy(&stHashNode,m_pBuffer+iBuffPos,sizeof(THashNode));
			iBuffPos += sizeof(THashNode);

			THashNode* pHashNode = _GetHashNode(stHashNode.m_szKey);
			if (!pHashNode)
			{
				ssize_t iObjIdx = -1;
				pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(stHashNode.m_szKey,HASH_KEY_LEN,iObjIdx);	
				if (!pHashNode)
				{
					printf("[%s]CreateObjectByKey failed!\n",__FUNCTION__);
					continue;
				}		
			}
			memcpy(pHashNode,&stHashNode,sizeof(THashNode));
		}
		
		iCount++;
	}

	printf("recover from binlog %lld records.\n",(long long)iCount);
	return 0;
}

ssize_t NodeCache::DumpInit(ssize_t iDumpMin,
					char *DumpFile,
					ssize_t iDumpType,
					ssize_t iBinLogOpen/*=0*/,
					char * pBinLogBaseName/*=NULL*/,
					ssize_t iMaxBinLogSize/*=20000000*/, 
					ssize_t iMaxBinLogNum/*=20*/,
					ssize_t iBinlogCommitSecs/*=0*/)
{
	m_iDumpMin = iDumpMin;
	strcpy(m_szDumpFile,DumpFile);
	m_iDumpType  = iDumpType;
	
	m_iBinLogOpen = iBinLogOpen;
	m_iTotalBinLogSize = iMaxBinLogSize*iMaxBinLogNum;
	m_iBinlogCommitSecs = iBinlogCommitSecs;
	m_tLastRefreshTime = time(0);
	
	if (m_iBinLogOpen)
	{
		m_stBinLog.Init(pBinLogBaseName,iMaxBinLogSize,iMaxBinLogNum,!m_iBinlogCommitSecs);
	}
	return 0;
}

ssize_t NodeCache::TimeTick(time_t iNow)
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
		time_t tNow = iNow;
		if (!tNow)	
			tNow = time(NULL);
		
		if(tNow -m_pNodeCacheHead->m_tLastDumpTime >=  (m_iDumpMin*60))
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

ssize_t NodeCache::GetNodeFlg(char szKey[HASH_KEY_LEN])
{
#ifdef _HASH_NODE_DIRTY
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pHashNode)	
		return 0;

	return pHashNode->m_iFlag;

#endif
	return 0;
}

//当一个节点从干净变脏的时候,需要记录全量数据
ssize_t NodeCache::SetNodeFlg(char szKey[HASH_KEY_LEN],ssize_t iFlag)
{
#ifdef _HASH_NODE_DIRTY
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pHashNode)	
	{
		pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
		if (!pHashNode)
			return E_NO_HASH_SPACE;		
	}

	ssize_t iOldFlg = pHashNode->m_iFlag;
	if(iOldFlg == iFlag)
		return 0;
	
	pHashNode->m_iFlag = iFlag;

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);
	
	if (m_iBinLogOpen)
	{
		if ((!(iOldFlg&F_DATA_DIRTY)) && (iFlag&F_DATA_DIRTY))
		{
			_WriteDataLog(szKey);
		}
		else
		{
			_WriteNodeLog(szKey);
		}
	}
#endif
	return 0;
}

char* NodeCache::GetReserve(char szKey[HASH_KEY_LEN])
{
	if(sizeof(THashNode::m_szReserve) == 0)
		return NULL;
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pHashNode)
		return NULL;
	
	return pHashNode->m_szReserve;
}

ssize_t NodeCache::SetReserve(char szKey[HASH_KEY_LEN],char* pReserve,ssize_t iReserveLen)
{
	if(sizeof(THashNode::m_szReserve) == 0)
		return -1;
	
	if(iReserveLen > THashNode::HASH_NODE_RESERVE_LEN || iReserveLen <= 0)
		return -2;
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);	
		if (!pHashNode)
			return E_NO_HASH_SPACE;	
	}

	memset(pHashNode->m_szReserve,0,sizeof(pHashNode->m_szReserve));
	memcpy(pHashNode->m_szReserve,pReserve,iReserveLen);
	
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);
	
	if (m_iBinLogOpen)
	{
		_WriteNodeLog(szKey);
	}
	return 0;
}

ssize_t NodeCache::AppendBlock(char szKey[HASH_KEY_LEN],char* pBlockData,ssize_t iDataFlag/*=F_DATA_DIRTY*/)
{
	SetNodeFlg(szKey,GetNodeFlg(szKey)|iDataFlag);
	
	ssize_t iRet = _AppendBlock(szKey,pBlockData);
	if(iRet)
	{
		return iRet;
	}

	if (m_iBinLogOpen)
	{ 			
		ssize_t BLOCKSIZE = m_stBlockMng.GetBlockObjSize();

		ssize_t iOp = op_append;
		ssize_t iBuffLen = 0;
		
		memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		memcpy(m_pBuffer+iBuffLen,szKey,HASH_KEY_LEN);
		iBuffLen += HASH_KEY_LEN;

		memcpy(m_pBuffer+iBuffLen,pBlockData,BLOCKSIZE);
		iBuffLen += BLOCKSIZE;
		
		m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);	
	}

	return 0;
}

ssize_t NodeCache::InsertBlock(char szKey[HASH_KEY_LEN],char* pBlockData,ssize_t iInsertPos/*=0*/,ssize_t iDataFlag/*=F_DATA_DIRTY*/)
{
	SetNodeFlg(szKey,GetNodeFlg(szKey)|iDataFlag);
	
	ssize_t iRet = _InsertBlock(szKey,pBlockData,iInsertPos);
	if(iRet)
	{
		return iRet;
	}
	
	if (m_iBinLogOpen)
	{ 			
		ssize_t BLOCKSIZE = m_stBlockMng.GetBlockObjSize();

		ssize_t iOp = op_insert;
		ssize_t iBuffLen = 0;
		
		memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		memcpy(m_pBuffer+iBuffLen,szKey,HASH_KEY_LEN);
		iBuffLen += HASH_KEY_LEN;

		memcpy(m_pBuffer+iBuffLen,&iInsertPos,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		memcpy(m_pBuffer+iBuffLen,pBlockData,BLOCKSIZE);
		iBuffLen += BLOCKSIZE;
		
		m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);			
	}

	return 0;
}

ssize_t NodeCache::Set(char szKey[HASH_KEY_LEN],char* pBlockBuffer,ssize_t iBufferLen,
	ssize_t iDataFlag/*=F_DATA_DIRTY*/,char* pReserve/*=NULL*/,ssize_t iReserveLen/*=0*/)
{
	ssize_t iRet = _Set(szKey,pBlockBuffer,iBufferLen);
	if(iRet)
	{
		return iRet;
	}
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}	
	
	if((sizeof(THashNode::m_szReserve) > 0) && 
		(iReserveLen>0) && 
		(iReserveLen<=sizeof(THashNode::m_szReserve)))
	{
		memset(pHashNode->m_szReserve,0,sizeof(pHashNode->m_szReserve));
		memcpy(pHashNode->m_szReserve,pReserve,iReserveLen);
	}
	
#ifdef _HASH_NODE_DIRTY	
	pHashNode->m_iFlag = iDataFlag;
#endif

	if (m_iBinLogOpen)
	{ 			
		_WriteDataLog(szKey,pBlockBuffer,iBufferLen);
	}
	
	return 0;
}

ssize_t NodeCache::DeleteBlock(char szKey[HASH_KEY_LEN],BLOCK_DEL_EQU_FUNC fDelEquFunc,void* pDeleteArg/*=NULL*/)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

#ifdef _HASH_NODE_DIRTY	
	pHashNode->m_iFlag |= F_DATA_DIRTY;
#endif

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);

	ssize_t iDelNum = m_stBlockMng.DeleteBlock(iObjIdx,fDelEquFunc,pDeleteArg);
	
	if (m_iBinLogOpen && (iDelNum>0))
	{
		_WriteDataLog(szKey);
	}

	return 0;
}

ssize_t NodeCache::DeleteBlock(char szKey[HASH_KEY_LEN],ssize_t iPos)
{	
	SetNodeFlg(szKey,GetNodeFlg(szKey)|F_DATA_DIRTY);

	ssize_t iRet = _DeleteBlock(szKey,iPos);
	if(iRet)
	{
		return iRet;
	}
	
	if (m_iBinLogOpen)
	{
		ssize_t iOp = op_del_block;
		ssize_t iBuffLen = 0;
		
		memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		memcpy(m_pBuffer+iBuffLen,szKey,HASH_KEY_LEN);
		iBuffLen += HASH_KEY_LEN;	

		memcpy(m_pBuffer+iBuffLen,&iPos,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);		
	}	

	return 0;
}

ssize_t NodeCache::Del(char szKey[HASH_KEY_LEN])
{
	ssize_t iRet = _Del(szKey);
	if(iRet)
	{
		return iRet;
	}

	if (m_iBinLogOpen)
	{
		ssize_t iOp = op_del;
		ssize_t iBuffLen = 0;
		
		memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		memcpy(m_pBuffer+iBuffLen,szKey,HASH_KEY_LEN);
		iBuffLen += HASH_KEY_LEN;	

		m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);
	}
	return 0;
}

//保留iBlockNum个节点
ssize_t NodeCache::TrimTail(char szKey[HASH_KEY_LEN],ssize_t iBlockNum)
{
	SetNodeFlg(szKey,GetNodeFlg(szKey)|F_DATA_DIRTY);
	
	ssize_t iRet = _TrimTail(szKey,iBlockNum);
	if(iRet)
	{
		return iRet;
	}

	if (m_iBinLogOpen)
	{
		ssize_t iOp = op_trim_tail;
		ssize_t iBuffLen = 0;
		
		memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		memcpy(m_pBuffer+iBuffLen,szKey,HASH_KEY_LEN);
		iBuffLen += HASH_KEY_LEN;	

		memcpy(m_pBuffer+iBuffLen,&iBlockNum,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);		
		
		m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);		
	}

	return 0;
}

//返回数据长度
ssize_t NodeCache::SelectBlock(char szKey[HASH_KEY_LEN],char* pBuffer,const ssize_t iBUFFSIZE,BLOCK_SELECT_FUNC fBlockSelectFunc/*=NULL*/,void* pSelectArg/*=NULL*/,ssize_t iStartBlockPos/*=0*/,ssize_t iBlockNum/*=-1*/)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);	

	return m_stBlockMng.SelectBlock(iObjIdx,pBuffer,iBUFFSIZE,fBlockSelectFunc,pSelectArg,iStartBlockPos,iBlockNum);
}

//返回pos位置
ssize_t NodeCache::FindBlock(char szKey[HASH_KEY_LEN],BLOCK_FIND_EQU_FUNC fFindEquFunc,void* pFindArg/*=NULL*/)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);	

	return m_stBlockMng.FindBlock(iObjIdx,fFindEquFunc,pFindArg);
}

ssize_t NodeCache::GetBlockNum(char szKey[HASH_KEY_LEN])
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

	return m_stBlockMng.GetBlockNum(iObjIdx);
}

char* NodeCache::GetBlockData(char szKey[HASH_KEY_LEN],ssize_t iPos)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return NULL;
	}

	return m_stBlockMng.GetBlockData(iObjIdx,iPos);
}

char* NodeCache::GetFirstBlockData(char szKey[HASH_KEY_LEN])
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return NULL;
	}

	return m_stBlockMng.GetFirstBlockData(iObjIdx);
}

char* NodeCache::GetNextBlockData(char* pBlockData)
{
	return m_stBlockMng.GetNextBlockData(pBlockData);
}

ssize_t NodeCache::GetBlockPos(char szKey[HASH_KEY_LEN],char* pBlockData)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

	return m_stBlockMng.GetBlockPos(iObjIdx,pBlockData);
}

ssize_t NodeCache::GetUsage(ssize_t &iHashUsed,ssize_t &iHashCount,ssize_t &iBlockUsed,ssize_t &iBlockCount)
{
	return m_stBlockMng.GetUsage(iHashUsed, iHashCount, iBlockUsed,iBlockCount);
}

ssize_t NodeCache::GetBlockObjSize()
{
	return m_stBlockMng.GetBlockObjSize();
}

//淘汰
NodeCache::THashNode* NodeCache::GetOldestNode()
{
	THashNode* pHashNode = (THashNode*)m_stHashObjMng.GetAttachObj(m_stLRUQueue.GetHeadItem());
	
	return pHashNode;
}

void NodeCache::Print(FILE *fpOut)
{
	m_stHashObjMng.Print(fpOut);
	m_stBucketHashTab.Print(fpOut);

	m_stLRUQueue.Print(fpOut);

	m_stBlockMng.Print(fpOut);
}

//返回dump的字节数
ssize_t NodeCache::CoreDumpMem(char* pBuffer,const ssize_t BUFFSIZE,DUMP_MATCH_FUNC fDumpMatchFunc,void* pArg)
{
	size_t unDataLen = 0;
	ssize_t iBufferPos = 0;
	
	ssize_t iBucketNum = m_stBucketHashTab.GetBucketNum();
	for (ssize_t iBucketIdx=0; iBucketIdx<iBucketNum; iBucketIdx++)
	{
		THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeHead(iBucketIdx);
		while(pHashNode)
		{
			//不符合条件的
			if(fDumpMatchFunc((void*)pHashNode,pArg) != 0)
			{
				pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
				continue;
			}

			ssize_t iDateSize = GetBlockNum(pHashNode->m_szKey)*GetBlockObjSize();
			if (iDateSize > BUFFSIZE-iBufferPos)
			{
				return E_BUFF_TOO_SMALL;
			}

			memcpy(pBuffer+iBufferPos,pHashNode,sizeof(THashNode));
			iBufferPos += sizeof(THashNode);
			
			unDataLen = SelectBlock(pHashNode->m_szKey,m_pBuffer+iBufferPos+sizeof(ssize_t),BUFFSIZE-iBufferPos-sizeof(ssize_t));

			memcpy(pBuffer+iBufferPos,&unDataLen,sizeof(ssize_t));
			iBufferPos += sizeof(ssize_t);

			iBufferPos += unDataLen;

			pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
		}
	}

	return iBufferPos;
}

//返回恢复的记录数,写log
ssize_t NodeCache::CoreRecoverMem(char* pBuffer,ssize_t iBufferSize)
{
	THashNode stHashNode;
	ssize_t iDataLen=0;
	ssize_t iCount = 0;
	ssize_t iBufferPos = 0;
	
	while(iBufferPos < iBufferSize)
	{
		if (iBufferSize - iBufferPos < (ssize_t)sizeof(THashNode))
		{
			break;
		}
		
		memcpy(&stHashNode,pBuffer+iBufferPos,sizeof(THashNode));
		iBufferPos += sizeof(THashNode);

		memcpy(&iDataLen,pBuffer+iBufferPos,sizeof(ssize_t));
		iBufferPos += sizeof(ssize_t);
		
		if (iDataLen > iBufferSize - iBufferPos)
		{
			break;
		}
		
		ssize_t iRet = _Set(&stHashNode,pBuffer+iBufferPos,iDataLen);
		if(iRet)
		{
			printf("ERR:Set Node Failed!After recover %lld nodes[%s:%d]\n",(long long)iCount,__FILE__,__LINE__);
			iCount = -1;
			break;
		}
		
		if (m_iBinLogOpen)
		{ 			
			_WriteDataLog(stHashNode.m_szKey,pBuffer+iBufferPos,iDataLen);
		}
		
		iBufferPos += iDataLen;

		iCount++;
	}
	return iCount;
}

//返回清除的记录数,不写log
ssize_t NodeCache::CleanNode(DUMP_MATCH_FUNC fMatchFunc,void* pArg)
{
	ssize_t iCount = 0;
	THashNode* pNextHashNode = NULL;
	
	ssize_t iBucketNum = m_stBucketHashTab.GetBucketNum();
	for (ssize_t iBucketIdx=0; iBucketIdx<iBucketNum; iBucketIdx++)
	{
		THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeHead(iBucketIdx);
		while(pHashNode)
		{
			//符合条件的
			if(fMatchFunc((void*)pHashNode,pArg) == 0)
			{
				pNextHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
				_Del(pHashNode->m_szKey);
				pHashNode = pNextHashNode;
				iCount++;
				continue;
			}
			pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
		}
	}

	return iCount;
}

ssize_t NodeCache::CoreDump()
{
	if (0 <= _CoreDump(m_iDumpType))
	{			
		//delete binlog
		m_stBinLog.ClearAllBinLog();
		m_pNodeCacheHead->m_tLastDumpTime = time(0);
		return 0;
	}	
	return -1;
}

ssize_t NodeCache::_CoreDump(ssize_t iType)
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
	
	ssize_t iBucketNum = m_stBucketHashTab.GetBucketNum();
	for (ssize_t iBucketIdx=0; iBucketIdx<iBucketNum; iBucketIdx++)
	{
		THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeHead(iBucketIdx);
		while(pHashNode)
		{
#ifdef _HASH_NODE_DIRTY	
			if (iType == DUMP_TYPE_DIRTY_NODE)
			{
				if(!(pHashNode->m_iFlag&F_DATA_DIRTY))
				{
					pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
					continue;
				}			
			}
#endif			

			ssize_t iPos = 0;
			memcpy(m_pBuffer,pHashNode,sizeof(THashNode));
			iPos += sizeof(THashNode);

			ssize_t iDataLen = SelectBlock(pHashNode->m_szKey,m_pBuffer+iPos+sizeof(ssize_t),BUFFSIZE-iPos-sizeof(ssize_t));

			memcpy(m_pBuffer+iPos,&iDataLen,sizeof(ssize_t));
			iPos += sizeof(ssize_t);

			iPos += iDataLen;
			fwrite(m_pBuffer,1,iPos,pFile);
			
			pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
		}
	}

	ssize_t iLen = ftell(pFile);
	fclose(pFile);	
	return iLen;
}

ssize_t NodeCache::_CoreRecover(ssize_t iType)
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
		
	THashNode stHashNode;
	ssize_t iDataLen=0;
	ssize_t iReadLen = 0;
	ssize_t iCount = 0;	
	while(!feof(pFile))
	{
		if(sizeof(THashNode) != fread(&stHashNode,1,sizeof(THashNode),pFile))
		{
			break;
		}

		if(sizeof(ssize_t) != fread(&iDataLen,1,sizeof(ssize_t),pFile))
		{
			break;
		}
		
		if (iDataLen > BUFFSIZE)
		{
			printf("ERR:%s may be crashed!iDataLen(%lld) too large\n",m_szDumpFile,(long long)iDataLen);
			fclose(pFile);
			return -3;
		}

		iReadLen = fread(m_pBuffer,1,iDataLen,pFile);
		if (iReadLen != iDataLen)
		{
			break;
		}

		ssize_t iRet = _Set(&stHashNode,m_pBuffer,iDataLen);
		if(iRet)
		{
			printf("ERR:Set Node Failed!After recover %d nodes[%s:%d]\n",(long long)iCount,__FILE__,__LINE__);
			iCount = -1;
			break;
		}
		
		iCount++;
	}

	fclose(pFile);
	return iCount;
}

ssize_t NodeCache::_AppendBlock(char szKey[HASH_KEY_LEN],char* pBlockData)
{
	bool bNewCreate = false;
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		bNewCreate = true;
		pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);	
		if (!pHashNode)
			return E_NO_HASH_SPACE;	
	}

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);

	ssize_t iRet = m_stBlockMng.AppendBlock(iObjIdx,pBlockData);
	if(iRet && bNewCreate)
	{
		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);
	}
	return iRet;
}

ssize_t NodeCache::_InsertBlock(char szKey[HASH_KEY_LEN],char* pBlockData,ssize_t iInsertPos)
{
	bool bNewCreate = false;
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		bNewCreate = true;	
		pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);	
		if (!pHashNode)
			return E_NO_HASH_SPACE;		
	}
	
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);
	
	ssize_t iRet = m_stBlockMng.InsertBlock(iObjIdx,pBlockData,iInsertPos);
	if(iRet && bNewCreate)
	{
		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);
	}	
	return iRet;
}

ssize_t NodeCache::_Set(THashNode* pHashNode,char* pBlockBuffer,ssize_t iBufferLen)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pTmpHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN,iObjIdx);
	if (!pTmpHashNode)
	{
		pTmpHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(pHashNode->m_szKey,HASH_KEY_LEN,iObjIdx);	
		if (!pTmpHashNode)
			return E_NO_HASH_SPACE;	
	}

	memcpy(pTmpHashNode,pHashNode,sizeof(THashNode));
	
	return _Set(pHashNode->m_szKey,pBlockBuffer,iBufferLen);

}

ssize_t NodeCache::_Set(char szKey[HASH_KEY_LEN],char* pBlockBuffer,ssize_t iBufferLen)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);	
		if (!pHashNode)
			return E_NO_HASH_SPACE;	
	}
	
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);	
	
	m_stBlockMng.FreeBlock(iObjIdx);

	ssize_t BLOCKSIZE = m_stBlockMng.GetBlockObjSize();
	if(BLOCKSIZE <= 0)
		return -1;
	
	ssize_t iBlockNum = (ssize_t)(iBufferLen/(float)BLOCKSIZE);

	for (ssize_t i=iBlockNum-1; i>=0; i--)
	{
		ssize_t iRet = m_stBlockMng.InsertBlock(iObjIdx,pBlockBuffer+BLOCKSIZE*i,0);
		if(iRet)
		{
			m_stLRUQueue.DeleteItem(iObjIdx);
			m_stBlockMng.FreeBlock(iObjIdx);
			m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);		
			return E_NO_OBJ_SPACE;
		}
	}
	return 0;
}

ssize_t NodeCache::_DeleteBlock(char szKey[HASH_KEY_LEN],ssize_t iPos)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);	

	return m_stBlockMng.DeleteBlock(iObjIdx,iPos);
}


ssize_t NodeCache::_Del(char szKey[HASH_KEY_LEN])
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

	m_stBlockMng.FreeBlock(iObjIdx);
	
	m_stLRUQueue.DeleteItem(iObjIdx);
	
	m_stBucketHashTab.DeleteObjectByKey(szKey, HASH_KEY_LEN);
	return 0;
}

ssize_t NodeCache::_TrimTail(char szKey[HASH_KEY_LEN],ssize_t iBlockNum)
{
	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_PAGE_NO_DATA;
	}

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);	

	return m_stBlockMng.TrimTail(iObjIdx,iBlockNum);
}

NodeCache::THashNode* NodeCache::_GetHashNode(char szKey[HASH_KEY_LEN])
{
	ssize_t iObjIdx = -1;
	return (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
}

//节点拍照
ssize_t NodeCache::_WriteNodeLog(char szKey[HASH_KEY_LEN])
{
	ssize_t iOp = op_set_node;
	ssize_t iBuffLen = 0;

	THashNode* pHashNode = _GetHashNode(szKey);
	if(!pHashNode)
		return E_NO_HASH_SPACE;
	
	memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
	iBuffLen += sizeof(ssize_t);
	
	memcpy(m_pBuffer+iBuffLen,pHashNode,sizeof(THashNode));
	iBuffLen += sizeof(THashNode);
	
	return m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);		
}

//节点和数据拍照
ssize_t NodeCache::_WriteDataLog(char szKey[HASH_KEY_LEN],char* pBuffer/*=NULL*/,ssize_t iBufferSize/*=0*/)
{
	char *pDataBuffer = pBuffer;
	ssize_t iDataSize = iBufferSize;

	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return E_NO_HASH_SPACE;
	}
	
	ssize_t iOp = op_set;
	ssize_t iBuffLen = 0;
	
	memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
	iBuffLen += sizeof(ssize_t);

	memcpy(m_pBuffer+iBuffLen,pHashNode,sizeof(THashNode));
	iBuffLen += sizeof(THashNode);

	if(!pDataBuffer)
	{
		ssize_t iDataLen = m_stBlockMng.SelectBlock(iObjIdx,m_pBuffer+iBuffLen+sizeof(ssize_t), BUFFSIZE-iBuffLen-sizeof(ssize_t));
		
		memcpy(m_pBuffer+iBuffLen,&iDataLen,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		iBuffLen += iDataLen;	
	}
	else
	{
		memcpy(m_pBuffer+iBuffLen,&iDataSize,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);

		memcpy(m_pBuffer+iBuffLen,pDataBuffer,iDataSize);
		iBuffLen += iDataSize;
	}
	return m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);	
}

