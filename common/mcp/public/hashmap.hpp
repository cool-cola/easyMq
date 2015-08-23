
#ifndef __HASHMAP_HPP__
#define __HASHMAP_HPP__

#include "IdxObjMng.hpp"
#include "Base.hpp"
#include "Sem.hpp"
#include <list>
#include <map>

//hasp map array number
#define BUFFER_NUM				6

#define CACHE_MAP_BLOCK_NUM		10000


typedef struct
{
	volatile int active_buf_idx;	/* Currently used buffer idx */
	int updating_buf_idx;			/* Currently updating buffer  */
	u_int32_t buf_size;
}buffer_selector_t;

typedef struct
{
	u_int32_t ip_master;
	u_int32_t ip_slave;
	unsigned char master_status;	/* en_host_status */
	unsigned char slave_status;
}host_pair_item_t;

typedef struct
{
	u_int32_t ip_master;
	u_int32_t ip_slave;		/* 冗余，通过iplist可以找到ip_slave */
	u_int32_t dst_ip_master; /*迁移时的目的master的ip，默认值是0*/
	u_int32_t dst_ip_slave;/*迁移时的目的slave的ip，默认值是0*/
	u_int16_t cacheid;		/* use u_int16_t to saving space */
	u_int16_t dst_cacheid;  /*迁移时的目的ip，默认值是0*/
	char status;			/* en_cache_block_status */
	char reserved;
}cache_block_t;

typedef struct
{
	int32_t bid;
	int32_t version;
	cache_block_t cache_map[CACHE_MAP_BLOCK_NUM];
}cache_hashmap_t;


typedef struct
{
	u_int32_t data_good;			/* 0:共享内存不完整,1:共享内存OK */
	u_int16_t cache_rport;
	u_int16_t cache_wport;
	u_int32_t researved[8];
}cache_mem_head_t;

typedef struct
{
	ssize_t ds_suffix;
	ssize_t bucket_used;
	ssize_t bucket_num;
	ssize_t host_num;
	int32_t hostmap_ver;
}host_list_head_t;

typedef struct
{
	ssize_t ds_suffix;
	ssize_t bucket_used;
	ssize_t bucket_num;
	ssize_t bid_num;
}cache_hash_map_head_t;

typedef struct
{
	idx_t first_idx;
}bucket_desc_t;


class strategy_mng
{
public:
	enum
	{
		DISABLE_STRATEGY = -1,
		RD_ONLY_STRATEGY = 1 << 0,
		WR_ONLY_STRATEGY = 1 << 1,
		DE_ONLY_STRATEGY = 1 << 2,
		RD_ONLY_STRATEGY_INTER_COPY = 1 << 3,
		WR_ONLY_STRATEGY_INTER_COPY = 1 << 4,
		DE_ONLY_STRATEGY_INTER_COPY = 1 << 5,
		DEFAULT_STRATEGY = RD_ONLY_STRATEGY | WR_ONLY_STRATEGY | DE_ONLY_STRATEGY |
						RD_ONLY_STRATEGY_INTER_COPY | 
						WR_ONLY_STRATEGY_INTER_COPY | 
						DE_ONLY_STRATEGY_INTER_COPY,
	};
	struct bid_strategy
	{
		int m_i_bid;
		int m_i_strategy;
		int m_i_reserve;
	};

	typedef struct __strategy_shm_head_t
	{
		int lock;
		int ver;
		int param1;
		int param2;
		int param3;
		int param4;
		int bid_num;
		int total_bid_num;
	} strategy_shm_head_t;
	strategy_mng()
	{
		m_p_shm_addr = NULL; 
	}
	size_t count_mem_size(int bid_num)
	{
		return sizeof(strategy_shm_head_t) + sizeof(bid_strategy) * bid_num;
	}
	int init(int bid_num, char* cfg = NULL)
	{
		if (cfg)
			strncpy(m_sz_strategy_file, cfg, sizeof(m_sz_strategy_file) -1);
		else
			strncpy(m_sz_strategy_file, "../etc/strategy.cfg", sizeof(m_sz_strategy_file) -1);
		int ret = 0;
		char buffer[256];
		char* shm_file = ".strategy";
		sprintf(buffer, "touch %s", shm_file);
		system(buffer);

		size_t mem_size = count_mem_size(bid_num);
		key_t shm_key = ftok(shm_file, 'L');
		printf("Creating %s shm: key[0x%08x], size[%d].\n",
			 shm_file, (unsigned int)shm_key, (int)mem_size);
		assert(shm_key != -1);
		int err;
		bool bNewCreate;
		m_p_shm_addr = CreateShm(shm_key, mem_size, err, bNewCreate, 0);
		if (!m_p_shm_addr)
		{
			printf("attach %s key[0x%08x] size[%d] error\n", shm_file, (unsigned int)shm_key, (int)mem_size);
			return -1;
		}

		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		if (bNewCreate)
		{
			phead->lock =1;
			phead->ver = 0;
			phead->bid_num = 0;
			phead->total_bid_num = bid_num;
			memset(m_p_shm_addr + sizeof(strategy_shm_head_t), 0, sizeof(bid_strategy) * bid_num);
			
			ret = load_strategy(m_sz_strategy_file);
			if (ret)
			{
				phead->lock = 0;
				return ret;
			}
			
			phead->lock = 0;
			printf("new create bid_strategy shm!\n");
		}
				
		return 0;
	}
	
	bool strategy_ctrl(int bid, int strategy)
	{
		int my_strategy;
		if (get_strategy(bid, my_strategy) == 0)
		{
			return (strategy & my_strategy)? true : false;
		}
		return true;
	}
	int get_strategy(int bid, int& strategy)
	{
		//strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		//if (phead->lock != 0)
		//	return -11;
		//phead->lock = 1;
		bid_strategy* pStrategy = bid_strategy_by_bid( bid);
		if (pStrategy == NULL)
		{
	
			strategy =  DISABLE_STRATEGY;
		}
		else
		{
			strategy =  pStrategy->m_i_strategy;
		}
		//phead->lock = 0;
		return 0;
	}
	int get_strategy(std::list<bid_strategy>& strategy_list)
	{
		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		//if (phead->lock != 0)
		//	return -11;
		//phead->lock = 1;
		if (strategy_list.empty())
		{
			bid_strategy* pStrategy = 0;
			
			for (int i = 0; i < phead->bid_num; ++i)
			{
				pStrategy = bid_strategy_by_idx(i);
				if (pStrategy)
					strategy_list.push_back(*pStrategy);
			}
			
		}
		else
		{
			std::map< int, bid_strategy >::iterator bid_strategy_it;
			bid_strategy* pStrategy = 0;
			for (std::list<bid_strategy>::iterator it = strategy_list.begin();
				it != strategy_list.end();
				++it)
			{
				pStrategy = bid_strategy_by_bid(it->m_i_bid);
				if (pStrategy == NULL)
				{
					it->m_i_strategy = DISABLE_STRATEGY;
				}
				else
				{
					it->m_i_strategy = pStrategy->m_i_strategy;
				}
				it->m_i_reserve = 0;
			}
		}
		//phead->lock = 0;
		return 0;
	}
	int set_strategy(std::list<bid_strategy>& strategy_list)
	{
		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		if (phead->lock != 0)
			return -11;
		phead->lock =1;
		bid_strategy* pStrategy = 0;
		bool b_update_file = false;
		for (std::list<bid_strategy>::iterator it = strategy_list.begin();
			it != strategy_list.end();
			++it)
		{
			pStrategy = bid_strategy_by_bid(it->m_i_bid);
			
			switch(it->m_i_strategy)
			{
			case DISABLE_STRATEGY:
			case DEFAULT_STRATEGY:	
				{
					if (pStrategy == NULL)
					{
						continue;
					}
					else
					{
						del_bid_strategy(pStrategy);
						b_update_file = true;
					}
					break;
				}
			default:
				{
					if (pStrategy == NULL)
					{
						bid_strategy strategy;
						strategy.m_i_bid = it->m_i_bid;
						strategy.m_i_strategy = it->m_i_strategy;
						strategy.m_i_reserve = 0;
						append_bid_strategy(&strategy);
					}
					else
					{
						pStrategy->m_i_strategy = it->m_i_strategy;
					}
					b_update_file = true;
					break;
				}
			}
		}
		if (b_update_file)
		{
			save_strategy(m_sz_strategy_file);
		}
		phead->lock = 0;
		return 0;
	}
private:
	int load_strategy(char* cfg)
	{
		std::vector< std::string > vecConf;
		if(LoadConf(cfg, vecConf))
		{
			printf("WARNING:open %s Failed\n",cfg);
			return -1;
		}


		bid_strategy strategy;
		int iRet;
		size_t i = 0;
	
		size_t total_num = vecConf.size();
		for (i=0; i < total_num; i++)
		{
			iRet = sscanf((char*)vecConf[i].c_str(),"%d %d %d", 
						&strategy.m_i_bid, 
						&strategy.m_i_strategy, 
						&strategy.m_i_reserve);
			if(iRet != 3)
			{
				printf("Load %s failed -2!\n",(char*)vecConf[i].c_str());
				return -2;
			}
			printf("bid[%d] strategy[%d] reserve[%d]\n", 
						strategy.m_i_bid, 
						strategy.m_i_strategy, 
						strategy.m_i_reserve);
			
			iRet = append_bid_strategy(&strategy);
			if (iRet)
			{
				printf("Load bid num[%d] error!\n", (int)i);
				return -3;
			}
		}
		return 0;
	}
	int save_strategy(char* cfg)
	{
		if (!cfg)
			return -1;
		FILE* fp = fopen(cfg, "w+");
		if (!fp)
		{
			printf("save strategy file[%s] failed\n", cfg);
			return -2;
		}
		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		bid_strategy* pStrategy;
		for (int i = 0;
			i  < phead->bid_num;
			++i)
		{
			pStrategy = bid_strategy_by_idx(i);
			if (pStrategy)
			{
				fprintf(fp, "%d %d %d\n", 
					pStrategy->m_i_bid,
					pStrategy->m_i_strategy,
					pStrategy->m_i_reserve);
			}
			
		}
		fclose(fp);
		return 0;
	}
	
	bid_strategy* bid_strategy_by_bid(int bid)
	{
		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		for (int i = 0; i < phead->bid_num; ++i)
		{
			bid_strategy* pStrategy = bid_strategy_by_idx(i);
			if (pStrategy && pStrategy->m_i_bid == bid)
			{
				return pStrategy;
			}
		}
		return NULL;
	}
	bid_strategy* bid_strategy_by_idx(int idx)
	{
		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		if (idx < 0 || idx >= phead->total_bid_num)
			return NULL;
		
		
		return	(bid_strategy*)(m_p_shm_addr + sizeof(strategy_shm_head_t) + idx * sizeof(bid_strategy));
		
	}
	int append_bid_strategy(bid_strategy* pStrategy)
	{	
		if (!pStrategy)
			return -12;
		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;

		bid_strategy* p_shm_strategy = bid_strategy_by_idx(phead->bid_num);
		if (p_shm_strategy == NULL)
		{
			printf("add bid[%d] strategy[%d] error\n", pStrategy->m_i_bid, pStrategy->m_i_strategy);
			return -1;
		}
		memcpy(p_shm_strategy, pStrategy, sizeof(bid_strategy));
		phead->bid_num++;
		return 0;
	}

	int del_bid_strategy(bid_strategy* pStrategy)
	{
		strategy_shm_head_t* phead = (strategy_shm_head_t*)m_p_shm_addr;
		bid_strategy* p_last_strategy = bid_strategy_by_idx(phead->bid_num - 1);
		if (p_last_strategy)
		{	
			memcpy(pStrategy, p_last_strategy,sizeof(bid_strategy));
			phead->bid_num--;
		}
		return 0;
	}
public:
	char m_sz_strategy_file[256];
	char* m_p_shm_addr;
};
class c_hash_map
{
public:
	enum hash_map_error_code
	{
		ERROR_CODE_BASE 	= -11200,
		NO_FOUND 			= ERROR_CODE_BASE -0,
		ILLEGAL_PARAMETER  	= ERROR_CODE_BASE -1,
		NO_MEMORY  			= ERROR_CODE_BASE -2,
		ERROR_INTERNAL 		= ERROR_CODE_BASE -3,
	};

	c_hash_map();
	~c_hash_map();

	int hashmap_init(ssize_t hostpair_node_num, ssize_t bid_node_num, int atomic);
	static ssize_t count_mem_size(ssize_t host_node_num, ssize_t bid_node_num);
	ssize_t AttachMem(int buf_idx, char* pMemPtr,const ssize_t MEMSIZE,ssize_t host_node_num,
			ssize_t bid_node_num, ssize_t iInitType=emInit);

	/* RCU sync function */
	void w_lock();
	void w_unlock();
	int update_start();
	int update_end();
	int get_updating_buf();
	int get_curr_buf();

	void set_cache_rw_port(u_int16_t cache_rport, u_int16_t cache_wport);
	u_int16_t get_cache_rport(void);
	u_int16_t get_cache_wport(void);

	ssize_t host_list_hash_func(unsigned int ip);
	ssize_t get_host_pair_idx(int buf_idx, unsigned int ip, ssize_t *prev_idx=NULL, ssize_t bucket=0);

	int set_hostmap_version(int ip_ver);
	int get_hostmap_version();
	int get_host_pair(unsigned int ip_slave, host_pair_item_t *host);
	int set_host_pair(host_pair_item_t *host);
	int del_host_pair(unsigned int ip_slave);
	int clearall_hostmap();

	ssize_t hashmap_hash_func(int32_t bid);
	ssize_t get_hashmap_idx(int buf_idx, int32_t bid, ssize_t *prev_idx=NULL, ssize_t bucket=0);
	
	cache_hashmap_t *get_hashmap_ptr(int bid);
	int get_hashmap(int bid, cache_hashmap_t *hashmap);
	int set_hashmap(cache_hashmap_t *hashmap);
	int del_hashmap(int bid);

	int DumpHashMap(cache_hashmap_t* phashmap);

	// dump 所有bid的hash map
	int DumpAllHashMap(vector<int> *pVecBid);

	cache_mem_head_t *cache_mem_head[BUFFER_NUM];

	/* host map */
	host_list_head_t *host_list_head[BUFFER_NUM];
	bucket_desc_t* host_bucket[BUFFER_NUM];
	TIdxObjMng host_obj_mng[BUFFER_NUM];

	/* cache block map */
	cache_hash_map_head_t *cache_hash_map_head[BUFFER_NUM];
	bucket_desc_t* cache_hashmap_bucket[BUFFER_NUM];
	TIdxObjMng hashmap_obj_mng[BUFFER_NUM];

	/* synchrize struture: hint: RCU */
	CSem m_sem;	/* semophore is only use for sync update */
	buffer_selector_t *buffer_ctrl;
	int m_atomic;
};

#endif

