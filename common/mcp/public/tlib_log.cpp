#ifndef _TLIB_LOG_C_
#define _TLIB_LOG_C_

#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "tlib_log.h"

static int ShiftFiles(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum)
{
	struct stat stStat;
	char sLogFileName[300];
	char sNewLogFileName[300];
	int i;

	sprintf(sLogFileName,"%s.log", sLogBaseName);

	if(stat(sLogFileName, &stStat) < 0)
	{
		return -1;
	}

	if (stStat.st_size < lMaxLogSize)
	{
		return 0;
	}

	sprintf(sLogFileName,"%s%d.log", sLogBaseName, iMaxLogNum-1);
	if (access(sLogFileName, F_OK) == 0)
	{
		if (remove(sLogFileName) < 0 )
		{
			return -1;
		}
	}

	for(i = iMaxLogNum-2; i >= 0; i--)
	{
		if (i == 0)
			sprintf(sLogFileName,"%s.log", sLogBaseName);
		else
			sprintf(sLogFileName,"%s%d.log", sLogBaseName, i);
			
		if (access(sLogFileName, F_OK) == 0)
		{
			sprintf(sNewLogFileName,"%s%d.log", sLogBaseName, i+1);
			if (rename(sLogFileName,sNewLogFileName) < 0 )
			{
				return -1;
			}
		}
	}
	return 0;
}

void TLib_Tools_GetCurDateTimeStr(char* szTimeStr)
{
	timeval tval;
	gettimeofday(&tval,NULL);
	struct tm curr;
	curr = *localtime(&tval.tv_sec);

	if (curr.tm_year > 50)
	{
		sprintf(szTimeStr, "%04d-%02d-%02d %02d:%02d:%02d.%06d", 
			curr.tm_year+1900, curr.tm_mon+1, curr.tm_mday,
			curr.tm_hour, curr.tm_min, curr.tm_sec,(int)tval.tv_usec);
	}
	else
	{
		sprintf(szTimeStr, "%04d-%02d-%02d %02d:%02d:%02d.%06d",
	        curr.tm_year+2000, curr.tm_mon+1, curr.tm_mday,
	        curr.tm_hour, curr.tm_min, curr.tm_sec,(int)tval.tv_usec);
	}
}

int TLib_Log_VWriteLog(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum,const char *sFormat, va_list ap)
{
	FILE  *pstFile;
	char sLogFileName[300];

   	sprintf(sLogFileName,"%s.log", sLogBaseName);
	if ((pstFile = fopen(sLogFileName, "a+")) == NULL)
	{
		printf("[%s]Fail to open log file %s\n",__FUNCTION__,sLogFileName);
		return -1;
	}

	char szTimeStr[256];
	TLib_Tools_GetCurDateTimeStr(szTimeStr);
	
	fprintf(pstFile, "[%s] ", szTimeStr);
	vfprintf(pstFile, sFormat, ap);
	fclose(pstFile);

	return ShiftFiles(sLogBaseName, lMaxLogSize, iMaxLogNum);
}

int TLib_Log_WriteLog(char *sLogBaseName, long lMaxLogSize, int iMaxLogNum, const char *sFormat, ...)
{
	int iRetCode;
	va_list ap;
	
	va_start(ap, sFormat);
	iRetCode = TLib_Log_VWriteLog(sLogBaseName, lMaxLogSize, iMaxLogNum, sFormat, ap);
	va_end(ap);

	return iRetCode;
}

static char sLogBaseName[256];
static long lMaxLogSize;
static int iMaxLogNum;
static int iLogInitialized = 0;
static time_t tLastLogTime = 0;

void TLib_Log_LogInit(char *sPLogBaseName, long lPMaxLogSize, int iPMaxLogNum)
{
	memset(sLogBaseName, 0, sizeof(sLogBaseName));
	strncpy(sLogBaseName, sPLogBaseName, sizeof(sLogBaseName)-1);
	lMaxLogSize = lPMaxLogSize;
	iMaxLogNum = iPMaxLogNum;
	iLogInitialized = 1;
}

void TLib_Log_LogMsg(const char *sFormat, ...)
{
	va_list ap;
	if (iLogInitialized != 0)
	{
		va_start(ap, sFormat);
		TLib_Log_VWriteLog(sLogBaseName, lMaxLogSize, iMaxLogNum,sFormat, ap);
		va_end(ap);
	}
}

void TLib_Log_LogMsgFrequency(int iSecs,const char *sFormat, ...)
{
	time_t tNow = time(0);
	if(tNow - tLastLogTime < iSecs)
	{
		return;
	}

	tLastLogTime = tNow;
	
	va_list ap;
	if (iLogInitialized != 0)
	{
		va_start(ap, sFormat);
		TLib_Log_VWriteLog(sLogBaseName, lMaxLogSize, iMaxLogNum,sFormat, ap);
		va_end(ap);
	}
}

#endif
