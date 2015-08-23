#include "Bitmap.hpp"
#include <math.h>
enum
{
	op_set
};

CBitmap::CBitmap()
{
	m_pHeadInfo = NULL;
	m_pBodyMem = NULL;
	m_unBodySize = 0;
	
	memset(m_pBitMem,0,sizeof(m_pBitMem));
	m_unMapSize = 0;
	
	m_iBinLogOpen = 0;
	m_iDumpMin = 0;
	m_iTotalBinLogSize = 0;
	memset(m_szDumpFile,0,sizeof(m_szDumpFile));
}

CBitmap::~CBitmap()
{
}

ssize_t CBitmap::CountBaseSize(size_t unBitCount,size_t unBitWords/*=1*/)
{
	size_t unMapSize = unBitCount/8;
	if(unBitCount%8 != 0)
		unMapSize++;
	
	return sizeof(THeadInfo)+unMapSize*unBitWords;
}

ssize_t CBitmap::AttachMem(char* pMem,size_t unMemSize,
				size_t unBitWords/*=1*/,ssize_t iInitType/*=emInit*/)
{
	if (unBitWords > 4 || unBitWords == 0)
	{
		printf("BitWords must be 1-4\n");
		return -1;
	}
	
	if(unMemSize<= sizeof(THeadInfo))
	{
		printf("Bitmap Need More memory!\n");
		return -2;
	}
	
	m_pHeadInfo = (THeadInfo*)pMem;
	m_pBodyMem = pMem+sizeof(THeadInfo);
	m_unBodySize = unMemSize-sizeof(THeadInfo);
	m_unMapSize = m_unBodySize/unBitWords;
	
	for (size_t i=0; i<unBitWords; i++)
	{
		m_pBitMem[i] = m_pBodyMem + m_unMapSize*i;
	}
	
	m_iInitType = iInitType;
	if(iInitType == emInit)
	{
		memset(m_pHeadInfo,0,sizeof(THeadInfo));
		memset(m_pBodyMem,0,m_unBodySize);
		
		m_pHeadInfo->m_unBitWords = unBitWords;
		m_pHeadInfo->m_unValStat[0] = m_unMapSize*8;
	}
	else
	{
		if(m_pHeadInfo->m_unBitWords != unBitWords)
		{
			printf("Bitmap Mem check failed, BitWords must be %d, not %d\n",
				m_pHeadInfo->m_unBitWords,unBitWords);
			return -1;
		}	

		size_t unAllBitCount = 0;
		for (ssize_t i=0; i<2*2*2*2; i++)
		{
			unAllBitCount += m_pHeadInfo->m_unValStat[i];
		}
		if(unAllBitCount != m_unMapSize*8)
		{
			printf("Bitmap Mem check failed, BitWords must be %d, not %d\n",
				unAllBitCount,m_unMapSize*8);
			return -1;
		}		
	}
	
	return unMemSize;
}

ssize_t CBitmap::DumpInit(ssize_t iDumpMin,char *DumpFile/*=cache.dump*/,ssize_t iBinLogOpen/*=0*/,char * pBinLogBaseName/*="binlog_"*/, ssize_t iMaxBinLogSize/*=50000000*/, ssize_t iMaxBinLogNum/*=10*/)
{
	m_iDumpMin = iDumpMin;
	strcpy(m_szDumpFile,DumpFile);
	
	m_iBinLogOpen = iBinLogOpen;
	m_iTotalBinLogSize = iMaxBinLogSize*iMaxBinLogNum;
	
	if (m_iBinLogOpen)
	{
		m_stBinLog.Init(pBinLogBaseName, iMaxBinLogSize,iMaxBinLogNum);
	}
	return 0;
}

ssize_t CBitmap::StartUp()
{
	//非新建的共享内存,不用恢复
	if (m_iInitType!=emInit)
	{
		return 0;
	}

	if (access(m_szDumpFile, F_OK) == 0)
	{
		//恢复core
		ssize_t iRet = _CoreRecover();
		if (iRet < 0)
		{
			printf("Recover from dumpfile(%s) failed, ret %lld.\n",m_szDumpFile,(long long)iRet);
			return iRet;
		}
		printf("Recover from %d bytes from dumpfile.\n",iRet);
	}
	else
	{
		printf("no dumpfile to recover.\n");
	}

	ssize_t iOp = 0;	
	ssize_t iCount = 0;
	ssize_t iLogLen = 0;
	char m_szBuffer[1024];
	u_int64_t tLogTime;
	
	//恢复日志流水
	m_stBinLog.ReadRecordStart();
	while(0<(iLogLen = m_stBinLog.ReadRecordFromBinLog(m_szBuffer,sizeof(m_szBuffer),tLogTime)))
	{
		size_t unUin = 0;
		ssize_t iBuffPos = 0;
		
		memcpy(&iOp,m_szBuffer,sizeof(ssize_t));
		iBuffPos += sizeof(ssize_t);

		if  (iOp == op_set)
		{
			memcpy(&unUin,m_szBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);

			ssize_t iVal = 0;
			memcpy(&iVal,m_szBuffer+iBuffPos,sizeof(ssize_t));
			iBuffPos += sizeof(ssize_t);
			
			_SetBit(unUin,iVal);			
		}
		
		iCount++;
	}

	printf("recover from binlog %lld records.\n",(long long)iCount);
	return 0;
}

ssize_t CBitmap::TimeTick(time_t iNow)
{
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
		
		if(tNow -m_pHeadInfo->m_tLastDumpTime >=  (m_iDumpMin*60))
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

ssize_t CBitmap::SetBit(size_t unBitPos,ssize_t iBitVal)
{
	size_t unBytePos = (ssize_t)(unBitPos/8.0f);		
	if (unBytePos >= m_unMapSize)
		return -1;
	
	if((iBitVal < 0) || 
		(iBitVal >= (ssize_t)pow(2,(double)m_pHeadInfo->m_unBitWords)))
	{
		return -2;
	}
	
    	if (0!=_SetBit(unBitPos,iBitVal))
    	{
    		return -3;
    	}
		
	m_pHeadInfo->m_unDataFlag |= DF_DATA_DIRTY;
	
	if (m_iBinLogOpen)
	{
		char m_szBuffer[1024];
		
		ssize_t iOp = op_set;
		ssize_t iBuffLen = 0;
		
		memcpy(m_szBuffer,&iOp,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		memcpy(m_szBuffer+iBuffLen,&unBitPos,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);

		memcpy(m_szBuffer+iBuffLen,&iBitVal,sizeof(ssize_t));
		iBuffLen += sizeof(ssize_t);
		
		m_stBinLog.WriteToBinLog(m_szBuffer,iBuffLen);		
	}
	
	return 0;
}

ssize_t CBitmap::GetBit(size_t unBitPos)
{
	size_t unBytePos = (ssize_t)(unBitPos/8.0f);		
	if (unBytePos >= m_unMapSize)
		return -1;
	
	ssize_t iBitOldVal = 0;
	for (ssize_t i=0; i<(ssize_t)m_pHeadInfo->m_unBitWords; i++)
	{
		ssize_t iMask = (ssize_t)pow(2,(double)i);
		
		if(_IsBitSet(m_pBitMem[i],unBitPos))
			iBitOldVal |= iMask;

	}
		
	return iBitOldVal;
}

ssize_t CBitmap::GetUsage(size_t punValStat[2*2*2*2])
{	
	for (ssize_t i=0; i<2*2*2*2; i++)
	{
		punValStat[i] = m_pHeadInfo->m_unValStat[i];
	}
	return 0;
}

ssize_t CBitmap::CoreDump()
{
	if (0 <= _CoreDump())
	{			
		//delete binlog
		m_stBinLog.ClearAllBinLog();
		
		m_pHeadInfo->m_tLastDumpTime = time(0);
		m_pHeadInfo->m_unDataFlag &= (~DF_DATA_DIRTY);
		return 0;
	}	
	return -1;
}

ssize_t CBitmap::_SetBit(size_t unBitPos,ssize_t iBitVal)
{
	ssize_t iBitOldVal = 0;
	for (ssize_t i=0; i<(ssize_t)m_pHeadInfo->m_unBitWords; i++)
	{
		ssize_t iMask = (ssize_t)pow(2,(double)i);
		
		if(iBitVal & iMask)
		{
			if(0!=_SetBit(m_pBitMem[i],unBitPos))
				iBitOldVal |= iMask;
		}
		else
		{
			if(0==_ClearBit(m_pBitMem[i],unBitPos))
				iBitOldVal |= iMask;
		}	
	}

	//无改变
    	if (iBitOldVal == iBitVal)
    	{
    		return -2;
    	}
		
	m_pHeadInfo->m_unValStat[iBitOldVal]--;
	m_pHeadInfo->m_unValStat[iBitVal]++;
	
	return 0;
}

ssize_t CBitmap::_SetBit(char* pBitMem,size_t unBitPos)
{
	size_t unBytePos = (ssize_t)(unBitPos/8.0f);
	
	if (unBytePos >= m_unMapSize)
		return -1;
	
	unsigned char cMask = 1<<(7-unBitPos%8);

	//无改变
    	if ((pBitMem[unBytePos] & cMask) != 0)
    	{
    		return 1;
    	}
		
	pBitMem[unBytePos] |= cMask;
	return 0;
}

ssize_t CBitmap::_ClearBit(char* pBitMem,size_t unBitPos)
{
	size_t unBytePos = (ssize_t)(unBitPos/8.0f);
	if (unBytePos >= m_unMapSize)
		return -1;

	unsigned char cTmpMask = 1<<(7-unBitPos%8);
	
	unsigned char cMask = ~cTmpMask;

	//无改变
    	if ((pBitMem[unBytePos] & cTmpMask) == 0)
    	{
		return 1;
    	}
	
	pBitMem[unBytePos] &= cMask;
	return 0;
}

ssize_t CBitmap::_IsBitSet(char* pBitMem,size_t unBitPos)
{
	size_t unBytePos = (ssize_t)(unBitPos/8.0f);
	if (unBytePos >= m_unMapSize)
		return -1;

	unsigned char cMask = 1<<(7-unBitPos%8);

    	if ((pBitMem[unBytePos] & cMask) == 0)
    	{
    		return 0;
    	}

	return 1;
}	

ssize_t CBitmap::_CoreDump()
{
	if(m_szDumpFile[0] == 0)
		return -1;

	//数据未改变
	if(!(m_pHeadInfo->m_unDataFlag & DF_DATA_DIRTY))
	{
		return 0;
	}
	
	char szBkFile[256];
	sprintf(szBkFile,"%s.bk",m_szDumpFile);
	
	FILE *fp =fopen(szBkFile,"w+");
	if (!fp)
	{
		return -1;
	}

	fwrite((void*)m_pHeadInfo,1,sizeof(THeadInfo),fp);
	fwrite(m_pBodyMem,1,m_unBodySize,fp);
	fclose(fp);

	char szCmd[256];
	sprintf(szCmd,"mv %s %s",szBkFile,m_szDumpFile);
	system(szCmd);
	return 0;
}

ssize_t CBitmap::_CoreRecover()
{	
	FILE *pFile =fopen(m_szDumpFile,"rb+");
	if (!pFile)
	{
		return -1;
	}

	fread(m_pHeadInfo,1,sizeof(THeadInfo),pFile); 
	fread(m_pBodyMem,1,m_unBodySize,pFile); 
	fclose(pFile);	

	return sizeof(THeadInfo)+m_unBodySize;
}

