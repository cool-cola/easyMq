#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "BinlogNR.hpp"

#define FCLOSE(pFP)	{\
if(pFP) fclose(pFP);\
pFP = NULL;\
}

CBinLog::CBinLog()
{
	m_iMaxLogSize = 50000000;
	strcpy(m_szLogBaseName,"binlog_");

	m_ullTotalLogSize = 0;	
	m_unCurrLogSize = 0;
	
	m_pWriteFp = NULL;
	m_pReadFp = NULL;
	m_iReadLogIdx = 0;
	m_iWriteLogIdx = 0;

	m_szMergeBuff = NULL;
	m_iMergeLen = 0;

	m_b_use_o_direct = false;
	m_b_cur_use_o_direct = false;

	MERGE_BUFF_SIZE = 0;

	m_bEnableShift = true;
}

CBinLog::~CBinLog()
{
	CloseWriteFP();
	FCLOSE(m_pReadFp);	

	if (m_szMergeBuff)
	{
		free(m_szMergeBuff);
	}
	m_szMergeBuff = NULL;
}

int32_t CBinLog::Init(char *pLogBaseName/*="binlog_"*/,int32_t iMaxLogSize/*=50000000*/,bool bAutoFlushDisk/*=false*/)
{
	CloseWriteFP();
	FCLOSE(m_pReadFp);

	//记录参数
	memset(m_szLogBaseName, 0, sizeof(m_szLogBaseName));
	strncpy(m_szLogBaseName, pLogBaseName, sizeof(m_szLogBaseName)-1);
	m_iWriteLogIdx = 1;
	m_iMaxLogSize = iMaxLogSize;	
	m_bAutoFlushDisk = bAutoFlushDisk;

	//初始计算m_unCurrLogSize,m_unTotalSize 找最新的log
	m_unCurrLogSize = 0;
	m_ullTotalLogSize = 0;
	struct stat stStat;
	char szLogFileName[256];
	for (int32_t i=1;; i++)
	{		
		sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, i);

		if (access(szLogFileName, F_OK) != 0)
			break;

		if(stat(szLogFileName, &stStat) < 0)
			break;

		m_unCurrLogSize = stStat.st_size;	
		m_ullTotalLogSize += (u_int64_t)stStat.st_size;

		printf("CBinLog:: %s %d bytes\n",szLogFileName,m_unCurrLogSize);
		
		m_iWriteLogIdx = i;
	}
	sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, m_iWriteLogIdx);
	if (access(szLogFileName, F_OK) == 0)
	{
		if (stat(szLogFileName, &stStat)==0)
		{
			if (stStat.st_size > 0)
			{
				//为了保证数据的完整性，不接着上次文件写
				m_iWriteLogIdx++;  
			}
		}
	}

	printf("CBinLog:: CurrLog %d,TotalLogSize %llu\n",m_iWriteLogIdx,(unsigned long long)m_ullTotalLogSize);
	return 0;
}

void CBinLog::OpenMerge(int iMergeBuffSize, bool b_use_o_direct/* = true*/)
{
	if (b_use_o_direct && iMergeBuffSize < 4096)
	{
		printf("merge buff size do not support O_DIRECT!\n");
		return;
	}

	printf("CBinLog OpenMerge iMergeBuffSize=%d,b_use_o_direct=%d\n",iMergeBuffSize,b_use_o_direct);
	
	if (m_szMergeBuff)
		free(m_szMergeBuff);

	if (iMergeBuffSize <= 0)
	{
		m_b_use_o_direct = false;
		MERGE_BUFF_SIZE = 0;
	}
	m_szMergeBuff = NULL;
	m_iMergeLen = 0;
	m_b_use_o_direct = b_use_o_direct;

	
	if(b_use_o_direct)
	{
		int i_page_size = getpagesize();
		printf("page size=%d\n", i_page_size);
		if (i_page_size <= 0)
		{
			printf("page size error!\n");
			MERGE_BUFF_SIZE = 0;
			m_b_use_o_direct = false;
			return;
		}
		int i_page_num = iMergeBuffSize / i_page_size;
		MERGE_BUFF_SIZE = i_page_num * i_page_size;
		int iErrNo = posix_memalign((void**)&m_szMergeBuff, i_page_size, MERGE_BUFF_SIZE);
		if (0 != iErrNo)
		{
		    perror("posix_memalign error");
			MERGE_BUFF_SIZE = 0;
			m_b_use_o_direct = false;
		    return;
		}
	}
	else
	{
		m_szMergeBuff = (char*)malloc((size_t)iMergeBuffSize);
		if (m_szMergeBuff)
		{
			MERGE_BUFF_SIZE = iMergeBuffSize; 
		}
	}
	return;
}
	
int32_t CBinLog::ClearAllBinLog()
{
	CloseWriteFP();
	FCLOSE(m_pReadFp);
	
	char szLogFileName[256];
	char szLogFileNameDirty[256];
	for(int i=1;;i++)
	{
       	snprintf(szLogFileName, sizeof(szLogFileName), "%s%d.log", m_szLogBaseName, i);
		
		if (access(szLogFileName, F_OK) != 0)
		{
			if(i<512)
				continue;
			else
				break;
		}	
		snprintf(szLogFileNameDirty, sizeof(szLogFileNameDirty), "%s.todel", szLogFileName);
		rename(szLogFileName, szLogFileNameDirty);
	}

	m_unCurrLogSize = 0;
	m_ullTotalLogSize = 0;

	m_iWriteLogIdx = 1;	
	char szCmd[512];
	snprintf(szCmd, sizeof(szCmd), "rm -f %s*.todel &", m_szLogBaseName);
	system(szCmd);
	return 0;
}
/*
清理掉除当前文件外的其它老的文件
*/

int32_t CBinLog::ClearOldBinLog()
{
	if(m_iWriteLogIdx <= 1)
		return 0;
	
	int iOldWriteIdx = m_iWriteLogIdx;

	//close file
	CloseWriteFP();
	FCLOSE(m_pReadFp);

	//delete old file
	char szLogFileName[256];
	for(int i=1;i<iOldWriteIdx;i++)
	{
       	sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, i);
		
		if (access(szLogFileName, F_OK) != 0)
		{
			if(i<512)
				continue;
			else
				break;
		}	

		remove(szLogFileName);
	}

	//mv current file
	char szCurrLogFileName[256];
	sprintf(szCurrLogFileName,"%s%d.log", m_szLogBaseName,iOldWriteIdx);
	
	sprintf(szLogFileName,"%s1.log", m_szLogBaseName);
	rename(szCurrLogFileName,szLogFileName);

	m_unCurrLogSize = 0;
	m_ullTotalLogSize = 0;

	struct stat stStat;
	if(stat(szLogFileName, &stStat)==0)
	{
		m_ullTotalLogSize = stStat.st_size;
	}

	//use next file
	m_iWriteLogIdx = 2;	
	return 0;
}


int32_t CBinLog::ReadRecordStart()
{
	FCLOSE(m_pReadFp);
	m_iReadLogIdx = 0;
	return 0;
}
//返回长度<0时结束
int32_t CBinLog::ReadRecordFromBinLog(char* pLogBuff, int32_t iMaxSize,
			u_int64_t &tLogTime,char* szLogFile/*=NULL*/)
{
	char szLogFileName[256];	
	int32_t iLogSize = 0;
	char tmp[128];
	int time_record_size = sizeof(u_int64_t) + sizeof(int32_t);

	int ret = 0;
	while (1)
	{
		if(!m_pReadFp)
		{
			//找下一个文件
			m_iReadLogIdx++;
			sprintf(szLogFileName,"%s%d.log", m_szLogBaseName, m_iReadLogIdx);
			if(access(szLogFileName, F_OK) != 0)  //文件不存退出循环
				return -1;

			m_pReadFp = fopen(szLogFileName,"rb+");	
		}
		if (!m_pReadFp)
			return -2;
		
		ret = fread(tmp, 1, time_record_size, m_pReadFp);
		if (feof(m_pReadFp) || ret != time_record_size)
		{
			//本文件结束,或者出错，继续下一个文件
			FCLOSE(m_pReadFp);
			continue;
		}
		memcpy(&tLogTime, tmp, sizeof(u_int64_t));
		memcpy(&iLogSize, tmp + sizeof(u_int64_t), sizeof(int32_t));
		if (iLogSize > iMaxSize)
		{
			FCLOSE(m_pReadFp);
			return -3;
		}
		ret = fread(pLogBuff, 1, iLogSize, m_pReadFp);
		if (ret != iLogSize)
		{
			//本文件结束,或者出错，继续下一个文件
			FCLOSE(m_pReadFp);
			continue;
		}
		else
		{
			if(szLogFile)
			{
				strncpy(szLogFile, szLogFileName, sizeof(szLogFile));
			}
			break;
		}
	}
	return iLogSize;
}


int32_t CBinLog::WriteToBinLog(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime/*=0*/)
{
	if (!pLogBuff || iLogSize<=0)
		return -1;

	if (!m_pWriteFp)
	{
		if (OpenWriteFP())
			return -1;
	}
	if(MERGE_BUFF_SIZE > 0) //如果合并写打开,则启用合并写功能
	{
		return AppendToMergeBuff(pLogBuff,iLogSize,tLogTime);
	}
	else
	{
		u_int64_t tNow = tLogTime;
		if(tNow == 0)
			tNow = time(NULL);
		
		int32_t iItemSize = sizeof(u_int64_t)+sizeof(int32_t)+iLogSize;

		if(sizeof(u_int64_t) != fwrite(&tNow,1,sizeof(u_int64_t),m_pWriteFp))
		{
			CloseWriteFP();
			snprintf(strErr, sizeof(strErr), "err:WriteToBinLog  fwrite time failed %s\n", strerror(errno)); 
			return -2;			
		}
		if(sizeof(int32_t) != fwrite(&iLogSize,1,sizeof(int32_t),m_pWriteFp))
		{
			CloseWriteFP();
			snprintf(strErr, sizeof(strErr), "err:WriteToBinLog  fwrite size failed %s\n", strerror(errno)); 
			return -3;			
		}
		if(iLogSize != (int32_t)fwrite(pLogBuff,1,iLogSize,m_pWriteFp))
		{
			CloseWriteFP();
			snprintf(strErr, sizeof(strErr), "err:WriteToBinLog  fwrite data failed %s\n", strerror(errno)); 
			return -4;
		}

		if(m_bAutoFlushDisk)
			fflush(m_pWriteFp);

		m_unCurrLogSize += iItemSize;
		m_ullTotalLogSize += (u_int64_t)iItemSize;

		//next
		if((m_unCurrLogSize >= (u_int32_t)m_iMaxLogSize) && m_bEnableShift)
		{
			return CloseWriteFP();
		}
		return 0;
	}
	return 0;
}

u_int64_t CBinLog::GetBinLogTotalSize()
{
	return m_ullTotalLogSize;			
}

int CBinLog::Flush(bool bFlushAndCloseODirectFile/* = false*/)
{
	if (m_b_cur_use_o_direct) //当前使用O_DIRECT方式，不用flush
	{
		if(bFlushAndCloseODirectFile)
		{
			return CloseWriteFP();
		}
		else
			return 0;
	}	
	if(m_iMergeLen > 0)
	{
		if (!m_pWriteFp)
		{	
			snprintf(strErr, sizeof(strErr), "err:CBinLog::Flush already closed,no need flush\n"); 
			return -1;
		}

		if(m_iMergeLen != (int32_t)fwrite(m_szMergeBuff, 1, m_iMergeLen, m_pWriteFp))
		{
			CloseWriteFP();
			snprintf(strErr, sizeof(strErr), "err:CBinLog::Flush  fwrite flushdata failed %s\n", strerror(errno)); 
			return -1;
		}

		fflush(m_pWriteFp);

		//next
		m_unCurrLogSize += m_iMergeLen;
		m_ullTotalLogSize += m_iMergeLen;
		m_iMergeLen = 0;
	}
	if((m_unCurrLogSize >= (u_int32_t)m_iMaxLogSize) && m_bEnableShift)
	{
		return CloseWriteFP();
	}	
	return 0;
}

int CBinLog::OpenWriteFP()
{
	if (m_pWriteFp)
	{
		return 0;
	}
	sprintf(m_szLogFileName,"%s%d.log", m_szLogBaseName,m_iWriteLogIdx);
	m_pWriteFp = fopen(m_szLogFileName,"ab+");
	if (!m_pWriteFp)
	{
		snprintf(strErr, sizeof(strErr), "ERR:%s:: Can not open %s error %s[%s:%d]\n",__FUNCTION__,m_szLogFileName, strerror(errno), __FILE__,__LINE__);
		return -1;
	}

	struct stat stStat;
	m_b_cur_use_o_direct = m_b_use_o_direct;
	if(stat(m_szLogFileName, &stStat)==0)
	{
		m_unCurrLogSize = stStat.st_size;
		if (m_unCurrLogSize > 0)
		{
			m_b_cur_use_o_direct = false;
		}
	}
	else
	{
		m_unCurrLogSize = 0;
	}
	if (m_b_cur_use_o_direct)
	{
		int fd = fileno(m_pWriteFp);
		int mod = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, mod | O_DIRECT);
	}
	return 0;

}
int CBinLog::CloseWriteFP()
{
	int flusherr = 0;
	if (!m_pWriteFp)
	{
		return 0;
	}
	if (m_iMergeLen > 0)
	{
		if (m_b_cur_use_o_direct)
		{
			int fd = fileno(m_pWriteFp);
			int mod = fcntl(fd, F_GETFL, 0);
			fcntl(fd, F_SETFL, mod & ~O_DIRECT);
		}
		if(m_iMergeLen != (int32_t)fwrite(m_szMergeBuff,1,m_iMergeLen,m_pWriteFp))
		{
			snprintf(strErr, sizeof(strErr), "err:CloseWriteFP fwrite failed %s\n", strerror(errno)); 
			flusherr = -1;
		}
		m_unCurrLogSize += m_iMergeLen;
		m_ullTotalLogSize += (u_int64_t)m_iMergeLen;
		m_iMergeLen = 0;
	}

	
	FCLOSE(m_pWriteFp);
	//binlog必须存在且不为空才写入新的文件
	char szLogFileName[512];
	snprintf(szLogFileName,sizeof(szLogFileName), "%s%d.log", m_szLogBaseName, m_iWriteLogIdx);
	if (access(szLogFileName, F_OK) == 0)
	{
		struct stat stStat;
		if (stat(szLogFileName, &stStat)==0)
		{
			if (stStat.st_size > 0)
			{
				//为了保证数据的完整性，不接着上次文件写
				m_iWriteLogIdx++;
			}
		}
	}
	
	return flusherr;
}

int32_t CBinLog::_AppendBufToMergeBuf(char* p_buf, int32_t i_buf_len)
{
	int i_ret = 0;
	int i_left_merge_buf_len = MERGE_BUFF_SIZE - m_iMergeLen;
	int i_cur_write_buf_len = 0;
	int i_will_write_len = 0;
	
	while (i_cur_write_buf_len < i_buf_len)
	{
		i_left_merge_buf_len = MERGE_BUFF_SIZE - m_iMergeLen;
		i_will_write_len = (i_left_merge_buf_len < i_buf_len - i_cur_write_buf_len) ? 
			i_left_merge_buf_len : (i_buf_len - i_cur_write_buf_len);
		memcpy(m_szMergeBuff + m_iMergeLen, p_buf + i_cur_write_buf_len, i_will_write_len);
		
		i_cur_write_buf_len += i_will_write_len;
		m_iMergeLen += i_will_write_len;
		i_left_merge_buf_len = MERGE_BUFF_SIZE - m_iMergeLen;
		if (i_left_merge_buf_len <= 0)
		{
			//如果出现写失败，不能继续在写，尽量保证binlog的完整性
			i_ret = FlushMergeBuff(); 
			if(i_ret)
			{
				CloseWriteFP();
				return i_ret;
			}
		} 
	} 	 
	return 0;
}
int32_t CBinLog::AppendToMergeBuff(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime/*=0*/)
{
	int i_ret = 0;
	u_int64_t tNow = tLogTime;
	if(tNow == 0)
		tNow = time(NULL);
	i_ret = _AppendBufToMergeBuf((char*)&tNow, sizeof(u_int64_t));
	if(i_ret)
	{		
		return i_ret;
	}	
	i_ret = _AppendBufToMergeBuf((char*)&iLogSize, sizeof(int32_t));
	if(i_ret)
	{
		return i_ret;
	}
	i_ret = _AppendBufToMergeBuf(pLogBuff, iLogSize);
	if(i_ret)
	{
		return i_ret;
	}
	if ((m_unCurrLogSize > (u_int32_t)m_iMaxLogSize) && m_bEnableShift)
	{
		return CloseWriteFP();
	}

	return 0;
}

int CBinLog::FlushMergeBuff()
{
	if(m_iMergeLen == 0)
		return 0;
	
	if (!m_pWriteFp)
	{		
		snprintf(strErr, sizeof(strErr), "err:FlushMergeBuff %d file not opened!\n", m_iWriteLogIdx);
		return -1;
	}
	if(m_iMergeLen != (int32_t)fwrite(m_szMergeBuff,1,m_iMergeLen,m_pWriteFp))
	{
		snprintf(strErr, sizeof(strErr), "err:FlushMergeBuff fwrite failed %s\n", strerror(errno)); 
		return -1;
	}
	
	m_unCurrLogSize += m_iMergeLen;
	m_ullTotalLogSize += (u_int64_t)m_iMergeLen;
	m_iMergeLen = 0;	
	return 0;
}
char* CBinLog::GetLastErr()
{
	return strErr;
}

/*
#include <assert.h>
#include <math.h>
#include <vector>
using namespace std;
int32_t main()
{
	srand(time(0));
	for(int n=0;n<100000;n++)
	{
		printf("begin...\n");
		int size = rand()%(1*1024*1024);
		if(size < 100)
			size  =1000;

		CBinLog *pstBinLog= new CBinLog();
		pstBinLog->Init("binlog_",size);
		pstBinLog->OpenMerge(16*1024);

		pstBinLog->ClearAllBinLog();

		char szbuff[60*1024];
		memset(szbuff,'a',60*1024);
		vector<int> vec_len;
	
		for (int32_t i=0; i<1000;i++)
		{
			int len = rand()%(50*1024);
			if(len == 0) len = 1;
			
			pstBinLog->WriteToBinLog(szbuff,len);
			vec_len.push_back(len);

			if(i == 500)
				pstBinLog->EnableFileShift(false);
		}
		sleep(10);
		pstBinLog->ClearOldBinLog();
		delete pstBinLog;
		//pstBinLog->Flush(true);
		printf("write ok\n");
		return 0;

	//-------------------------
		CBinLog *pstBinLog2= new CBinLog();
		pstBinLog2->Init("binlog_",size);
		pstBinLog2->OpenMerge(16*1024);

		
		return 0;

		char szbuff2[60*1024];
		memset(szbuff2,'0',50*1024);
		
		u_int64_t tLogtime;
		int32_t iloglen=  0;
		char szlogFile[1024];
		pstBinLog2->ReadRecordStart();
		int32_t i=0;
		int cnt = 0;
		while((iloglen = pstBinLog2->ReadRecordFromBinLog(szbuff2,sizeof(szbuff2),tLogtime,szlogFile))>=0)
		{
			szbuff2[iloglen] = 0;

			//printf("%lld %s\n",tLogtime,szbuff);

			if(iloglen !=  vec_len[cnt])
			{
				printf("record  %d len bad! read %d != vec %d\n",cnt,iloglen,vec_len[cnt]);
				return -1;
			}

			if(memcmp(szbuff,szbuff2,iloglen) != 0)
			{
				printf("record %d data  bad!\n",cnt);
				return -1;
			}	

			memset(szbuff2,'0',50*1024);
			
			cnt++;
			continue;
		}

		if(cnt != vec_len.size())
		{
			printf("record count  bad!\n");
			return -1;	
		}
		
		printf("cnt %d\n",cnt);
		printf("read ok\n");
	}
}
*/

