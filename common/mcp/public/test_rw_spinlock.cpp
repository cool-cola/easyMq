#include "rw_spinlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <asm/atomic.h>
#include <assert.h>
atomic_t writer_locked[1024];
atomic_t reader_locked[1024];

rw_spinlock_t* lock = shm_rw_spinlock_init(0x00000001);


int g_read1_lock_cnt[1024] = {0};
int g_writer1_lock_cnt[1024] = {0};

int g_reader_cnt = 0;
int g_writer_cnt = 0;
void* reader1(void* args)
{
	int index = *(int*)(args);
	delete (int*)(args);
	printf("create reader %d\n", index);
	while(1)
	{
		read_lock(lock);	
		atomic_set(&reader_locked[index], 1);
		for(int i = 0; i < g_writer_cnt; ++i)
		{
			if(atomic_read(& writer_locked[i]) != 0)
			{
				printf("reader%d writer%d both locked sucess\n", index, i);
				assert(0);
			}
		}
		printf("reader%d rdlock lock ", index);
		g_read1_lock_cnt[index]++;
		int us = random()%100000;
		usleep(us);
		printf("sleep %dus reader%d rdlock unlock\n", us, index);
		atomic_set(&reader_locked[index], 0);
		read_unlock(lock);
		usleep(0);		
	}
	return 0;
}
void* writer1(void* args)
{
	int index = *(int*)(args);
	delete (int*)(args);
	printf("create writer %d\n", index);
	while(1)
	{
		write_lock(lock);
		atomic_set(&writer_locked[index], 1);
		for(int i = 0; i < g_reader_cnt; ++i)
		{
			if(atomic_read(& reader_locked[i]) != 0)
			{
				printf("reader%d writer%d both locked sucess\n", index, i);
				assert(0);
			}
		}
		for(int i = 0; i < g_writer_cnt; ++i)
		{
			if(i == index)
				continue;
			if(atomic_read(& writer_locked[i]) != 0)
			{
				printf("writer%d writer%d both locked sucess\n", index, i);
				assert(0);
			}
		}
		printf("writer%d wrlock lock ", index);
		g_writer1_lock_cnt[index]++;
		int us = random()%100000;
		usleep(us);
		printf("sleep %dus writer%d wrlock unlock\n", us, index);
		atomic_set(&writer_locked[index], 0);
		write_unlock(lock);
		usleep(0);
   }
   return 0;
}
int main(int argc, char *argv[])
{
		if(argc != 3)
		{
			printf("xxx reader_number writer_number\n");
			return -1;
		}
		

		int iReaders = atoi(argv[1]);
		int iWriters = atoi(argv[2]);
		
		if(iReaders >= 1024 || iReaders < 0 || iWriters >= 1024 || iWriters < 0)
		{
			printf("invalid readers %d or writers %d\n", iReaders, iWriters);
			return -1;
		}
		g_reader_cnt = iReaders;
		g_writer_cnt = iWriters;
		for(int i = 0; i < g_writer_cnt; ++i)
			atomic_set(&writer_locked[i], 0);

		for(int i = 0; i < g_reader_cnt; ++i)	
			atomic_set(&reader_locked[i], 0);	
		
		pthread_t tid1, tid2;
		for(int i = 0; i < iReaders; ++i)
		{
			int *pCnt = new int(i);
			pthread_create(&tid1, NULL, reader1, pCnt);
		}
		for(int i = 0; i < iWriters; ++i)
		{
			int *pCnt = new int(i);
			pthread_create(&tid2, NULL, writer1, pCnt);
		}
		sleep(1);
		memset(g_read1_lock_cnt, 0, sizeof(g_read1_lock_cnt));
		memset(g_writer1_lock_cnt, 0, sizeof(g_writer1_lock_cnt));
		
		sleep(10);
		int iTotalRead = 0, iTotalWrite = 0;
		for(int i = 0; i < iReaders; ++i)
		{
			printf("reader%d %d ", i, g_read1_lock_cnt[i]);
			iTotalRead += g_read1_lock_cnt[i];
		}
		for(int i = 0; i < iWriters; ++i)
		{
			printf("writer%d %d ", i, g_writer1_lock_cnt[i]);
			iTotalWrite += g_writer1_lock_cnt[i];
		}
		printf("\n");	
		printf("totalread %d ", iTotalRead);
		printf("totalwrite %d ", iTotalWrite);
		printf("total %d\n", iTotalWrite + iTotalRead);
		return 0;
}
