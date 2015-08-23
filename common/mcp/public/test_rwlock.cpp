#include "rw_lock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
pthread_rwlock_t* lock = shm_rw_lock_init(0x00000001);
int g_read1_lock_cnt[1024] = {0};
int g_writer1_lock_cnt[1024] = {0};
void* reader1(void* args)
{
	int index = *(int*)(args);
	delete (int*)(args);
	printf("create reader %d\n", index);
	while(1)
	{
		pthread_rwlock_rdlock(lock);		
		printf("reader%d rdlock lock ", index);	
		g_read1_lock_cnt[index]++;
		usleep(0);
		printf("reader%d rdlock unlock\n", index);
		pthread_rwlock_unlock(lock);
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
		pthread_rwlock_wrlock(lock);
		printf("writer%d wrlock lock ", index);
		g_writer1_lock_cnt[index]++;
		usleep(0);
		printf("writer%d wrlock unlock\n", index);
	    pthread_rwlock_unlock(lock);	
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
		}
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
		
		sleep(1000);
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
