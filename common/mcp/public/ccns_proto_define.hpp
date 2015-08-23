#ifndef __CCNS_PROTO_DEFINE_HPP__
#define __CCNS_PROTO_DEFINE_HPP__
#include "tlib_cfg.h"
#include "Base.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
using namespace std;
#include <assert.h>

typedef enum EEngineID
{
	em_engine_unkown = -1,
	em_engine_cmem = 0,
	em_engine_tssd = 1,
	em_engine_tdb = 2
}EEngineID;

enum en_protocol_id
{
	EN_PROTOCOL_UNKOWN = -1,
	EN_PROTOCOL_INTERNAL = 0,
	EN_PROTOCOL_EXT_ASN13 = 1,
	EN_PROTOCOL_EXT_TDB21 = 2,
	EN_PROTOCOL_EXT_MEMCACHED = 3,
	EN_PROTOCOL_EXT_REDIS = 4,
	EN_PROTOCOL_NUM = 50,

};

typedef enum EProtoType
{
	em_proto_unkown = -1,
	em_proto_asn20_ccns = 0,//ccns
	em_proto_asn20_cmem = 1,//cmem, asn20
	em_proto_asn13_tssd = 2,//tssd, asn13
	em_proto_asn13_cmem = 3,//未使用
	em_proto_asn20_ccns_ccs = 100,//ccns内部使用
	em_proto_asn20_ccns_aux = 101//ccns内部使用
}EProtoType;


typedef enum EMoveDirection
{
	em_move_direct_unkown = -1,
	em_move_left_to_right = 0,
	em_move_right_to_left = 1,
}EMoveDirection;


#define CMEM_ASN20_PORT 9100
#define CMEM_ASN13_PORT 9101
#define CMEM_TDB21_PORT 20000

enum
{
	emCCNSStatusUnknown = -1,
	emCCNSStatusOK = 0,
	emCCNSStatusDead = 1,
	emCCNSStatusDown = 2
};
//engine id 长度不可修改
typedef struct TStorageEngineItemID
{
	TStorageEngineItemID()
	{
		engineid = -1;
		masterid = 0;
		bid = 0;
	}
	bool operator== (const TStorageEngineItemID& o) const
	{
		if(engineid != o.engineid)
			return false;
		if(masterid != o.masterid)
			return false;
		if(bid != o.bid)
			return false;
		return true;		
	}
	bool operator <(const struct TStorageEngineItemID &o) const
	{
		if(engineid < o.engineid)
			return true;
		if(engineid == o.engineid && masterid < o.masterid)
			return true;
		if(engineid == o.engineid && masterid == o.masterid && bid < o.bid)
			return true;
		return false;
	}
	bool operator!= (const TStorageEngineItemID& o) const
	{
		return !(*this == o);
	}
	void ToString(string &s) const
	{
		char buf[1024];
		int len = snprintf(buf, sizeof(buf), "engineid %d masterid %d bid %d", engineid, masterid, bid);
		s.assign(buf, len);
	}
	
	int engineid;
	int masterid;
	int bid;	
}TStorageEngineItemID;
typedef struct TCCNSCommonIpAddrHead
{
	TCCNSCommonIpAddrHead()
	{
		ip = 0;
		port = 0;
	}
	bool operator== (const TCCNSCommonIpAddrHead& o) const
	{
		if(ip != o.ip)
			return false;
		if(port != o.port)
			return false;
		return true;
	}
	bool operator!= (const TCCNSCommonIpAddrHead& o) const
	{
		return !(*this == o);
	}

	bool operator <(const struct TCCNSCommonIpAddrHead &o) const
	{
		if(ip < o.ip)
			return true;
		if(ip == o.ip && port < o.port)
			return true;
		return false;
	}
	int CheckValid(string &strErr)
	{
		char szErr[1024];
		if(ip == 0 || ip == -1)
		{
			snprintf(szErr, sizeof(szErr), "invalid ip %d\n", ip);
			strErr = szErr;
			return -1;
		}
		if(port <= 0 || port > 65535)
		{
			snprintf(szErr, sizeof(szErr), "invalid port %d\n", port);
			strErr = szErr;
			return -1;
		}
		return 0;

	}
	void ToString(string &s) const
	{
		char szIp[64];
		char buf[1024];
		HtoP(ip, szIp);
		int len = snprintf(buf, sizeof(buf), "ip %s port %d ", szIp, port);
		s.assign(buf, len);
	}

	int ip;
	int port;
}TCCNSCommonIpAddrHead;
typedef struct TCCNSCommonIpAddr
{
	TCCNSCommonIpAddr()
	{
		ip = 0;
		port = 0;
		status = 0;
		proto = em_proto_unkown;
	}
	bool operator== (const TCCNSCommonIpAddr& o) const
	{
		if(ip != o.ip)
			return false;
		if(port != o.port)
			return false;
		if(status != o.status)
			return false;
		if(proto != o.proto)
			return false;
		return true;
	}
	bool operator!= (const TCCNSCommonIpAddr& o) const
	{
		return !(*this == o);
	}

	bool operator <(const struct TCCNSCommonIpAddr &o) const
	{
		if(ip < o.ip)
			return true;
		if(ip == o.ip && port < o.port)
			return true;
		if(ip == o.ip && port == o.port && status < o.status)
			return true;
		if(ip == o.ip && port == o.port && status == o.status && proto < o.proto)
			return true;
		return false;
	}
	int CheckValid(string &strErr)
	{
		char szErr[1024];
		if(ip == 0 || ip == -1)
		{
			snprintf(szErr, sizeof(szErr), "invalid ip %d\n", ip);
			strErr = szErr;
			return -1;
		}
		if(port <= 0 || port > 65535)
		{
			snprintf(szErr, sizeof(szErr), "invalid port %d\n", port);
			strErr = szErr;
			return -1;
		}
		if(status != emCCNSStatusDead && status != emCCNSStatusOK && 
			status != emCCNSStatusUnknown && status != emCCNSStatusDown)
		{
			snprintf(szErr, sizeof(szErr), "invalid status %d\n", status);
			strErr = szErr;
			return -1;
		}

		return 0;

	}
	void ToString(string &s) const
	{
		char szIp[64];
		char buf[1024];
		HtoP(ip, szIp);
		int len = snprintf(buf, sizeof(buf), "ip %s port %d status %d proto %d", szIp, port, status, (int)proto);
		s.assign(buf, len);
	}
	void ConvertTo(TCCNSCommonIpAddrHead &o) const
	{
		o.ip = ip;
		o.port = port;
	}

	int ip;
	int port;
	int status;
	EProtoType proto;
}TCCNSCommonIpAddr;



#endif
