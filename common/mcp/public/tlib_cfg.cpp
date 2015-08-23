#ifndef _TLIB_CFG_C_
#define _TLIB_CFG_C_

#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#include "tlib_cfg.h"

#define MAX_CONFIG_LINE_LEN 255

void _Cfg_TrimStr(char *strInput )
{
    char *pb;
    char *pe;
	int iTempLength;

	if( strInput == NULL )
	{
		return;
	}

	iTempLength = strlen( strInput);
	if( iTempLength == 0 )
	{
		return;
	}

    pb = strInput;

    while (((*pb == ' ' ) || (*pb == '\t' ) || (*pb == '\n' ) || (*pb == '\r' ) ) && (*pb != 0 ) )
    {
        pb ++;
    }

    pe = &strInput[iTempLength-1];
    while ((pe >= pb ) && ((*pe == ' ' ) || (*pe == '\t' ) || (*pe == '\n' ) || (*pe == '\r' ) ) )
    {
        pe --;
    }

	*(pe+1 ) = '\0';

	memmove(strInput, pb, strlen(pb)+1);//instead strcpy( strInput, pb) for tlinux
	return;
}

static void _Cfg_InitDefault(va_list ap)
{
	char *sParam, *sVal, *sDefault;
	double *pdVal, dDefault;
	long long*plVal, lDefault;
	int iType, *piVal, iDefault;
	long lSize;

	sParam = va_arg(ap, char *);
	while (sParam != NULL)
	{
		iType = va_arg(ap, int);
		switch(iType)
		{
			case CFG_STRING:
				sVal = va_arg(ap, char *);
				sDefault = va_arg(ap, char *);
				lSize = va_arg(ap, long);
				strncpy(sVal, sDefault, (int)lSize-1);
				sVal[lSize-1] = 0;
				break;
			case CFG_INT64:
				plVal = va_arg(ap, long long*);
				lDefault = va_arg(ap, long long);
				*plVal = lDefault;
				break;
			case CFG_INT:
				piVal = va_arg(ap, int *);
				iDefault = va_arg(ap, int);
				*piVal = iDefault;
				break;
			case CFG_DOUBLE:
				pdVal = va_arg(ap, double *);
				dDefault = va_arg(ap, double);
				*pdVal = dDefault;
				break;
		}
		sParam = va_arg(ap, char *);
	}
}

static void _Cfg_SetVal(va_list ap, char *sP, char *sV)
{
	char *sParam=NULL, *sVal=NULL, *sDefault=NULL;
	double *pdVal=NULL, dDefault=0;
	long long *plVal=NULL, lDefault=0;
	int iType=0, *piVal=NULL, iDefault=0;
	long lSize = 0;

	sParam = va_arg(ap, char *);
	while (sParam != NULL)
	{
		iType = va_arg(ap, int);
		switch(iType)
		{
			case CFG_STRING:
				sVal = va_arg(ap, char *);
				sDefault = va_arg(ap, char *);
				lSize = va_arg(ap, long);
				break;
			case CFG_INT64:
				plVal = va_arg(ap, long long*);
				lDefault = va_arg(ap, long long);
				if (strcmp(sP, sParam) == 0)
				{
					*plVal = atoll(sV);
				}
				break;
			case CFG_INT:
				piVal = va_arg(ap, int *);
				iDefault = va_arg(ap, int);
				if (strcmp(sP, sParam) == 0)
				{
					*piVal = iDefault;
				}
				break;
			case CFG_DOUBLE:
				pdVal = va_arg(ap, double *);
				dDefault = va_arg(ap, double);
				*pdVal = dDefault;
				break;
		}

		if (strcmp(sP, sParam) == 0)
		{
			switch(iType)
			{
				case CFG_STRING:
					strncpy(sVal, sV, (int)lSize-1);
					sVal[lSize-1] = 0;
					break;
				case CFG_INT64:
					*plVal = atoll(sV);
					break;
				case CFG_INT:
					*piVal = atoi(sV);
					break;
				case CFG_DOUBLE:
					*pdVal = atof(sV);
					break;
			}
			return;
		}

		sParam = va_arg(ap, char *);
	}
}

static int _Cfg_GetParamVal(char *sLine, char *sParam, char *sVal)
{
	char *p, *sP;
	
	p = sLine;
	while(*p != '\0')
	{
		if ((*p != ' ') && (*p != '\t') && (*p != '\n'))
			break;
		p++;
	}
	
	sP = sParam;
	while(*p != '\0')
	{
		if ((*p == ' ') || (*p == '\t') || (*p == '\n'))
			break;
			
		*sP = *p;
		p++;
		sP++;
	}
	*sP = '\0';
	
	strcpy(sVal, p);
	_Cfg_TrimStr(sVal);
	
	if (sParam[0] == '#')
		return 1;
		
	return 0;
}

/*
	long long 格式必须这样使用:
	
	TLib_Cfg_GetConfig(szFile,
		"netcom_user", CFG_INT64, &(ullLastHashNode), (long long)0,	
		NULL);

*/
int TLib_Cfg_GetConfig(const char *sConfigFilePath, ...)
{
	FILE *pstFile;
	char sLine[MAX_CONFIG_LINE_LEN+1], sParam[MAX_CONFIG_LINE_LEN+1], sVal[MAX_CONFIG_LINE_LEN+1];
	va_list ap;
	
	va_start(ap, sConfigFilePath);
	_Cfg_InitDefault(ap);
	va_end(ap);

	if ((pstFile = fopen(sConfigFilePath, "r")) == NULL)
	{
		return -1;
	}

	while (1)
	{
		strcpy(sLine, "");
		
		fgets(sLine, sizeof(sLine), pstFile);
		if (strcmp(sLine, "") != 0)
		{
			if (_Cfg_GetParamVal(sLine, sParam, sVal) == 0)
			{
				va_start(ap, sConfigFilePath);
				_Cfg_SetVal(ap, sParam, sVal);
				va_end(ap);
			}
		}
		
		if (feof(pstFile))
		{
			break; 
		}
	}
	
	fclose(pstFile);
	return 0;
}

void TLib_Cfg_GetNVConfig(const char *sConfigFilePath,TNVStu* pNVStu)
{
	FILE *pstFile;
	char sLine[MAX_CONFIG_LINE_LEN+1], sParam[MAX_CONFIG_LINE_LEN+1], sVal[MAX_CONFIG_LINE_LEN+1];

	if ((pstFile = fopen(sConfigFilePath, "r")) == NULL)
	{
		return;
	}

	int iLinePos = 0;
	while (1)
	{
		strcpy(sLine, "");
		
		fgets(sLine, sizeof(sLine), pstFile);
		if (strcmp(sLine, "") != 0)
		{
			if (_Cfg_GetParamVal(sLine, sParam, sVal) == 0)
			{
				strcpy(pNVStu[iLinePos].szName,sParam);
				strcpy(pNVStu[iLinePos].szVal,sVal);
				iLinePos++;
			}
		}
		
		if (feof(pstFile))
		{
			break; 
		}
	}
	
	fclose(pstFile);
	
}

#endif
