#ifndef _BINLOG
#define _BINLOG

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>

class CBinLog
{
public:
	CBinLog();
	~CBinLog();

	void OpenMerge(int iMergeBuffSize=16*1024*1024);
		
	int32_t Init(char *pLogBaseName="binlog_", int32_t iMaxLogSize=50000000, 
			     int32_t iMaxLogNum=10,bool bAutoFlushDisk=true, bool bEachWriteClose=false);
	
	int32_t ReadRecordStart();
	int32_t ReadRecordFromBinLog(char* pLogBuff, int32_t iMaxSize,
					u_int64_t &tLogTime,char* szLogFile=NULL);

	int32_t WriteToBinLog(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime=0);

	u_int32_t GetBinLogTotalSize();
	int32_t ClearOldBinLog();
	int32_t ClearAllBinLog();
	void EnableFileShift(bool bEnableShift){m_bEnableShift = bEnableShift;}
	void Flush();
	
private:
	int32_t AppendToMergeBuff(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime=0);
	void FlushMergeBuff();
	int32_t ShiftFiles();
	
public:	
	//相关参数
	char m_szLogBaseName[256];
	char m_szLogFileName[256];
	int32_t m_iMaxLogSize;
	int32_t 	m_iMaxLogNum;
	bool m_bAutoFlushDisk;
	bool m_bEachWriteClose;				/* 多进程时写完必须关闭*/

	//总写入量
	u_int32_t m_unTotalLogSize;
	//当前文件量
	u_int32_t m_unCurrLogSize;

	FILE *m_pWriteFp;
	FILE *m_pReadFp;	
	int32_t m_iReadLogIdx;

	int MERGE_BUFF_SIZE;
	char *m_szMergeBuff;
	int m_iMergeLen;

	bool m_bEnableShift;
};

#endif

