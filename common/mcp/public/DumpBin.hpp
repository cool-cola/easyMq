#ifndef _DUMPBIN_H_
#define _DUMPBIN_H_

#include "BinlogNR.hpp"
#include <string>
using namespace std;
#define COREDUMP_MAGIC 		0x706d7564
#define MD5DUMP_MAGIC 			0x10000001
#define CRCDUMP_MAGIC 			0x10000002
#define KEYDUMP_MAGIC 			0x20000001
#define COREDUMP_VERSION	0x01
#define COREDUMP_PARTIAL   	0x00
#define COREDUMP_FULL     	0x01
#define RESERVED_SPACE        	1024
#define MERGE_BUFSIZE		(1UL << 16)
#define FLUSH_DEFAULT		5

#define DIR_MODE			0755
#define FILE_MODE			0644

#define PER_READ_SIZE (16*1024)
#define MAX_BUFF_SIZE (16*1024*1024)

typedef struct
{
	unsigned int magic;
	unsigned int version;
	unsigned int flag;
	unsigned int timestamp;
	unsigned int total_size;
	unsigned int reserved[8];
}CCoreDumpHead;

class CCoreDump
{
public:
	CCoreDump();
	virtual ~CCoreDump();

public:
	int Init(const char* pFileName, int nGroup, int iMergeBufSize = MERGE_BUFSIZE, int iDumpMagic = COREDUMP_MAGIC);
	int SetFile(const char* pFileName, int nGroup, int iDumpMagic = COREDUMP_MAGIC);

public:
	ssize_t GetTotalSize();//2012-06-19 patxiao:add for recover value dump
	ssize_t GetFileSize(); // 已经罗盘的文件大小
	unsigned int GetFlag();
	unsigned int GetTimestamp();
	int UpdateTimestamp();
	char* GetLastErr();
public:
	int WriteStart(int iClearDirty = 1);
	int WriteCoreDump(char* Key, int KeyLen, char* Data, int DataLen, char* extData, int extDataLen = 0);
	int WriteComplete();
	void AppendToMerge(char* Key, int KeyLen, char* Data, int DataLen, char* extData, int extDataLen = 0);
	int FlushMerge();
private:
	CCoreDumpHead m_DumpHead;
	unsigned int m_FileLen;
	char* m_MergeBuf;
	int m_MergeBufSize;
	int m_iMergeLen;
	char m_FileName[256];
	char strErr[512];
	int m_iMagic;
	bool m_b_shared_buf_flag;
};

class CDumpBin
{
public:
	CDumpBin();
	virtual ~CDumpBin();

public:
	int Initialize(char* pLogBaseName, int iCacheID, int32_t iMaxLogSize, int32_t iMaxLogNum, 
		bool bAutoFlush = true, int32_t iMergeBufSize = MERGE_BUFSIZE, int iUseODirect = 1);

public:
	int ChooseDumpBin(string &strDebug);
	bool IsNeedDump();
	char* GetLastErr();
public:
	int WriteComplete(void);
	int WriteCoreDump(char* Key, int KeyLen, char* Data, int DataLen, char* extData, int extDataLen);
	int32_t WriteBinLog(char* pLogBuff, int32_t iLogSize, u_int64_t tLogTime = 0);
	int32_t ClearAllBinLog();

public:
	int Run(struct timeval now);

public:
	void SetFlushInterval(int iFlushInterval);
	int GetFlushInterval();
	u_int64_t GetBinLogTotalSize();
	int GetCurIndex() const;
	CBinLog* GetCurBinLog()const;
	CCoreDump* GetCurCoreDump()const;

private:
	CCoreDump* m_pCoreDump;
	CCoreDump m_CoreDump[2];
	CBinLog* m_pBinLog;
	CBinLog m_BinLog[2];
	int m_CurIndex;
	int m_iFlushInterval;
	time_t m_iLastTime;
	bool m_bInit;
	bool m_bNeedDump;
	char strErr[512];
};


class CDumpFastReader
{
public:
	CDumpFastReader()
	{
		m_iDumpfd = -1;
		pBuffer = NULL;
		pReadPos = pWritePos = pEndFence = NULL;
		memset(szErr, 0, sizeof(szErr));
		m_iPerReadSize = PER_READ_SIZE;
		m_llFileSize = 0;
		
	}
	~CDumpFastReader()
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
	}
	int Init(const char *pDumpFile, int iPerReadSize = PER_READ_SIZE);
	void Close();
	int Parse(string &key, string &value, string &ext);
	long long Seek(long long llOffset, int whence);
	// 相对于文件开头，当前的读取位置偏移
	long long CurOffset();
protected:
	int _ReadData();
	void _PullData();
	int _DataCount();
	int _LeftCount();
public:
	int m_iDumpfd;
	char *pBuffer;
	char *pReadPos;
	char *pWritePos;
	char *pEndFence;
	int m_iPerReadSize;
	char szErr[256];
	string sDumpFile;
	long long m_llFileSize;
	
};

#endif // end of _DUMPBIN_H_
