/**************************************************************************

DESC: 统计类:TypeInfo统计项目单元,按操作+结果区分

AUTHOR: nekeyzhong 

DATE: 2007年9月

PROJ: Part of MCP Project

 **************************************************************************/
#ifndef __STATISTIC_H__
#define __STATISTIC_H__

#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
 #include <stdarg.h>
 #include <pthread.h>

#define TYPE_NUM 			1024
#define TYPE_NAME_LEN		64
#define MAX_TIMEOUT_LEVEL	3

typedef struct
{
	unsigned long long tv_sec;
	unsigned long long tv_usec;
}ULLTimeVal;

typedef struct
{
	char m_szName[TYPE_NAME_LEN];
	int m_iNameID;	
	int m_iResultID;
}TypeKey;

typedef struct
{
	//key
	TypeKey m_TypeKey;
	
	unsigned long long m_ullAllCount;
	unsigned int m_unMaxTime;
	unsigned int m_unMinTime;
	ULLTimeVal m_stTime;	
	unsigned int m_unTimeOut[MAX_TIMEOUT_LEVEL];
	char m_szRecordAtMax[TYPE_NAME_LEN];
	unsigned long long m_ullSumVal;
}TypeInfo;

#define CLEAN_TYPE_INFO_DATA(stTypeInfo) { \
	(stTypeInfo).m_ullAllCount = 0;\
	(stTypeInfo).m_unMaxTime = 0;\
	(stTypeInfo).m_unMinTime = 0;\
	(stTypeInfo).m_stTime.tv_sec = 0;\
	(stTypeInfo).m_stTime.tv_usec = 0;\
	memset(&((stTypeInfo).m_unTimeOut[0]),0,sizeof((stTypeInfo).m_unTimeOut));\
	memset(&((stTypeInfo).m_szRecordAtMax[0]),0,sizeof((stTypeInfo).m_szRecordAtMax));\
	(stTypeInfo).m_ullSumVal=0;\
}

class CStatistic
{
public:
	static CStatistic* Instance();
	static CStatistic* m_pInstance;
	
	CStatistic(bool bUseMutex=false);
	~CStatistic();

	//iTimeOutUs :时间计数标尺,单位us
	int Inittialize(const char *pszLogBaseFile=(char *)"./Statistic",
		int iMaxSize=20000000,
		int iMaxNum=10,
		int iTimeOutUs1=100000,
		int iTimeOutUs2=500000,
		int iTimeOutUs3=1000000);

	//pTypeName: 统计名称
	//iNameID:对应的业务分类
	//szRecordAtMax: 最多延时产生时记录的数据<64bytes
	//iStatCount统计个数
	//iSumVal 累加的值,记录流量等
	int AddStat(const char* szTypeName,
				int iResultID=0, 
				struct timeval *pstBegin=NULL, 
				struct timeval *pstEnd=NULL,
				char* szRecordAtMax=NULL,
				int iSumVal=0,
				unsigned int unStatCount=1, const TypeInfo* pTypeInfoIncr = NULL)
	{return AddStat(szTypeName,-1,iResultID,pstBegin,pstEnd,szRecordAtMax,
				(unsigned long long)iSumVal,(unsigned long long)unStatCount,pTypeInfoIncr);}

/*
*	显示格式:
*	Name			 |	RESULT| 		  TOTAL|		  SUMVAL|  AVG(ms)|  MAX(ms)|  MIN(ms)|RECATMAX 		 |	 >0.100ms|	 >0.500ms|	 >1.000ms|
*	统计条目
*	Name = iNameID_szTypeName
*
*	主键 Name+RESULT
*	
*	TOTAL += ullStatCount
*	SUMVAL += ullSumVal
*
*
*	时间统计:
* 	如果pstBegin和pstEnd都为NULL，则不进行时间统计
* 	如果pstBegin不为NULL，pstEnd为NULL，则pstEnd取当前时间
*	AVG，MAX, MIN对(pstEnd-pstBegin)的统计
*	RECATMAX =	(pstEnd-pstBegin)最大的szRecordAtMax
*/
	int AddStat(const char* szTypeName,int iNameID,int iResultID, 
				struct timeval *pstBegin=NULL, 
				struct timeval *pstEnd=NULL,
				char* szRecordAtMax=NULL,
				unsigned long long ullSumVal=0,
				unsigned long long ullStatCount=1, const TypeInfo* pTypeInfoIncr = NULL);

	int GetStat(char* szTypeName,int iNameID/*=-1*/,int iResultID,TypeInfo &stTypeInfo);
	const TypeInfo* GetStat(int &iTypeNum)
	{
		iTypeNum = m_iTypeNum;
		return m_astTypeInfo;
	};
	int AddStat(const TypeInfo &stTypeInfo);
	int WriteToFile();	
	void ClearStat();	
	void _AddTimeByTypeInfo(TypeInfo* pTypeInfo, const TypeInfo* pTypeInfoIncr);
private:
	void GetDateString(char *szTimeStr);
		
	int _AddStat(const char* pszTypeName,int iNameID,
				int iResultID, 
				struct timeval *pstBegin, 
				struct timeval *pstEnd,
				char* szRecordAtMax,
				unsigned long long ullSumVal,
				unsigned long long ullStatCount, const TypeInfo* pTypeInfoIncr);	
	int _GetStat(const char* szTypeName,int iNameID,int iResultID,TypeInfo &stTypeInfo);
	
	int _ShiftFiles();
	void _WriteLog(const char *sFormat, ...);
	void _AddTime(TypeInfo* pTypeInfo,
				struct timeval *pstBegin, 
				struct timeval *pstEnd,
				char* szRecordAtMax);

	//互斥使用
	bool m_bUseMutex;
	pthread_mutex_t m_stMutex;   
	
	int m_iTypeNum;
	TypeInfo m_astTypeInfo[TYPE_NUM];
	
	char m_szLogBase[256];
	int m_iLogMaxSize;
	int m_iLogMaxNum;

	unsigned int m_iTimeOutUs[MAX_TIMEOUT_LEVEL];
	
	int m_iLastClearTime;

	int m_iMaxTypeNameLen;
};
#endif

