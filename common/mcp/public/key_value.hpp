#ifndef __KEY_VALUE_HPP__
#define  __KEY_VALUE_HPP__
#include "MemKeyCache.hpp"
#include "Base.hpp"
#include "tlib_cfg.h"
#include "storage_asn.h"
#include <map>
#include <set>
#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN 16777216
#endif
class CNameValueMng
{
public:
	CNameValueMng(){}
	virtual ~CNameValueMng(){}
	virtual int Set(const std::string& strKey, const std::string& strData)
	{
		std::map< std::string, std::string >::iterator it = m_map_keydata.find(strKey);
		if (it == m_map_keydata.end())
		{
			m_map_keydata.insert(std::pair< std::string, std::string >(strKey, strData));
		}
		else
		{
			it->second = strData;
		}
		return 0;
	}
	virtual int Get(const std::string& strKey, std::string& data)
	{
		std::map< std::string, std::string >::iterator it = m_map_keydata.find(strKey);
		if (it != m_map_keydata.end())
		{
			data = it->second;
			return 0;
		}
		return -13200;
	}
	virtual int Del(const std::string& strKey)
	{
		std::map< std::string, std::string >::iterator it = m_map_keydata.find(strKey);
		if (it != m_map_keydata.end())
		{
			m_map_keydata.erase(it);
			return 0;
		}
		return -13200;
	}
protected:
	std::map< std::string, std::string > m_map_keydata; 
};

class CKeyValueMng : public CNameValueMng
{
public:
	CKeyValueMng()
	{
		m_pSpinlock = 0;
		m_pMemKeyCache = 0;
		m_p_shm_addr = 0;
		m_iMemSize = 0;
	}
	virtual ~CKeyValueMng()
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
	virtual int Set(const std::string& strKey, const std::string& strData)
	{
		Lock();
		int ret = m_pMemKeyCache->Set(strKey.c_str(), strKey.size(),
			(char*)strData.c_str(), strData.size());
		UnLock();
		return ret;
	}
	virtual int Get(const std::string& strKey, std::string& data)
	{
		Lock();
		ssize_t iDataFlag;
		int ret = m_pMemKeyCache->Get(strKey.c_str(), strKey.size(), m_Buff, MAX_MSG_LEN, iDataFlag);
		UnLock();
		if (ret > 0)
		{
			data.assign(m_Buff, ret);
			return 0;
		}
		return ret;
	}
	virtual int Del(const std::string& strKey)
	{
		Lock();
		int ret = m_pMemKeyCache->Del(strKey.c_str(), strKey.size());		
		UnLock();
		return ret;
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
	char m_Buff[MAX_MSG_LEN];
};

class AttrNameTypeMng
{
public:
	enum
	{
		em_attr_beg = 0,
		em_attr_ipport_ver = 0, //ipport版本
		em_attr_master_ip,
		em_attr_bid_hashmap_version,   //bid hashmap 版本
		em_attr_bid_white_iplist,
		em_attr_bid_support_expire,     //配置expire属性
		em_attr_bid_desc,		//bid描述信息
		em_attr_bid_passwd,		//bid passwd
		em_attr_bid_attr_ver,
		em_attr_bid_strategy,
		em_attr_bid_white_ip_protocol,//使用白名单的协议
		em_attr_bid_set_expire_time,
		em_attr_bid_suffix,
		em_attr_bid_support_evict,  //支持LRU淘汰
		em_attr_end,
	};
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
	static  std::string Type2Name(int type)
	{
		std::map< int, std::string >::iterator it = TypeName().find(type);
		if (it != TypeName().end())
		{
			return it->second;
		}
		return std::string("");
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
	AttrNameTypeMng()
	{
		char *g_name[32] = 
		{
			"ipport_ver",
			"master_ip",
			"bid_hashmap_ver",
			"bid_white_iplist",
			"bid_support_expire",
			"bid_desc",
			"bid_passwd",
			"bid_attr_ver",
			"bid_strategy",
			"bid_white_ip_protocol",
			"bid_set_expire_time",
			"bid_suffix",
			"bid_support_evict"
		};
		for (int i = em_attr_beg; i < em_attr_end; i++)
		{
			m_mapAttrNameType[std::string(g_name[i])] = i;
			m_mapAttrTypeName[i] = std::string(std::string(g_name[i]));
		}
	}
	int GetName(int bid, int type, std::string& strName)
	{	
		if (type < em_attr_beg || type >= em_attr_end)
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
	static AttrNameTypeMng& Instance()
	{
		static AttrNameTypeMng stAttrNameTypeMng;
		return stAttrNameTypeMng;
	}
	
private:
	std::map< std::string, int > m_mapAttrNameType;
	std::map< int, std::string > m_mapAttrTypeName;
};

class AttrMng
{
public:
	AttrMng()
	{
		m_pKeyValueMng = 0;
		m_iShmStore = 0;
	}
	virtual ~AttrMng()
	{
		if (m_pKeyValueMng)
		{
			delete m_pKeyValueMng;
			m_pKeyValueMng = 0;
		}
	}
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
			m_pKeyValueMng = new CKeyValueMng;
			assert(m_pKeyValueMng);
			return ((CKeyValueMng*)m_pKeyValueMng)->Init(mem_size, block_size);
		}
		else
		{
			m_pKeyValueMng = new CNameValueMng;
			assert(m_pKeyValueMng);			
		}
		return 0;
	}
	virtual int Del(int iBid, int type)
	{
		std::string name;
		int ret = AttrNameTypeMng::KeyName(iBid, type, name);
		if (ret == 0)
		{
			return m_pKeyValueMng->Del(name);
		}
		return ret;
	}
	virtual int GetData(int iBid, int type, std::string& data)
	{
		std::string name;
		int ret = AttrNameTypeMng::KeyName(iBid, type, name);
		if (ret == 0)
		{
			return m_pKeyValueMng->Get(name, data);
		}
		return ret;
	}


	virtual int GetBidAttr(int iBid, NameValueList& stNameValueList)
	{
		int ret = 0;
		if (stNameValueList.Count() > 0)
		{
			stNameValueList.SetCurrToFirst();
			for (NameValue* pnv = stNameValueList.First();
				pnv != NULL;
				pnv = stNameValueList.GoNext())
			{
				char name[256];
				if (pnv->name.Len() == 0 || pnv->name.Len() > 255)
					continue;
				memcpy(name, pnv->name, pnv->name.Len());
				name[pnv->name.Len()] = '\0';
				std::string data;
				int type = AttrNameTypeMng::Name2Type(std::string(name));
				if (type >= 0)
				{
					ret = GetData(iBid, type, data);
					if (0 == ret)
					{
						pnv->value.Set(data.c_str(), data.size());
					}
				}
			}
		}
		else
		{
			NameValue* pnv = NULL;
			std::string data;
			for (int type = AttrNameTypeMng::em_attr_bid_hashmap_version; 
					type < AttrNameTypeMng::em_attr_end; 
					++type)
			{
				ret = GetData(iBid, type, data);
				if (ret == 0)
				{
					pnv = stNameValueList.Append();
					std::string name = AttrNameTypeMng::Type2Name(type);
					pnv->name.Set(name.c_str(), name.size());
					pnv->value.Set(data.c_str(), data.size());
				}
			}
			
		}
		return 0;
	}

	virtual int GetBidPasswd(int iBid, std::string& passwd)
	{
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_passwd, passwd);
		return ret;
	}
	virtual int GetBidDesc(int iBid, std::string& desc)
	{
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_desc, desc);
		return ret;
	}
	virtual int GetBidSupportExpire(int iBid, int& iSupportExpire)
	{
		std::string data;
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_support_expire, data);
		if (0 == ret && data.size() == sizeof(int))
		{
			memcpy(&iSupportExpire, data.c_str(), sizeof(int));
			return 0;
		}
		return -1;
	}

	virtual int GetBidSupportEvict(int iBid, int& iSupportEvict)
	{
		std::string data;
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_support_evict, data);
		if (0 == ret && data.size() == sizeof(int))
		{
			memcpy(&iSupportEvict, data.c_str(), sizeof(int));
			return 0;
		}
		return -1;
	}
		
	virtual int GetBidSetExpireTime(int iBid, int& iSetExpireTime)
	{
		std::string data;
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_set_expire_time, data);
		if (0 == ret && data.size() == sizeof(int))
		{
			memcpy(&iSetExpireTime, data.c_str(), sizeof(int));
			return 0;
		}
		return -1;
	}
	virtual int GetBidHashmapVer(int iBid, int& ver)
	{
		std::string data;
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_hashmap_version, data);
		if (0 == ret && data.size() == sizeof(int))
		{
			memcpy(&ver, data.c_str(), sizeof(int));
			return 0;
		}
		return -1;
	}
	virtual int GetBidAttrVer(int iBid, int& ver)
	{
		std::string data;
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_attr_ver, data);
		if (0 == ret && data.size() == sizeof(int))
		{
			memcpy(&ver, data.c_str(), sizeof(int));
			return 0;
		}
		return -1;
	}
	virtual int GetIpportVer(int iBid, int& ipport_ver)
	{
		std::string data;
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_ipport_ver, data);
		if (0 == ret && data.size() == sizeof(int))
		{
			memcpy(&ipport_ver, data.c_str(), sizeof(int));
			return 0;
		}
		return -1;
	}
	virtual int GetMasterIp(int iBid, unsigned int& master_ip)
	{
		std::string data;
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_master_ip, data);
		if (0 == ret && data.size() == sizeof(unsigned int))
		{
			memcpy(&master_ip, data.c_str(), sizeof(unsigned int));
			return 0;
		}
		return -1;
	}
	virtual int GetBidSuffix(int iBid, std::string& data)
	{
		int ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_suffix, data);
		if (0 == ret)
		{
			return 0;
		}
		return -1;
	}

public:
	virtual int SetData(int iBid, int type, const std::string& data)
	{	
		std::string name;
		int ret = AttrNameTypeMng::KeyName(iBid, type, name);
		if (0 == ret )
		{
			ret = m_pKeyValueMng->Set(name, data);
		}
		return ret;
	}
	virtual int GetBidAttr(int iBid, const std::string& name, std::string& data)
	{
		int type = AttrNameTypeMng::Name2Type(name);
		int ret = 0;
		char szValue[512];
		switch(type)
		{
		case AttrNameTypeMng::em_attr_bid_attr_ver:
			{
				int ver = 0;
				ret = GetBidAttrVer(iBid, ver);
				if (0 == ret)
				{
					snprintf(szValue, sizeof(szValue), "%d", ver);
					data = szValue;
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_ipport_ver:
			{
				int ver = 0;
				ret = GetIpportVer(iBid, ver);
				if (0 == ret)
				{
					snprintf(szValue, sizeof(szValue), "%d", ver);
					data = szValue;
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_master_ip:
			{
				unsigned int master_ip = 0;
				ret = GetMasterIp(iBid, master_ip);
				if (0 == ret)
				{
					snprintf(szValue, sizeof(szValue), "%u", master_ip);
					data = szValue;
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_hashmap_version:
			{
				int ver = 0;
				ret = GetBidHashmapVer(iBid, ver);
				if (0 == ret)
				{
					snprintf(szValue, sizeof(szValue), "%d", ver);
					data = szValue;
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_white_iplist:
			{
				ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_white_iplist, data);
				if (0 == ret)
				{
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_support_expire:
			{
				int support_expire = 0;
				ret = GetBidSupportExpire(iBid, support_expire);
				if (0 == ret)
				{
					snprintf(szValue, sizeof(szValue), "%d", support_expire);
					data = szValue;
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_support_evict:
			{
				int support_evict = 0;
				ret = GetBidSupportEvict(iBid, support_evict);
				if (0 == ret)
				{
					snprintf(szValue, sizeof(szValue), "%d", support_evict);
					data = szValue;
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_set_expire_time:
			{
				int set_expire_time = 0;
				ret = GetBidSetExpireTime(iBid, set_expire_time);
				if (0 == ret)
				{
					snprintf(szValue, sizeof(szValue), "%d", set_expire_time);
					data = szValue;
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_desc:
			{
				ret = GetBidDesc(iBid, data);
				if (0 == ret)
				{
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_passwd:
			{
				ret = GetBidPasswd(iBid, data);
				if (0 == ret)
				{
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_white_ip_protocol:
			{
				ret = GetData(iBid, AttrNameTypeMng::em_attr_bid_white_ip_protocol, data);
				if (0 == ret)
				{
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_suffix:
			{
				ret= GetData(iBid, AttrNameTypeMng::em_attr_bid_suffix, data);
				if (0 == ret)
				{
					return 0;
				}
				break;
			}
		case AttrNameTypeMng::em_attr_bid_strategy:
		default:
			return -1;
			
		}
		return ret;
	}
	/*
	* 隐患接口，慎用。
	*/
	virtual int SetBidAttr(int iBid, const std::string& name, const char* szValue)
	{
		int type = AttrNameTypeMng::Name2Type(name);
		switch(type)
		{
		case AttrNameTypeMng::em_attr_bid_attr_ver:
			return SetBidAttrVer(iBid, atoi(szValue));
		case AttrNameTypeMng::em_attr_ipport_ver:
			return SetIpportVer(iBid, atoi(szValue));
		case AttrNameTypeMng::em_attr_master_ip:
			return SetMasterIp(iBid, (unsigned int)atoi(szValue));
		case AttrNameTypeMng::em_attr_bid_hashmap_version:
			return SetBidHashmapVer(iBid, atoi(szValue));
		case AttrNameTypeMng::em_attr_bid_support_expire:
			return SetBidSupportExpire(iBid, atoi(szValue));
		case AttrNameTypeMng::em_attr_bid_set_expire_time:
			return SetBidSetExpireTime(iBid, atoi(szValue));
		case AttrNameTypeMng::em_attr_bid_desc:
			return SetBidDesc(iBid, szValue);
		case AttrNameTypeMng::em_attr_bid_passwd:
			return SetBidPasswd(iBid, szValue);
		case AttrNameTypeMng::em_attr_bid_support_evict:
			return SetBidSupportEvict(iBid, atoi(szValue));
		case AttrNameTypeMng::em_attr_bid_strategy:
		default:
			return -1;
			
		}
	}


	virtual int SetBidAttr(int iBid, NameValueList& stNameValueList)
	{
		int ret = 0;
		std::string data;
		stNameValueList.SetCurrToFirst();
		for (NameValue* pnv = stNameValueList.Curr();
			pnv != NULL;
			pnv = stNameValueList.GoNext())
		{
			char name[256];
			if (pnv->name.Len() == 0 || pnv->name.Len() > 255)
				continue;
			memcpy(name, pnv->name, pnv->name.Len());
			name[pnv->name.Len()] = '\0';
			int type = AttrNameTypeMng::Name2Type(std::string(name));
			if (type >= 0)
			{
				data.assign((char*)pnv->value, pnv->value.Len());
				ret = SetData(iBid, type, data);
				if (ret)
					return ret;
			}
			else
			{
				return -11;
			}
		}
		return 0;
	}
	virtual int SetBidPasswd(int iBid, const std::string& passwd)
	{
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_passwd, passwd);
	}
	virtual int SetBidDesc(int iBid, const std::string& desc)
	{	
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_passwd, desc);
	}
	virtual int SetBidSupportExpire(int iBid, int iSupportExpire)
	{
		std::string data;
		data.assign((char*)&iSupportExpire, sizeof(int));
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_support_expire, data);
	}
	virtual int SetBidSupportEvict(int iBid, int iSupportEvict)
	{
		std::string data;
		data.assign((char*)&iSupportEvict, sizeof(int));
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_support_evict, data);
	}
	virtual int SetBidSetExpireTime(int iBid, int iSetExpireTime)
	{
		std::string data;
		data.assign((char*)&iSetExpireTime, sizeof(int));
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_set_expire_time, data);
	}
	virtual int SetBidHashmapVer(int iBid, int ver)
	{
		std::string data;
		data.assign((char*)&ver, sizeof(int));
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_hashmap_version, data);
	}
	virtual int SetBidAttrVer(int iBid, int ver)
	{
		std::string data;
		data.assign((char*)&ver, sizeof(int));
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_attr_ver, data);
	}
	virtual int SetIpportVer(int iBid, int ipport_ver)
	{
		std::string data;
		data.assign((char*)&ipport_ver, sizeof(int));
		return SetData(iBid, AttrNameTypeMng::em_attr_ipport_ver, data);
	}
	virtual int SetMasterIp(int iBid, unsigned int master_ip)
	{
		std::string data;
		data.assign((char*)&master_ip, sizeof(unsigned int));
		return SetData(iBid, AttrNameTypeMng::em_attr_master_ip, data);
	}
	virtual int SetBidSuffix(int iBid, std::string& data)
	{
		return SetData(iBid, AttrNameTypeMng::em_attr_bid_suffix, data);
	}
protected:
	int m_iShmStore;
	CNameValueMng* m_pKeyValueMng; 
};
#endif

