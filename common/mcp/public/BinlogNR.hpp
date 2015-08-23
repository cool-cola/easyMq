/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2007-09
Description: 支持O_DIRECT方式的二进制流水，分文件自增
***********************************************************/

#ifndef _BINLOGNR_
#define _BINLOGNR_

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

	int32_t Init(char *pLogBaseName="binlog_", int32_t iMaxLogSize=50000000,bool bAutoFlushDisk=false);
	
	void OpenMerge(int iMergeBuffSize=8*4096, bool b_use_o_direct = true);
	
	int32_t ReadRecordStart();
	int32_t ReadRecordFromBinLog(char* pLogBuff, int32_t iMaxSize,
					u_int64_t &tLogTime,char* szLogFile=NULL);

	int32_t WriteToBinLog(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime=0);

	u_int64_t GetBinLogTotalSize();
	int32_t ClearAllBinLog();
	int32_t ClearOldBinLog();
	int Flush(bool bFlushAndCloseODirectFile = false);
	void EnableFileShift(bool bEnableShift){m_bEnableShift = bEnableShift;};
	char* GetLastErr();
private:
	int OpenWriteFP();
	int CloseWriteFP();
	int32_t _AppendBufToMergeBuf(char* p_buf, int32_t i_buf_len);
	int32_t AppendToMergeBuff(char* pLogBuff, int32_t iLogSize,u_int64_t tLogTime=0);
	int FlushMergeBuff();
	
public:	
	//相关参数
	char m_szLogBaseName[256];
	char m_szLogFileName[256];
	int32_t m_iMaxLogSize;
	bool m_bAutoFlushDisk;

	//总写入量
	u_int64_t m_ullTotalLogSize;
	//当前文件量
	u_int32_t m_unCurrLogSize;

	FILE *m_pWriteFp;
	FILE *m_pReadFp;	
	int32_t m_iReadLogIdx;
	int32_t m_iWriteLogIdx;

	bool m_b_cur_use_o_direct;
	bool m_b_use_o_direct;
	
	int MERGE_BUFF_SIZE;
	char *m_szMergeBuff;
	int m_iMergeLen;

	bool m_bEnableShift;
	char strErr[512];
};

#endif

