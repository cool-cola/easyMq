/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2007-09
Description: 位图,最多支持4位

|------0------|------1------|------2------|------3------|
  m_pBitMem[0]    m_pBitMem[1]    m_pBitMem[2]    m_pBitMem[3]
 -m_unMapSize-
---------------------m_unBodySize---------------------

***********************************************************/
 
#ifndef _BITMAP_HPP
#define _BITMAP_HPP

#include <sys/time.h>
#include <sys/types.h>

#include "BinlogNR.hpp"

class CBitmap
{
public:

	typedef enum
	{
	    emInit = 0,
	    emRecover = 1
	} emInitType;

	//初始
	CBitmap();
	~CBitmap();
	
	//unBitWords 每个数字使用多少bit,支持1-4
	static ssize_t CountBaseSize(size_t unBitCount,size_t unBitWords=1);
	ssize_t AttachMem(char* pMem,size_t unMemSize,size_t unBitWords=1,ssize_t iInitType=emInit);

	/*
		iDumpMin: dump的时间间隔(min),0表示此条件无效
		DumpFile: dump出的文件名
		iDumpType: dump的类型
		iBinLogOpen: 是否打开binlog
		pBinLogBaseName: binlog的名字
		iMaxBinLogSize: 单个binlog文件最大数据量
		iMaxBinLogNum:总的binlog文件最大数目
	*/	
	ssize_t DumpInit(ssize_t iDumpMin,char *DumpFile="cache.dump",ssize_t iBinLogOpen=0,
		char * pBinLogBaseName="binlog_", ssize_t iMaxBinLogSize=50000000, ssize_t iMaxBinLogNum=10);
	ssize_t StartUp();	
	ssize_t TimeTick(time_t iNow=0);		//循环调用,dump

	//操作
	ssize_t SetBit(size_t unBitPos,ssize_t iBitVal);
	ssize_t GetBit(size_t unBitPos);

	//属性
	ssize_t GetUsage(size_t punValStat[2*2*2*2]);
	//能够表达的数量
	size_t GetBitCount(){return m_unMapSize*8;}
	//体部占用空间
	size_t GetBodySize(){return m_unBodySize;}
	
	//备份
	ssize_t CoreDump();
	
private:
	ssize_t _SetBit(size_t unBitPos,ssize_t iBitVal);
	
	ssize_t _SetBit(char* pBitMem,size_t unBitPos);
	ssize_t _ClearBit(char* pBitMem,size_t unBitPos);
	ssize_t _IsBitSet(char* pBitMem,size_t unBitPos);
	
	ssize_t _CoreDump();
	ssize_t _CoreRecover();
	
private:
	enum
	{
		DF_DATA_DIRTY = 0x1
	};
	typedef struct
	{
		size_t	m_unDataFlag;
		time_t	m_tLastDumpTime;	

		size_t m_unBitWords;		//每个数字使用多少bit
		size_t m_unValStat[2*2*2*2]; //4BIT 最多表达16种值
	}THeadInfo;

	THeadInfo* m_pHeadInfo;
	char* m_pBodyMem;
	size_t m_unBodySize;

	char* m_pBitMem[4];
	size_t m_unMapSize;
	
	ssize_t m_iInitType;

	//二进制日志
	CBinLog m_stBinLog;	

	ssize_t m_iBinLogOpen;
	ssize_t m_iDumpMin;
	char m_szDumpFile[256];
	ssize_t m_iTotalBinLogSize;		//binlog的最大数据量
	
};

#endif

