#ifndef _RW_LOCK_H_
#define _RW_LOCK_H_

#include <pthread.h>
//----------------------------------------------------------------------
#include <sys/shm.h>

inline pthread_rwlock_t* shm_rw_lock_init(key_t iShmKey,int iNumber=1)
{
	int iFirstCreate = 1;
	int iShmID = shmget(iShmKey,sizeof(pthread_rwlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
	if(iShmID < 0)
	{
		iFirstCreate = 0;
		iShmID = shmget(iShmKey,sizeof(pthread_rwlock_t)*iNumber,0666);
	}
	
	if(iShmID < 0)
	{
		iShmID = shmget(iShmKey,0,0666);
		if( iShmID < 0)
			return NULL;

		struct shmid_ds s_ds;
		shmctl(iShmID,IPC_STAT,&s_ds);
		if(s_ds.shm_nattch > 0)
		{
			return NULL;
		}
				
		if(shmctl(iShmID,IPC_RMID,NULL))
			return NULL;

		iShmID = shmget(iShmKey,sizeof(pthread_rwlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
		if( iShmID < 0)
			return NULL;

		iFirstCreate = 1;
	}

	pthread_rwlock_t* g_shm_rwlock_t = (pthread_rwlock_t*)shmat(iShmID,0,0);
	if((char*)g_shm_rwlock_t == (char*)-1)
		return 0;

	struct shmid_ds s_ds;
	shmctl(iShmID,IPC_STAT,&s_ds);
	int iAttach = s_ds.shm_nattch;

	if((iAttach==1) || iFirstCreate)
	{
		for(int i=0; i<iNumber; i++)
		{
			pthread_rwlock_init(g_shm_rwlock_t+i, NULL);
		}		
	}

	return g_shm_rwlock_t;
}

#endif

/*
加解锁1000w次： 无-O2
pthread_spin_lock  cost 224ms
spin lock cost 307ms
pthread_mutex  cost 558ms
sem  cost 4563ms


加解锁1000w次： 有-O2
spin lock cost 107ms
pthread_spin_lock  cost 200ms
pthread_mutex  cost 470ms
sem  cost 4449ms
*/

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "Sem.hpp"

int main()
{
	spinlock_t lock;
	spin_lock_init(&lock);

	timeval t1,t2;
	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		spin_lock(&lock);
		spin_unlock(&lock);	
	}
	gettimeofday(&t2,NULL);
	int cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("spin lock cost %dms\n",cost);

//
	pthread_mutex_t mp1 = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_init(&mp1,NULL);
	
	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		pthread_mutex_lock(&mp1);
		pthread_mutex_unlock(&mp1);
	}
	gettimeofday(&t2,NULL);
	cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("pthread_mutex  cost %dms\n",cost);
//
	CSem m_stSem;
	m_stSem.Open(34343343,2);

	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		m_stSem.Wait(0);
		m_stSem.Post(0);
	}
	gettimeofday(&t2,NULL);
	cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("sem  cost %dms\n",cost);

//
	pthread_spinlock_t   pthr_lock;
	pthread_spin_init(&pthr_lock,PTHREAD_PROCESS_SHARED); //PTHREAD_PROCESS_PRIVATE 
	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		pthread_spin_lock(&pthr_lock);
		pthread_spin_unlock(&pthr_lock);
	}
	gettimeofday(&t2,NULL);
	cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("pthread_spin_lock  cost %dms\n",cost);
	

	return 0;
}
#endif

/*
带-O2
读写获得锁的概率测试,1秒钟加锁个数
读写个数1:0,
	reader0 12674946 totalread 12674946
	
读写个数2:0,
	reader0 1991249 reader1 4758144 
	totalread 6749393 
	
读写个数4:0,
	reader0 849403 reader1 602848 reader2 700684 reader3 841315 
	totalread 2994250
 
读写个数8:0,	
	reader0 350820 reader1 343057 reader2 340842 reader3 345758 reader4 356578 reader5 357716 reader6 364855 reader7 361192 
	totalread 2820833

读写个数1:1,
	reader0 1591592 writer0 1562264 
	totalread 1591592 totalwrite 1562265 total 3153857
	
读写个数2:1,
	reader0 421198 reader1 691484 writer0 305138 
	totalread 1112682 totalwrite 305138 total 1417820
	
读写个数4:1,	
	reader0 505290 reader1 566163 reader2 561699 reader3 587563 writer0 30336 
	totalread 2220715 totalwrite 30336 total 2251051
	 
读写个数8:1,	
	reader0 311621 reader1 311273 reader2 325486 reader3 363148 reader4 344580 reader5 334229 reader6 359702 reader7 335011 writer0 1392 
	totalread 2685060 totalwrite 1392 total 2686452
	
读写个数0:1,
	writer0 13196017 
	totalread 0 totalwrite 13196017 total 13196017
	
读写个数0:2,
	writer0 1253418 writer1 1212519 
	totalread 0 totalwrite 2465937 total 2465937
	
读写个数0:4,
	writer0 197700 writer1 175850 writer2 198866 writer3 179565 
	totalread 0 totalwrite 751983 total 751983
	
读写个数0:8,	
	writer0 80820 writer1 85137 writer2 88636 writer3 83918 writer4 83897 writer5 85155 writer6 84889 writer7 83928 
	totalread 0 totalwrite 676382 total 676382	
*/
#if 0
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
		g_read1_lock_cnt[index]++;
		pthread_rwlock_unlock(lock);
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
		g_writer1_lock_cnt[index]++;
		pthread_rwlock_unlock(lock);
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

            sleep(1);
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

#endif

