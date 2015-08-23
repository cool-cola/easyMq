#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "Base.hpp"
#include "DumpBin.hpp"
#include <assert.h>

//////////////////////////////////////////////////////////////////////////////
CCoreDump::CCoreDump()
{
	m_MergeBuf = NULL;
	m_MergeBufSize = MERGE_BUFSIZE;
	m_iMergeLen = 0;
	m_FileLen = 0;
	memset(strErr, 0, sizeof(strErr));
	m_iMagic = 0;
}

CCoreDump::~CCoreDump()
{
	if(m_MergeBuf)
		delete [] m_MergeBuf;
	m_MergeBuf = NULL;
}
char* CCoreDump::GetLastErr()
{
	return strErr;
}
int CCoreDump::Init(const char* pFileName, int nGroup, int iMergeBufSize, int iDumpMagic)
{
	int iRet = SetFile(pFileName, nGroup, iDumpMagic);
	if (iRet)
	{
		return iRet;
	}
	if (m_MergeBuf)
	{
		delete []m_MergeBuf;
		m_MergeBuf = NULL;
	}
	m_MergeBufSize = iMergeBufSize;
	m_MergeBuf = new char[m_MergeBufSize + RESERVED_SPACE];
	return 0;
}

int CCoreDump::SetFile(const char* pFileName, int nGroup, int iDumpMagic)
{
	int fd;
	
	m_iMergeLen = 0;
	m_FileLen = 0;
	memset(strErr, 0, sizeof(strErr));
	m_iMagic = iDumpMagic;
	
	if (!access(pFileName, F_OK))
	{
		fd = open(pFileName, O_RDONLY);
		if (fd < 0)
		{
			snprintf(strErr, sizeof(strErr), "err:open:%s %s", pFileName, strerror(errno));
			return -1;
		}

		if (read(fd, &m_DumpHead, sizeof(m_DumpHead)) != sizeof(m_DumpHead))
		{
			snprintf(strErr, sizeof(strErr), "err:read:%s %s", pFileName, strerror(errno));
			close(fd);
			return -1;
		}
		
		/*check file info*/
		if (m_DumpHead.magic != (unsigned int)m_iMagic)
		{
			snprintf(strErr, sizeof(strErr), "err:file %s magic error!", pFileName);
			close(fd);
			return -1;
		}
		
		close(fd);
	}
	else
	{
		/*if not exist, create it!*/
		fd = open(pFileName, O_WRONLY | O_CREAT, FILE_MODE);
		if (fd < 0)
		{
			snprintf(strErr, sizeof(strErr), "err:open:%s %s", pFileName, strerror(errno));
			return -1;
		}

		memset(&m_DumpHead, 0x0, sizeof(CCoreDumpHead));
		m_DumpHead.magic = m_iMagic;
		m_DumpHead.version = COREDUMP_VERSION;
		m_DumpHead.flag = COREDUMP_FULL;
		m_DumpHead.timestamp = nGroup;
		m_DumpHead.total_size = sizeof(CCoreDumpHead);

		if (write(fd, &m_DumpHead, sizeof(m_DumpHead)) != sizeof(m_DumpHead))
		{
			snprintf(strErr, sizeof(strErr), "err:write:%s %s", pFileName, strerror(errno));
			close(fd);
			return -1;
		}

		close(fd);
	}
	m_FileLen = m_DumpHead.total_size;
	memset(m_FileName, 0x0, sizeof(m_FileName));
	strcpy(m_FileName, pFileName);

	return 0;
}


ssize_t CCoreDump::GetTotalSize()
{
	return m_DumpHead.total_size;
}

ssize_t CCoreDump::GetFileSize()
{
	return m_FileLen;
}


unsigned int CCoreDump::GetFlag()
{
	return m_DumpHead.flag;
}

unsigned int CCoreDump::GetTimestamp()
{
	return m_DumpHead.timestamp;
}

int CCoreDump::UpdateTimestamp()
{
	int fd;
	
	fd = open(m_FileName, O_RDWR);
	if (fd < 0)
	{
		snprintf(strErr, sizeof(strErr), "err:open:%s %s", m_FileName, strerror(errno));
		return -1;
	}

	/*note: the file pointer is at the end of the file*/
	lseek(fd, 0, SEEK_SET);

	if (read(fd, &m_DumpHead, sizeof(m_DumpHead)) != sizeof(m_DumpHead))
	{
		snprintf(strErr, sizeof(strErr), "err:read:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	/*move the file pointer to the head of the file*/
	lseek(fd, 0, SEEK_SET);

	m_DumpHead.magic = m_iMagic;
	m_DumpHead.version = COREDUMP_VERSION;
	m_DumpHead.flag = COREDUMP_FULL;
	m_DumpHead.timestamp = time(NULL);

	if (write(fd, &m_DumpHead, sizeof(m_DumpHead)) != sizeof(m_DumpHead))
	{	
		snprintf(strErr, sizeof(strErr), "err:write:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

int CCoreDump::WriteStart(int iClearDirty)
{
	if(iClearDirty)
	{
		char m_FileDirty[256];
		snprintf(m_FileDirty, sizeof(m_FileDirty), "%s.todel", m_FileName);
		rename(m_FileName, m_FileDirty);
		char szCmd[512];
		snprintf(szCmd, sizeof(szCmd), "rm -f %s &", m_FileDirty);
		system(szCmd);
	}
	int fd;
	fd = open(m_FileName, O_RDWR | O_CREAT | O_TRUNC, FILE_MODE);
	if (fd < 0)
	{
		snprintf(strErr, sizeof(strErr), "err:open:%s %s", m_FileName, strerror(errno));
		return -1;
	}

	/*set complete flag*/
	memset(&m_DumpHead, 0x0, sizeof(CCoreDumpHead));
	m_DumpHead.magic = m_iMagic;
	m_DumpHead.version = COREDUMP_VERSION;
	m_DumpHead.flag = COREDUMP_PARTIAL;
	m_DumpHead.timestamp = time(NULL);
	m_DumpHead.total_size = sizeof(CCoreDumpHead);

	/*for key length*/
	if (write(fd, &m_DumpHead, sizeof(m_DumpHead)) != sizeof(m_DumpHead))
	{
		snprintf(strErr, sizeof(strErr), "err:write:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	if (close(fd)) {
		snprintf(strErr, sizeof(strErr), "close err:%s %s", m_FileName, strerror(errno));
		return -6;	
	}

	/*set for initialize length*/
	m_FileLen = sizeof(CCoreDumpHead);
	return 0;
}
/*
 * format:
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * + total length + key length + key + data length + data + ext length + ext data +
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
void CCoreDump::AppendToMerge(char* Key, int KeyLen, char* Data, int DataLen, char* extData, int extDataLen)
{
	int total = KeyLen + DataLen + extDataLen + 12; /*sizeof(int) * 3*/
	memcpy(m_MergeBuf + m_iMergeLen, &total, sizeof(total));
	m_iMergeLen += sizeof(total);
	memcpy(m_MergeBuf + m_iMergeLen, &KeyLen, sizeof(KeyLen));
	m_iMergeLen += sizeof(KeyLen);
	memcpy(m_MergeBuf + m_iMergeLen, Key, KeyLen);
	m_iMergeLen += KeyLen;
	memcpy(m_MergeBuf + m_iMergeLen, &DataLen, sizeof(DataLen));
	m_iMergeLen += sizeof(DataLen);
	memcpy(m_MergeBuf + m_iMergeLen, Data, DataLen);
	m_iMergeLen += DataLen;
	memcpy(m_MergeBuf + m_iMergeLen, &extDataLen, sizeof(extDataLen));
	m_iMergeLen += sizeof(extDataLen);
	memcpy(m_MergeBuf + m_iMergeLen, extData, extDataLen);
	m_iMergeLen += extDataLen;
}
int CCoreDump::FlushMerge()
{
	if(!m_MergeBuf || m_iMergeLen <= 0)
		return 0;
	int fd;
	fd = open(m_FileName, O_RDWR);
	if (fd < 0)
	{
		snprintf(strErr, sizeof(strErr), "err:open:%s %s", m_FileName, strerror(errno));
		return -1;
	}
	off_t pos = lseek(fd, 0L, SEEK_END);
	if (pos < 0)
	{
		snprintf(strErr, sizeof(strErr), "err:lseek:%s %s", m_FileName, strerror(errno));
		return -1;
	}
	if (write(fd, m_MergeBuf, m_iMergeLen) != m_iMergeLen)
	{
		snprintf(strErr, sizeof(strErr), "err:write:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}
	if (close(fd)) {
		snprintf(strErr, sizeof(strErr), "close err:%s %s", m_FileName, strerror(errno));
		return -6;	
	}
	
	m_FileLen += m_iMergeLen;
	m_iMergeLen = 0;
	return 0;
}

/*
 * format:
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * + total length + key length + key + data length + data + ext length + ext data +
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
int CCoreDump::WriteCoreDump(char* Key, int KeyLen, char* Data, int DataLen, char* extData, int extDataLen)
{
	
	int total = KeyLen + DataLen + extDataLen + 12; /*sizeof(int) * 3*/

	if (!Key || !KeyLen)
	{
		snprintf(strErr, sizeof(strErr), "err:invalid param!");
		return -1;
	}
	if(m_MergeBuf)
	{
		//实际mergeBuf大小为m_MergeBufSize+RESERVED_SPACE,防止内存被踩
		if(m_MergeBufSize - m_iMergeLen > total)
		{
			AppendToMerge(Key, KeyLen, Data, DataLen, extData, extDataLen);
			return 0;
		}
		else//空间不够清掉merge,写回磁盘
		{
			int ret = FlushMerge();
			if(ret < 0)
			{
				return -1;
			}
			if(m_MergeBufSize - m_iMergeLen > total + 4)
			{
				AppendToMerge(Key, KeyLen, Data, DataLen, extData, extDataLen);
				return 0;	
			}
		}

	}
	//merge buf太小或无mergeBuf
	int fd;
	fd = open(m_FileName, O_RDWR);
	if (fd < 0)
	{
		snprintf(strErr, sizeof(strErr), "err:open:%s %s", m_FileName, strerror(errno));
		return -1;
	}
	//  not use open( O_APPEND), because cfs does not support yet
	off_t pos = lseek(fd, 0L, SEEK_END);
	if (pos < 0)
	{
		snprintf(strErr, sizeof(strErr), "err:lseek:%s %s", m_FileName, strerror(errno));
		return -1;
	}

	/*for total length*/
	if (write(fd, &total, sizeof(total)) != sizeof(total))
	{
		snprintf(strErr, sizeof(strErr), "err:write total length:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	/*for key length*/
	if (write(fd, &KeyLen, sizeof(KeyLen)) != sizeof(KeyLen))
	{
		snprintf(strErr, sizeof(strErr), "err:write key length:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	/*for key*/
	if (write(fd, Key, KeyLen) != KeyLen)
	{
		snprintf(strErr, sizeof(strErr), "err:write key:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	/*for data length*/
	if (write(fd, &DataLen, sizeof(DataLen)) != sizeof(DataLen))
	{
		snprintf(strErr, sizeof(strErr), "err:write data length:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	/*for data*/
	if (DataLen && write(fd, Data, DataLen) != DataLen)
	{
		snprintf(strErr, sizeof(strErr), "err:write data:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	/*for ext data length*/
	if (write(fd, &extDataLen, sizeof(extDataLen)) != sizeof(extDataLen))
	{
		snprintf(strErr, sizeof(strErr), "err:write ext data length:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	/*for ext data*/
	if (extDataLen && write(fd, extData, extDataLen) != extDataLen)
	{
		snprintf(strErr, sizeof(strErr), "err:write ext data:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -1;
	}

	if (close(fd)) {
		snprintf(strErr, sizeof(strErr), "close err:%s %s", m_FileName, strerror(errno));
		return -6;	
	}
	
	/*calculate for total length of coredump file*/
	m_FileLen += sizeof(int);
	m_FileLen += sizeof(int);
	m_FileLen += KeyLen;
	m_FileLen += sizeof(int);
	m_FileLen += DataLen;
	m_FileLen += sizeof(int);
	m_FileLen += extDataLen;
	return 0;
}

int CCoreDump::WriteComplete()
{
	//先刷新merge buf
	int ret = FlushMerge();
	if(ret < 0)
	{
		return -1;
	}
	
	struct stat stStat;
	if(stat(m_FileName, &stStat) < 0)	
	{
		snprintf(strErr, sizeof(strErr), "err:stat:%s %s", m_FileName, strerror(errno));
		return -2;		
	}
	if(stStat.st_size != m_FileLen)
	{
		snprintf(strErr, sizeof(strErr), "err:dumpsize:%s stat size %u cal size %u", 
			m_FileName, (unsigned int)stStat.st_size, m_FileLen);
		return -3;		
	}	
	int fd;
	fd = open(m_FileName, O_RDWR);
	if (fd < 0)
	{
		snprintf(strErr, sizeof(strErr), "err:open:%s %s", m_FileName, strerror(errno));
		return -4;
	}

	/*set complete flag*/
	memset(&m_DumpHead, 0x0, sizeof(CCoreDumpHead));
	m_DumpHead.magic = m_iMagic;
	m_DumpHead.version = COREDUMP_VERSION;
	m_DumpHead.flag = COREDUMP_FULL;
	m_DumpHead.timestamp = time(NULL);
	m_DumpHead.total_size = m_FileLen;

	/*for key length*/
	if (write(fd, &m_DumpHead, sizeof(m_DumpHead)) != sizeof(m_DumpHead))
	{
		snprintf(strErr, sizeof(strErr), "err:write dumphead:%s %s", m_FileName, strerror(errno));
		close(fd);
		return -5;
	}

	if (close(fd)) {
		snprintf(strErr, sizeof(strErr), "close err:%s %s", m_FileName, strerror(errno));
		return -6;	
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////
CDumpBin::CDumpBin()
{
	m_pBinLog = NULL;
	m_pCoreDump = NULL;
	m_bInit = false;
	m_bNeedDump = false;
	m_CurIndex = -1;	
	m_iLastTime = time(NULL);
	m_iFlushInterval = FLUSH_DEFAULT;
	memset(strErr, 0, sizeof(strErr));
}

CDumpBin::~CDumpBin()
{
	if (m_pBinLog)
	{
		m_pBinLog->Flush();
		m_pBinLog = NULL;
	}

	if (m_pCoreDump)
		m_pCoreDump = NULL;

	m_bInit = false;	
}
char* CDumpBin::GetLastErr()
{
	return strErr;
}
int CDumpBin::Initialize(char* pBaseDir, int iCacheID, int32_t iMaxLogSize, int32_t iMaxLogNum, 
	bool bAutoFlush, int32_t iMergeBufSize, int iUseODirect/*=1*/)
{
	char cachedir[256];
	char filename[256];
	int iLoop;
	assert(iMaxLogSize > 0);
	/*create cache dir first, we assume %pBaseDir exist*/
	memset(cachedir, 0x0, sizeof(cachedir));
	sprintf(cachedir, "%s/cache%d", pBaseDir, iCacheID);
	if (mkdir(cachedir, DIR_MODE) != 0 && errno != EEXIST)
	{
		snprintf(strErr, sizeof(strErr), "err:mkdir:%s %s", cachedir, strerror(errno));
		return -1;
	}

	/*initialize binlog*/
	for (iLoop = 0; iLoop < 2; iLoop++)
	{
		memset(filename, 0x0, sizeof(filename));
		sprintf(filename, "%s/binlog_%c", cachedir, iLoop + 'a');
		printf("initialize binlog file %s, max log size %d.\n", filename, iMaxLogSize);
		
//#ifndef USE_BINLOG_NR
//		m_BinLog[iLoop].Init(filename, iMaxLogSize, iMaxLogNum, bAutoFlush, false);
/*open merge buffer?*/
//		if (iMergeBufSize)
//			m_BinLog[iLoop].OpenMerge(iMergeBufSize, iUseODirect? true : false);
//#else
		m_BinLog[iLoop].Init(filename, iMaxLogSize, bAutoFlush);
/*open merge buffer?*/
		if (iMergeBufSize)
			m_BinLog[iLoop].OpenMerge(iMergeBufSize, iUseODirect? true : false);
//#endif

		
	}

	/*initialize coredump*/
	for (iLoop = 0; iLoop < 2; iLoop++)
	{
		memset(filename, 0x0, sizeof(filename));
		sprintf(filename, "%s/dump_%c", cachedir, iLoop + 'a');
		printf("initialize coredump file %s.\n", filename);
	
		if (m_CoreDump[iLoop].Init(filename, iLoop, iMergeBufSize) < 0)
		{
			snprintf(strErr, sizeof(strErr), "%s", m_CoreDump[iLoop].GetLastErr());
			return -1;
		}
	}

	/*choose the newer coredump+bin group to write*/
	if (m_CoreDump[0].GetTimestamp() < m_CoreDump[1].GetTimestamp())
	{
		m_CurIndex = 1;
		m_pBinLog = &m_BinLog[1];
		m_pCoreDump = &m_CoreDump[1];
	}
	else
	{
		m_CurIndex = 0;
		m_pBinLog = &m_BinLog[0];
		m_pCoreDump = &m_CoreDump[0];
	}

	if (m_pCoreDump->GetFlag() != COREDUMP_FULL)
		m_bNeedDump = true;

	m_bInit = true;
	return 0;
}

int CDumpBin::ChooseDumpBin(string &strDebug)
{
	char buf[256];
	int iRet = 0;
	int iUsedUs = 0;
	if (m_bInit)
	{
		//add by nekeyzhong for buffer flush
		timeval tb,te;
		gettimeofday(&tb,NULL);
		iRet = m_pBinLog->Flush(true);
		gettimeofday(&te,NULL);
		iUsedUs = TimeDiff(te, tb);
		snprintf(buf, sizeof(buf), "m_pBinLog->Flush time %d us\n", iUsedUs);
		strDebug = buf;
		if(iRet)
		{
			snprintf(strErr, sizeof(strErr), "%s", m_pBinLog->GetLastErr());
			return iRet;
		}
		//end add
		
		if (m_CoreDump[0].GetFlag() != COREDUMP_FULL)
		{
			m_CurIndex = 0;
			m_pBinLog = &m_BinLog[0];
			m_pCoreDump = &m_CoreDump[0];
		}
		else if (m_CoreDump[1].GetFlag() != COREDUMP_FULL)
		{
			m_CurIndex = 1;
			m_pBinLog = &m_BinLog[1];
			m_pCoreDump = &m_CoreDump[1];
		}
		else
		{
			/*choose the older coredump+bin group to write*/
			if (m_CoreDump[0].GetTimestamp() > m_CoreDump[1].GetTimestamp())
			{
				m_CurIndex = 1;
				m_pBinLog = &m_BinLog[1];
				m_pCoreDump = &m_CoreDump[1];
			}
			else
			{
				m_CurIndex = 0;
				m_pBinLog = &m_BinLog[0];
				m_pCoreDump = &m_CoreDump[0];
			}
		}
		gettimeofday(&tb,NULL);
		if(m_pCoreDump->WriteStart() < 0)
		{
			snprintf(strErr, sizeof(strErr), "%s", m_pCoreDump->GetLastErr());
			return -1;
		}
		gettimeofday(&te,NULL);
		iUsedUs = TimeDiff(te, tb);
		snprintf(buf, sizeof(buf), "m_pCoreDump->WriteStart time %d us\n", iUsedUs);
		strDebug.append(buf);
	}
	return 0;
}

int CDumpBin::WriteCoreDump(char* Key, int KeyLen, char* Data, int DataLen, char* extData, int extDataLen)
{
	int retval;
	
	if (!m_bInit || !m_pCoreDump)
		return 0;
	
	retval = m_pCoreDump->WriteCoreDump(Key, KeyLen, Data, DataLen, extData, extDataLen);
	if (retval < 0)
	{
		snprintf(strErr, sizeof(strErr), "%s", m_pCoreDump->GetLastErr());
		return -1;
	}

	return retval;
}

int CDumpBin::WriteComplete(void)
{
	int retval;
	if (!m_bInit || !m_pBinLog)
		return 0;
		
	retval = m_pCoreDump->WriteComplete();
	if (retval < 0)
	{
		snprintf(strErr, sizeof(strErr), "%s", m_pCoreDump->GetLastErr());
		return -1;
	}

	return retval;
}

int32_t CDumpBin::WriteBinLog(char* pLogBuff, int32_t iLogSize, u_int64_t tLogTime)
{
	int retval;
	if (!m_bInit || !m_pBinLog)
		return 0;

	retval = m_pBinLog->WriteToBinLog(pLogBuff, iLogSize, tLogTime);
	if (retval < 0)
	{
		snprintf(strErr, sizeof(strErr), "%s", m_pBinLog->GetLastErr());
		return -1;
	}
	return retval;
}

int32_t CDumpBin::ClearAllBinLog()
{
	return m_pBinLog->ClearAllBinLog();
}

CBinLog* CDumpBin::GetCurBinLog()const
{
	return m_pBinLog;
}

CCoreDump* CDumpBin::GetCurCoreDump()const
{
	return m_pCoreDump;
}

int CDumpBin::GetCurIndex() const
{
	return m_CurIndex;
}

u_int64_t CDumpBin::GetBinLogTotalSize()
{
	return m_pBinLog->GetBinLogTotalSize();
}

/*in seconds*/
void CDumpBin::SetFlushInterval(int iFlushInterval)
{
	m_iFlushInterval = iFlushInterval;
}

int CDumpBin::GetFlushInterval()
{
	return m_iFlushInterval;
}

int CDumpBin::Run(struct timeval now)
{
	int iRet = 0;
	if (now.tv_sec - m_iLastTime >= m_iFlushInterval)
	{
		m_iLastTime = now.tv_sec;
		iRet = m_pBinLog->Flush();
		if(iRet)
		{
			snprintf(strErr, sizeof(strErr), "%s", m_pBinLog->GetLastErr());
			return -1;			
		}

	}
	return 0;
}

bool CDumpBin::IsNeedDump()
{
	return m_bNeedDump;
}


int CDumpFastReader::Init(const char *pDumpFile, int iPerReadSize)
{
	assert(pDumpFile);
	if(iPerReadSize <= 0 || iPerReadSize > MAX_BUFF_SIZE)
	{
		snprintf(szErr, sizeof(szErr), "iPerReadSize invalid %d\n", iPerReadSize);
		return -1;
	}
	m_iPerReadSize = iPerReadSize;
	sDumpFile = pDumpFile;
	if (NULL != pBuffer)
	{
		delete []pBuffer;
		pBuffer = NULL;
	}
	pBuffer = new char[MAX_BUFF_SIZE + 1024];
	pReadPos = pWritePos = pBuffer;
	pEndFence = pBuffer + MAX_BUFF_SIZE;
	ssize_t read_len;
	CCoreDumpHead core_head;
	struct stat stat_file;

	if ( stat(pDumpFile, &stat_file) )
	{
		snprintf(szErr, sizeof(szErr), "stat failed %d\n", errno);
		return -4;
	}
	//2013-02-21 add by patxiao
	if(m_iDumpfd > 0)
	{
		close(m_iDumpfd);
		m_iDumpfd = -1;
	}
	m_iDumpfd = open(pDumpFile, O_RDONLY);
	if ( m_iDumpfd < 0 )
	{
		snprintf(szErr, sizeof(szErr), "open failed %d\n", errno);
		return -1;
	}
	read_len = read(m_iDumpfd, &core_head, sizeof(CCoreDumpHead));
	if ( read_len != (ssize_t)sizeof(CCoreDumpHead) )
	{
		snprintf(szErr, sizeof(szErr), "read failed %d\n", errno);
		return -2;
	}

	if ( (unsigned int)stat_file.st_size != core_head.total_size )
	{
		snprintf(szErr, sizeof(szErr), "size %u != corehead size %u\n",  (unsigned int)stat_file.st_size, core_head.total_size);
		return -3;
	}

	m_llFileSize = (long long)stat_file.st_size;

	if(core_head.magic != COREDUMP_MAGIC 
		&& core_head.magic != MD5DUMP_MAGIC
		&& core_head.magic != CRCDUMP_MAGIC
		&& core_head.magic != KEYDUMP_MAGIC)
	{
		snprintf(szErr, sizeof(szErr), "unkown magic %x\n",  core_head.magic);
		return -5;
	}	
	if(_ReadData() < 0)
	{
		return -6;
	}
	return 0;
}

void CDumpFastReader::Close()
{
	if(pBuffer)
	{
		delete [] pBuffer;
		pBuffer = NULL;
	}
	if(m_iDumpfd > 0)
	{
		close(m_iDumpfd);
		m_iDumpfd = -1;
	}

	m_iDumpfd = -1;
	pBuffer = NULL;
	pReadPos = pWritePos = pEndFence = NULL;
	memset(szErr, 0, sizeof(szErr));
	m_iPerReadSize = PER_READ_SIZE;		
}

int CDumpFastReader::_DataCount()
{
	return  pWritePos - pReadPos;;
}
int CDumpFastReader::_LeftCount()
{
	return pEndFence - pWritePos;
}
void CDumpFastReader::_PullData()
{
	if(pReadPos != pBuffer)
	{
		int iDataCnt = _DataCount();
		if(iDataCnt > 0)
			memmove(pBuffer, pReadPos, iDataCnt);
		pReadPos = pBuffer;
		pWritePos = pBuffer + iDataCnt;
	}
}
//返回缓冲区内的总字节数
int CDumpFastReader::_ReadData()
{
	//
	_PullData();
	
	int iLeftCnt = _LeftCount();
	while(iLeftCnt > m_iPerReadSize)
	{
		int iReadLen = 0;	
		iReadLen = read(m_iDumpfd, pWritePos, m_iPerReadSize);
		if(iReadLen < 0)
		{
			snprintf(szErr, sizeof(szErr), "read failed %d\n", errno);
			return -1;
		}

		pWritePos += iReadLen;
		iLeftCnt = _LeftCount();
		if(iReadLen < m_iPerReadSize)
		{
			break;
		}
	};
	return _DataCount();
}
/*
 * format:
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * + total length + key length + key + data length + data + ext length + ext data +
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
 /*
return < 0,出错0,无数据了,1,1个数据
*/
int CDumpFastReader::Parse(string &key, string &value, string &ext)
{
	if(_DataCount() < 16)
	{
		_ReadData();
	}
	if(_DataCount() == 0)
	{
		return 0;
	}

	if(_DataCount() < 16)
	{
		snprintf(szErr, sizeof(szErr), "_DataCount %d < 16\n", _DataCount());
		return -1;
	}
	int iTotalLen = *(int *)pReadPos;;
	pReadPos += sizeof(int);
	if(iTotalLen > MAX_BUFF_SIZE || iTotalLen<0)
	{
		snprintf(szErr, sizeof(szErr), "iTotalLen %d > %d\n", iTotalLen, MAX_BUFF_SIZE);
		return -2;
	}
	if(_DataCount() < iTotalLen)
	{
		if (_ReadData() < 0 ) {
			return -3;
		}
		if(_DataCount() < iTotalLen)
		{
			snprintf(szErr, sizeof(szErr), "_DataCount %d < iTotalLen %d\n", _DataCount(), iTotalLen);
			return -4;
		}
	}
	int iKeyLen = *(int *)pReadPos;
	pReadPos += sizeof(int);
	if(_DataCount() < iKeyLen || iKeyLen < 0)
	{
		snprintf(szErr, sizeof(szErr), "_DataCount %d < iKeyLen %d\n", _DataCount(), iKeyLen);
		return -5;
	}
	key.assign(pReadPos, iKeyLen);
	pReadPos += iKeyLen;

	int iDataLen = *(int *)pReadPos;
	pReadPos += sizeof(int);
	if(_DataCount() < iDataLen || iDataLen < 0)
	{
		snprintf(szErr, sizeof(szErr), "_DataCount %d < iDataLen%d\n", _DataCount(), iDataLen);
		return -6;
	}
	value.assign(pReadPos, iDataLen);
	pReadPos += iDataLen;

	int iExtLen = *(int *)pReadPos;
	pReadPos += sizeof(int);
	if(_DataCount() < iExtLen || iExtLen < 0)
	{
		snprintf(szErr, sizeof(szErr), "_DataCount %d < iExtLen%d\n", _DataCount(), iExtLen);
		return -7;
	}
	ext.assign(pReadPos, iExtLen);
	pReadPos += iExtLen;

	return 1;
	
}

long long CDumpFastReader::Seek(long long llOffset, int whence)
{
	if (m_iDumpfd < 0)
	{
		return -1;
	}
	if (llOffset<0 || llOffset>=m_llFileSize)
	{
		return -2;
	}
	pReadPos = pWritePos = pBuffer;
	int iRet = lseek(m_iDumpfd, llOffset, whence);
	
	return iRet;
}

long long CDumpFastReader::CurOffset()
{
	if (m_iDumpfd < 0)
	{
		return -1;
	}
	return lseek(m_iDumpfd, 0, SEEK_CUR) - _DataCount();
}


/*int main(int args, char** argv)
{
	CDumpBin dumpbin;
	char* databuf = new char[1UL << 10];
	char key[] = {"hello world"};

	if (dumpbin.Initialize("./backup", 0, 500000, 5))
	{
		printf("dumpbin initialize failed!\n");
		return -1;
	}

	dumpbin.ChooseDumpBin();
	dumpbin.WriteCoreDump(key, sizeof(key), databuf, 1UL << 10, true);
	return 0;
}*/

