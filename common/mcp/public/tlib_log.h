/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: 循环日志类

***********************************************************/

#ifndef _TLIB_LOG_H_
#define _TLIB_LOG_H_

#include <time.h>
#include <stdarg.h>

void TLib_Tools_GetCurDateTimeStr(char* szTimeStr);

int TLib_Log_VWriteLog(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum, const char *sFormat, va_list ap);
int TLib_Log_WriteLog(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum, const char *Format, ...);

void TLib_Log_LogMsg(const char *sFormat, ...);
void TLib_Log_LogMsgFrequency(int iSecs,const char *sFormat, ...);
void TLib_Log_LogInit(char *sPLogBaseName, long lPMaxLogSize, int iPMaxLogNum);

#endif

