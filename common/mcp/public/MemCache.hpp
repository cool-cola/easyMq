/**************************************************************************

DESC: 不定长数据块哈希表,组合运用IdxObj组件key->blob

AUTHOR: nekeyzhong 

DATE: 2007年9月

PROJ: Part of MCP Project

Set/Get 1024字节测试:
写入 	<1us(nobinlog)	18us(binlog)
读取	<1us
 **************************************************************************/
/**************************************************************************
编译项目:
 g++ -g -DNODE_RESERVE_LEN=0 -DHASH_KEY_LEN=4 -DNODE_EXPIRE
 g++ -g -DNODE_RESERVE_LEN=0 -DHASH_KEY_LEN=16
 
 **************************************************************************/
#ifndef _MEMCACHE_HPP
#define _MEMCACHE_HPP

#include "IdxObjMng.hpp"
#include "Binlog.hpp"

//设置TBucketNode的主键
ssize_t SetBucketNodeKey(void* pObj,void* pKey,ssize_t iKeyLen);
//获取TBucketNode的主键
ssize_t GetBucketNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen);

//match return 0
typedef ssize_t (*DUMP_MATCH_FUNC)(void* pHashNode,void* pArg);

class MemCache
{
public:	
	typedef struct
	{
		char m_szKey[HASH_KEY_LEN];
		ssize_t m_iFlag;							//脏标记	
		char m_szReserve[NODE_RESERVE_LEN];	//保留区
	}THashNode;

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
		E_NO_DATA = -1000,
		E_BUFF_TOO_SMALL = -1001,	
		E_RESERVE_BUFF_SMALL = -1002,
		E_NO_HASH_SPACE = -1003,
		E_NO_OBJ_SPACE = -1004,
		
	};
	
	MemCache();
	~MemCache();

	//初始
	static ssize_t CountBaseSize(ssize_t iHashNum,ssize_t iBlockSize=512);
	char* GetMemPtr(){return m_pMemPtr;}
	ssize_t GetBlockSize(){return m_stBuffObjMng.GetObjSize();}
	
	/*绑定内存,iBlockSize 越大，对大数据的处理越快,
		iBlockSize=0表示不需要数据区
	*/
	ssize_t AttachMem(char* pMemPtr,const ssize_t MEMSIZE,ssize_t iHashNum,ssize_t iInitType=emInit,ssize_t iBlockSize=512);

	/*
		iDumpMin: dump的时间间隔(min),0表示此条件无效
		DumpFile: dump出的文件名
		iDumpType: dump的类型
		iBinLogOpen: 是否打开binlog
		pBinLogBaseName: binlog的名字
		iMaxBinLogSize: 单个binlog文件最大数据量
		iMaxBinLogNum:总的binlog文件最大数目
	*/
	ssize_t DumpInit(ssize_t iDumpMin,char *DumpFile="cache.dump",ssize_t iDumpType=DUMP_TYPE_NODE,
		ssize_t iBinLogOpen=0,char * pBinLogBaseName="binlog", ssize_t iMaxBinLogSize=50000000, 
		ssize_t iMaxBinLogNum=20,ssize_t iBinlogCommitSecs=0);
	ssize_t StartUp();	
	ssize_t TimeTick(time_t iNow=0);		//循环调用,dump

	//node节点操作
	ssize_t GetNodeFlg(char szKey[HASH_KEY_LEN]);
	ssize_t SetNodeFlg(char szKey[HASH_KEY_LEN],ssize_t iFlag);	
	const char* GetReserve(char szKey[HASH_KEY_LEN]);
	ssize_t SetReserve(char szKey[HASH_KEY_LEN],char* pReserve,ssize_t iReserveLen);
	
	//data数据操作	
	ssize_t Set(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,
			ssize_t iDataFlag=F_DATA_DIRTY,char* pReserve=NULL,ssize_t iReserveLen=0);
	ssize_t Get(char szKey[HASH_KEY_LEN],char* szData,const ssize_t DATASIZE, size_t &unDataLen);
	ssize_t Del(char szKey[HASH_KEY_LEN]);

#ifdef _BUFFMNG_APPEND_SKIP	
	ssize_t Append(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize,ssize_t iDataFlag=F_DATA_DIRTY);
#endif

	//节点遍历操作
	CHashTab* GetBucketHashTab() {return &m_stBucketHashTab;}

	//属性操作	
	ssize_t GetDataSize(char szKey[HASH_KEY_LEN]);
	
	//总属性	
	ssize_t GetUsage(ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount);
	THashNode* GetOldestNode();
	void Print(FILE *fpOut);

	//迁移使用
	ssize_t CoreDumpMem(char* pBuffer,const ssize_t BUFFSIZE,DUMP_MATCH_FUNC fDumpMatchFunc,void* pArg);
	ssize_t CoreRecoverMem(char* pBuffer,ssize_t iBufferSize);
	ssize_t CleanNode(DUMP_MATCH_FUNC fMatchFunc,void* pArg);

	//备份	
	ssize_t CoreDump();
private:	
	ssize_t _WriteNodeLog(char szKey[HASH_KEY_LEN]);
	ssize_t _WriteDataLog(char szKey[HASH_KEY_LEN],char* pBuffer=NULL,ssize_t iBufferSize=0);
	
	ssize_t _CoreDump(ssize_t iType);
	ssize_t _CoreRecover(ssize_t iType);

	ssize_t _Set(THashNode* pHashNode,char* pBlockBuffer,ssize_t iBufferLen);
	ssize_t _Set(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize);
	ssize_t _Del(char szKey[HASH_KEY_LEN]);

#ifdef _BUFFMNG_APPEND_SKIP		
	ssize_t _Append(char szKey[HASH_KEY_LEN],char* szData,ssize_t iDataSize);
#endif

private:

	char *m_pMemPtr;
	ssize_t m_iMemSize;
	ssize_t m_iDumpType;
	
	typedef struct
	{
		time_t m_tLastDumpTime;
	}TMemCacheHead;

	TMemCacheHead* m_pMemCacheHead;
	
	//头部哈希
	TIdxObjMng m_stHashObjMng;
	CHashTab m_stBucketHashTab;
	
	//LRU淘汰链
	CObjQueue m_stLRUQueue;

	//真实数据空间
	TIdxObjMng m_stBuffObjMng;	
	CBuffMng m_stBuffMng;

	//二进制日志
	CBinLog m_stBinLog;

	//临时数据
	ssize_t m_iInitType;

	ssize_t m_iBinLogOpen;
	ssize_t m_iDumpMin;
	char m_szDumpFile[256];
	ssize_t m_iTotalBinLogSize;		//binlog的最大数据量
	ssize_t m_iBinlogCommitSecs;
	time_t m_tLastRefreshTime;

	static const ssize_t BUFFSIZE = 32*1024*1024;
	char* m_pBuffer;
};

#endif	/*_MEMCACHE_HPP*/
