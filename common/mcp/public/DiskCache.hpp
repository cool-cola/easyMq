/**************************************************************************

DESC: 不定长数据块哈希表,组合运用IdxObj组件key->blob

AUTHOR: nekeyzhong 

DATE: 2007年9月

PROJ: Part of MCP Project

 **************************************************************************/
/**************************************************************************
编译项目:
 g++ -g -DNODE_RESERVE_LEN=0 -DHASH_KEY_LEN=4 -DNODE_EXPIRE
 g++ -g -DNODE_RESERVE_LEN=0 -DHASH_KEY_LEN=16

 dd if=/dev/zero of=/data/aa bs=1024 count=1000
 dd if=/dev/zero of=/data/aa bs=1G count=1
 
 **************************************************************************/
#ifndef _DISKCACHE_HPP
#define _DISKCACHE_HPP

#include "IdxObjMng.hpp"
#include "Binlog.hpp"
#include "LockGuard.hpp"

//设置TBucketNode的主键
ssize_t SetBucketNodeKey(void* pObj,void* pKey,ssize_t iKeyLen);
//获取TBucketNode的主键
ssize_t GetBucketNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen);

class DiskCache
{
public:	

#pragma pack(1)
	typedef struct
	{
		char m_szKey[HASH_KEY_LEN];
		char m_cFlag;							//操作标记
		int32_t m_iLastUpdate;					//最后更新时间

#ifdef NODE_EXPIRE
		int32_t m_iExpireTime;
#endif
		char m_szReserve[NODE_RESERVE_LEN];		//给用户使用的保留区
	}THashNode;
#pragma pack()

	typedef enum tagENodeWFlag
	{
		NODE_WFLAG_INVALID = 0x00,
		NODE_WFLAG_WRITING = 0x01,
		NODE_WFLAG_VALID   = 0x02
	} ENodeWFlag;

	enum
	{
		E_NO_DATA = -1000,
		E_BUFF_TOO_SMALL = -1001,	
		E_RESERVE_BUFF_SMALL = -1002,
		E_NO_HASH_SPACE = -1003,
		E_NO_OBJ_SPACE = -1004,
		E_ERROR = -1005,
		E_READING  = -1006,
		E_WRITTING = -1007,
		E_ERR_WRITE_BINLOG = -1008,
		E_INVALID  = -1009,
	};
	
	DiskCache();
	~DiskCache();

//-------------初始操作	-----------------	
	static size_t CountMemSize(ssize_t iBucketNum/*=0*/,size_t iHashNum/*=0*/,size_t FILESIZE,size_t iBlockSize=512);
	char* GetMemPtr(){return m_pMemPtr;}
	size_t GetBlockSize(){return m_stBuffObjMng.GetObjSize();}
	ssize_t AttachMemFile(char* pMemPtr,const size_t MEMSIZE, ssize_t iInitType,
				ssize_t iBucketNum/*=0*/,size_t iHashNum/*=0*/,
				char* pDiskFile,size_t iDiskFileStartPos/*=0*/,const size_t FILESIZE,size_t iBlockSize=512);

	/*
		DumpFile: dump出的文件名
		pBinLogBaseName: binlog的名字
		iMaxBinLogSize: 单个binlog文件最大数据量
		iMaxBinLogNum:总的binlog文件最大数目
	*/
	ssize_t DumpInit(char *DumpFile="cache.dump",char * pBinLogBaseName="binlog_", ssize_t iMaxBinLogSize=50000000, 
						ssize_t iMaxBinLogNum=20);
	ssize_t StartUp();	
	ssize_t GetCurrBinLogSize(){return m_stBinLog.GetBinLogTotalSize();}

//-------------数据操作	(改变LRU)-----------------
	
	//node
	ssize_t GetNodeReserve(char szKey[HASH_KEY_LEN],char* pReserve,const ssize_t RESERVELEN=NODE_RESERVE_LEN);
	ssize_t SetNodeReserve(char szKey[HASH_KEY_LEN],char* pReserve,const ssize_t RESERVELEN=NODE_RESERVE_LEN);
	
	//data
	ssize_t Set(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,char* pReserve=NULL,ssize_t iReserveLen=0);
	ssize_t SetNoLock(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,char* pReserve=NULL,ssize_t iReserveLen=0);
	ssize_t Get(char szKey[HASH_KEY_LEN],char* szData,const ssize_t DATASIZE,char* pReserve=NULL,const ssize_t RESERVELEN=NODE_RESERVE_LEN);	
	ssize_t GetNoLockNoLRU(char szKey[HASH_KEY_LEN],char* szData,const ssize_t DATASIZE,char* pReserve=NULL,const ssize_t RESERVELEN=NODE_RESERVE_LEN);
	ssize_t Del(char szKey[HASH_KEY_LEN]);
	ssize_t DelNoLock(char szKey[HASH_KEY_LEN]);

	//节点遍历操作,参考Print方法
	CHashTab* GetBucketHashTab() {return &m_stBucketHashTab;}
	CBuffMng* GetBuffMng() {return &m_stBuffMng;}

//-------------属性操作	(不改变LRU)-----------------
	ssize_t GetDataSize(char szKey[HASH_KEY_LEN]);
	ssize_t GetNode(char szKey[HASH_KEY_LEN],THashNode *pHashNode);	
#ifdef NODE_EXPIRE
	ssize_t SetExpireTime(char szKey[HASH_KEY_LEN],int32_t iExpireTime);
#endif	

	void LockCache();
	void UnLockCache();	

	//总属性	
	ssize_t GetUsage(ssize_t &iBucketNum,ssize_t &iUsedBucket,
							ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,
							ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount);
	ssize_t GetOldestNode(THashNode* pHashNode);
	void Print(FILE *fpOut);
	//外面若未告知时间,则自行time(NULL),否则使用外面告知的时间,(初始化为0)
	void SetTimeNow(int32_t iTimeNow){m_iTimeNow = iTimeNow;}

	//备份	
	ssize_t CoreDump();
	ssize_t CoreDumpNoLock();
	CBinLog* GetBinLog(){return &m_stBinLog;}

	ssize_t _WriteNodeLog(ssize_t iOp,THashNode* pHashNode,ssize_t iDataSize);
	ssize_t _Del(THashNode* pHashNode);	

private:	
	ssize_t _CoreDump();
	ssize_t _CoreRecover();
	ssize_t _Set(THashNode* pHashNode,ssize_t iDataSize);

private:
	int32_t m_iTimeNow;

	char *m_pMemPtr;
	ssize_t m_iMemSize;
	
	typedef struct
	{
		int32_t m_iDataOK;			//数据是否一致
	}TMemCacheHead;

	TMemCacheHead* m_pMemCacheHead;
	
	//头部哈希
	CHashTab m_stBucketHashTab;	
	TIdxObjMng m_stHashObjMng;

	//LRU淘汰链
	CObjQueue m_stLRUQueue;

	//真实数据空间
	CBuffMng m_stBuffMng;	
	TDiskIdxObjMng m_stBuffObjMng;	

	//二进制日志
	CBinLog m_stBinLog;

	//临时数据
	ssize_t m_iInitType;

	char m_szDumpFile[256];

	static const ssize_t BUFFSIZE = 32*1024*1024;
	char* m_pBuffer;

	pthread_mutex_t m_MutexLock;
};

#endif	/*_MEMCACHE_HPP*/
