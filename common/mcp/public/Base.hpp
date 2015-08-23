/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2007-09
Description: 基础函数库
***********************************************************/

#ifndef _BASE_HPP_
#define _BASE_HPP_

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dlfcn.h>
//#include <endian.h>

#if defined(LINUX) 
#include <linux/ip.h> 
#include <linux/tcp.h> 
#else 
#include <netinet/ip.h> 
#include <netinet/tcp.h> 
#endif 

#include <vector>
#include <string>
#include <set>
#include <map>
using namespace std;

extern char g_CharMap[][4];

/*
faster than inet_ntoa  4.1 times
*/
void INET_NTOA(unsigned int unIP,char *pIP);
#define NtoP INET_NTOA
void HtoP (unsigned int unIP,char *pIP);

#define arch__swab32(x) ({__asm__("bswapl %0" : "=r" (x) : "0" (x));  x;})

//print int ip by net order
#define NIPQUAD(addr) (int)((unsigned char*)&(addr))[0],(int)((unsigned char*)&(addr))[1],(int)((unsigned char*)&(addr))[2],(int)((unsigned char*)&(addr))[3]
//print int ip by host order
#define HIPQUAD(addr) (int)((unsigned char*)&(addr))[3],(int)((unsigned char*)&(addr))[2],(int)((unsigned char*)&(addr))[1],(int)((unsigned char*)&(addr))[0]


char* uint2str(uint32_t,char*);
char* int2str(int32_t,char*);
bool IsCPULittleEndian();
int ex(int i);

u_int64_t NTOHL64(u_int64_t N);
u_int64_t HTONL64(u_int64_t H);

void GetDateTimeStr(time_t tNow,char* szTimeStr);
void PrintBin2Buff(string &strBuff,char *pBuffer, int iLength);
void PrintBin(FILE* pLogfp,char *pBuffer, int iLength);
void PrintHexStr(FILE* pLogfp,char *pBuffer, int iLength);
void HexStr2Bin(char* pStrHex,string& strBin);
void GetNameFromPath(char *szPath,char* szName);
char* CreateShm(key_t iShmKey,size_t iShmSize,int &iErrNo,bool &bNewCreate,int iForceAttach=0);
//Protocol: 0 tcp,1 udp
int CreateListenSocket(u_int32_t unListenIP,u_int16_t usListenPort,
						int RCVBUF=-1,int SNDBUF=-1,int iProtocol=0,int iListenBackLog=1024);
//Protocol: 0 tcp,1 udp
int CreateConnectSocket(u_int32_t unDestIP,u_int16_t usDestPort,
						int RCVBUF=-1,int SNDBUF=-1,int iProtocol=0);
bool IsIpAddr(char* pHostStr);
bool isSocketAlive(int iSocket);
int LoadConf(char* pConfFile,vector<string> &vecConf);
int SplitTag(char* pContentLine,char* pSplitTag,vector<string> &vecContent);
int LoadGridCfgFile(char* pConfFile,std::vector<std::vector<std::string> >&vecCfgItem);
int DumpGridCfgFile(std::vector<std::vector<std::string> >&vecCfgItem,char* pConfFile);
void TrimHeadTail(char* pContentLine);
long long TimeDiff(struct timeval t1, struct timeval t2);
inline string trim_left(const string &s,const string& filt=" ")
{
	char *head=const_cast<char *>(s.c_str());
	char *p=head;
	while(*p) {
		bool b = false;
		for(size_t i=0;i<filt.length();i++) {
			if((unsigned char)*p == (unsigned char)filt.c_str()[i]) {b=true;break;}
		}
		if(!b) break;
		p++;
	}
	return string(p,0,s.length()-(p-head));
}

inline string trim_right(const string &s,const string& filt=" ")
{
	if(s.length() == 0) return string();
	char *head=const_cast<char *>(s.c_str());
	char *p=head+s.length()-1;
	while(p>=head) {
		bool b = false;
		for(size_t i=0;i<filt.length();i++) {
			if((unsigned char)*p == (unsigned char)filt.c_str()[i]) {b=true;break;}
		}
		if(!b) {break;}
		p--;
	}
	return string(head,0,p+1-head);
}


/*
	负载检查器,CLoadGrid(100000,50,99999) 即
	在时间轴上以50ms为每次前进的跨度,在100s的
	总长时间内限制包量99999个
*/
#include "spin_lock.h"
class CLoadGrid
{
public:
	enum LoadRet
	{
		LR_ERROR = -1,
		LR_NORMAL =0,
		LR_FULL = 1
	};
	CLoadGrid(int iTimeAllSpan,int iEachGridSpan,int iMaxNumInAllSpan=0);
	~CLoadGrid();

	//用户每次调用，检查是否达到水位限制
	//return enum LoadRet{};
	int CheckLoad(timeval tCurrTime);

	//获取当前水位信息
	void FetchLoad(int& iTimeAllSpanMs, int& iReqNum);

	void OpenLock(){m_bLock = true;};
	void CloseLock(){m_bLock = false;};
private:
	void UpdateLoad(timeval *ptCurrTime=NULL);
	
	int m_iTimeAllSpanMs;
	int m_iEachGridSpanMs;
	int m_iMaxNumInAllSpan;

	int m_iAllGridCount;			//需要的送格子数
	int* m_iGridArray;			//时间轴
	timeval m_tLastGridTime;
	int m_iNumInAllSpan;

	//当前位
	int m_iCurrGrid;

	bool m_bLock;
	spinlock_t m_spinlock_t;
};
#define CMEM_TIME_BEGIN 1325347200 //cmem time的开始时间
//unixtime转换成cmem的时间，从2012-01-01 00:00:00 年(1325347200s)以来经过的小时数
typedef struct cmem_time_t
{
	uint16_t hour;
}cmem_time_t;
inline int UnixTimeToCmemTime(int iUnixTime, cmem_time_t &stCmemTime)
{
	//invalid time
	if(iUnixTime < CMEM_TIME_BEGIN)
		return -1;
	int iTimeLeft = iUnixTime - CMEM_TIME_BEGIN;
	stCmemTime.hour = iTimeLeft/3600;
	return 0;
}
inline int CmemTimeToUnixTime(cmem_time_t stCmemTime)
{
	int iTime = CMEM_TIME_BEGIN + stCmemTime.hour*3600;
	return iTime;
}
#endif

