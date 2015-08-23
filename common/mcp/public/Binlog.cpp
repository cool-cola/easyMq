#include "Binlog.hpp"

#define FCLOSE(pFP)	{\
if(pFP) fclose(pFP);\
pFP = NULL;\
}

CBinLog::CBinLog()
{
	m_iMaxLogSize = 50000000;
	m_iMaxLogNum = 10;
	strcpy(m_szLogBaseName,"binlog_");

	m_unTotalLogSize = 0;	
	m_unCurrLogSize = 0;
	
	m_pWriteFp = NULL;
	m_pReadFp = NULL;
	m_iReadLogIdx = m_iMaxLogNum;

	m_szMergeBuff = NULL;
	m_iMergeLen = 0;

	m_bEnableShift = true;
}

CBinLog::~CBinLog()
{
	Flush();
	
	FCLOSE(m_pWriteFp);
	FCLOSE(m_pReadFp);	

	delete []m_szMergeBuff;
	m_szMergeBuff = NULL;
}

void CBinLog::OpenMerge(int iMergeBuffSize/*=16*1024*1024*/)
{
	delete []m_szMergeBuff;
	m_szMergeBuff = NULL;
	m_iMergeLen = 0;
	
	MERGE_BUFF_SIZE = iMergeBuffSize;
	if(MERGE_BUFF_SIZE > 0)
		m_szMergeBuff = new char[MERGE_BUFF_SIZE];
}

int32_t CBinLog::Init(char *pLogBaseName/*="binlog_"*/,
				int32_t iMaxLogSize/*=50000000*/, int32_t iMaxLogNum/*=10*/,
				bool bAutoFlushDisk/*=true*/, bool bEachWriteClose/*=false*/)
{
	FCLOSE(m_pWriteFp);
	FCLOSE(m_pReadFp);

	//记录参数
	memset(m_szLogBaseName, 0, sizeof(m_szLogBaseName));
	strncpy(m_szLogBaseName, pLogBaseName, sizeof(m_szLogBaseName)-1);
	sprintf(m_szLogFileName,"%s.log", m_szLogBaseName);
	m_iMaxLogNum = iMaxLogNum;
	m_iMaxLogSize = iMaxLogSize;	
	m_bAutoFlushDisk = bAutoFlushDisk;
	m_bEachWriteClose = bEachWriteClose;

	//初始计算m_unCurrLogSize,m_unTotalSize
	m_unCurrLogSize = 0;
	m_unTotalLogSize = 0;
	char szLogFileName[256];
	struct stat stStat;
	for (int32_t i=m_iMaxLogNum-1; i>=0; i--)
	{
		if (i == 0)	
			sprintf(szLogFileName,"%s.log", m_szLogBaseName);
		else		
			sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, i);

		if (access(szLogFileName, F_OK) != 0)
			continue;

		if(stat(szLogFileName, &stStat) < 0)
			continue;

		if(i == 0)
		{
			m_unCurrLogSize = stStat.st_size;
		}	
		
		m_unTotalLogSize += stStat.st_size;
	}

	printf("BinLog:: CurrLogSize %u,TotalLogSize %u\n",m_unCurrLogSize,m_unTotalLogSize);
	return 0;
}

int32_t CBinLog::ClearOldBinLog()
{
	char szLogFileName[256];
	for (int32_t i=m_iMaxLogNum-1; i>0; i--)
	{
        sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, i);
		
		if (access(szLogFileName, F_OK) != 0)
			continue;

		remove(szLogFileName);
		printf("clear binlog %s\n",szLogFileName);
	}

	m_unTotalLogSize = m_unCurrLogSize;
	return 0;
}

int32_t CBinLog::ClearAllBinLog()
{
	Flush();
	
	char szLogFileName[256];
	for (int32_t i=m_iMaxLogNum-1; i>=0; i--)
	{
        if (i == 0)
            sprintf(szLogFileName,"%s.log", m_szLogBaseName);
        else
            sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, i);
		
		if (access(szLogFileName, F_OK) != 0)
			continue;

		remove(szLogFileName);
	}

	FCLOSE(m_pWriteFp);
	FCLOSE(m_pReadFp);	

	m_unCurrLogSize = 0;
	m_unTotalLogSize = 0;
	return 0;
}

int32_t CBinLog::ReadRecordStart()
{
	FCLOSE(m_pReadFp);
	m_iReadLogIdx = m_iMaxLogNum;
	return 0;
}

//返回长度<0时结束,=0请忽略并继续
int32_t CBinLog::ReadRecordFromBinLog(char* pLogBuff, int32_t iMaxSize,
			u_int64_t &tLogTime,char* szLogFile/*=NULL*/)
{
	char szLogFileName[256];	
	int32_t iLogSize = 0;

	//定位下一个_read_fp
	if(!m_pReadFp)
	{
		//找下一个文件
		do
		{
			m_iReadLogIdx--;
			if (m_iReadLogIdx == 0)
				sprintf(szLogFileName,"%s.log", m_szLogBaseName);
			else
				sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, m_iReadLogIdx);
		}while((m_iReadLogIdx>=0) && (access(szLogFileName, F_OK)!=0));

		//读完了
		if (m_iReadLogIdx < 0)
			return -1;
		
		m_pReadFp = fopen(szLogFileName,"rb+");		
	}
	
	if (!m_pReadFp)
		return -2;

	if(szLogFile)
	{
		if (m_iReadLogIdx == 0)
			sprintf(szLogFile,"%s.log", m_szLogBaseName);
		else
			sprintf(szLogFile,"%s%d.log", m_szLogBaseName, m_iReadLogIdx);	
	}

	//只有read一次才知道feof...
	fread(&tLogTime,1,sizeof(u_int64_t),m_pReadFp);
	fread(&iLogSize,1,sizeof(int32_t),m_pReadFp);

	if (iLogSize > iMaxSize)
	{
		FCLOSE(m_pReadFp);
		return -5;
	}
	if (feof(m_pReadFp))
	{
		//本文件结束,继续下一个文件
		FCLOSE(m_pReadFp);
		return ReadRecordFromBinLog(pLogBuff,iMaxSize,tLogTime);
	}
	
	return fread(pLogBuff,1,iLogSize,m_pReadFp);
}

//return 0 success
int32_t CBinLog::WriteToBinLog(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime/*=0*/)
{
	if (!pLogBuff || iLogSize<=0)
		return -1;

	if(m_szMergeBuff)
	{
		if(AppendToMergeBuff(pLogBuff,iLogSize,tLogTime) == 0)
			return 0;
		
		FlushMergeBuff();
	}
	
	if (!m_pWriteFp || access(m_szLogFileName, F_OK))
	{	    
		FCLOSE(m_pWriteFp);
		
		m_pWriteFp = fopen(m_szLogFileName,"ab+");
		if (!m_pWriteFp)
		{
			printf("ERR:%s:: Can not open %s[%s:%d]\n",__FUNCTION__,m_szLogFileName,__FILE__,__LINE__);
			return -2;
		}
		m_unCurrLogSize = 0;
		
		struct stat stStat;
		if(stat(m_szLogFileName, &stStat)==0)
		{
			m_unCurrLogSize = stStat.st_size;
		}
	}

	u_int64_t tNow = tLogTime;
	if(tNow == 0)
		tNow = time(NULL);
	
	int32_t iItemSize = sizeof(u_int64_t)+sizeof(int32_t)+iLogSize;

	fwrite(&tNow,1,sizeof(u_int64_t),m_pWriteFp);
	fwrite(&iLogSize,1,sizeof(int32_t),m_pWriteFp);
	if(iLogSize != (int32_t)fwrite(pLogBuff,1,iLogSize,m_pWriteFp))
	{
		FCLOSE(m_pWriteFp);
		return -3;
	}

	if(m_bAutoFlushDisk)
		fflush(m_pWriteFp);

	m_unCurrLogSize += iItemSize;
	m_unTotalLogSize += iItemSize;

	if ( m_bEachWriteClose == true )
	{
		FCLOSE(m_pWriteFp);
	}
	
	ShiftFiles();
	return 0;
}

u_int32_t CBinLog::GetBinLogTotalSize()
{
	if ( m_bEachWriteClose == false )
		return m_unTotalLogSize;

	/* m_unTotalLogSize对于多进程来说，不准确，
	 * 需要从文件中获取真正的大小
	 */
	char szLogFileName[256];
	struct stat stStat;

	m_unTotalLogSize = 0;
	for (int32_t i=m_iMaxLogNum-1; i>=0; i--)
	{
		if (i == 0)	
			sprintf(szLogFileName,"%s.log", m_szLogBaseName);
		else		
			sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, i);

		if (access(szLogFileName, F_OK) != 0)
		{
			continue;
		}

		if(stat(szLogFileName, &stStat) < 0)
		{
			continue;
		}
		
		m_unTotalLogSize += stStat.st_size;
	}

	return m_unTotalLogSize;			
}

void CBinLog::Flush()
{
	FlushMergeBuff();
	
	if(m_pWriteFp)
		fflush(m_pWriteFp);

	ShiftFiles();	
}

int32_t CBinLog::AppendToMergeBuff(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime/*=0*/)
{
	int32_t iItemSize = sizeof(u_int64_t)+sizeof(int32_t)+iLogSize;
	if(m_iMergeLen + iItemSize >= MERGE_BUFF_SIZE)
		return -1;
	
	u_int64_t tNow = tLogTime;
	if(tNow == 0)
		tNow = time(NULL);
		
	memcpy(m_szMergeBuff+m_iMergeLen,&tNow,sizeof(u_int64_t));
	m_iMergeLen += sizeof(u_int64_t);
	memcpy(m_szMergeBuff+m_iMergeLen,&iLogSize,sizeof(int32_t));
	m_iMergeLen += sizeof(int32_t);
	memcpy(m_szMergeBuff+m_iMergeLen,pLogBuff,iLogSize);
	m_iMergeLen += iLogSize;

	return 0;
}

void CBinLog::FlushMergeBuff()
{
	if(m_iMergeLen == 0)
		return;
	
	if (!m_pWriteFp || access(m_szLogFileName, F_OK))
	{	    
		FCLOSE(m_pWriteFp);
		
		m_pWriteFp = fopen(m_szLogFileName,"ab+");
		if (!m_pWriteFp)
		{
			printf("ERR:%s:: Can not open %s[%s:%d]\n",__FUNCTION__,m_szLogFileName,__FILE__,__LINE__);
			return;
		}
		m_unCurrLogSize = 0;
		
		struct stat stStat;
		if(stat(m_szLogFileName, &stStat)==0)
		{
			m_unCurrLogSize = stStat.st_size;
		}
	}

	fwrite(m_szMergeBuff,1,m_iMergeLen,m_pWriteFp);

	m_unCurrLogSize += m_iMergeLen;
	m_unTotalLogSize += m_iMergeLen;
	m_iMergeLen = 0;	
	
	ShiftFiles();
}


int32_t CBinLog::ShiftFiles()
{
	if(!m_bEnableShift)
		return 0;
	
	if(m_unCurrLogSize < (u_int32_t)m_iMaxLogSize)
	{
		return 0;
	}
	
	struct stat stStat;
	char szLogFileName[256];
	char sNewLogFileName[256];

	if(stat(m_szLogFileName, &stStat) < 0)
	{
		FCLOSE(m_pWriteFp);
		return -1;
	}

	if (stStat.st_size < m_iMaxLogSize)
		return 0;

	FCLOSE(m_pWriteFp);	
	
	//last file delete
	sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, m_iMaxLogNum-1);
	if (access(szLogFileName, F_OK) == 0)
	{
		remove(szLogFileName);
	}

	//moving file
	for(int32_t i = m_iMaxLogNum-2; i >= 0; i--)
	{
		if (i == 0)
			sprintf(szLogFileName,"%s.log", m_szLogBaseName);
		else
			sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, i);

		if (access(szLogFileName, F_OK) == 0)
		{
			sprintf(sNewLogFileName,"%s%d.log", m_szLogBaseName, i+1);
			if (rename(szLogFileName,sNewLogFileName) < 0 )
				return -1;
		}
	}
	return 0;
}

/*
#include <assert.h>
int32_t main()
{
	CBinLog stBinLog;
	stBinLog.Init("binlog_",1000,500);
	stBinLog.OpenMerge();
	
char szbuff[1024];
char szbuff2[1024];
	for (int32_t i=0; i<3000;i++)
	{
		sprintf(szbuff,"aaaaaaa");	
		stBinLog.WriteToBinLog(szbuff,strlen(szbuff));
	}
	stBinLog.Flush();
	
	u_int64_t tLogtime;
	int32_t iloglen=  0;
	char szlogFile[1024];
	stBinLog.ReadRecordStart();
	int32_t i=0;
	while((iloglen =stBinLog.ReadRecordFromBinLog(szbuff,sizeof(szbuff),tLogtime,szlogFile))>0)
	{
		szbuff[iloglen] = 0;
		
		sprintf(szbuff2,"aaaaaaa");	
		//szbuff2[10] = 0;
		
		if(0 != strcmp(szbuff,szbuff2))
		{
			printf("error!! %d\n",i);
			assert(0);
			return 0;
		}
		//printf("%s:%s\n",szlogFile,szbuff);
	}

	printf(",currsize %d,total size=%d\n",stBinLog.m_unCurrLogSize,stBinLog.GetBinLogTotalSize());
}
*/

