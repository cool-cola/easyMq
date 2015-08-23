/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: 配置文件读取

***********************************************************/

#ifndef _TLIB_CFG_H_
#define _TLIB_CFG_H_

#define CFG_STRING	(int)1
#define CFG_INT		(int)2
#define CFG_INT64		(int)3
#define CFG_DOUBLE	(int)4

typedef struct
{
	char szName[256];
	char szVal[256];
}TNVStu;

int TLib_Cfg_GetConfig(const char *sConfigFilePath, ...);
void TLib_Cfg_GetNVConfig(const char *sConfigFilePath,TNVStu* pNVStu);

#endif
