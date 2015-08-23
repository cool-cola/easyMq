#ifndef __CCNS_DEFINE_HPP__
#define __CCNS_DEFINE_HPP__
#include <ccns_proto_define.hpp>
#include "msg_help.h"
#include "storage_asn.h"
#include <string>
#include <vector>
#include <map>
#include <set>
using namespace std;
//最多32份数据
#define MAX_ENGINE_NUM 3 
#define MAX_ACCESS_NUM 256 
enum
{
	em_ccns_bid_push_set = 0,
	em_ccns_bid_push_del = 1
};	
inline void ConvertTCCNSCommonIpAddr(TCCNSCommonIpAddr &from, CCNSCommonIpAddr &to)
{
	to.ip = from.ip;
	to.port = from.port;
	to.status = from.status;
	to.proto = from.proto;
}

inline void ConvertTCCNSCommonIpAddr(CCNSCommonIpAddr &from, TCCNSCommonIpAddr &to)
{
	to.ip = (int)from.ip;
	to.port = (int)from.port;
	to.status = (int)from.status;
	to.proto = (EProtoType)(int)from.proto;
}

typedef struct TEngineMasterID
{
	bool operator== (const TEngineMasterID& o) const
	{
		if(engineid != o.engineid)
			return false;
		if(masterid != o.masterid)
			return false;
		return true;
	}
	bool operator!= (const TEngineMasterID& o) const
	{
		return !(*this == o);
	}
	bool operator <(const struct TEngineMasterID &o) const
	{
		if(engineid < o.engineid)
			return true;
		if(engineid == o.engineid && masterid < o.masterid)
			return true;
		return false;
	}
	void ToString(string &s) const
	{
		char buf[1024];
		int len = snprintf(buf, sizeof(buf), "engineid %d masterid %d ", engineid, masterid);
		s.assign(buf, len);
	}
	int engineid;
	int masterid;
}TEngineMasterID;

typedef struct TStorageEngineItem
{
	TStorageEngineItem()
	{
		engineid = 0;
		masterid = 0;
		bid = 0;
		vecMaster.clear();
		vecAccess.clear();
	}
	int CheckValid(string &strErr)
	{
		int iRet = 0;
		for(int i = 0; i < (int)vecMaster.size(); ++i)
		{
			TCCNSCommonIpAddr &stAddr = vecMaster[i];
			iRet = stAddr.CheckValid(strErr);
			if(iRet)
				return -1;
		}
		for(int i = 0; i < (int)vecAccess.size(); ++i)
		{
			TCCNSCommonIpAddr &stAddr = vecAccess[i];
			iRet = stAddr.CheckValid(strErr);
			if(iRet)
				return -1;
		}
		return 0;
	}

	bool operator== (const TStorageEngineItem& o) const
	{
		if(engineid != o.engineid)
			return false;
		if(masterid != o.masterid)
			return false;
		if(bid != o.bid)
			return false;
		if(vecMaster.size() != o.vecMaster.size())
			return false;
		for(int i = 0; i < (int)vecMaster.size(); ++i)
		{
			if(vecMaster[i] != o.vecMaster[i])
				return false;
		}
		if(vecAccess.size() != o.vecAccess.size())
			return false;
		for(int i = 0; i < (int)vecAccess.size(); ++i)
		{
			if(vecAccess[i] != o.vecAccess[i])
				return false;
		}	
		return true;
	}
	bool operator!= (const TStorageEngineItem& o) const
	{
		return !(*this == o);
	}
	
	void ToString(string &s) const
	{
		char buf[1024];
		int len = snprintf(buf, sizeof(buf), "engineid %d masterid %d bid %d mastercount %d accesscount %d", 
			engineid, masterid, bid, (int)vecMaster.size(), (int)vecAccess.size());
		s.assign(buf, len);
	}

	void ConvertTo(StorageEngineItem &to)
	{
		to.engineid = engineid;
		to.masterid = masterid;
		to.bid = bid;	

		for(int i = 0; i < (int)vecMaster.size(); ++i)
		{
			TCCNSCommonIpAddr &stAddr = vecMaster[i];
			CCNSCommonIpAddr asnAddr;
			ConvertTCCNSCommonIpAddr(stAddr, asnAddr);
			to.masterAddrList.AppendCopy(asnAddr);
		}

		for(int i = 0; i < (int)vecAccess.size(); ++i)
		{
			TCCNSCommonIpAddr &stAddr = vecAccess[i];
			CCNSCommonIpAddr asnAddr;
			ConvertTCCNSCommonIpAddr(stAddr, asnAddr);
			to.accessAddrList.AppendCopy(asnAddr);
		}
	}

	void ConvertFrom(StorageEngineItem &from)
	{
		engineid = from.engineid;
		masterid = from.masterid;
		bid = from.bid;	
		vecMaster.clear();
		from.masterAddrList.SetCurrToFirst();
		while(1)	
		{
			CCNSCommonIpAddr *pAddr = from.masterAddrList.Curr();
			if(!pAddr)		
				break;	
			TCCNSCommonIpAddr stAddr;
			ConvertTCCNSCommonIpAddr(*pAddr, stAddr);
			vecMaster.push_back(stAddr);
			from.masterAddrList.GoNext();
		}
		vecAccess.clear();
		from.accessAddrList.SetCurrToFirst();
		while(1)	
		{
			CCNSCommonIpAddr *pAddr = from.accessAddrList.Curr();
			if(!pAddr)		
				break;	
			TCCNSCommonIpAddr stAddr;
			ConvertTCCNSCommonIpAddr(*pAddr, stAddr);
			vecAccess.push_back(stAddr);
			from.accessAddrList.GoNext();
		}
	}	
	void ConvertTo(TStorageEngineItemID &to)
	{
		to.engineid = engineid;
		to.masterid = masterid;
		to.bid = bid;
	}
	void ConvertTo(TEngineMasterID &to)
	{
		to.engineid = engineid;
		to.masterid = masterid;
	}
	bool isSameMaster (const TEngineMasterID& stEngId) const
	{
		if(engineid != stEngId.engineid)
			return false;
		if(masterid != stEngId.masterid)
			return false;		
		return true;
	}
	int engineid;
	int masterid;
	int bid;
	vector<TCCNSCommonIpAddr>  vecMaster;
	vector<TCCNSCommonIpAddr>  vecAccess;
	
}TStorageEngineItem;

typedef struct TStorageEngineKeySchedulerRelation
{
	bool operator== (const TStorageEngineKeySchedulerRelation& o) const
	{
		if(left_engineid != o.left_engineid)
			return false;

		if(left_masterid != o.left_masterid)
			return false;	

		if(left_bid != o.left_bid)
			return false;	

		if(right_engineid != o.right_engineid)
			return false;	

		if(right_masterid != o.right_masterid)
			return false;	

		if(right_bid != o.right_bid)
			return false;	
		return true;
	}
	bool operator!= (const TStorageEngineKeySchedulerRelation& o) const
	{
		return !(*this == o);
	}
	bool operator <(const struct TStorageEngineKeySchedulerRelation &o) const
	{
		if(left_engineid < o.left_engineid)
			return true;
		if(left_engineid == o.left_engineid && left_masterid < o.left_masterid)
			return true;
		if(left_engineid == o.left_engineid && left_masterid == o.left_masterid && left_bid < o.left_bid)
			return true;

		if(left_engineid == o.left_engineid && left_masterid == o.left_masterid && left_bid ==o.left_bid &&
			right_engineid < o.right_engineid)
			return true;
		if(left_engineid == o.left_engineid && left_masterid == o.left_masterid && left_bid ==o.left_bid &&
			right_engineid == o.right_engineid && right_masterid < o.right_masterid)
			return true;
		if(left_engineid == o.left_engineid && left_masterid == o.left_masterid && left_bid ==o.left_bid &&
			right_engineid == o.right_engineid && right_masterid == o.right_masterid && right_bid < o.right_bid)
			return true;
		return false;
	}
	void ConvertTo(StorageEngineKeySchedulerRelation &to) const
	{
		to.left.engineid = left_engineid;
		to.left.masterid = left_masterid;
		to.left.bid = left_bid;
		to.right.engineid = right_engineid;
		to.right.masterid = right_masterid;
		to.right.bid = right_bid;
		to.iState = 1;
	}
	void ConvertFrom(StorageEngineKeySchedulerRelation &from)
	{
		left_engineid = from.left.engineid;
		left_masterid = from.left.masterid;
		left_bid = from.left.bid;
		right_engineid = from.right.engineid;
		right_masterid = from.right.masterid;
		right_bid = from.right.bid;
	}
	void ConvertTo(TStorageEngineItemID &left, TStorageEngineItemID &right) const
	{
		left.engineid = left_engineid;
		left.masterid = left_masterid;
		left.bid = left_bid;
		
		right.engineid = right_engineid;
		right.masterid = right_masterid;
		right.bid = right_bid;
	}
	int CheckValid(string &strErr)
	{
		char szErr[1024];
		if((left_engineid == right_engineid) && 
			(left_masterid == left_masterid) &&
			(left_bid == right_bid) )
		{
			snprintf(szErr, sizeof(szErr), "invalid relation %d %d %d == %d %d %d\n", 
				left_engineid, left_masterid, left_bid, right_engineid, right_masterid, right_bid);
			strErr = szErr;
			return -1;
		}
		return 0;
	}
	void ToString(string &s) const
	{
		char buf[1024];
		int len = snprintf(buf, sizeof(buf), "left_engineid %d left_masterid %d left_bid %d right_engineid %d right_masterid %d right_bid %d", 
			left_engineid, left_masterid, left_bid, right_engineid, right_masterid, right_bid);
		s.assign(buf, len);
	}
	int left_engineid;
	int left_masterid;
	int left_bid;

	int right_engineid;
	int right_masterid;
	int right_bid;
}TStorageEngineKeySchedulerRelation;

typedef struct TStorageEngineKeySchedulerRelationProp
{
	TStorageEngineKeySchedulerRelationProp& operator = (const TStorageEngineKeySchedulerRelationProp& o)
	{
		iState = o.iState;
		iScheIntervalH = o.iScheIntervalH;
		iScheRate = o.iScheRate;
		iImmedScheThred = o.iImmedScheThred;
		return *this;
	}

	int	iState;	/* 0:em_relation_schedule_off ; 1:em_relation_schedule_on */
	int	iScheIntervalH; /* 淘汰时间 */
	int	iScheRate;/* 淘汰速度，CU最大只能按此速度淘汰 */
	int	iImmedScheThred;/* 立即淘汰阈值，CU负载超过此阈值则立即淘汰 */
}TStorageEngineKeySchedulerRelationProp;

/*
*0, 淘汰调度,writeback 1，镜像调度，一主多从 2,割接调度 3,用户指定存储调度
*/
enum
{
	em_scheduler_writeback = 0,
};
/*
*0,内存级别强一致性, 1,磁盘级别强一致性, 2, 最终一致性
*/
enum
{
	em_consistency_strong = 0,
};
/*
*0,待定是否可以关联, 1,禁止关联, 2,允许关联
*/
enum
{
	em_undetermined_plugin	= 0,
	em_forbid_plugin			= 1,
	em_permitted_plugin		= 2,
};

typedef struct TCCNSBidDesc
{
	#define T_BID_DESC_BASE_DIR  "../etc/bid"

	TCCNSBidDesc()
	{
		iDataVer = 1;
		iCCNSBid = -1;
		iSchedulerType = em_scheduler_writeback;
		iConsistent = em_consistency_strong;
		iPluginProp = em_undetermined_plugin;
	}
	bool operator== (const TCCNSBidDesc& o) const
	{
		if(iCCNSBid != o.iCCNSBid)
			return false;
		if(szDesc.compare(o.szDesc))
			return false;
		if(iDataVer != o.iDataVer)
			return false;
		if(mainEngine != o.mainEngine)
			return false;
		if(vecPluginEngine.size() != o.vecPluginEngine.size())
			return false;
		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
		{
			if(vecPluginEngine[i] != o.vecPluginEngine[i])
				return false;
		}
		if(vecRelation.size() != o.vecRelation.size())
			return false;
		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			if(vecRelation[i] != o.vecRelation[i])
				return false;
		}	
		if(iSchedulerType != o.iSchedulerType)
			return false;
		if(iConsistent != o.iConsistent)
			return false;
		if(iPluginProp != o.iPluginProp)
			return false;
		return true;
	}
	bool operator!= (const TCCNSBidDesc& o) const
	{
		return !(*this == o);
	}
	void ConvertTo(CCNSBidDesc &to)
	{
		to.ccnsBid = iCCNSBid;
		to.sDesc.Set(szDesc.c_str(), szDesc.size());
		to.iDataVer = iDataVer;
		mainEngine.ConvertTo(to.mainStorageEngine);
		
		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
		{
			TStorageEngineItem &stEngine = vecPluginEngine[i];
			StorageEngineItem engine;
			stEngine.ConvertTo(engine);
			to.pluginStorageEngineList.AppendCopy(engine);
		}
		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &stRelation = vecRelation[i];
			StorageEngineKeySchedulerRelation relation;
			stRelation.ConvertTo(relation);
			to.keySchedulerRelationList.AppendCopy(relation);
		}
		to.iSchedulerType = iSchedulerType;
		to.iConsistent = iConsistent;
		to.iPluginProp = iPluginProp;
	}
	void GetSEID(set<TStorageEngineItemID> &setID)
	{
		TStorageEngineItemID stSEID;
		mainEngine.ConvertTo(stSEID);
		setID.insert(stSEID);
		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
		{
			vecPluginEngine[i].ConvertTo(stSEID);
			setID.insert(stSEID);
		}
	}
	void GetAllEngine(vector<TStorageEngineItem> &vecEngine)
	{
		vecEngine.push_back(mainEngine);
		
		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
		{
			vecEngine.push_back(vecPluginEngine[i]);
		}
	}
	void ConvertFrom(CCNSBidDesc &from)
	{
		iCCNSBid = from.ccnsBid;
		szDesc.assign((char*)from.sDesc, from.sDesc.Len());
		iDataVer = from.iDataVer;
		mainEngine.ConvertFrom(from.mainStorageEngine);
		from.pluginStorageEngineList.SetCurrToFirst();
		while(1)
		{
			StorageEngineItem* pStorageEngineItem = from.pluginStorageEngineList.Curr();
			if(!pStorageEngineItem)
				break;

			TStorageEngineItem stEngine;
			stEngine.ConvertFrom(*pStorageEngineItem);
			vecPluginEngine.push_back(stEngine);
			from.pluginStorageEngineList.GoNext();
		}

		from.keySchedulerRelationList.SetCurrToFirst();
		while(1)
		{
			StorageEngineKeySchedulerRelation* pRelation = from.keySchedulerRelationList.Curr();
			if(!pRelation)
				break;

			TStorageEngineKeySchedulerRelation stRelation;
			stRelation.ConvertFrom(*pRelation);
			vecRelation.push_back(stRelation);
			from.keySchedulerRelationList.GoNext();
		}
		iSchedulerType = from.iSchedulerType;
		iConsistent = from.iConsistent;
		iPluginProp = from.iPluginProp;
	}
	int CheckWriteBackValid(string &strErr)
	{
		int iRet = 0;
		char szErr[1024];
		map<TStorageEngineItemID, int> mapSEIDLevel;
		map<TStorageEngineItemID, int>::iterator it_mapSEID;
		iRet = mainEngine.CheckValid(strErr);
		if(iRet)
			return -1;
		TStorageEngineItemID mainSEID;
		mainEngine.ConvertTo(mainSEID);

		mapSEIDLevel[mainSEID] = 100;

		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
		{
			TStorageEngineItem &stEngine = vecPluginEngine[i];
			iRet = stEngine.CheckValid(strErr);
			if(iRet)
				return -1;
			TStorageEngineItemID pluginSEID;
			stEngine.ConvertTo(pluginSEID);
			
			if(mapSEIDLevel.find(pluginSEID) != mapSEIDLevel.end())
			{
				snprintf(szErr, sizeof(szErr), "dup SEID[%d %d %d]", 
					pluginSEID.engineid, pluginSEID.masterid, pluginSEID.bid);
				strErr = szErr;
				return -1;
			}
			mapSEIDLevel[pluginSEID] = 200 + i*100; 
		}
		if((int)mapSEIDLevel.size() > MAX_ENGINE_NUM)
		{
			snprintf(szErr, sizeof(szErr), "too many engine, %d > max %d", 
				(int)mapSEIDLevel.size(), MAX_ENGINE_NUM);
			strErr = szErr;
			return -1;			
		}
		set<TStorageEngineItemID> setLeftSEID, setRightSEID;
		map<TStorageEngineItemID, int> mapSEIDOverlapped;
		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &stRelation = vecRelation[i];
			iRet = stRelation.CheckValid(strErr);
			if(iRet)
				return -1;
			TStorageEngineItemID left, right;
			stRelation.ConvertTo(left, right);
			
			//left必须在全局能找到
			it_mapSEID = mapSEIDLevel.find(left);
			if(it_mapSEID == mapSEIDLevel.end())
			{
				snprintf(szErr, sizeof(szErr), "not find SEID[%d %d %d]", 
					left.engineid, left.masterid, left.bid);
				strErr = szErr;
				return -1;
			}
			int left_level = it_mapSEID->second;
			//right必须在全局能找到
			it_mapSEID = mapSEIDLevel.find(right);
			if(it_mapSEID == mapSEIDLevel.end())
			{
				snprintf(szErr, sizeof(szErr), "not find SEID[%d %d %d]", 
					right.engineid, right.masterid, right.bid);
				strErr = szErr;
				return -1;
			}
			
			int right_level = it_mapSEID->second;
			//left只能出现一次
			if(setLeftSEID.find(left) != setLeftSEID.end())
			{
				snprintf(szErr, sizeof(szErr), "dup relation id SEID[%d %d %d]", 
					left.engineid, left.masterid, left.bid);
				strErr = szErr;
				return -1;			
			}
			//right只能出现一次
			if(setRightSEID.find(right) != setRightSEID.end())
			{
				snprintf(szErr, sizeof(szErr), "dup relation id SEID[%d %d %d]", 
					right.engineid, right.masterid, right.bid);
				strErr = szErr;
				return -1;			
			}
			if(left_level > right_level)
			{
				snprintf(szErr, sizeof(szErr), "invalid level left[%d %d %d] level[%d] > right[%d %d %d] level[%d] ", 
					left.engineid, left.masterid, left.bid, left_level,
					right.engineid, right.masterid, right.bid, right_level);
				strErr = szErr;
				return -1;				
			}
			setLeftSEID.insert(left);
			setRightSEID.insert(right);	
			mapSEIDOverlapped[left] = 0;
			mapSEIDOverlapped[right] = 0;
		}
		//判断是否有交叉
		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &stRelation = vecRelation[i];
			TStorageEngineItemID left, right;
			stRelation.ConvertTo(left, right);
			//left必须在全局能找到
			it_mapSEID = mapSEIDLevel.find(left);
			if(it_mapSEID == mapSEIDLevel.end())
			{
				snprintf(szErr, sizeof(szErr), "not find SEID[%d %d %d]", 
					left.engineid, left.masterid, left.bid);
				strErr = szErr;
				return -1;
			}
			int left_level = it_mapSEID->second;
			//right必须在全局能找到
			it_mapSEID = mapSEIDLevel.find(right);
			if(it_mapSEID == mapSEIDLevel.end())
			{
				snprintf(szErr, sizeof(szErr), "not find SEID[%d %d %d]", 
					right.engineid, right.masterid, right.bid);
				strErr = szErr;
				return -1;
			}
			
			int right_level = it_mapSEID->second;
			map<TStorageEngineItemID, int>::iterator it_mapSEIDOverlapped = mapSEIDOverlapped.begin();
			for(; it_mapSEIDOverlapped != mapSEIDOverlapped.end(); ++it_mapSEIDOverlapped)
			{
				
				int level = mapSEIDLevel[it_mapSEIDOverlapped->first];
				if(level >= left_level && level <= right_level)
				{
					it_mapSEIDOverlapped->second ++;
				}
			}
		}
		map<TStorageEngineItemID, int>::iterator it_mapSEIDOverlapped = mapSEIDOverlapped.begin();
		for(; it_mapSEIDOverlapped != mapSEIDOverlapped.end(); ++it_mapSEIDOverlapped)
		{
			const TStorageEngineItemID &stSEID = it_mapSEIDOverlapped->first;
			int iVertexCnt = 0;
			if(setLeftSEID.find(stSEID) != setLeftSEID.end())
				iVertexCnt++;
			if(setRightSEID.find(stSEID) != setRightSEID.end())
				iVertexCnt++;
			if(iVertexCnt == 1 && it_mapSEIDOverlapped->second >= 2)
			{
				snprintf(szErr, sizeof(szErr), "invalid SEID[%d %d %d], OverLapped", 
					stSEID.engineid, stSEID.masterid, stSEID.bid);
				strErr = szErr;
				return -1;	
			}
			if(iVertexCnt == 2 && it_mapSEIDOverlapped->second >= 3)
			{
				snprintf(szErr, sizeof(szErr), "invalid SEID[%d %d %d], OverLapped", 
					stSEID.engineid, stSEID.masterid, stSEID.bid);
				strErr = szErr;
				return -1;	
			}	
		}		
		return 0;
	}
	int CheckValid(string &strErr)
	{
		//engine id 长度不可修改
		assert(sizeof(TStorageEngineItemID) == 3*sizeof(int));
		char szErr[1024];
		if(szDesc.size() < 0 || szDesc.size() > 256)
		{
			snprintf(szErr, sizeof(szErr), "invalid szDesc size %d", (int)szDesc.size());
			strErr = szErr;
			return -1;
		}
		if(iSchedulerType != em_scheduler_writeback)
		{
			snprintf(szErr, sizeof(szErr), "invalid iSchedulerType %d", iSchedulerType);
			strErr = szErr;
			return -1;		
		}
		if(iConsistent != em_consistency_strong)
		{
			snprintf(szErr, sizeof(szErr), "invalid iConsistent %d", iConsistent);
			strErr = szErr;
			return -1;		
		}
		if(iSchedulerType == em_scheduler_writeback && iConsistent == em_consistency_strong)
		{
			return CheckWriteBackValid(strErr);
		}
		return 0;
	}
	int SafeChange(TCCNSBidDesc &to, string &strErr)
	{
		int iRet = 0;
		iRet = _CheckChange(to, 0, strErr);
		if(iRet)
			return -1;
		
		iRet =  _CheckChange(to, 1, strErr);
		if(iRet)
			return -1;
		return CheckValid(strErr);
	}
	int _CheckChange(TCCNSBidDesc &to, int iChange, string &strErr)
	{
		int iRet = 0;
		char szErr[1024];
		int iChangeCount = 0;
		iRet = to.CheckValid(strErr);
		if(iRet)
			return -1;
		strErr ="";
		if(to.szDesc.compare(szDesc))
		{
			iChangeCount++;
			snprintf(szErr, sizeof(szErr), "desc change from [%s] to [%s];\n", szDesc.c_str(), to.szDesc.c_str());
			strErr = szErr;
			if(iChange)
			{
				szDesc = to.szDesc;
				return 0;
			}
		}
		if(to.iPluginProp != iPluginProp)
		{
			iChangeCount++;
			if(iChange)
			{
				iPluginProp = to.iPluginProp;
			}
		}

		set<TStorageEngineItemID> setSEID;
		set<TStorageEngineKeySchedulerRelation> setFromRelation, setToRelation;
		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &relation = vecRelation[i];
			setFromRelation.insert(relation);
			TStorageEngineItemID left, right;
			relation.ConvertTo(left, right);
			setSEID.insert(left);
			setSEID.insert(right);
		}
		
		for(int i = 0; i < (int)to.vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &relation = to.vecRelation[i];
			if(setFromRelation.find(relation) ==  setFromRelation.end())
			{
				iChangeCount++;
				string s;
				relation.ToString(s);
				snprintf(szErr, sizeof(szErr), "need add relation [%s];", s.c_str());
				strErr += szErr;
				if(iChange)
				{
					if(i != (int)to.vecRelation.size() - 1)
					{
						strErr += "add relation must push to end for safe";
						return -1;
					}
					vecRelation.push_back(relation);
					return 0;
				}
			}
			setToRelation.insert(relation);
		}	

		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &relation = vecRelation[i];
			if(setToRelation.find(relation) ==  setToRelation.end())
			{
				iChangeCount++;
				string s;
				relation.ToString(s);
				snprintf(szErr, sizeof(szErr), "need del relation [%s];", s.c_str());
				strErr += szErr;
				if(iChange)
				{
					if(i != (int)vecRelation.size() - 1)
					{
						strErr = "del relation must push to end for safe";
						return -1;
					}
					strErr = "not support del relation";
					return -1;
					//assert(0); //必须加上安全保护措施才可以del
					//vecRelation.erase(i);
					//return 0;
				}
			}
		}
		vector<TStorageEngineItem> vecAllFromEngine, vecAllToEngine;
		vecAllFromEngine.push_back(mainEngine);
		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
			vecAllFromEngine.push_back(vecPluginEngine[i]);
		
		vecAllToEngine.push_back(to.mainEngine);
		for(int i = 0; i < (int)to.vecPluginEngine.size(); ++i)
			vecAllToEngine.push_back(to.vecPluginEngine[i]);	

		if(iSchedulerType == em_scheduler_writeback && iConsistent == em_consistency_strong)
		{
			TStorageEngineItemID thisSEID, toSEID;
			to.mainEngine.ConvertTo(toSEID);
			mainEngine.ConvertTo(thisSEID);
			if(toSEID !=  thisSEID)
			{
				char szErr[1024];
				snprintf(szErr, sizeof(szErr), "main EngineIDItem can not changed from [%d %d %d] to [%d %d %d]", 
					thisSEID.engineid, thisSEID.masterid, thisSEID.bid,
					toSEID.engineid, toSEID.masterid, toSEID.bid);
				strErr = szErr;		
				return -1;		
			}
		}

		if(vecAllFromEngine.size() > vecAllToEngine.size())
		{
			int i = 0; 
			for(; i < (int)vecAllToEngine.size(); ++i)
			{
				TStorageEngineItem &stTStorageEngineItem = vecAllToEngine[i];
				if(stTStorageEngineItem != vecAllFromEngine[i])
				{
					iChangeCount++;
					string s;
					stTStorageEngineItem.ToString(s);
					snprintf(szErr, sizeof(szErr), "cannot change engine [%s];", s.c_str());
					strErr = szErr;
					return -1;
				}
			}
			
			for(; i < (int)vecAllFromEngine.size(); ++i)
			{
				TStorageEngineItem &engine = vecAllFromEngine[i];
				TStorageEngineItemID stSEID;
				engine.ConvertTo(stSEID);
				
				iChangeCount++;
				string s;
				stSEID.ToString(s);
				snprintf(szErr, sizeof(szErr), "need del engine [%s];", s.c_str());
				strErr += szErr;
				if(iChange)
				{
					if(i != (int)vecAllFromEngine.size() - 1)
					{
						snprintf(szErr, sizeof(szErr), "del engine must be end for safe [%s];", s.c_str());
						strErr = szErr;
						return -1;
					}

					if(setSEID.find(stSEID) != setSEID.end())
					{
						snprintf(szErr, sizeof(szErr), "can not del engine that have no relaion [%s];", s.c_str());
						strErr = szErr;
						return -1;
					}
					strErr = "not support del now";
					
					return -1;
					//assert(0);//必须加上安全保护措施才可以del
					//return 0;
				}
			}
		}
		else if(vecAllFromEngine.size() <= vecAllToEngine.size())
		{
			int i = 0; 
			for(; i < (int)vecAllFromEngine.size(); ++i)
			{
				TStorageEngineItem &stTStorageEngineItem = vecAllFromEngine[i];
				if(stTStorageEngineItem != vecAllToEngine[i])
				{
					iChangeCount++;
					string s;
					stTStorageEngineItem.ToString(s);
					snprintf(szErr, sizeof(szErr), "cannot change engine [%s];", s.c_str());
					strErr = szErr;
					return -1;
				}
			}
			
			for(; i < (int)vecAllToEngine.size(); ++i)
			{
				TStorageEngineItem &engine = vecAllToEngine[i];
				TStorageEngineItemID stSEID;
				engine.ConvertTo(stSEID);
				
				iChangeCount++;
				string s;
				stSEID.ToString(s);
				snprintf(szErr, sizeof(szErr), "need add engine [%s];", s.c_str());
				strErr += szErr;
				if(iChange)
				{
					if(i != (int)vecAllToEngine.size() - 1)
					{
						strErr += "add engine must push to end for safe";
						return -1;
					}
					vecAllFromEngine.push_back(engine);
					mainEngine = vecAllFromEngine[0];
					vecPluginEngine.clear();
					for(int j = 1; j < (int)vecAllFromEngine.size(); ++j)
					{
						
						TStorageEngineItem &stItem = vecAllFromEngine[j];
						vecPluginEngine.push_back(stItem);
					}
					return 0;
				}
			}
		}
			
		if(iChangeCount > 1)
		{
			char szErr[1024];
			snprintf(szErr, sizeof(szErr), "iChangeCount %d > 1",  iChangeCount);
			strErr += szErr;		
			return -1;			
		}
		else if(iChangeCount == 0)
		{
			char szErr[1024];
			snprintf(szErr, sizeof(szErr), "iChangeCount %d, no need do change change",  iChangeCount);
			strErr = szErr;		
			return -1;		
		}
		return 0;
	}
	int DumpToFile(string &strErr, char *pBaseDir = T_BID_DESC_BASE_DIR)
	{
		char szFile[1024];
		char szErr[1024];
		int iRet = 0;
		//dump bid.desc
		snprintf(szFile, sizeof(szFile), "%s/%d.desc", pBaseDir, iCCNSBid);
		if(!access(szFile, F_OK))
		{
			char szBakFile[1024];
			snprintf(szBakFile, sizeof(szBakFile), "%s/%d.desc.bak", pBaseDir, iCCNSBid);
			iRet = rename(szFile, szBakFile);
			if(iRet)
			{
				snprintf(szErr, sizeof(szErr), "rename[%s] to [%s] failed errno %d", szFile, szBakFile, errno);
				strErr = szErr;
				return -1;
			}
		}
		FILE *fp = fopen(szFile,"w+");
		if(!fp)
		{
			snprintf(szErr, sizeof(szErr), "fopen[%s] failed errno %d", szFile, errno);
			strErr = szErr;
			return -1;
		}
		fprintf(fp,"Desc %s\n", szDesc.c_str());
		fprintf(fp,"DataVer	%d\n", iDataVer);	
		fclose(fp);

		snprintf(szFile, sizeof(szFile), "%s/%d.hash", pBaseDir, iCCNSBid);

		if(!access(szFile, F_OK))
		{
			char szBakFile[1024];
			snprintf(szBakFile, sizeof(szBakFile), "%s/%d.hash.bak", pBaseDir, iCCNSBid);
			iRet = rename(szFile, szBakFile);
			if(iRet)
			{
				snprintf(szErr, sizeof(szErr), "rename[%s] to [%s] failed errno %d", szFile, szBakFile, errno);
				strErr = szErr;
				return -1;
			}
		}
		fp = fopen(szFile,"w+");
		if(!fp)
		{
			snprintf(szErr, sizeof(szErr), "fopen[%s] failed errno %d", szFile, errno);
			strErr = szErr;
			return -1;
		}
		fprintf(fp, "SchedulerType %d\n", iSchedulerType);
		fprintf(fp, "Consistent %d\n", iConsistent);
		fprintf(fp, "PluginProp %d\n", iPluginProp);
		vector<TStorageEngineItem> vecEngine;
		vecEngine.push_back(mainEngine);
		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
			vecEngine.push_back(vecPluginEngine[i]);
		
		fprintf(fp, "Engine_cnt %d\n", (int)vecEngine.size());
		for(int i = 0; i < (int)vecEngine.size(); ++i)
		{
			TStorageEngineItem &stEngine = vecEngine[i];
			fprintf(fp, "Engine_%d_engineid %d\n", i, stEngine.engineid);
			fprintf(fp, "Engine_%d_masterid %d\n", i, stEngine.masterid);
			fprintf(fp, "Engine_%d_bid %d\n", i, stEngine.bid);
			fprintf(fp, "Engine_%d_master_cnt %d\n", i, (int)stEngine.vecMaster.size());
			for(int j = 0; j < (int)stEngine.vecMaster.size(); ++j)
			{
				TCCNSCommonIpAddr &stAddr = stEngine.vecMaster[j];
				char szIp[64];
				HtoP(stAddr.ip, szIp);
				fprintf(fp, "Engine_%d_master_%d %s %d %d %d\n", i, j, szIp, stAddr.port, stAddr.status, stAddr.proto);
			}
			fprintf(fp, "Engine_%d_access_cnt %d\n", i, (int)stEngine.vecAccess.size());
			for(int j = 0; j < (int)stEngine.vecAccess.size(); ++j)
			{
				TCCNSCommonIpAddr &stAddr = stEngine.vecAccess[j];
				char szIp[64];
				HtoP(stAddr.ip, szIp);
				fprintf(fp, "Engine_%d_access_%d %s %d %d %d\n", i, j, szIp, stAddr.port, stAddr.status, stAddr.proto);
			}
		}

		fprintf(fp, "Relation_cnt %d\n", (int)vecRelation.size());
		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &stRelation = vecRelation[i];
			fprintf(fp, "Relation_%d %d %d %d %d %d %d\n", i, 
				stRelation.left_engineid, stRelation.left_masterid, stRelation.left_bid,
				stRelation.right_engineid, stRelation.right_masterid, stRelation.right_bid);
			
		}
		fclose(fp);
		
		return CheckDataFromFile(strErr, iCCNSBid, pBaseDir);
	}
	int CheckDataFromFile(string &strErr, int iLoadCCNSBid, char *pBaseDir)
	{
		char szFile[1024];
		char szErr[1024];
		int iRet = 0;
		char szDescTmp[1024];
		int iDataVerTmp;
		if(iLoadCCNSBid != iCCNSBid)
		{
			snprintf(szErr, sizeof(szErr), "iCCNSBid check failed iLoadCCNSBid [%d] != iCCNSBid [%d]", iLoadCCNSBid, iCCNSBid);
			strErr = szErr;
			return -1;			
		}
		snprintf(szFile, sizeof(szFile), "%s/%d.desc", pBaseDir, iLoadCCNSBid);
		if(access(szFile, F_OK))
		{
			snprintf(szErr, sizeof(szErr), "access [%s] failed %d", szFile, errno);
			strErr = szErr;
			return -1;
		}
		TLib_Cfg_GetConfig(szFile,
			"Desc", CFG_STRING, szDescTmp, "", sizeof(szDescTmp),
			"DataVer", CFG_INT, &(iDataVerTmp), -1,  
			NULL);
		if(strlen(szDescTmp) <= 0 || szDesc.compare(szDescTmp))
		{
			snprintf(szErr, sizeof(szErr), "szTitle check failed real [%s] != need [%s]", szDescTmp, szDesc.c_str());
			strErr = szErr;
			return -1;
		}

		if(iDataVer == -1 || iDataVerTmp != iDataVer)
		{
			snprintf(szErr, sizeof(szErr), "iDataVer check failed real [%d] != need [%d]", iDataVerTmp, iDataVer);
			strErr = szErr;
			return -1;			
		}

		vector<TStorageEngineItem> vecEngine;
		vecEngine.push_back(mainEngine);
		for(int i = 0; i < (int)vecPluginEngine.size(); ++i)
			vecEngine.push_back(vecPluginEngine[i]);
		snprintf(szFile, sizeof(szFile), "%s/%d.hash", pBaseDir, iCCNSBid);
		if(access(szFile, F_OK))
		{
			snprintf(szErr, sizeof(szErr), "access [%s] failed %d", szFile, errno);
			strErr = szErr;
			return -1;
		}
		int iEngine_cnt = -1, iRelation_cnt = -1, iSchedulerTypeTmp = -1, iConsistentTmp = -1, iPluginPropTmp = -1;
		TLib_Cfg_GetConfig(szFile, 
			"Engine_cnt", CFG_INT, &(iEngine_cnt), -1,  
			"Relation_cnt", CFG_INT, &(iRelation_cnt), -1,  
			"SchedulerType", CFG_INT, &(iSchedulerTypeTmp), -1,  
			"Consistent", CFG_INT, &(iConsistentTmp), -1,
			"PluginProp", CFG_INT, &(iPluginPropTmp), -1,
			NULL);

		if(iEngine_cnt == -1 || iEngine_cnt != (int)vecEngine.size())
		{
			snprintf(szErr, sizeof(szErr), "Engine_cnt check failed real [%d] != need [%d]", iEngine_cnt, (int)vecEngine.size());
			strErr = szErr;
			return -1;
		}
		if(iRelation_cnt == -1 || iRelation_cnt != (int)vecRelation.size())
		{
			snprintf(szErr, sizeof(szErr), "Relation_cnt check failed real [%d] != need [%d]", iRelation_cnt, (int)vecRelation.size());
			strErr = szErr;
			return -1;
		}
		if(iSchedulerTypeTmp == -1 || iSchedulerTypeTmp != iSchedulerType)
		{
			snprintf(szErr, sizeof(szErr), "SchedulerType check failed real [%d] != need [%d]", iSchedulerTypeTmp, iSchedulerType);
			strErr = szErr;
			return -1;
		}
		if(iConsistentTmp == -1 || iConsistent != iConsistentTmp)
		{
			snprintf(szErr, sizeof(szErr), "Consistent check failed real [%d] != need [%d]", iConsistentTmp, iConsistent);
			strErr = szErr;
			return -1;
		}
		if(iPluginPropTmp == -1 || iPluginPropTmp != iPluginProp)
		{
			snprintf(szErr, sizeof(szErr), "PluginProp check failed real [%d] != need [%d]", iPluginPropTmp, iPluginProp);
			strErr = szErr;
			return -1;
		}
		for(int i = 0; i < (int)vecEngine.size(); ++i)
		{
			TStorageEngineItem &stEngine = vecEngine[i];
			char szKey_engineid[256] = {0};
			char szKey_masterid[256] = {0};
			char szKey_bid[256] = {0};
			char szKey_mastercnt[256] = {0};
			char szKey_accesscnt[256] = {0};
			int engineid = -1, masterid = -1, bid = -1, mastercnt = -1, accesscnt = -1;

			snprintf(szKey_engineid, sizeof(szKey_engineid), "Engine_%d_engineid", i);
			snprintf(szKey_masterid, sizeof(szKey_masterid), "Engine_%d_masterid", i);
			snprintf(szKey_bid, sizeof(szKey_bid), "Engine_%d_bid", i);
			snprintf(szKey_mastercnt, sizeof(szKey_mastercnt), "Engine_%d_master_cnt", i);
			snprintf(szKey_accesscnt, sizeof(szKey_accesscnt), "Engine_%d_access_cnt", i);
	
			TLib_Cfg_GetConfig(szFile,
				szKey_engineid, CFG_INT, &(engineid), -1,  
				szKey_masterid, CFG_INT, &(masterid), -1,  
				szKey_bid, CFG_INT, &(bid), -1,  
				szKey_mastercnt, CFG_INT, &(mastercnt), -1,  
				szKey_accesscnt, CFG_INT, &(accesscnt), -1,  
				NULL);
			
			if(engineid == -1 || engineid != stEngine.engineid)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].engineid check failed real [%d] != need [%d]", i, engineid, stEngine.engineid);
				strErr = szErr;
				return -1;				
			}
			if(masterid == -1 || masterid != stEngine.masterid)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].masterid check failed real [%d] != need [%d]", i, masterid, stEngine.masterid);
				strErr = szErr;
				return -1;				
			}
			if(bid == -1 || bid != stEngine.bid)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].bid check failed real [%d] != need [%d]", i, bid, stEngine.bid);
				strErr = szErr;
				return -1;				
			}
			if(mastercnt == -1 || mastercnt != (int)stEngine.vecMaster.size())
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].vecMaster.size() check failed real [%d] != need [%d]", i, mastercnt, (int)stEngine.vecMaster.size());
				strErr = szErr;
				return -1;				
			}
			if(accesscnt == -1 || accesscnt != (int)stEngine.vecAccess.size())
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].vecAccess.size() check failed real [%d] != need [%d]", i, accesscnt, (int)stEngine.vecAccess.size());
				strErr = szErr;
				return -1;				
			}

			for(int j= 0; j < (int)stEngine.vecMaster.size(); ++j)
			{
				TCCNSCommonIpAddr &stAddr = stEngine.vecMaster[j];
				char szKey[256];
				char szValue[1024];
				snprintf(szKey, sizeof(szKey), "Engine_%d_master_%d", i, j);
				TLib_Cfg_GetConfig(szFile, 
					szKey, CFG_STRING, szValue, "", sizeof(szValue),
					NULL);
				if(strlen(szValue) <= 0)
				{
					snprintf(szErr, sizeof(szErr), "%s check failed, empty", szKey);
					strErr = szErr;	
					return -1;
				}
				char szIp[64] = {0};
				int port = -1;
				int status = -1;
				int proto = -1;
				iRet = sscanf(szValue, "%63s %d %d %d", szIp, &port, &status, &proto);
				if(iRet != 4)
				{
					snprintf(szErr, sizeof(szErr), "%s check failed, count != 4, %s", szKey, szValue);
					strErr = szErr;	
					return -1;
				}
				char szNeedIp[64];
				HtoP(stAddr.ip, szNeedIp);
				if((int)inet_network(szIp) != stAddr.ip)
				{
					snprintf(szErr, sizeof(szErr), "%s szIp check failed, real[%s] != need [%s]", szKey, szIp, szNeedIp);
					strErr = szErr;	
					return -1;
				}
				if(port == -1 || port != stAddr.port)
				{
					snprintf(szErr, sizeof(szErr), "%s port check failed, real[%d] != need [%d]", szKey, port, stAddr.port);
					strErr = szErr;	
					return -1;
				}
				if(status == -1 || status != stAddr.status)
				{
					snprintf(szErr, sizeof(szErr), "%s status check failed, real[%d] != need [%d]", szKey, status, stAddr.status);
					strErr = szErr;	
					return -1;
				}
				if(proto == -1 || proto != stAddr.proto)
				{
					snprintf(szErr, sizeof(szErr), "%s proto check failed, real[%d] != need [%d]", szKey, proto, stAddr.proto);
					strErr = szErr;	
					return -1;
				}
			}

			for(int j= 0; j < (int)stEngine.vecAccess.size(); ++j)
			{
				TCCNSCommonIpAddr &stAddr = stEngine.vecAccess[j];
				char szKey[256];
				char szValue[1024];
				snprintf(szKey, sizeof(szKey), "Engine_%d_access_%d", i, j);
				TLib_Cfg_GetConfig(szFile, 
					szKey, CFG_STRING, szValue, "", sizeof(szValue),
					NULL);
				if(strlen(szValue) <= 0)
				{
					snprintf(szErr, sizeof(szErr), "%s check failed, empty", szKey);
					strErr = szErr;	
					return -1;
				}
				char szIp[64] = {0};
				int port = -1;
				int status = -1;
				int proto = -1;
				iRet = sscanf(szValue, "%63s %d %d %d", szIp, &port, &status, &proto);
				if(iRet != 4)
				{
					snprintf(szErr, sizeof(szErr), "%s check failed, count != 4, %s", szKey, szValue);
					strErr = szErr;	
					return -1;
				}
				char szNeedIp[64];
				HtoP(stAddr.ip, szNeedIp);
				if((int)inet_network(szIp) != stAddr.ip)
				{
					snprintf(szErr, sizeof(szErr), "%s szIp check failed, real[%s] != need [%s]", szKey, szIp, szNeedIp);
					strErr = szErr;	
					return -1;
				}
				if(port == -1 || port != stAddr.port)
				{
					snprintf(szErr, sizeof(szErr), "%s port check failed, real[%d] != need [%d]", szKey, port, stAddr.port);
					strErr = szErr;	
					return -1;
				}
				if(status == -1 || status != stAddr.status)
				{
					snprintf(szErr, sizeof(szErr), "%s status check failed, real[%d] != need [%d]", szKey, status, stAddr.status);
					strErr = szErr;	
					return -1;
				}
				if(proto == -1 || proto != stAddr.proto)
				{
					snprintf(szErr, sizeof(szErr), "%s szProto check failed, real[%d] != need [%d]", szKey, proto, stAddr.proto);
					strErr = szErr;	
					return -1;
				}
			}

					
		}

		for(int i = 0; i < (int)vecRelation.size(); ++i)
		{
			TStorageEngineKeySchedulerRelation &relation = vecRelation[i];
			
			char szKey[256];
			char szValue[1024];
			snprintf(szKey, sizeof(szKey), "Relation_%d", i);
			TLib_Cfg_GetConfig(szFile, 
				szKey, CFG_STRING, szValue, "", sizeof(szValue),
				NULL);
			if(strlen(szValue) <= 0)
			{
				snprintf(szErr, sizeof(szErr), "%s check failed, empty", szKey);
				strErr = szErr;	
				return -1;
			}
			int iLeft_engineid = -1, iLeft_masterid = -1, iLeft_bid = -1, iRight_engineid = -1, iRight_masterid = -1, iRight_bid = -1;
			iRet = sscanf(szValue, "%d %d %d %d %d %d", &iLeft_engineid, &iLeft_masterid, &iLeft_bid, &iRight_engineid, &iRight_masterid, &iRight_bid);
			if(iRet != 6)
			{
				snprintf(szErr, sizeof(szErr), "%s check failed, count != 6, %s", szKey, szValue);
				strErr = szErr;	
				return -1;
			}

			if((iLeft_engineid == -1 || iLeft_engineid != relation.left_engineid) ||
				(iLeft_masterid == -1 || iLeft_masterid != relation.left_masterid) ||
				(iLeft_bid == -1 || iLeft_bid != relation.left_bid) ||
				(iRight_engineid == -1 || iRight_engineid != relation.right_engineid) ||
				(iRight_masterid == -1 || iRight_masterid != relation.right_masterid) ||
				(iRight_bid == -1 || iRight_bid != relation.right_bid))
			{
				snprintf(szErr, sizeof(szErr), "%s check failed, real[%s] need [%d %d %d %d %d %d]", szKey, szValue,
					relation.left_engineid, relation.left_masterid, relation.left_bid,
					relation.right_engineid, relation.right_masterid, relation.right_bid);
				strErr = szErr;	
				return -1;	
			}
		}
		return 0;
	}

	int LoadFromFile(string &strErr, int iLoadCCNSBid, char *pBaseDir = T_BID_DESC_BASE_DIR)
	{
		char szFile[1024];
		char szErr[1024];
		int iRet = 0;
		char szDescTmp[1024];
		snprintf(szFile, sizeof(szFile), "%s/%d.desc", pBaseDir, iLoadCCNSBid);
		if(access(szFile, F_OK))
		{
			snprintf(szErr, sizeof(szErr), "access [%s] failed %d", szFile, errno);
			strErr = szErr;
			return -1;
		}
		TLib_Cfg_GetConfig(szFile,
			"Desc", CFG_STRING, szDescTmp, "", sizeof(szDescTmp),
			"DataVer", CFG_INT, &(iDataVer), -1,  
			NULL);
		iCCNSBid = iLoadCCNSBid;
		if(strlen(szDescTmp) <= 0 )
		{
			snprintf(szErr, sizeof(szErr), "szDesc invalid [%s]", szDescTmp);
			strErr = szErr;
			return -1;
		}
		szDesc = szDescTmp;
		
		if(-1 == iDataVer)
		{
			snprintf(szErr, sizeof(szErr), "iDataVer invalid [%d]", iDataVer);
			strErr = szErr;
			return -1;			
		}
		snprintf(szFile, sizeof(szFile), "%s/%d.hash", pBaseDir, iLoadCCNSBid);
		if(access(szFile, F_OK))
		{
			snprintf(szErr, sizeof(szErr), "access [%s] failed %d", szFile, errno);
			strErr = szErr;
			return -1;
		}
		vector<TStorageEngineItem> vecEngine;
		
		int iEngine_cnt = -1, iRelation_cnt = -1;
		TLib_Cfg_GetConfig(szFile, 
			"Engine_cnt", CFG_INT, &(iEngine_cnt), -1,  
			"Relation_cnt", CFG_INT, &(iRelation_cnt), -1,  
			"SchedulerType", CFG_INT, &(iSchedulerType), -1,  
			"Consistent", CFG_INT, &(iConsistent), -1,
			"PluginProp", CFG_INT, &(iPluginProp), 0,
			NULL);
		if(iEngine_cnt == -1)
		{
			snprintf(szErr, sizeof(szErr), "Engine_cnt invalid [%d]", iEngine_cnt);
			strErr = szErr;
			return -1;
		}
		if(iRelation_cnt == -1)
		{
			snprintf(szErr, sizeof(szErr), "Relation_cnt invalid [%d]", iRelation_cnt);
			strErr = szErr;
			return -1;
		}
		if(iSchedulerType == -1)
		{
			snprintf(szErr, sizeof(szErr), "iSchedulerType  invalid [%d]", iSchedulerType);
			strErr = szErr;
			return -1;
		}
		if(iConsistent == -1)
		{
			snprintf(szErr, sizeof(szErr), "iConsistent  invalid [%d]", iConsistent);
			strErr = szErr;
			return -1;
		}	
		for(int i = 0; i < iEngine_cnt; ++i)
		{
			TStorageEngineItem stEngine;
			char szKey_engineid[256] = {0};
			char szKey_masterid[256] = {0};
			char szKey_bid[256] = {0};
			char szKey_mastercnt[256] = {0};
			char szKey_accesscnt[256] = {0};
			int engineid = -1, masterid = -1, bid = -1, mastercnt = -1, accesscnt = -1;

			snprintf(szKey_engineid, sizeof(szKey_engineid), "Engine_%d_engineid", i);
			snprintf(szKey_masterid, sizeof(szKey_masterid), "Engine_%d_masterid", i);
			snprintf(szKey_bid, sizeof(szKey_bid), "Engine_%d_bid", i);
			snprintf(szKey_mastercnt, sizeof(szKey_mastercnt), "Engine_%d_master_cnt", i);
			snprintf(szKey_accesscnt, sizeof(szKey_accesscnt), "Engine_%d_access_cnt", i);
	
			TLib_Cfg_GetConfig(szFile,
				szKey_engineid, CFG_INT, &(engineid), -1,  
				szKey_masterid, CFG_INT, &(masterid), -1,  
				szKey_bid, CFG_INT, &(bid), -1,  
				szKey_mastercnt, CFG_INT, &(mastercnt), -1,  
				szKey_accesscnt, CFG_INT, &(accesscnt), -1,  
				NULL);
			
			if(engineid == -1)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].engineid invalid [%d]", i, engineid);
				strErr = szErr;
				return -1;				
			}
			if(masterid == -1)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].masterid invalid [%d]", i, masterid);
				strErr = szErr;
				return -1;				
			}
			if(bid == -1)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].bid invalid [%d]", i, bid);
				strErr = szErr;
				return -1;				
			}
			if(mastercnt == -1)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].vecMaster.size() invalid [%d]", i, mastercnt);
				strErr = szErr;
				return -1;				
			}
			if(accesscnt == -1)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].vecAccess.size() invalid [%d]", i, accesscnt);
				strErr = szErr;
				return -1;				
			}
			stEngine.engineid = engineid;
			stEngine.masterid = masterid;
			stEngine.bid = bid;
			for(int j= 0; j < (int)mastercnt; ++j)
			{
				TCCNSCommonIpAddr stAddr;
				char szKey[256];
				char szValue[1024];
				snprintf(szKey, sizeof(szKey), "Engine_%d_master_%d", i, j);
				TLib_Cfg_GetConfig(szFile, 
					szKey, CFG_STRING, szValue, "", sizeof(szValue),
					NULL);
				if(strlen(szValue) <= 0)
				{
					snprintf(szErr, sizeof(szErr), "%s invalid, empty", szKey);
					strErr = szErr;	
					return -1;
				}
				char szIp[64] = {0};
				int port = -1;
				int status = -1;
				int proto = -1;
				iRet = sscanf(szValue, "%63s %d %d %d", szIp, &port, &status, &proto);
				if(iRet != 4)
				{
					snprintf(szErr, sizeof(szErr), "%s invalid, count != 4, %s", szKey, szValue);
					strErr = szErr;	
					return -1;
				}

				if(port == -1)
				{
					snprintf(szErr, sizeof(szErr), "%s port invalid [%d]", szKey, port);
					strErr = szErr;	
					return -1;
				}
				if(status == -1)
				{
					snprintf(szErr, sizeof(szErr), "%s status invalid[%d]", szKey, status);
					strErr = szErr;	
					return -1;
				}
				stAddr.ip = inet_network(szIp);
				stAddr.port = port;
				stAddr.status = status;
				stAddr.proto = (EProtoType)proto;
				stEngine.vecMaster.push_back(stAddr);
			}
			if((int)stEngine.vecMaster.size() != mastercnt)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].vecMaster.size() status invalid[%d] != need[%d]", i, (int)stEngine.vecMaster.size(), mastercnt);
				strErr = szErr;	
				return -1;	
			}
			
			for(int j= 0; j < (int)accesscnt; ++j)
			{
				TCCNSCommonIpAddr stAddr;
				char szKey[256];
				char szValue[1024];
				snprintf(szKey, sizeof(szKey), "Engine_%d_access_%d", i, j);
				TLib_Cfg_GetConfig(szFile, 
					szKey, CFG_STRING, szValue, "", sizeof(szValue),
					NULL);
				if(strlen(szValue) <= 0)
				{
					snprintf(szErr, sizeof(szErr), "%s invalid, empty", szKey);
					strErr = szErr;	
					return -1;
				}
				char szIp[64] = {0};
				int port = -1;
				int status = -1;
				int proto = -1;
				iRet = sscanf(szValue, "%63s %d %d %d", szIp, &port, &status, &proto);
				if(iRet != 4)
				{
					snprintf(szErr, sizeof(szErr), "%s invalid, count != 4, %s", szKey, szValue);
					strErr = szErr;	
					return -1;
				}

				if(port == -1)
				{
					snprintf(szErr, sizeof(szErr), "%s port invalid [%d]", szKey, port);
					strErr = szErr;	
					return -1;
				}
				if(status == -1)
				{
					snprintf(szErr, sizeof(szErr), "%s status invalid[%d]", szKey, status);
					strErr = szErr;	
					return -1;
				}
				stAddr.ip = inet_network(szIp);
				stAddr.port = port;
				stAddr.status = status;
				stAddr.proto = (EProtoType)proto;
				stEngine.vecAccess.push_back(stAddr);
			}

			if((int)stEngine.vecAccess.size() != accesscnt)
			{
				snprintf(szErr, sizeof(szErr), "stEngine[%d].vecAccess.size() status invalid[%d] != need[%d]", i, (int)stEngine.vecAccess.size(), accesscnt);
				strErr = szErr;	
				return -1;	
			}	

			vecEngine.push_back(stEngine);
		}
		if((int)vecEngine.size() != iEngine_cnt || (int)vecEngine.size() < 1)
		{
			snprintf(szErr, sizeof(szErr), "vecEngine.size() invalid[%d] != need[%d]",(int)vecEngine.size(), iEngine_cnt);
			strErr = szErr;	
			return -1;	
		}
		mainEngine = vecEngine[0];
		vecPluginEngine.clear();
		for(int i = 1; i < (int)vecEngine.size(); ++i)
		{
			vecPluginEngine.push_back(vecEngine[i]);
		}
		vecRelation.clear();
		for(int i = 0; i < iRelation_cnt; ++i)
		{
			TStorageEngineKeySchedulerRelation relation;
			
			char szKey[256];
			char szValue[1024];
			snprintf(szKey, sizeof(szKey), "Relation_%d", i);
			TLib_Cfg_GetConfig(szFile, 
				szKey, CFG_STRING, szValue, "", sizeof(szValue),
				NULL);
			if(strlen(szValue) <= 0)
			{
				snprintf(szErr, sizeof(szErr), "%s check failed, empty", szKey);
				strErr = szErr;	
				return -1;
			}
			int iLeft_engineid = -1, iLeft_masterid = -1, iLeft_bid = -1, iRight_engineid = -1, iRight_masterid = -1, iRight_bid = -1;
			iRet = sscanf(szValue, "%d %d %d %d %d %d", &iLeft_engineid, &iLeft_masterid, &iLeft_bid, &iRight_engineid, &iRight_masterid, &iRight_bid);
			if(iRet != 6)
			{
				snprintf(szErr, sizeof(szErr), "%s check failed, count != 6, %s", szKey, szValue);
				strErr = szErr;	
				return -1;
			}

			if(iLeft_engineid == -1||
				iLeft_masterid == -1 ||
				iLeft_bid == -1 || 
				iRight_engineid == -1 ||
				iRight_masterid == -1 ||
				iRight_bid == -1)
			{
				snprintf(szErr, sizeof(szErr), "%s check failed, real[%d %d %d %d %d %d]", szKey,
					iLeft_engineid, iLeft_masterid, iLeft_bid,
					iRight_engineid, iRight_masterid, iRight_bid);
				
				strErr = szErr;	
				return -1;	
			}
			relation.left_engineid = iLeft_engineid;
			relation.left_masterid = iLeft_masterid;
			relation.left_bid = iLeft_bid;
			relation.right_engineid = iRight_engineid;
			relation.right_masterid = iRight_masterid;
			relation.right_bid = iRight_bid;
			vecRelation.push_back(relation);
		}
		if((int)vecRelation.size() != iRelation_cnt)
		{
			snprintf(szErr, sizeof(szErr), "vecRelation.size() invalid[%d] != need[%d]",(int)vecRelation.size(), iRelation_cnt);
			strErr = szErr;	
			return -1;	
		}
		return 0;
	}
	int iCCNSBid;
	string szDesc;
	int iDataVer;	//衍生数据
	TStorageEngineItem mainEngine;
	vector<TStorageEngineItem> vecPluginEngine;
	vector<TStorageEngineKeySchedulerRelation> vecRelation;
	int iSchedulerType;//em_scheduler_writeback
	int iConsistent;//em_consistency_strong
	int iPluginProp;//em_undetermined_plugin/em_forbid_plugin/em_permitted_plugin
}TCCNSBidDesc;


#endif

