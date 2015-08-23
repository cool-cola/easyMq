#ifndef __CCNS_KEY_VALUE_HPP__
#define  __CCNS_KEY_VALUE_HPP__
#include "MemKeyCache.hpp"
#include "Base.hpp"
#include "tlib_cfg.h"
#include <map>
#include <set>
#include "ccns_define.hpp"

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN 16777216
#endif
/*
enum en_protocol_id
{
	EN_PROTOCOL_UNKOWN = -1,
	EN_PROTOCOL_INTERNAL = 0,
	EN_PROTOCOL_EXT_ASN13 = 1,
	EN_PROTOCOL_EXT_TDB21 = 2,
	EN_PROTOCOL_EXT_MEMCACHED = 3,
	EN_PROTOCOL_NUM = 50,

};
*/

class CCCNSNameValueMng
{
public:
	CCCNSNameValueMng(){}
	virtual ~CCCNSNameValueMng(){}
	virtual int Set(char *pKey, int iKeyLen, char *pData, int iDataLen)
	{
		if(iDataLen > MAX_MSG_LEN)
		{
			return -1;
		}
		string key, data;
		key.assign(pKey, iKeyLen);
		data.assign(pData, iDataLen);
		m_map_keydata[key] = data;
		
		return 0;
	}
	virtual int Get(char *pKey, int iKeyLen, char *&pData, int &iDataLen)
	{
		iDataLen = 0;
		string key;
		key.assign(pKey, iKeyLen);
		std::map< std::string, std::string >::iterator it = m_map_keydata.find(key);
		if (it != m_map_keydata.end())
		{
			memcpy(m_Buff, it->second.c_str(), it->second.size());
			pData = m_Buff;
			iDataLen = it->second.size();
			return 0;
		}
		return -1;
	}
	virtual int Del(char *pKey, int iKeyLen)
	{
		string key;
		key.assign(pKey, iKeyLen);
		std::map< std::string, std::string >::iterator it = m_map_keydata.find(key);
		if (it != m_map_keydata.end())
		{
			m_map_keydata.erase(it);
			return 0;
		}
		return -1;
	}
	virtual int GetKeyCount(ssize_t &iKeyCount)
	{
		iKeyCount = (ssize_t)m_map_keydata.size();
		return 0;
	}
	virtual void ToString(string &s)
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "map size %llu", (unsigned long long)m_map_keydata.size());
		s = buf;
	}
protected:
	std::map< std::string, std::string > m_map_keydata; 
	char m_Buff[MAX_MSG_LEN];
};

class CCCNSKeyValueMng : public CCCNSNameValueMng
{
public:
	CCCNSKeyValueMng()
	{
		m_pSpinlock = 0;
		m_pMemKeyCache = 0;
		m_p_shm_addr = 0;
		m_iMemSize = 0;
	}
	virtual ~CCCNSKeyValueMng()
	{
	}
	virtual int Init(size_t memlen, unsigned int bloc_size, int id = 0)
	{
		LockInit(id);
		size_t mem_size;
		bool bNewCreate;
		ssize_t iInitType;
		mem_size = memlen;
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "touch .mem_cache_%d_key_value_mng", id);
		system(buffer);
		snprintf(buffer, sizeof(buffer), ".mem_cache_%d_key_value_mng", id);
		key_t shm_key = ftok(buffer, 'A');
		printf("Creating %s cache[%d] shm: key[0x%08x], size[%u B].\n",
			buffer, id, (unsigned int)shm_key,(unsigned int)mem_size);
		assert(shm_key != -1);
		m_pMemKeyCache = new MemKeyCache();
		m_iMemSize = mem_size;
		int err;
		m_p_shm_addr = CreateShm(shm_key, mem_size, err, bNewCreate, 0);
		if (!m_p_shm_addr)
		{
			printf("attach %s key[0x%08x] size[%d] error\n", buffer, (unsigned int)shm_key, (int)mem_size);
			return -1;
		}
		iInitType = bNewCreate?emInit:emRecover;
		ssize_t atBytes = m_pMemKeyCache->AttachMem(m_p_shm_addr, mem_size,
						0, iInitType, bloc_size);
		if (atBytes < 0)
		{
			return -2;
		}
		return 0;
	}
	virtual int Set(char *pKey, int iKeyLen, char *pData, int iDataLen)
	{
		if(iDataLen > MAX_MSG_LEN)
		{
			return -1;
		}
		Lock();
		int ret = m_pMemKeyCache->Set(pKey, iKeyLen,
			pData, iDataLen);
		UnLock();
		return ret;
	}
	virtual int Get(char *pKey, int iKeyLen, char *&pData, int &iDataLen)
	{
		pData = m_Buff;
		iDataLen = 0;
		Lock();
		ssize_t iDataFlag;
		int ret = m_pMemKeyCache->Get(pKey, iKeyLen, m_Buff, MAX_MSG_LEN, iDataFlag);
		UnLock();
		if (ret > 0)
		{
			iDataLen = ret;
			pData = m_Buff;
			return 0;
		}
		else if(ret < 0)
			return -1;
		return 0;
	}
	virtual int Del(char *pKey, int iKeyLen)
	{
		Lock();
		int ret = m_pMemKeyCache->Del(pKey, iKeyLen);		
		UnLock();
		if(ret != 0)
			return -1;
		return 0;
	}
	virtual int GetUsage(ssize_t &iBucketUsed,ssize_t &iBucketNum,
												ssize_t &iHashNodeUsed,ssize_t &iHashNodeCount,
												ssize_t &iObjNodeUsed,ssize_t &iObjNodeCount,
												ssize_t &iDirtyNodeCnt)
	{
		return m_pMemKeyCache->GetUsage(iBucketUsed, iBucketNum,
					iHashNodeUsed, iHashNodeCount,
					iObjNodeUsed,iObjNodeCount,
					iDirtyNodeCnt);
	}

	virtual int GetKeyCount(ssize_t &iKeyCount)
	{
 		ssize_t iBucketUsed;
		ssize_t iBucketNum;
 		ssize_t iHashNodeUsed;
 		ssize_t iHashNodeCount;
 		ssize_t iObjNodeUsed;
		ssize_t iObjNodeCount;
 		ssize_t iDirtyNodeCnt;

		m_pMemKeyCache->GetUsage(iBucketUsed, iBucketNum,
					iHashNodeUsed, iHashNodeCount,
					iObjNodeUsed,iObjNodeCount,
					iDirtyNodeCnt);
		iKeyCount = iHashNodeUsed;
		return 0;
	}
	virtual void ToString(string &s)
	{
		char buf[1024];
		 ssize_t iBucketUsed;
		ssize_t iBucketNum;
 		ssize_t iHashNodeUsed;
 		ssize_t iHashNodeCount;
 		ssize_t iObjNodeUsed;
		ssize_t iObjNodeCount;
 		ssize_t iDirtyNodeCnt;

		m_pMemKeyCache->GetUsage(iBucketUsed, iBucketNum,
					iHashNodeUsed, iHashNodeCount,
					iObjNodeUsed,iObjNodeCount,
					iDirtyNodeCnt);
		snprintf(buf, sizeof(buf), "shm usage iBucketUsed[%llu], iBucketNum[%llu], iHashNodeUsed[%llu], iHashNodeCount[%llu], iObjNodeUsed[%llu],iObjNodeCount[%llu], iDirtyNodeCnt[%llu]",
			(unsigned long long)iBucketUsed, (unsigned long long)iBucketNum,
					(unsigned long long)iHashNodeUsed, (unsigned long long)iHashNodeCount,
					(unsigned long long)iObjNodeUsed, (unsigned long long)iObjNodeCount,
					(unsigned long long)iDirtyNodeCnt);
		s = buf;
	}
protected:
	ssize_t LockInit(int id)
	{
		char buffer[256];
		sprintf(buffer, "touch .key_value_mng_spinlock_%d", id);
		system(buffer);
		sprintf(buffer, ".key_value_mng_spinlock_%d", id);
		key_t shm_key = ftok(buffer, 'L');
		assert(shm_key != -1);
		printf("Creating %s [%d] spinlock: key[0x%08x]\n", buffer, id, (unsigned int)shm_key);
		m_pSpinlock = shm_spin_lock_init(shm_key);
		if ( !m_pSpinlock )
			return -3;
		return 0;
	}	
	void Lock()
	{
		spin_lock(m_pSpinlock);
	}
	void UnLock()
	{
		spin_unlock(m_pSpinlock);
	}
protected:
	spinlock_t* m_pSpinlock;
	size_t m_iMemSize;
	MemKeyCache* m_pMemKeyCache;
	char* m_p_shm_addr;
};

enum
{
	em_ccns_kv_beg = 0,
	em_ccns_bid_hash_key = 0,//路由
	em_ccns_port_proto_key = 1,//端口协议映射
	em_ccns_engine_hash_key = 2,//引擎接入映射
	em_ccns_relation_hash_key = 3,//引擎关系
	em_ccns_main_engine_set_key = 4,//第一个进入的主引擎
	em_ccns_port_ver_key = 5,//端口协议映射ver
	em_ccns_bid_attr_ver = 6,//2013-01-08 整体attr ver，以上均不属于attr，master kv不维护
	em_ccns_bid_support_expire = 7,//2013-01-08 expire支持
	em_ccns_bid_set_expire_time = 8,//2013-01-08 expire 设置时间
	em_ccns_bid_white_ip_protocol = 9,//2013-01-08 whiter protocol
	em_ccns_bid_white_iplist = 10,//2013-01-08 white iplist
	em_ccns_bid_plugin_value_struct = 11,//2013-01-14 plugin engine(tssd etc.) value struct(raw/type1/type2 etc.)
	em_ccns_reverse_relation_hash_key = 12, //not attr, engine reverse relation
	em_ccns_bid_passwd_protocol = 13, //passwd deny or not,  select deny protocol: asn, memcached, tdb, internal
	em_ccns_bid_passwd = 14,
	em_ccns_bid_support_cache = 15,
	em_ccns_bid_enable_fun = 16,
	em_ccns_kv_end,
};
/*
* 2013-01-08
* patxiao
* key type id to key type string, for dump load
*/
class CCNSAttrNameTypeMng
{
public:
	//return key first Byte, convert it to type
	static int Key2Type(const std::string& name, char* pErr="", int iErrLen = 0)
	{
		int *pKey = (int*)name.c_str(); 
		int iType = *pKey;
		if (iType>=em_ccns_kv_beg && iType<em_ccns_kv_end) {
			return iType;
		}
		if (iErrLen) {
			snprintf(pErr, iErrLen, "unknown type %d", iType);			
		}		
		return -1;
	}
	//for dump file
	static  std::string Type2Name(int type)
	{
		std::map< int, std::string >::iterator it = TypeName().find(type);
		if (it != TypeName().end())
		{
			return it->second;
		}
		return std::string("");
	}

	
	static int KeyName(int bid, int type, std::string& strName)
	{	
		return Instance().GetName(bid, type, strName);
	}
	static int Name2Type(const std::string& name)
	{
		std::map< std::string, int >::iterator it =  NameType().find(name);
		if (it != NameType().end())
		{
			return it->second;
		}
		return -1;
	}
	
	static std::map< std::string, int >& NameType()
	{
		return Instance().GetNameType();
	}
	static std::map< int, std::string >& TypeName()
	{
		return Instance().GetTypeName();
	}
	
private:
	CCNSAttrNameTypeMng()
	{
		char *g_name[32] = 
		{
			"bid_hash",
			"port_proto",
			"engine_hash",
			"relation_hash",
			"main_engine_set",
			"port_ver",
			"bid_attr_ver",			
			"bid_support_expire",
			"bid_set_expire_time",
			"bid_white_ip_protocol",			
			"bid_white_iplist",
			"bid_plugin_value_struct",
			"reverse_relation_hash",
			"bid_passwd_protocol",
			"bid_passwd",
			"bid_support_cache",
			"bid_enable_fun"
		};
		for (int i = em_ccns_kv_beg; i < em_ccns_kv_end; i++)
		{
			m_mapAttrNameType[std::string(g_name[i])] = i;
			m_mapAttrTypeName[i] = std::string(std::string(g_name[i]));
		}
	}
	int GetName(int bid, int type, std::string& strName)
	{	
		if (type < em_ccns_kv_beg || type >= em_ccns_kv_end)
			return -1;
		char name[256];
		snprintf(name, 255, "%d_%s", bid, m_mapAttrTypeName[type].c_str());
		strName = std::string(name);
		return 0;
	}
	std::map< std::string, int >& GetNameType()
	{
		return m_mapAttrNameType;
	}
	std::map< int, std::string >& GetTypeName()
	{
		return m_mapAttrTypeName;
	}
	static CCNSAttrNameTypeMng& Instance()
	{
		static CCNSAttrNameTypeMng stAttrNameTypeMng;
		return stAttrNameTypeMng;
	}
	
private:
	std::map< std::string, int > m_mapAttrNameType;
	std::map< int, std::string > m_mapAttrTypeName;
};
/*
* 2013-01-14
* patxiao
* plugin engine(tssd etc.) value struct(raw/type1/type2 etc.)
*/
typedef struct TCCNSBidPluginValueStructKey
{
	TCCNSBidPluginValueStructKey()
	{
		iKeyType = em_ccns_bid_plugin_value_struct;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidPluginValueStructKey;

typedef struct TCCNSBidPluginValueStructValue
{
	enum
	{
		em_valuestruct_unknown = 0,
		em_valuestruct_structurized = 1,
		em_valuestruct_raw = 2,		
	};
	TCCNSBidPluginValueStructValue()
	{
		iPluginValueStruct = 0;
	}
	int iPluginValueStruct;//version
}TCCNSBidPluginValueStructValue;

/*
* 2013-01-08
* patxiao
* bid white ip list key
* value is string, will not define struct
*/
typedef struct TCCNSBidWhiteipListKey
{
	TCCNSBidWhiteipListKey()
	{
		iKeyType = em_ccns_bid_white_iplist;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidWhiteipListKey;

typedef struct TCCNSBidWhiteipProtoKey
{
	TCCNSBidWhiteipProtoKey()
	{
		iKeyType = em_ccns_bid_white_ip_protocol;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidWhiteipProtoKey;

typedef struct TCCNSBidWhiteipProtoValue
{
	TCCNSBidWhiteipProtoValue()
	{
		iWhiteipProtocol = 0;
	}
	int iWhiteipProtocol;//version
}TCCNSBidWhiteipProtoValue;

typedef struct TCCNSBidPasswdProtoKey
{
	TCCNSBidPasswdProtoKey()
	{
		iKeyType = em_ccns_bid_passwd_protocol;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidPasswdProtoKey;

typedef struct TCCNSBidPasswdProtoValue
{
	TCCNSBidPasswdProtoValue()
	{
		iPasswdProtocol = 0;
	}
	int iPasswdProtocol;//version
}TCCNSBidPasswdProtoValue;

typedef struct TCCNSBidPasswdKey
{
	TCCNSBidPasswdKey()
	{
		iKeyType = em_ccns_bid_passwd;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidPasswdKey;

typedef struct TCCNSBidPasswdValue
{
	TCCNSBidPasswdValue()
	{
		iPasswd = 0;
	}
	int iPasswd;
}TCCNSBidPasswdValue;


typedef struct TCCNSBidEnableFunKey
{
	TCCNSBidEnableFunKey()
	{
		iKeyType = em_ccns_bid_enable_fun;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidEnableFunKey;

typedef struct TCCNSBidEnableFunValue
{
	enum
	{
		em_bid_disable_all = 0,
		em_bid_enable_add_cold = 1,
		//em_bid_enable_xx_fun = 1 << 1,
	};
	TCCNSBidEnableFunValue()
	{
		iEnableFun = em_bid_disable_all;
	}
	int iEnableFun;
}TCCNSBidEnableFunValue;


typedef struct TCCNSBidSupportCacheKey
{
	TCCNSBidSupportCacheKey()
	{
		iKeyType = em_ccns_bid_support_cache;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidSupportCacheKey;

typedef struct TCCNSBidSupportCacheValue
{
	enum
	{
		em_bid_supprot_cache_disable = 0,
		em_bid_supprot_cache_enable = 1
	};
	TCCNSBidSupportCacheValue()
	{
		iSupprotCache = em_bid_supprot_cache_disable;
	}
	int iSupprotCache;
}TCCNSBidSupportCacheValue;

typedef struct TCCNSBidSetExpireTimeKey
{
	TCCNSBidSetExpireTimeKey()
	{
		iKeyType = em_ccns_bid_set_expire_time;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidSetExpireTimeKey;

typedef struct TCCNSBidSetExpireTimeValue
{
	TCCNSBidSetExpireTimeValue()
	{
		iTime = 0;
	}
	int iTime;//version
}TCCNSBidSetExpireTimeValue;


typedef struct TCCNSBidSupportExpireKey
{
	TCCNSBidSupportExpireKey()
	{
		iKeyType = em_ccns_bid_support_expire;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidSupportExpireKey;

typedef struct TCCNSBidSupportExpireValue
{
	enum
	{
		em_supportexpire_off = 0,
		em_supportexpire_on = 1,		
	};
	TCCNSBidSupportExpireValue()
	{
		iSupportExpire = 0;
	}
	int iSupportExpire;//version
}TCCNSBidSupportExpireValue;

typedef struct TCCNSBidAttrVerKey
{
	TCCNSBidAttrVerKey()
	{
		iKeyType = em_ccns_bid_attr_ver;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidAttrVerKey;

typedef struct TCCNSBidAttrVerValue
{
	TCCNSBidAttrVerValue(int iVer = 0)
	{
		iAttrVer = iVer;
	}
	int iAttrVer;//version
}TCCNSBidAttrVerValue;

typedef struct TCCNSPortVerKey
{
	TCCNSPortVerKey()
	{
		iKeyType = em_ccns_port_ver_key;
	}
	int iKeyType;
}TCCNSPortVerKey;
typedef struct TCCNSPortVerValue
{
	TCCNSPortVerValue()
	{
		iVer = 0;
	}
	int iVer;
}TCCNSPortVerValue;
typedef struct TCCNSMainEngineSetKey
{
	TCCNSMainEngineSetKey()
	{
		iKeyType = em_ccns_main_engine_set_key;
	}
	int iKeyType;
}TCCNSMainEngineSetKey;

typedef struct TCCNSMainEngineSetValue
{
	TCCNSMainEngineSetValue()
	{
		engineid = -1;
		masterid = 0;
	}
	int engineid;
	int masterid;
}TCCNSMainEngineSetValue;

typedef struct TCCNSBidHashKey
{
	TCCNSBidHashKey()
	{
		iKeyType = em_ccns_bid_hash_key;
		iCCNSBid = -1;
	}
	int iKeyType;
	int iCCNSBid;
}TCCNSBidHashKey;

typedef struct TCCNSBidHashValue
{
	TCCNSBidHashValue()
	{
		iVer = -1;
		iSchedulerType = -1;
		iConsistent = -1;
		iSEIDNum = 0;
	}
	int iVer;//version
	int iSchedulerType;//调度类型,--0, 淘汰调度,writeback 1，镜像调度，一主多从 2,割接调度 3,用户指定存储调度
	int iConsistent;//数据一致性类型,--0内存级别强一致性, 1磁盘级别强一致性, 2, 最终一致性
	int iSEIDNum;
	TStorageEngineItemID arraySEID[MAX_ENGINE_NUM];
}TCCNSBidHashValue;

typedef struct TCCNSEngineHashKey
{
	TCCNSEngineHashKey()
	{
		iKeyType = em_ccns_engine_hash_key;
	}
	bool operator <(const struct TCCNSEngineHashKey &o) const
	{
		if(stSEID < o.stSEID)
			return true;
		return false;
	}
	int iKeyType;
	TStorageEngineItemID stSEID;
}TCCNSEngineHashKey;

typedef struct TCCNSEngineHashValue
{
	TCCNSEngineHashValue()
	{
		iAccessNum = 0;
		iCCNSBid = -1;
		iSchedulerType = em_scheduler_writeback;
		iConsistent = em_consistency_strong;
	}
	int iCCNSBid;
	int iSchedulerType;
	int iConsistent;
	int iAccessNum;
	TCCNSCommonIpAddr arrayAddr[MAX_ACCESS_NUM];
}TCCNSEngineHashValue;


typedef struct TCCNSBidPortProtoKey
{
	TCCNSBidPortProtoKey()
	{
		iKeyType = em_ccns_port_proto_key;
		iPort = -1;
	}
	bool operator <(const struct TCCNSBidPortProtoKey &o) const
	{
		if(iPort < o.iPort)
			return true;
		return false;
	}
	int iKeyType;
	int iPort;//端口
}TCCNSBidPortProtoKey;

typedef struct TCCNSBidPortProtoValue
{
	TCCNSBidPortProtoValue()
	{
		iProto = EN_PROTOCOL_UNKOWN;
		iCCNSBid = 0;
		iStatus = 0;
	}
	int iProto;//协议
	int iCCNSBid;//
	int iStatus; //大小流量
}TCCNSBidPortProtoValue;

//2013-03-04 beg patxiao for del reverse relation
typedef struct TCCNSReverseRelationHashKey
{
	TCCNSReverseRelationHashKey()
	{
		iKeyType = em_ccns_reverse_relation_hash_key;
	}
	int iKeyType;
	TStorageEngineItemID stSEID;
}TCCNSReverseRelationHashKey;

typedef struct TCCNSReverseRelationHashValue
{
	TCCNSReverseRelationHashValue()
	{
		iCCNSBid = -1;
		iSchedulerType = em_scheduler_writeback;
		iConsistent = em_consistency_strong;
	}
	int iCCNSBid;
	int iSchedulerType;
	int iConsistent;
	TStorageEngineItemID stSEID;
}TCCNSReverseRelationHashValue;
//2013-03-04 end patxiao for del reverse relation

typedef struct TCCNSRelationHashKey
{
	TCCNSRelationHashKey()
	{
		iKeyType = em_ccns_relation_hash_key;
	}
	int iKeyType;
	TStorageEngineItemID stSEID;
}TCCNSRelationHashKey;

typedef struct TCCNSRelationHashValue
{
	TCCNSRelationHashValue()
	{
		iCCNSBid = -1;
		iSchedulerType = em_scheduler_writeback;
		iConsistent = em_consistency_strong;
	}
	int iCCNSBid;
	int iSchedulerType;
	int iConsistent;
	TStorageEngineItemID stSEID;
}TCCNSRelationHashValue;

class CCNSAttrMng
{
public:
	CCNSAttrMng()
	{
		m_pKeyValueMng = 0;
		m_iShmStore = 0;
	}
	virtual ~CCNSAttrMng(){}
	int Init(const char* cfg, int iShmStore = 0)
	{
		m_iShmStore = iShmStore;
		if (m_iShmStore)
		{
			int mem_size = 0;
			int block_size = 0;
			TLib_Cfg_GetConfig(cfg,
				"attr_mem_size", CFG_INT, &mem_size, 104857600,
				"attr_block_size", CFG_INT, &block_size, 84,
				NULL);
			m_pKeyValueMng = new CCCNSKeyValueMng;
			assert(m_pKeyValueMng);
			return ((CCCNSKeyValueMng*)m_pKeyValueMng)->Init(mem_size, block_size);
		}
		else
		{
			m_pKeyValueMng = new CCCNSNameValueMng;
			assert(m_pKeyValueMng);			
		}
		return 0;
	}
/*
* create: 2013-01-08
* patxiao
* get all need sync attr
* beg: em_ccns_bid_attr_ver
* end: em_ccns_kv_end
*/	
	virtual int GetBidAttr(int iBid, CCNSNameValueList& stNameValueList)
	{
		int ret = 0;
		if (stNameValueList.Count() > 0)
		{
			stNameValueList.SetCurrToFirst();
			for (CCNSNameValue* pnv = stNameValueList.First();
				pnv != NULL;
				pnv = stNameValueList.GoNext())
			{
				if (pnv->name.Len() == 0)
					continue;
				char *pData = NULL;
				int iDataLen = 0;
				ret = GetData((char *)pnv->name, pnv->name.Len(), pData, iDataLen);
				if (0 == ret)
				{
					pnv->value.Set(pData, iDataLen);
				}
			}
		}
		else
		{		
			//em_ccns_bid_attr_ver = 6,//2013-01-08 整体attr ver，以上均不属于attr，master kv不维护
			TCCNSBidAttrVerKey stAttrVerKey;
			stAttrVerKey.iCCNSBid = iBid;
			char *pData = NULL;
			int iDataLen = 0;
			ret = GetData((char *)&stAttrVerKey, sizeof(stAttrVerKey), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidAttrVerValue))
			{				
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stAttrVerKey, sizeof(stAttrVerKey));
				pnv->value.Set(pData, iDataLen);
			}
			
			//em_ccns_bid_support_expire = 7,//2013-01-08 expire支持
			TCCNSBidSupportExpireKey stSupExpKey;
			stSupExpKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stSupExpKey, sizeof(stSupExpKey), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidSupportExpireValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stSupExpKey, sizeof(stSupExpKey));
				pnv->value.Set(pData, iDataLen);
			}
			
			//em_ccns_bid_set_expire_time = 8,//2013-01-08 expire 设置时间
			TCCNSBidSetExpireTimeKey stSetExpKey;
			stSetExpKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stSetExpKey, sizeof(stSetExpKey), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidSetExpireTimeValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stSetExpKey, sizeof(stSetExpKey));
				pnv->value.Set(pData, iDataLen);
			}

			//em_ccns_bid_white_ip_protocol = 9,//2013-01-08 whiter protocol
			TCCNSBidWhiteipProtoKey stWhiteProtoKey;
			stWhiteProtoKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stWhiteProtoKey, sizeof(stWhiteProtoKey), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidWhiteipProtoValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stWhiteProtoKey, sizeof(stWhiteProtoKey));
				pnv->value.Set(pData, iDataLen);
			}
			
			//em_ccns_bid_white_iplist = 10,//2013-01-08 white iplist
			TCCNSBidWhiteipListKey stWhiteListKey;
			stWhiteListKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stWhiteListKey, sizeof(stWhiteListKey), pData, iDataLen);
			if (0==ret && iDataLen >= 8 && iDataLen%4 == 0)
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stWhiteListKey, sizeof(stWhiteListKey));
				pnv->value.Set(pData, iDataLen);	
			}

			//em_ccns_bid_plugin_value_struct = 11,//2013-01-14 plugin engine(tssd etc.) value struct(raw/type1/type2 etc.)	
			TCCNSBidPluginValueStructKey stPluginValueStructKey;
			stPluginValueStructKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stPluginValueStructKey, sizeof(stPluginValueStructKey), pData, iDataLen);
			if (0==ret && iDataLen == sizeof(TCCNSBidPluginValueStructValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stPluginValueStructKey, sizeof(stPluginValueStructKey));
				pnv->value.Set(pData, iDataLen);	
			}

			//bid passwd protocal
			TCCNSBidPasswdProtoKey stPasswdProtoKey;
			stPasswdProtoKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stPasswdProtoKey, sizeof(stPasswdProtoKey), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidPasswdProtoValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stPasswdProtoKey, sizeof(stPasswdProtoKey));
				pnv->value.Set(pData, iDataLen);
			}

			//bid passwd
			TCCNSBidPasswdKey stPasswdKey;
			stPasswdKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stPasswdKey, sizeof(stPasswdKey), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidPasswdValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stPasswdKey, sizeof(stPasswdKey));
				pnv->value.Set(pData, iDataLen);
			}

			// enable fun
			TCCNSBidEnableFunKey stEnableFunKey;
			stEnableFunKey.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stEnableFunKey, sizeof(stEnableFunKey), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidEnableFunValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stEnableFunKey, sizeof(stEnableFunKey));
				pnv->value.Set(pData, iDataLen);
			}

			//em_ccns_bid_support_cache = 15
			TCCNSBidSupportCacheKey stSupprotCache;
			stSupprotCache.iCCNSBid = iBid;
			pData = NULL;
			iDataLen = 0;
			ret = GetData((char *)&stSupprotCache, sizeof(stSupprotCache), pData, iDataLen);
			if(0==ret && iDataLen == sizeof(TCCNSBidSupportCacheValue))
			{
				CCNSNameValue* pnv = NULL;
				pnv = stNameValueList.Append();
				pnv->name.Set((char *)&stSupprotCache, sizeof(stSupprotCache));
				pnv->value.Set(pData, iDataLen);
			}
		}
		return 0;
	}
	//2013-01-08 set list
	virtual int SetBidAttr(int iBid, CCNSNameValueList& stNameValueList)
	{
		int ret = 0;
		std::string data;
		stNameValueList.SetCurrToFirst();
		for (CCNSNameValue* pnv = stNameValueList.Curr();
			pnv != NULL;
			pnv = stNameValueList.GoNext())
		{
			if (pnv->name.Len() == 0)
				continue;
			std::string strName;
			strName.assign((char*)pnv->name, pnv->name.Len());
			int type = CCNSAttrNameTypeMng::Key2Type(strName);//just for check
			if (type >= 0)
			{
				ret = SetData((char*)pnv->name,  pnv->name.Len(), (char*)pnv->value,  pnv->value.Len());				
				if (ret)
					return ret;
			}
			else
			{
				snprintf(szErr, sizeof(szErr), "unknown type %d", type);
				return -11;
			}
		}
		return 0;
	}

	virtual int GetData(char *pKey, int iKeyLen, char *&pData, int &iDataLen)
	{
		return m_pKeyValueMng->Get(pKey, iKeyLen, pData, iDataLen);
	}

	virtual int SetData(char *pKey, int iKeyLen, char *pData, int iDataLen)
	{	
		return m_pKeyValueMng->Set(pKey, iKeyLen, pData, iDataLen);
	}
	virtual int DelData(char *pKey, int iKeyLen)
	{	
		return m_pKeyValueMng->Del(pKey, iKeyLen);
	}
	virtual int GetKeyCount(ssize_t &iKeyCount)
	{
		return m_pKeyValueMng->GetKeyCount(iKeyCount);
	}
	virtual void ToString(string &s)
	{
		return m_pKeyValueMng->ToString(s);
	}
	virtual char* GetErrMsg()
	{
		return szErr;
	}
protected:
	int m_iShmStore;
	CCCNSNameValueMng* m_pKeyValueMng; 
	char szErr[128];
};
#endif

