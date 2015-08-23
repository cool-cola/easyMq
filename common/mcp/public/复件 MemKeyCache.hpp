/**************************************************************************

DESC: ���������ݿ��ϣ��,�������IdxObj���key->blob

AUTHOR: nekeyzhong 

DATE: 2007��9��

PROJ: Part of MCP Project

Set/Get ��1024�ֽڲ���:
д�� 	2us(nobinlog)	10us(binlog)
��ȡ	2us

memkeycache  256chunk ��200data��-O2
107w��set 
117w�� get 

 **************************************************************************/
/**************************************************************************
 **************************************************************************/
#ifndef _MEMKEYCACHE_HPP
#define _MEMKEYCACHE_HPP

#include <pthread.h>

#include "IdxObjMng.hpp"
#include "Binlog.hpp"

//match return 0
typedef ssize_t (*DUMP_MATCH_FUNC)(void* pKey,ssize_t iKeyLen,ssize_t iFlag,void* pArg);

class MemKeyCache
{
public:	
	enum
	{
		F_DATA_CLEAN = 0x0,
		F_DATA_DIRTY = 0x1,
		F_DATA_DEL = 0x2,
	};
	
	enum
	{
		DUMP_TYPE_MIRROR,
		DUMP_TYPE_NODE,	
		DUMP_TYPE_DIRTY_NODE,		
	};	

	enum
	{
		SUCCESS = 0,
		E_NO_DATA = -1000,
		E_KEY_BUFF_SMALL = -1001,		
		E_DATA_BUFF_SMALL = -1002,
		E_RESERVE_BUFF_SMALL = -1003,
		E_PARAM = -1004,
		E_NO_SPACE = -1005,
		E_WRITE_BINLOG = -1006
	};
	
	MemKeyCache();
	~MemKeyCache();

	//��ʼ
	static ssize_t CountBaseSize(ssize_t iHashNum,ssize_t iBlockSize=512);
	char* GetMemPtr(){return m_pMemPtr;}
	ssize_t GetBlockSize(){return m_BuffMng.GetBlockSize();}
	
	/*���ڴ�,iBlockSize Խ�󣬶Դ����ݵĴ���Խ��,
		iBlockSize=0��ʾ����Ҫ������
	*/
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iNodeNum,ssize_t iInitType=emInit,ssize_t iBlockSize=512);

	/*
		iDumpMin: dump��ʱ����(min),0��ʾ��������Ч
		DumpFile: dump�����ļ���
		iDumpType: dump������
		iBinLogOpen: �Ƿ��binlog
		pBinLogBaseName: binlog������
		iMaxBinLogSize: ����binlog�ļ����������
		iMaxBinLogNum:�ܵ�binlog�ļ������Ŀ
	*/
	ssize_t DumpInit(ssize_t iDumpMin=24*60,char *DumpFile="cache.dump",
				ssize_t iDumpType=DUMP_TYPE_DIRTY_NODE,
				ssize_t iBinLogOpen=0,char * pBinLogBaseName="binlog", 
				ssize_t iMaxBinLogSize=30000000, 
				ssize_t iMaxBinLogNum=10,ssize_t iBinlogCommitSecs=0);
	ssize_t StartRecover();	
	ssize_t TimeTick(time_t iNow=0);		//ѭ������,dump	
	
	//�û�����	
	ssize_t MarkFlag(const char* pKey,const ssize_t KEYSIZE,ssize_t iDataFlag);
	ssize_t Set(const char* pKey,const ssize_t KEYSIZE,char* szData,ssize_t iDataSize,ssize_t iDataFlag=F_DATA_DIRTY,
			char* pReserve=NULL,ssize_t iReserveLen=0);
	//�������ݳ���
	ssize_t Get(const char* pKey,const ssize_t KEYSIZE,
				char* pData,const ssize_t DATASIZE,ssize_t &iDataFlag)
	{
		ssize_t iReserveLen;
		return Get(pKey,KEYSIZE,pData,DATASIZE,iDataFlag,NULL,0,iReserveLen);
	}
	ssize_t Get(const char* pKey,const ssize_t KEYSIZE,
				char* pData,const ssize_t DATASIZE,ssize_t &iDataFlag,
				char* pReserve,const ssize_t RESERVESIZE,ssize_t& iReserveLen);	
	ssize_t GetNoLRU(const char* pKey,const ssize_t KEYSIZE,
				char* pData,const ssize_t DATASIZE,ssize_t &iDataFlag,
				char* pReserve,const ssize_t RESERVESIZE,ssize_t& iReserveLen);		
	ssize_t Del(const char* pKey,const ssize_t KEYSIZE);
	ssize_t Append(const char* pKey,const ssize_t KEYSIZE,char* szData,ssize_t iDataSize,ssize_t iDataFlag);

	ssize_t GetWListLen(){return m_NodeWLruQueue.GetLength();};
	ssize_t GetRListLen(){return m_NodeRLruQueue.GetLength();};
	ssize_t GetMemSize(){return m_iMemSize;};
	
	//�ڵ��������
	ssize_t GetBucketNum() {return m_pMemCacheHead->m_iBucketNum;}
	ssize_t GetBucketFirstNodeIdx(ssize_t iBucketIdx) {return m_piBucket[iBucketIdx];}
	//return keylen
	ssize_t GetKeyByIdx(ssize_t iNodeIdx,char* pKey=NULL,const ssize_t KEYSIZE=0);
	//return datalen
	ssize_t GetDataByIdx(ssize_t iNodeIdx,
					char* pData,const ssize_t DATASIZE,ssize_t &iDataFlag,
					char* pReserve/*=NULL*/,const ssize_t RESERVESIZE,ssize_t &iReserveLen);
	ssize_t GetBucketNextNodeIdx(ssize_t iNodeIdx)
	{return m_NodeObjMng.GetDsIdx(iNodeIdx,m_pMemCacheHead->m_iDSSuffix);};

	//���Բ���	
	ssize_t GetBufferSize(ssize_t iNodeIdx);
	ssize_t GetNodeIdx(const char* pKey,const ssize_t KEYSIZE);
	ssize_t GetFlag(ssize_t iNodeIdx);

	//lru
	
	enum
	{
		em_r_list = 0,
		em_w_list,
	};
	//return key len
	ssize_t GetOldestKey(char *pKey,const ssize_t KEYSIZE,ssize_t iLRUType);
	ssize_t GetOldestNodeIdx(ssize_t iLRUType);
	ssize_t GetNextLruNodeIdx(ssize_t iNodeIdx);

	//������	
	ssize_t GetUsage(ssize_t &iBucketUsed,ssize_t &iBucketNum,
					ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,
					ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount,
					ssize_t &iDirtyNodeCnt);
	void Print(FILE *fpOut);
	void PrintBinLog(FILE *fpOut);

	//Ǩ��ʹ��
	ssize_t CoreDumpMem(char* pBuffer,const ssize_t BUFFSIZE,DUMP_MATCH_FUNC fDumpMatchFunc,void* pArg);
	ssize_t CoreRecoverMem(char* pBuffer,ssize_t iBufferSize);
	ssize_t CleanNode(DUMP_MATCH_FUNC fMatchFunc,void* pArg);

	//����	
	ssize_t CoreDump();

	//lock
	void LockCache(){pthread_mutex_lock(&m_MutexLock);};
	void UnLockCache(){pthread_mutex_unlock(&m_MutexLock);}
	
private:	
	ssize_t _WriteDataLog(ssize_t iOp,const char* pKey,const ssize_t KEYSIZE,char* pData,ssize_t iDataSize,ssize_t iDataFlag,
			char* pReserve=NULL,ssize_t iReserveLen=0);
	
	ssize_t _CoreDump(ssize_t iType);
	ssize_t _CoreRecover(ssize_t iType);

	ssize_t _Set(const char* pKey,const ssize_t KEYSIZE,
				char* szData,ssize_t iDataSize,ssize_t iDataFlag,ssize_t &iOldDataFlag,
				char* pReserve,ssize_t iReserveLen);
	ssize_t _Del(const char* pKey,const ssize_t KEYSIZE,ssize_t &iOldDataFlag);
private:

	char *m_pMemPtr;
	ssize_t m_iMemSize;
	ssize_t m_iDumpType;
	
	typedef struct
	{
		int32_t m_iDataOK;
		time_t m_tLastDumpTime;
		ssize_t m_iDSSuffix;
		ssize_t m_iBucketUsed;
		ssize_t m_iBucketNum;
	}TMemCacheHead;

	TMemCacheHead* m_pMemCacheHead;
	ssize_t* m_piBucket;
	TIdxObjMng m_NodeObjMng;
	
	//LRU��̭��
	CObjQueue m_NodeRLruQueue;
	CObjQueue m_NodeWLruQueue;

	//��ʵ���ݿռ�
	CBuffMng m_BuffMng;	
	TIdxObjMng m_BuffObjMng;	

	//��������־
	CBinLog m_stBinLog;

	//��ʱ����
	ssize_t m_iInitType;

	ssize_t m_iBinLogOpen;
	ssize_t m_iDumpMin;
	char m_szDumpFile[256];
	ssize_t m_iTotalBinLogSize;		//binlog�����������
	ssize_t m_iBinlogCommitSecs;
	time_t m_tLastRefreshTime;

	static const ssize_t BUFFSIZE = 32*1024*1024;
	char* m_pBuffer;

	pthread_mutex_t m_MutexLock;
};

#endif	/*_MEMCACHE_HPP*/

