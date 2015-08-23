#include "MemCache.hpp"

enum
{
	op_set = 1,
	op_del,
	op_set_node,
	op_append,
};

//设置TBucketNode的主键
ssize_t Mem_SetNodeKey(void* pObj,void* pKey,ssize_t iKeyLen)
{
	MemCache::THashNode* pHashNode = (MemCache::THashNode*)pObj;
	if (iKeyLen < (ssize_t)sizeof(pHashNode->m_szKey))
	{
		return -1;
	}
	memset(pHashNode,0,sizeof(MemCache::THashNode));
	memcpy(pHashNode->m_szKey,pKey,sizeof(pHashNode->m_szKey));
	return 0;
}

//获取TBucketNode的主键
ssize_t Mem_GetNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen)
{
	MemCache::THashNode* pHashNode = (MemCache::THashNode*)pObj;
	memcpy(pKey,pHashNode->m_szKey,sizeof(pHashNode->m_szKey));
	iKeyLen = sizeof(pHashNode->m_szKey);
	return 0;
}

MemCache::MemCache()
{
	m_pMemCacheHead = NULL;
	m_iDumpMin = 0;
	memset(m_szDumpFile,0,sizeof(m_szDumpFile));
	m_iInitType = 0;

	m_iBinLogOpen = 0;
	m_iTotalBinLogSize = 0;
	m_pBuffer = new char[BUFFSIZE];
}

MemCache::~MemCache()
{
	delete []m_pBuffer;
}

ssize_t MemCache::CountBaseSize(ssize_t iHashNum,ssize_t iBlockSize/*=512*/)
{
	ssize_t iSize = sizeof(TMemCacheHead)+CHashTab::CountMemSize((ssize_t)(iHashNum/(float)2)) +
		TIdxObjMng::CountMemSize(sizeof(THashNode),iHashNum,3) +
		CObjQueue::CountMemSize();

	if(iBlockSize > 0)
	{
		iSize+=(CBuffMng::CountMemSize(iHashNum)+TIdxObjMng::CountMemSize(iBlockSize,iHashNum,1));
	}
	return iSize;
}

//绑定内存,iBlockSize 越大，对大数据的处理越快
ssize_t MemCache::AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iHashNum,ssize_t iInitType/*=emInit*/,ssize_t iBlockSize/*=512*/)
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
	m_pMemCacheHead = (TMemCacheHead*)pMemPtr;
	iAllocBytes = sizeof(TMemCacheHead);

	m_iInitType = iInitType;
	if (iInitType == emInit)
	{
		m_pMemCacheHead->m_tLastDumpTime = 0;
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
	if(iBlockSize > 0)
	{
		//m_stBuffMng 需要1条链
		iAttachBytes = m_stBuffMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iHashNum,iInitType);
		if (iAttachBytes < 0)
			return -8;
		iAllocBytes += iAttachBytes;

		//剩下的分给数据块
		iAttachBytes = m_stBuffObjMng.AttachMem(pMemPtr+iAllocBytes,MEMSIZE-iAllocBytes,iBlockSize,0,iInitType,1);
		if (iAttachBytes < 0)
			return -9;
		iAllocBytes += iAttachBytes;
		
		if(m_stBuffMng.AttachIdxObjMng(&m_stBuffObjMng))
		{
			return -7;
		}		
	}

	m_pMemPtr = pMemPtr;
	m_iMemSize = iAllocBytes;	
	return iAllocBytes;
}

ssize_t MemCache::DumpInit(ssize_t iDumpMin,
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

ssize_t MemCache::StartUp()
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

	ssize_t iOp = 0;	
	char szHashKey[HASH_KEY_LEN];
	ssize_t iDataLen = 0;
	char* pData = NULL;
	ssize_t iCount = 0;
	ssize_t iLogLen = 0;
	
	time_t tLogTime;
	//恢复日志流水,均为脏数据
	m_stBinLog.ReadRecordStart();	
	while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(m_pBuffer,BUFFSIZE,tLogTime)))
	{
		ssize_t iBuffPos = 0;
		
		memcpy(&iOp,m_pBuffer,sizeof(ssize_t));
		iBuffPos += sizeof(ssize_t);

		if (iOp == op_set)
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

			ssize_t iObjIdx = -1;
			THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(stHashNode.m_szKey,HASH_KEY_LEN,iObjIdx);
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
		else if  (iOp == op_del)
		{	
			memcpy(szHashKey,m_pBuffer+iBuffPos,HASH_KEY_LEN);
			iBuffPos += HASH_KEY_LEN;
		
			_Del(szHashKey);
		}		
		else if (iOp == op_append)
		{		
			memcpy(szHashKey,m_pBuffer+iBuffPos,HASH_KEY_LEN);
			iBuffPos += HASH_KEY_LEN;
			
			memcpy(&iDataLen,m_pBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);
			
			pData = m_pBuffer+iBuffPos;
			_Append(szHashKey,pData,iDataLen);
		}
		else 
		{
			printf("bad binlog op %d.\n",iOp);
		}
		
		iCount++;
	}

	printf("recover from binlog %lld records.\n",(long long)iCount);
	return 0;
}

ssize_t MemCache::TimeTick(time_t iNow)
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
		
		if(tNow -m_pMemCacheHead->m_tLastDumpTime >=  (m_iDumpMin*60))
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

ssize_t MemCache::GetNodeFlg(char szKey[HASH_KEY_LEN])
{
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pHashNode)
		return 0;

	return pHashNode->m_iFlag;
}

//当一个节点从干净变脏的时候,需要记录全量数据
ssize_t MemCache::SetNodeFlg(char szKey[HASH_KEY_LEN],ssize_t iFlag)
{
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
		if (iOldFlg != F_DATA_CLEAN)
		{
			_WriteNodeLog(szKey);
		}
		else
		{
			_WriteDataLog(szKey);
		}
	}
	return 0;
}

const char* MemCache::GetReserve(char szKey[HASH_KEY_LEN])
{
	if(NODE_RESERVE_LEN == 0)
		return NULL;
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pHashNode)
		return NULL;

	return pHashNode->m_szReserve;
}

ssize_t MemCache::SetReserve(char szKey[HASH_KEY_LEN],char* pReserve,ssize_t iReserveLen)
{
	if(iReserveLen > NODE_RESERVE_LEN || iReserveLen <= 0)
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
	return 0;
}

ssize_t MemCache::Set(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,
			ssize_t iDataFlag/*=F_DATA_DIRTY*/,char* pReserve/*=NULL*/,ssize_t iReserveLen/*=0*/)
{
	ssize_t iRet = _Set(szKey,szData,iDataSize);
	if(iRet)
		return iRet;
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
		return E_NO_DATA;	
	
	if(iReserveLen > NODE_RESERVE_LEN)
	{
		return E_RESERVE_BUFF_SMALL;
	}
	
	memset(pHashNode->m_szReserve,0,sizeof(pHashNode->m_szReserve));
	memcpy(pHashNode->m_szReserve,pReserve,iReserveLen);
	
	ssize_t iOldDataFlag = pHashNode->m_iFlag;
	pHashNode->m_iFlag = iDataFlag;

	//脏数据写binlog
	if (m_iBinLogOpen)
	{
		if((iDataFlag != F_DATA_CLEAN) || (iOldDataFlag != F_DATA_CLEAN))
		{
			_WriteDataLog(szKey,szData,iDataSize);
		}
	}
	return 0;
}

//返回0和非0
ssize_t MemCache::Get(char szKey[HASH_KEY_LEN],char* szData,const ssize_t DATASIZE,size_t &unDataLen)
{
	if (!szKey || !szData || DATASIZE<=0)
	{
		return -1;
	}
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
		return E_NO_DATA;

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);

	if (DATASIZE < m_stBuffMng.GetBufferSize(iObjIdx))
	{
		return E_BUFF_TOO_SMALL;
	}
	
	unDataLen = m_stBuffMng.GetBuffer(iObjIdx,szData,DATASIZE);
	return 0;
}

ssize_t MemCache::Del(char szKey[HASH_KEY_LEN])
{
	ssize_t iRet = _Del(szKey);
	if(iRet)
	{
		return iRet;
	}

	if (m_iBinLogOpen)
	{
		if(GetNodeFlg(szKey) != F_DATA_CLEAN)
		{
			ssize_t iOp = op_del;
			ssize_t iBuffLen = 0;
			
			memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
			iBuffLen += sizeof(ssize_t);
			
			memcpy(m_pBuffer+iBuffLen,szKey,HASH_KEY_LEN);
			iBuffLen += HASH_KEY_LEN;	

			m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);		
		}

	}
	return 0;
}

#ifdef _BUFFMNG_APPEND_SKIP	
ssize_t MemCache::Append(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,ssize_t iDataFlag/*=F_DATA_DIRTY*/)
{
	ssize_t iRet = _Append(szKey,szData,iDataSize);
	if(iRet)
	{
		return iRet;
	}
	return 0;
}
#endif

ssize_t MemCache::GetDataSize(char szKey[HASH_KEY_LEN])
{
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
		return 0;

	return m_stBuffMng.GetBufferSize(iObjIdx);	
}

ssize_t MemCache::GetUsage(ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount)
{
	//哈希节点和buffmng的节点一一对应的
	return m_stBuffMng.GetBufferUsage(iHashNodeUsed,iHashNodeCount,iObjNodeUsed,iObjNodeCount);
}

//淘汰
MemCache::THashNode* MemCache::GetOldestNode()
{
	THashNode* pHashNode = (THashNode*)m_stHashObjMng.GetAttachObj(m_stLRUQueue.GetHeadItem());
	
	return pHashNode;
}

void MemCache::Print(FILE *fpOut)
{
	m_stHashObjMng.Print(fpOut);
	m_stBucketHashTab.Print(fpOut);

	m_stLRUQueue.Print(fpOut);

	m_stBuffMng.Print(fpOut);
}

//返回dump的字节数
ssize_t MemCache::CoreDumpMem(char* pBuffer,const ssize_t BUFFSIZE,DUMP_MATCH_FUNC fDumpMatchFunc,void* pArg)
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

			if (GetDataSize(pHashNode->m_szKey) > BUFFSIZE-iBufferPos)
			{
				return E_BUFF_TOO_SMALL;
			}

			memcpy(pBuffer+iBufferPos,pHashNode,sizeof(THashNode));
			iBufferPos += sizeof(THashNode);

			unDataLen = 0;
			Get(pHashNode->m_szKey,pBuffer+iBufferPos+sizeof(ssize_t),BUFFSIZE-iBufferPos-sizeof(ssize_t),unDataLen);

			memcpy(pBuffer+iBufferPos,&unDataLen,sizeof(ssize_t));
			iBufferPos += sizeof(ssize_t);

			iBufferPos += unDataLen;

			pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
		}
	}

	return iBufferPos;
}

//返回恢复的记录数
ssize_t MemCache::CoreRecoverMem(char* pBuffer,ssize_t iBufferSize)
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
		
		_Set(&stHashNode,pBuffer+iBufferPos,iDataLen);
		
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
ssize_t MemCache::CleanNode(DUMP_MATCH_FUNC fMatchFunc,void* pArg)
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

ssize_t MemCache::CoreDump()
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

ssize_t MemCache::_CoreDump(ssize_t iType)
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
	
	size_t unDataLen = 0;
	
	ssize_t iBucketNum = m_stBucketHashTab.GetBucketNum();
	for (ssize_t iBucketIdx=0; iBucketIdx<iBucketNum; iBucketIdx++)
	{
		THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeHead(iBucketIdx);
		while(pHashNode)
		{
			if (iType == DUMP_TYPE_DIRTY_NODE)
			{
				if(pHashNode->m_iFlag == F_DATA_CLEAN)
				{
					pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
					continue;
				}			
			}		

			ssize_t iPos = 0;
			memcpy(m_pBuffer,pHashNode,sizeof(THashNode));
			iPos += sizeof(THashNode);
			
			unDataLen = 0;
			Get(pHashNode->m_szKey,m_pBuffer+iPos+sizeof(ssize_t),BUFFSIZE-iPos-sizeof(ssize_t),unDataLen);

			memcpy(m_pBuffer+iPos,&unDataLen,sizeof(ssize_t));
			iPos += sizeof(ssize_t);

			iPos += unDataLen;
			fwrite(m_pBuffer,1,iPos,pFile);

			pHashNode = (THashNode*)m_stBucketHashTab.GetBucketNodeNext((char *)pHashNode);
		}
	}

	ssize_t iLen = ftell(pFile);
	fclose(pFile);	
	return iLen;
}

ssize_t MemCache::_CoreRecover(ssize_t iType)
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
	ssize_t iDataLen = 0;
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
			printf("ERR:%s may be crashed!\n",m_szDumpFile);
			fclose(pFile);
			return -1;
		}

		iReadLen = fread(m_pBuffer,1,iDataLen,pFile);
		if (iReadLen != iDataLen)
		{
			break;
		}

		_Set(&stHashNode,m_pBuffer,iDataLen);	
		iCount++;
	}

	fclose(pFile);
	return iCount;
}

ssize_t MemCache::_Set(THashNode* pHashNode,char* pBlockBuffer,ssize_t iBufferLen)
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

ssize_t MemCache::_Set(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize)
{
	if (!szKey || !szData || iDataSize<0)
	{
		return -1;
	}

	ssize_t iObjIdx = -1;
	//无则创建,有则返回
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (pHashNode)
	{
		m_stBuffMng.FreeBuffer(iObjIdx);	
	}
	else
	{
		pHashNode = (THashNode*)m_stBucketHashTab.CreateObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
		if (!pHashNode)
			return E_NO_HASH_SPACE;	
	}

	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stLRUQueue.AppendToTail(iObjIdx);

	//空数据
	if (iDataSize == 0)
	{
		return 0;
	}
		
	if(m_stBuffMng.SetBuffer(iObjIdx,szData,iDataSize))
	{
		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);
		return E_NO_OBJ_SPACE;
	}
	return 0;
}

ssize_t MemCache::_Del(char szKey[HASH_KEY_LEN])
{
	if (!szKey)
	{
		return -1;
	}
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
		return E_NO_HASH_SPACE;
	
	m_stBuffMng.FreeBuffer(iObjIdx);
	
	m_stLRUQueue.DeleteItem(iObjIdx);
	m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);
	return 0;
}

#ifdef _BUFFMNG_APPEND_SKIP	
ssize_t MemCache::_Append(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize)
{
	if (!szKey || !szData || iDataSize<0)
	{
		return -1;
	}

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

	//空数据
	if (iDataSize == 0)
	{
		return 0;
	}
		
	if(m_stBuffMng.AppendBuffer(iObjIdx,szData,iDataSize))
	{
		m_stLRUQueue.DeleteItem(iObjIdx);
		m_stBucketHashTab.DeleteObjectByKey(szKey,HASH_KEY_LEN);
		return E_NO_OBJ_SPACE;
	}
	return 0;
}

#endif

//节点拍照
ssize_t MemCache::_WriteNodeLog(char szKey[HASH_KEY_LEN])
{
	ssize_t iOp = op_set_node;
	ssize_t iBuffLen = 0;
	
	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if(!pHashNode)
		return -1;
	
	memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
	iBuffLen += sizeof(ssize_t);
	
	memcpy(m_pBuffer+iBuffLen,pHashNode,sizeof(THashNode));
	iBuffLen += sizeof(THashNode);
	
	return m_stBinLog.WriteToBinLog(m_pBuffer,iBuffLen);		
}

//节点和数据拍照
ssize_t MemCache::_WriteDataLog(char szKey[HASH_KEY_LEN],char* pBuffer/*=NULL*/,ssize_t iBufferSize/*=0*/)
{
	char *pDataBuffer = pBuffer;
	ssize_t iDataSize = iBufferSize;

	ssize_t iObjIdx = -1;
	THashNode* pHashNode = (THashNode*)m_stBucketHashTab.GetObjectByKey(szKey,HASH_KEY_LEN,iObjIdx);
	if (!pHashNode)
	{
		return -1;
	}
	
	ssize_t iOp = op_set;
	ssize_t iBuffLen = 0;
	
	memcpy(m_pBuffer,&iOp,sizeof(ssize_t));
	iBuffLen += sizeof(ssize_t);

	memcpy(m_pBuffer+iBuffLen,pHashNode,sizeof(THashNode));
	iBuffLen += sizeof(THashNode);

	if(!pDataBuffer)
	{
		ssize_t iDataLen = m_stBuffMng.GetBuffer(iObjIdx,m_pBuffer+iBuffLen+sizeof(ssize_t),BUFFSIZE-iBuffLen-sizeof(ssize_t));
		
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

/*
#include <sys/time.h>

int main()
{
	int MEMSIZE = 1024*1024*100;
	char *pMem = new char[MEMSIZE];

	MemCache stMemCache;
	int len = stMemCache.AttachMem(pMem, MEMSIZE,10000,emInit,512);
	printf("alloc: %d\n",len);	

		char data[2560];
		memset(data,0,sizeof(data));
			unsigned int  unDataLen = 0;
			
	stMemCache.DumpInit(1, "cache.dump", MemCache::DUMP_TYPE_NODE, 1,"binlog",50000000);
	stMemCache.StartUp();

	char szKey[MemCache::HASH_KEY_LEN];
	memset(szKey,0,sizeof(szKey));

	char szbb[1024];

	timeval ttt,ttt2;

			int kk = 999;
			stMemCache.Set((char*)&kk, szbb, 1024);
			
	gettimeofday(&ttt,NULL);
	
	for (int i=0; i<1000000;i++)
		{
			int kk = 999;
			uint_t ll;
			//stMemCache.Set((char*)&kk, szbb, 1024,ll);
			stMemCache.Get((char*)&kk, szbb, 1024,ll);
		}
	gettimeofday(&ttt2,NULL);
	int iMillSecsSpan = (ttt2.tv_sec-ttt.tv_sec)*1000000+(ttt2.tv_usec-ttt.tv_usec);
	printf("time %d us ,avg %d us\n",iMillSecsSpan,iMillSecsSpan/1000000);

	return 0;
}
*/
