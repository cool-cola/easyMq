/**************************************************************************

DESC: 链表式缓存器key->block1->block2->block3...

AUTHOR: nekeyzhong 

DATE: 2008年1月

PROJ: Part of MCP Project

Set/Get 1024字节测试:
 **************************************************************************/ 
 /**************************************************************************
编译项目:
_HASH_LONG_KEY16	16字节长度key
_HASH_LONG_KEY32	32字节长度key(默认4字节)

_HASH_NODE_DIRTY	支持脏数据
HASH_NODE_RESERVE_LEN	定义节点内保留字段长度
 **************************************************************************/
#ifndef _NODECACHE_HPP
#define _NODECACHE_HPP

#include "IdxObjMng.hpp"
#include "BinlogNR.hpp"

#ifndef HASH_NODE_RESERVE_LEN
#define HASH_NODE_RESERVE_LEN		0
#endif

//设置TBucketNode的主键
ssize_t SetBucketNodeKey(void* pObj,void* pKey,ssize_t iKeyLen);
//获取TBucketNode的主键
ssize_t GetBucketNodeKey(void* pObj,void* pKey,ssize_t &iKeyLen);

//match return 0
typedef ssize_t (*DUMP_MATCH_FUNC)(void* pHashNode,void* pArg);

class NodeCache
{
public:	
#ifdef _HASH_LONG_KEY16
	const static ssize_t HASH_KEY_LEN=16;
#elif  _HASH_LONG_KEY32
	const static ssize_t HASH_KEY_LEN=32;
#else
	const static ssize_t HASH_KEY_LEN= 4;
#endif

	typedef struct
	{
		char m_szKey[HASH_KEY_LEN];

#ifdef _HASH_NODE_DIRTY		
		ssize_t m_iFlag;
#endif

	//保留区
	char m_szReserve[HASH_NODE_RESERVE_LEN];

	}THashNode;

	enum
	{
		F_DATA_CLEAN = 0,
		F_DATA_DIRTY = 1,
	};

	enum
	{
		DUMP_TYPE_MIRROR,
		DUMP_TYPE_NODE,
#ifdef _HASH_NODE_DIRTY		
		DUMP_TYPE_DIRTY_NODE,
#endif		
	};
	
	enum
	{
		E_PAGE_NO_DATA = -1000,
		E_BUFF_TOO_SMALL = -1001,	
		E_NO_HASH_SPACE = -1002,
		E_NO_OBJ_SPACE = -1003,
	};

	NodeCache();
	~NodeCache();

	//初始
	static ssize_t CountBaseSize(ssize_t iHashNum,ssize_t iBlockSize=512);
	char* GetMemPtr(){return m_pMemPtr;}
	ssize_t GetBlockSize(){return m_stBlockObjMng.GetObjSize();}
	
	//绑定内存,iBlockSize=0表示不需要数据区
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
	ssize_t DumpInit(ssize_t iDumpMin,
				char *DumpFile="cache.dump",
				ssize_t iDumpType=DUMP_TYPE_NODE,
				ssize_t iBinLogOpen=0,
				char * pBinLogBaseName="binlog",
				ssize_t iMaxBinLogSize=20000000,
				ssize_t iMaxBinLogNum=20,
				ssize_t iBinlogCommitSecs=0);
	ssize_t StartUp();	
	ssize_t TimeTick(time_t iNow=0);//循环调用,dump

	//node节点操作
	ssize_t GetNodeFlg(char szKey[HASH_KEY_LEN]);
	ssize_t SetNodeFlg(char szKey[HASH_KEY_LEN],ssize_t iFlag);
	char* GetReserve(char szKey[HASH_KEY_LEN]);
	ssize_t SetReserve(char szKey[HASH_KEY_LEN],char* pReserve,ssize_t iReserveLen);	
	
	//block块操作
	ssize_t Set(char szKey[HASH_KEY_LEN],char* pBlockBuffer,ssize_t iBufferLen,
				ssize_t iDataFlag=F_DATA_DIRTY,char* pReserve=NULL,ssize_t iReserveLen=0);
	ssize_t Del(char szKey[HASH_KEY_LEN]);	
	ssize_t SelectBlock(char szKey[HASH_KEY_LEN],char* pBuffer,const ssize_t iBUFFSIZE,BLOCK_SELECT_FUNC fBlockSelectFunc=NULL,void* pSelectArg=NULL,ssize_t iStartBlockPos=0,ssize_t iBlockNum=-1);	
	ssize_t FindBlock(char szKey[HASH_KEY_LEN],BLOCK_FIND_EQU_FUNC fFindEquFunc,void* pFindArg=NULL);	
	ssize_t AppendBlock(char szKey[HASH_KEY_LEN],char* pBlockData,ssize_t iDataFlag=F_DATA_DIRTY);
	ssize_t InsertBlock(char szKey[HASH_KEY_LEN],char* pBlockData,ssize_t iInsertPos=0,ssize_t iDataFlag=F_DATA_DIRTY);
	ssize_t DeleteBlock(char szKey[HASH_KEY_LEN],BLOCK_DEL_EQU_FUNC fDelEquFunc,void* pDeleteArg=NULL);
	ssize_t DeleteBlock(char szKey[HASH_KEY_LEN],ssize_t iPos);
	ssize_t TrimTail(char szKey[HASH_KEY_LEN],ssize_t iBlockNum);

	//节点遍历操作
	CHashTab* GetBucketHashTab() {return &m_stBucketHashTab;}
		
	//属性操作
	ssize_t GetBlockObjSize();
	ssize_t GetBlockNum(char szKey[HASH_KEY_LEN]);
	char* GetBlockData(char szKey[HASH_KEY_LEN],ssize_t iPos);

	char* GetFirstBlockData(char szKey[HASH_KEY_LEN]);
	/*
	通过指针位置偏移计算索引位置,得到下一个节点,
	pBlockData必须是原始位置
	*/
	char* GetNextBlockData(char* pBlockData);
	ssize_t GetBlockPos(char szKey[HASH_KEY_LEN],char* pBlockData);

	//总属性
	ssize_t GetUsage(ssize_t &iHashUsed,ssize_t &iHashCount,ssize_t &iBlockUsed,ssize_t &iBlockCount);
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

	THashNode* _GetHashNode(char szKey[HASH_KEY_LEN]);
	ssize_t _AppendBlock(char szKey[HASH_KEY_LEN],char* pBlockData);
	ssize_t _InsertBlock(char szKey[HASH_KEY_LEN],char* pBlockData,ssize_t iInsertPos);
	ssize_t _Del(char szKey[HASH_KEY_LEN]);
	ssize_t _Set(THashNode* pHashNode,char* pBlockBuffer,ssize_t iBufferLen);
	ssize_t _Set(char szKey[HASH_KEY_LEN],char* pBlockBuffer,ssize_t iBufferLen);
	ssize_t _DeleteBlock(char szKey[HASH_KEY_LEN],ssize_t iPos);
	ssize_t _TrimTail(char szKey[HASH_KEY_LEN],ssize_t iBlockNum);
	ssize_t _SetAttribute(char szKey[HASH_KEY_LEN],char* pAttribute,const ssize_t iAttributeLen);
private:	

	char *m_pMemPtr;
	ssize_t m_iMemSize;
	ssize_t m_iDumpType;
	
	typedef struct
	{
		time_t m_tLastDumpTime;
	}TNodeCacheHead;

	TNodeCacheHead* m_pNodeCacheHead;
 
	//头部哈希
	TIdxObjMng m_stHashObjMng;
	CHashTab m_stBucketHashTab;
	
	//LRU淘汰链
	CObjQueue m_stLRUQueue;

	//真实数据空间
	TIdxObjMng m_stBlockObjMng;	
	CBlockMng m_stBlockMng;

	ssize_t m_iInitType;
	
	//二进制日志
	CBinLog m_stBinLog;
	ssize_t m_iBinLogOpen;
	ssize_t m_iDumpMin;
	char m_szDumpFile[256];
	ssize_t m_iTotalBinLogSize;
	ssize_t m_iBinlogCommitSecs;
	time_t m_tLastRefreshTime;

	//自身使用的buffer
	static const ssize_t BUFFSIZE = 32*1024*1024;
	char* m_pBuffer;
};

#endif	/*_MEMCACHE_HPP*/
