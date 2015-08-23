

#include "hashmap.hpp"


#define ERR(p...) do{ fprintf(stderr, ":[%s %d]:", __FILE__, __LINE__); \
                         fprintf(stderr, ##p); \
                         fprintf(stderr, "\r\n");} while (0)

c_hash_map::c_hash_map()
{
	m_atomic = 0;
}

c_hash_map::~c_hash_map()
{
}

/* Create and initialse hashmap core structure */
int c_hash_map::hashmap_init(ssize_t hostpair_node_num, ssize_t bid_node_num, int atomic)
{
	char buffer[1024];

	if ( hostpair_node_num <= 0 || bid_node_num <= 0 )
	{
		return -1;
	}

	m_atomic = atomic;

	if ( m_atomic )
	{
		sprintf(buffer, "touch .access_lock");
		system(buffer);

		sprintf(buffer, ".access_lock");
		key_t sem_key = ftok(buffer, 'L');
		if (sem_key == -1)
		{
			ERR("ftok for file[%s] error.\n", buffer);
			return -2;
		}

		if ( m_sem.Open(sem_key) )
		{
			ERR("m_sem open failed.");
			return -2;
		}
	}

	sprintf(buffer, "touch .access_hashmap");
	system(buffer);

	sprintf(buffer, ".access_hashmap");
	key_t shm_key = ftok(buffer, 'L');
	if (shm_key == -1)
	{
		ERR("ftok for file[%s] error.\n", buffer);
		return -2;
	}

	bool bNewCreate;
	char *shm_addr;
	ssize_t iInitType;
	int iErrNo;
	ssize_t mem_size = count_mem_size(hostpair_node_num, bid_node_num);

	/* In order to use the quick sync way, create 3 copy of same memory block */
	ssize_t ttl_mem = sizeof(buffer_selector_t) + mem_size*BUFFER_NUM;

	printf("Create hashmap size[%ld] key[%08x]!\n", ttl_mem, shm_key);
	shm_addr = CreateShm(shm_key,ttl_mem, iErrNo, bNewCreate, 0);
	if ( !shm_addr )
	{
		ERR("Create hashmap memory size[%ld] failed[%d]!\n", mem_size, iErrNo);
		return -3;
	}

	iInitType = (bNewCreate) ? emInit : emRecover;

	buffer_ctrl = (buffer_selector_t *)shm_addr;
	if ( iInitType == emInit )
	{
		buffer_ctrl->active_buf_idx = 0;
		buffer_ctrl->updating_buf_idx = -1;
		buffer_ctrl->buf_size = (u_int32_t)mem_size;
	}

	for ( int bn=0; bn<BUFFER_NUM; bn++ )
	{
		ssize_t atBytes = AttachMem(bn, shm_addr+sizeof(buffer_selector_t)+mem_size*bn,
					mem_size,hostpair_node_num, bid_node_num, iInitType);
		if ( atBytes< 0 )
		{
			ERR("AttachMemFile failed, ret[%ld].\n", atBytes);
			return -4;
		}
	}


	return 0;
}

void c_hash_map::w_lock()
{
	if ( m_atomic )
		m_sem.Wait();
}

void c_hash_map::w_unlock()
{
	if ( m_atomic )
		m_sem.Post();
}

/* Start to update hash map core sturcture */
int c_hash_map::update_start()
{
	int buf_idx;
	w_lock();
	if ( !m_atomic )
	{
		buffer_ctrl->updating_buf_idx = 0;
		return 0;
	}

	if ( -1 != buffer_ctrl->updating_buf_idx )
	{
		ERR("update hashmap conflict!");
	}

	buf_idx = (buffer_ctrl->active_buf_idx+1) % BUFFER_NUM;

	memcpy(cache_mem_head[buf_idx], cache_mem_head[buffer_ctrl->active_buf_idx],
		buffer_ctrl->buf_size);

	buffer_ctrl->updating_buf_idx = buf_idx;
	return 0;
}

//Need wlock by calling function
int c_hash_map::update_end()
{
	buffer_ctrl->active_buf_idx = buffer_ctrl->updating_buf_idx;
	buffer_ctrl->updating_buf_idx = -1;

	w_unlock();
	return 0;
}

int c_hash_map::get_updating_buf()
{
	if ( buffer_ctrl->updating_buf_idx < 0 )
	{
		ERR("updating_buf_idx is no set!");
		assert(0);
	}

	return buffer_ctrl->updating_buf_idx;
}

int c_hash_map::get_curr_buf()
{
	return buffer_ctrl->active_buf_idx;
}


ssize_t c_hash_map::count_mem_size(ssize_t host_node_num, ssize_t bid_node_num)
{
	ssize_t mem_size;
	ssize_t host_bucket_num = host_node_num;
	ssize_t cache_bucket_num = bid_node_num;

	mem_size = sizeof(cache_mem_head_t) +
			sizeof(host_list_head_t) + host_bucket_num*sizeof(bucket_desc_t) +
			TIdxObjMng::CountMemSize(sizeof(host_pair_item_t),host_node_num, 1) +
		 	sizeof(cache_hash_map_head_t) + cache_bucket_num*sizeof(bucket_desc_t) +
			 TIdxObjMng::CountMemSize(sizeof(cache_hashmap_t),bid_node_num, 1);

	return mem_size;
}

//return used bytes or errcode
ssize_t c_hash_map::AttachMem(int buf_idx, char* pMemPtr,const ssize_t MEMSIZE,ssize_t host_node_num, ssize_t bid_node_num, ssize_t iInitType/*=emInit*/)
{
	ssize_t host_bucket_num = host_node_num;
	ssize_t cache_bucket_num = bid_node_num;

	if (MEMSIZE < count_mem_size(host_node_num,bid_node_num))
	{
		return -1;
	}

	ssize_t iAttachBytes=0,iAllocBytes = 0;


	cache_mem_head[buf_idx] = (cache_mem_head_t*)(pMemPtr + iAllocBytes);
	iAllocBytes += sizeof(cache_mem_head_t);

	if (iInitType==emRecover && !cache_mem_head[buf_idx]->data_good )
		iInitType = emInit;

	host_list_head[buf_idx] = (host_list_head_t *)(pMemPtr + iAllocBytes);
	iAllocBytes += sizeof(host_list_head_t);

	host_bucket[buf_idx] = (bucket_desc_t*)(pMemPtr + iAllocBytes);
	iAllocBytes += (sizeof(bucket_desc_t)*host_bucket_num);

	iAttachBytes = host_obj_mng[buf_idx].AttachMem(pMemPtr + iAllocBytes, MEMSIZE-iAllocBytes, sizeof(host_pair_item_t),
	                           host_node_num, iInitType, 1);
	if ( iAttachBytes <= 0 )
	{
		return -2;
	}

	iAllocBytes += iAttachBytes;

	cache_hash_map_head[buf_idx] = (cache_hash_map_head_t *)(pMemPtr+iAllocBytes);
	iAllocBytes += sizeof(cache_hash_map_head_t);

	cache_hashmap_bucket[buf_idx] = (bucket_desc_t*)(pMemPtr + iAllocBytes);
	iAllocBytes += (sizeof(bucket_desc_t)*cache_bucket_num);

	iAttachBytes = hashmap_obj_mng[buf_idx].AttachMem(pMemPtr + iAllocBytes, MEMSIZE-iAllocBytes, sizeof(cache_hashmap_t),
	                           bid_node_num, iInitType, 1);
	if ( iAttachBytes <= 0 )
	{
		return -3;
	}

	iAllocBytes += iAttachBytes;

	ssize_t host_link = host_obj_mng[buf_idx].GetOneFreeDS();
	ssize_t hashmap_link = hashmap_obj_mng[buf_idx].GetOneFreeDS();

	if (iInitType == emInit)
	{
		memset(cache_mem_head[buf_idx], 0, sizeof(cache_mem_head_t));
		cache_mem_head[buf_idx]->data_good = 0;

		host_list_head[buf_idx]->ds_suffix = host_link;
		host_list_head[buf_idx]->bucket_num = host_bucket_num;
		host_list_head[buf_idx]->bucket_used = 0;
		host_list_head[buf_idx]->host_num = 0;
		host_list_head[buf_idx]->hostmap_ver = -1;

		memset(host_bucket[buf_idx],-1,host_bucket_num*sizeof(bucket_desc_t));

		cache_hash_map_head[buf_idx]->ds_suffix = hashmap_link;
		cache_hash_map_head[buf_idx]->bucket_num = cache_bucket_num;
		cache_hash_map_head[buf_idx]->bucket_used = 0;
		cache_hash_map_head[buf_idx]->bid_num = 0;

		memset(cache_hashmap_bucket[buf_idx],-1,cache_bucket_num*sizeof(bucket_desc_t));
	}
	else
	{
		/* check if memory is corrupt */
		if(host_list_head[buf_idx]->bucket_num != host_bucket_num)
		{
			return -4;
		}
		if(host_list_head[buf_idx]->ds_suffix != host_link)
		{
			return -5;
		}
	}

	cache_mem_head[buf_idx]->data_good = 1;
	return iAllocBytes;
}

void c_hash_map::set_cache_rw_port(u_int16_t cache_rport, u_int16_t cache_wport)
{
	int buf_idx = get_updating_buf();
	cache_mem_head[buf_idx]->cache_rport = cache_rport;
	cache_mem_head[buf_idx]->cache_wport = cache_wport;
}

u_int16_t c_hash_map::get_cache_rport(void)
{
	int buf_idx = get_curr_buf();

	return cache_mem_head[buf_idx]->cache_rport;
}

u_int16_t c_hash_map::get_cache_wport(void)
{
	int buf_idx = get_curr_buf();

	return cache_mem_head[buf_idx]->cache_wport;
}

ssize_t c_hash_map::host_list_hash_func(unsigned int ip)
{
	int buf_idx = get_curr_buf();

	ssize_t hash = ip % host_list_head[buf_idx]->bucket_num;
	return hash;
}

ssize_t c_hash_map::get_host_pair_idx(int buf_idx, unsigned int ip, ssize_t *prev_idx/*=NULL*/, ssize_t bucket/*=0*/)
{
	host_pair_item_t *host;
	ssize_t found_idx = -1;
	ssize_t temp_idx = -1;
	ssize_t idx;

	if ( bucket == 0 )
	{
		bucket = host_list_hash_func(ip);
	}

	idx = host_bucket[buf_idx][bucket].first_idx;

	while ( idx >= 0 )
	{
		host = (host_pair_item_t *)host_obj_mng[buf_idx].GetAttachObj(idx);
		if ( host->ip_slave == ip )
		{
			found_idx = idx;
			if ( prev_idx )
				*prev_idx = temp_idx;
			break;
		}

		temp_idx = idx;
		idx = host_obj_mng[buf_idx].GetDsIdx(idx, host_list_head[buf_idx]->ds_suffix);
	}

	return found_idx;
}

int c_hash_map::set_hostmap_version(int ip_ver)
{
	int buf_idx = get_updating_buf();

	host_list_head[buf_idx]->hostmap_ver = ip_ver;
	return 0;
}

int c_hash_map::get_hostmap_version()
{
	int buf_idx = get_curr_buf();

	return host_list_head[buf_idx]->hostmap_ver;
}

int c_hash_map::get_host_pair(unsigned int ip_slave, host_pair_item_t *host)
{
	int buf_idx = get_curr_buf();
	ssize_t idx = get_host_pair_idx(buf_idx, ip_slave);
	ssize_t iCopyLen;

	if ( idx < 0 )
	{
		return NO_FOUND;
	}

	iCopyLen = host_obj_mng[buf_idx].CopyAttachObj(idx, 0, (char *)host, sizeof(*host));

	return (iCopyLen==sizeof(*host)) ? 0:ERROR_INTERNAL;
}

int c_hash_map::set_host_pair(host_pair_item_t *host)
{
	//unsigned int ip = host->ip_master;
	unsigned int ip = host->ip_slave;
	ssize_t bucket = host_list_hash_func(ip);
	ssize_t idx;
	int buf_idx = get_updating_buf();


	idx = get_host_pair_idx(buf_idx, ip, NULL, bucket);
	if ( idx >= 0 )
	{
		/* host already exist */
		host_obj_mng[buf_idx].SetAttachObj(idx, 0, (char *)host, sizeof(*host));
		return 0;
	}

	idx = host_obj_mng[buf_idx].CreateObject();
	if ( idx < 0 )
	{
		ERR("create obj error!");
		return NO_MEMORY;
	}

	host_obj_mng[buf_idx].SetAttachObj(idx, 0, (char *)host, sizeof(*host));
	host_obj_mng[buf_idx].SetDsIdx(idx, host_list_head[buf_idx]->ds_suffix,
				host_bucket[buf_idx][bucket].first_idx);

	if ( host_bucket[buf_idx][bucket].first_idx < 0 )
	{
		host_list_head[buf_idx]->bucket_used++;
	}

	host_bucket[buf_idx][bucket].first_idx = idx;

	host_list_head[buf_idx]->host_num++;
	return 0;
}

int c_hash_map::del_host_pair(unsigned int ip_slave)
{
	ssize_t bucket = host_list_hash_func(ip_slave);
	ssize_t idx;
	ssize_t prev_idx, next_idx;

	int buf_idx = get_updating_buf();

	idx = get_host_pair_idx(buf_idx, ip_slave, &prev_idx, bucket);
	if ( idx < 0 )
	{
		/* host do not exist */
		return NO_FOUND;
	}

	if ( idx == host_bucket[buf_idx][bucket].first_idx )
	{
		host_bucket[buf_idx][bucket].first_idx = host_obj_mng[buf_idx].GetDsIdx(idx,
					host_list_head[buf_idx]->ds_suffix);
		if ( host_bucket[buf_idx][bucket].first_idx < 0 )
			host_list_head[buf_idx]->bucket_used--;
	}
	else
	{
		next_idx = host_obj_mng[buf_idx].GetDsIdx(idx, host_list_head[buf_idx]->ds_suffix);
		host_obj_mng[buf_idx].SetDsIdx(prev_idx, host_list_head[buf_idx]->ds_suffix, next_idx);
	}

	host_obj_mng[buf_idx].DestroyObject(idx);
	host_list_head[buf_idx]->host_num--;
	return 0;
}

int c_hash_map::clearall_hostmap()
{
	ssize_t bucket;
	ssize_t idx;

	int buf_idx = get_updating_buf();
	for ( bucket=0; bucket<host_list_head[buf_idx]->bucket_num; bucket++ )
	{
		idx = host_bucket[buf_idx][bucket].first_idx;

		while ( idx >= 0 )
		{
			host_bucket[buf_idx][bucket].first_idx = host_obj_mng[buf_idx].GetDsIdx(idx, host_list_head[buf_idx]->ds_suffix);

			host_obj_mng[buf_idx].DestroyObject(idx);

			idx = host_bucket[buf_idx][bucket].first_idx ;
		}
	}

	host_list_head[buf_idx]->bucket_used = 0;
	host_list_head[buf_idx]->host_num = 0;
	host_list_head[buf_idx]->hostmap_ver = -1;
	return 0;
}

ssize_t c_hash_map::hashmap_hash_func(int32_t bid)
{
	int buf_idx = get_curr_buf();

	ssize_t hash = bid % cache_hash_map_head[buf_idx]->bucket_num;
	return hash;
}

ssize_t c_hash_map::get_hashmap_idx(int buf_idx, int32_t bid, ssize_t *prev_idx/*=NULL*/, ssize_t bucket/*=0*/)
{
	cache_hashmap_t *hashmap;
	ssize_t found_idx = -1;
	ssize_t temp_idx = -1;
	ssize_t idx;

	if ( bucket == 0 )
	{
		bucket = hashmap_hash_func(bid);
	}

	idx = cache_hashmap_bucket[buf_idx][bucket].first_idx;

	while ( idx >= 0 )
	{
		hashmap = (cache_hashmap_t *)hashmap_obj_mng[buf_idx].GetAttachObj(idx);
		if ( hashmap->bid == bid )
		{
			found_idx = idx;
			if ( prev_idx )
				*prev_idx = temp_idx;
			break;
		}

		temp_idx = idx;
		idx = hashmap_obj_mng[buf_idx].GetDsIdx(idx, cache_hash_map_head[buf_idx]->ds_suffix);
	}

	return found_idx;
}

cache_hashmap_t *c_hash_map::get_hashmap_ptr(int bid)
{
	cache_hashmap_t *hashmap;
	int buf_idx = get_curr_buf();	
	ssize_t idx = get_hashmap_idx(buf_idx, bid);

	if ( idx < 0 )
	{
		return NULL;
	}

	hashmap = (cache_hashmap_t *)hashmap_obj_mng[buf_idx].GetAttachObj(idx);
	return hashmap;
}

int c_hash_map::get_hashmap(int bid, cache_hashmap_t *hashmap)
{
	int buf_idx = get_curr_buf();
	ssize_t idx = get_hashmap_idx(buf_idx, bid);
	ssize_t iCopyLen;

	if ( idx < 0 )
	{
		return NO_FOUND;
	}

	iCopyLen = hashmap_obj_mng[buf_idx].CopyAttachObj(idx, 0, (char *)hashmap, sizeof(*hashmap));

	return (iCopyLen==sizeof(*hashmap))?0:ERROR_INTERNAL;
}

int c_hash_map::set_hashmap(cache_hashmap_t *hashmap)
{
	ssize_t bucket = hashmap_hash_func(hashmap->bid);
	ssize_t idx;
	int buf_idx = get_updating_buf();

	idx = get_hashmap_idx(buf_idx,hashmap->bid, NULL, bucket);
	if ( idx >= 0 )
	{
		/* host already exist */
		hashmap_obj_mng[buf_idx].SetAttachObj(idx, 0, (char *)hashmap, sizeof(*hashmap));
		return 0;
	}

	idx = hashmap_obj_mng[buf_idx].CreateObject();
	if ( idx < 0 )
	{
		ERR("create obj error!");
		return NO_MEMORY;
	}

	hashmap_obj_mng[buf_idx].SetAttachObj(idx, 0, (char *)hashmap, sizeof(*hashmap));
	hashmap_obj_mng[buf_idx].SetDsIdx(idx, cache_hash_map_head[buf_idx]->ds_suffix, cache_hashmap_bucket[buf_idx][bucket].first_idx);

	if ( cache_hashmap_bucket[buf_idx][bucket].first_idx < 0 )
	{
		cache_hash_map_head[buf_idx]->bucket_used++;
	}

	cache_hashmap_bucket[buf_idx][bucket].first_idx = idx;
	cache_hash_map_head[buf_idx]->bid_num++;
	return 0;
}

int c_hash_map::del_hashmap(int bid)
{
	ssize_t bucket = hashmap_hash_func(bid);
	ssize_t idx;
	ssize_t prev_idx, next_idx;
	int buf_idx = get_updating_buf();

	idx = get_hashmap_idx(buf_idx,bid, &prev_idx, bucket);
	if ( idx < 0 )
	{
		/* host do not exist */
		return NO_FOUND;
	}


	if ( idx == cache_hashmap_bucket[buf_idx][bucket].first_idx )
	{
		cache_hashmap_bucket[buf_idx][bucket].first_idx = hashmap_obj_mng[buf_idx].GetDsIdx(idx, cache_hash_map_head[buf_idx]->ds_suffix);
		if ( cache_hashmap_bucket[buf_idx][bucket].first_idx < 0 )
			cache_hash_map_head[buf_idx]->bucket_used--;
	}
	else
	{
		next_idx = hashmap_obj_mng[buf_idx].GetDsIdx(idx, cache_hash_map_head[buf_idx]->ds_suffix);
		hashmap_obj_mng[buf_idx].SetDsIdx(prev_idx, cache_hash_map_head[buf_idx]->ds_suffix, next_idx);
	}

	hashmap_obj_mng[buf_idx].DestroyObject(idx);
	cache_hash_map_head[buf_idx]->bid_num--;
	return 0;
}


int c_hash_map::DumpHashMap(cache_hashmap_t* phashmap)
{
	if (!phashmap)
		return -1;
	//cache_hashmap_t* phashmap = get_hashmap_ptr(bid);
	//if (phashmap == NULL)
	//	return -1;

	char file_name[256];
	
	if(mkdir("../etc/bid", S_IRWXU) && errno != EEXIST)
	{
		ERR("Create dir ../etc/bid failed!");
		return -2;
	}
	sprintf(file_name, "../etc/bid/%d.hash", phashmap->bid);
	FILE* fp = fopen(file_name, "w+");
	if (!fp)
	{
		ERR("open file[%s] failed!\n", file_name);
		return -3;
	}
	char szIpM[256];
	char szIpS[256];
	char szDestIpM[256];
	char szDestIpS[256];
	for(int i = 0; i < CACHE_MAP_BLOCK_NUM; ++i)
	{

		NtoP(htonl(phashmap->cache_map[i].ip_master),szIpM);
		NtoP(htonl(phashmap->cache_map[i].ip_slave),szIpS);

		if(phashmap->cache_map[i].dst_ip_master != 0 || phashmap->cache_map[i].dst_ip_slave != 0)
		{
			NtoP(htonl(phashmap->cache_map[i].dst_ip_master),szDestIpM);
			NtoP(htonl(phashmap->cache_map[i].dst_ip_slave),szDestIpS);
			fprintf(fp,"%d %s %s %d %d %s %s %d\n",
				i,szIpM,szIpS, phashmap->cache_map[i].cacheid, phashmap->cache_map[i].status,
				szDestIpM, szDestIpS, phashmap->cache_map[i].dst_cacheid);

		}
		else
		{
			fprintf(fp,"%d %s %s %d %d\n",
				i, szIpM, szIpS, phashmap->cache_map[i].cacheid,phashmap->cache_map[i].status);				
		}
	}
	fclose(fp);
	return 0;
}

int c_hash_map::DumpAllHashMap(vector<int> *pVecBid)
{
	ssize_t bucket;
	ssize_t idx;
	cache_hashmap_t *hashmap;
	int ret = 0;

	int buf_idx = get_curr_buf();
	for ( bucket=0; bucket<cache_hash_map_head[buf_idx]->bucket_num; bucket++ )
	{
		idx = cache_hashmap_bucket[buf_idx][bucket].first_idx;

		while ( idx >= 0 )
		{
			hashmap = (cache_hashmap_t *)hashmap_obj_mng[buf_idx].GetAttachObj(idx);
			ret = DumpHashMap(hashmap);
			if (ret<0)
			{
				return -1;
			}
			if (pVecBid)
			{
				pVecBid->push_back(hashmap->bid);
			}
			idx = hashmap_obj_mng[buf_idx].GetDsIdx(idx, cache_hash_map_head[buf_idx]->ds_suffix);
		}
	}

	return 0;
}


#if 0
#include <time.h>

int main()
{
	c_hash_map *my_map;
	ssize_t len = 1024*1024*2;
	ssize_t used_len;
	host_pair_item_t host;
	int ret;

	my_map = new c_hash_map();

	if ( my_map->hashmap_init(10000, 500, 1) )
	{
		return -1;
	}

	for ( int i=0; i<10; i++ )
	{
		my_map->update_start();
		my_map->clearall_hostmap();
		host.ip_master = 1;
		if ( (ret=my_map->set_host_pair(&host)) )
		{
			ERR("error[%d]", ret);
			return -1;
		}

		host.ip_master = 176299082;
		if ( (ret=my_map->set_host_pair(&host)) )
		{
			ERR("error[%d]", ret);
			return -1;
		}

		host.ip_master = 176299083;
		if ( (ret=my_map->set_host_pair(&host)) )
		{
			ERR("error[%d]", ret);
			return -1;
		}

		my_map->update_end();

		if ( (ret=my_map->get_host_pair(176299083, &host)) )
		{
			ERR("get i[%d] error[%d]", i, ret);
			return -1;
		}

		my_map->update_start();

		cache_hashmap_t hashmap;
		cache_hashmap_t *map_ptr;
		memset(&hashmap, 0xAA, sizeof(hashmap));		
		hashmap.bid = i;
		hashmap.version = i;
		my_map->set_hashmap(&hashmap);
		
		my_map->update_end();

		if ( !(map_ptr=my_map->get_hashmap_ptr(i)) )
		{
			ERR("get i[%d] error[%d]", i, ret);
			return -1;
		}
		
	}

	return 0;
}
#endif

