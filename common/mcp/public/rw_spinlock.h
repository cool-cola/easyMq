#ifndef _RW_SPINLOCK_H_
#define _RW_SPINLOCK_H_
#include <asm/atomic.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#define RW_LOCK_BIAS		 0x01000000
#define RW_LOCK_BIAS_STR	"0x01000000"

#define LOCK "lock ; "

typedef struct {
	volatile unsigned int lock;
} raw_rw_spinlock_t;
typedef struct {
	raw_rw_spinlock_t raw_lock;
} rw_spinlock_t;

#define RW_LOCK_UNLOCKED \
	(rw_spinlock_t)	{	.raw_lock = __RAW_RW_LOCK_UNLOCKED }

#define DEFINE_RWLOCK(x)	rw_spinlock_t x = RW_LOCK_UNLOCKED


inline int rwlock_init(rw_spinlock_t *rwlock)
{
	rwlock->raw_lock.lock = RW_LOCK_BIAS;
	return 0;
}


#define __RAW_RW_LOCK_UNLOCKED		{ RW_LOCK_BIAS }

#define __build_read_lock_ptr(rw, helper)   \
	asm volatile(LOCK "subl $1,(%0)\n\t" \
			"jns 1f\n" \
		"__read_lock_failed:\n\t"\
			LOCK "incl	(%0)\n"\
		"2:	rep; nop\n\t"\
			"cmpl	$1, (%0)\n\t"\
			"js	2b\n\t"\
			LOCK "decl	(%0)\n\t"\
			"js	__read_lock_failed\n\t"\
		"1:\n" \
		     ::"a" (rw) : "memory")



#define __build_write_lock_ptr(rw, helper) \
	asm volatile(LOCK "subl $" RW_LOCK_BIAS_STR ",(%0)\n\t" \
			"jz 1f\n" \
		 "__write_lock_failed:\n\t"\
			LOCK "addl	$" RW_LOCK_BIAS_STR ",(%0)\n"\
		"2:	rep; nop\n\t"\
			"cmpl	$" RW_LOCK_BIAS_STR ",(%0)\n\t"\
			"jne	2b\n\t"\
			LOCK "subl	$" RW_LOCK_BIAS_STR ",(%0)\n\t"\
			"jnz	__write_lock_failed\n\t"\
		 "1:\n" \
		     ::"a" (rw) : "memory")

/*
#define __build_read_lock_ptr(rw, helper)   \
	asm volatile(LOCK "subl $1,(%0)\n\t" \
		     "jns 1f\n" \
		     "call " helper "\n\t" \
		     "1:\n" \
		     ::"a" (rw) : "memory")

#define __build_write_lock_ptr(rw, helper) \
	asm volatile(LOCK "subl $" RW_LOCK_BIAS_STR ",(%0)\n\t" \
		     "jz 1f\n" \
		     "call " helper "\n\t" \
		     "1:\n" \
		     ::"a" (rw) : "memory")

asm(
".section .text\n"
".align	4\n"
".globl	__write_lock_failed\n"
"__write_lock_failed:\n\t"
	LOCK "addl	$" RW_LOCK_BIAS_STR ",(%eax)\n"
"1:	rep; nop\n\t"
	"cmpl	$" RW_LOCK_BIAS_STR ",(%eax)\n\t"
	"jne	1b\n\t"
	LOCK "subl	$" RW_LOCK_BIAS_STR ",(%eax)\n\t"
	"jnz	__write_lock_failed\n\t"
	"ret"
);

asm(
".section .text\n"
".align	4\n"
".globl	__read_lock_failed\n"
"__read_lock_failed:\n\t"
	LOCK "incl	(%eax)\n"
"1:	rep; nop\n\t"
	"cmpl	$1,(%eax)\n\t"
	"js	1b\n\t"
	LOCK "decl	(%eax)\n\t"
	"js	__read_lock_failed\n\t"
	"ret"
);
*/
#define __build_read_lock(rw, helper)	do { \
							__build_read_lock_ptr(rw, helper); \
					} while (0)


#define __build_write_lock(rw, helper)	do { \
							__build_write_lock_ptr(rw, helper); \
					} while (0)



/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 *
 * NOTE! it is quite common to have readers in interrupts
 * but no interrupt writers. For those circumstances we
 * can "mix" irq-safe locks - any writer needs to get a
 * irq-safe write-lock, but readers can get non-irqsafe
 * read-locks.
 *
 * On x86, we implement read-write locks as a 32-bit counter
 * with the high bit (sign) being the "contended" bit.
 *
 * The inline assembly is non-obvious. Think about it.
 *
 * Changed to use the same technique as rw semaphores.  See
 * semaphore.h for details.  -ben
 *
 * the helpers are in arch/i386/kernel/semaphore.c
 */

/**
 * read_can_lock - would read_trylock() succeed?
 * @lock: the rwlock in question.
 */
#define __raw_read_can_lock(x)		((int)(x)->lock > 0)

/**
 * write_can_lock - would write_trylock() succeed?
 * @lock: the rwlock in question.
 */
#define __raw_write_can_lock(x)		((x)->lock == RW_LOCK_BIAS)

static inline void __raw_read_lock(raw_rw_spinlock_t *rw)
{
	__build_read_lock(rw, "__read_lock_failed");
}

static inline void __raw_write_lock(raw_rw_spinlock_t *rw)
{
	__build_write_lock(rw, "__write_lock_failed");
}

static inline int __raw_read_trylock(raw_rw_spinlock_t *lock)
{
	atomic_t *count = (atomic_t *)lock;
	atomic_dec(count);
	if (atomic_read(count) >= 0)
		return 1;
	atomic_inc(count);
	return 0;
}

static inline int __raw_write_trylock(raw_rw_spinlock_t *lock)
{
	atomic_t *count = (atomic_t *)lock;
	if (atomic_sub_and_test(RW_LOCK_BIAS, count))
		return 1;
	atomic_add(RW_LOCK_BIAS, count);
	return 0;
}

static inline void __raw_read_unlock(raw_rw_spinlock_t *rw)
{
	asm volatile("lock ; incl %0" :"=m" (rw->lock) : : "memory");
}

static inline void __raw_write_unlock(raw_rw_spinlock_t *rw)
{
	asm volatile("lock ; addl $" RW_LOCK_BIAS_STR ", %0"
				 : "=m" (rw->lock) : : "memory");
}

#define write_lock(lock)		__raw_write_lock(&(lock)->raw_lock)
#define read_lock(lock)		__raw_read_lock(&(lock)->raw_lock)
#define read_unlock(lock)		__raw_read_unlock(&(lock)->raw_lock)
#define write_unlock(lock)	__raw_write_unlock(&(lock)->raw_lock)


#define read_can_lock(rwlock)		__raw_read_can_lock(&(rwlock)->raw_lock)
#define write_can_lock(rwlock)		__raw_write_can_lock(&(rwlock)->raw_lock)



inline rw_spinlock_t* shm_rw_spinlock_init(key_t iShmKey,int iNumber=1)
{
	int iFirstCreate = 1;
	int iShmID = shmget(iShmKey,sizeof(rw_spinlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
	if(iShmID < 0)
	{
		iFirstCreate = 0;
		iShmID = shmget(iShmKey,sizeof(rw_spinlock_t)*iNumber,0666);
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

		iShmID = shmget(iShmKey,sizeof(rw_spinlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
		if( iShmID < 0)
			return NULL;

		iFirstCreate = 1;
	}

	rw_spinlock_t* g_shm_rw_spinlock_t = (rw_spinlock_t*)shmat(iShmID,0,0);
	if((char*)g_shm_rw_spinlock_t == (char*)-1)
		return 0;

	struct shmid_ds s_ds;
	shmctl(iShmID,IPC_STAT,&s_ds);
	int iAttach = s_ds.shm_nattch;

	if((iAttach==1) || iFirstCreate)
	{
		for(int i=0; i<iNumber; i++)
		{
			rwlock_init(g_shm_rw_spinlock_t + i);
		}		
	}

	return g_shm_rw_spinlock_t;
}



#endif
/*
带-O2
读写获得锁的概率测试,1秒钟加锁个数
读写个数1:0,
	reader0 26221805 
	totalread 26221805 totalwrite 0 total 26221805
读写个数2:0,
	reader0 4893074 reader1 11718613 
	totalread 16611699 totalwrite 0 total 16611699
	
读写个数4:0,
	reader0 1546629 reader1 1579404 reader2 1320873 reader3 2852848 
	totalread 7299757 totalwrite 0 total 7299757
 
读写个数8:0,	
	reader0 740761 reader1 710643 reader2 727105 reader3 711743 reader4 710091 reader5 1602128 reader6 673871 reader7 713422 
	totalread 6589767 totalwrite 0 total 6589767

读写个数1:1,
	reader0 8091709 writer0 15660173 
	totalread 8091709 totalwrite 15660186 total 23751895
	
读写个数2:1,
	reader0 1903138 reader1 1934805 writer0 1388349 
	totalread 3837943 totalwrite 1388349 total 5226292
	
读写个数4:1,	
	reader0 802843 reader1 1598015 reader2 807105 reader3 1373903 writer0 37817 
	totalread 4581880 totalwrite 37817 total 4619697
	 
读写个数8:1,	
	reader0 609560 reader1 563200 reader2 619484 reader3 617070 reader4 636803 reader5 346897 reader6 339697 reader7 534809 writer0 20938 
	totalread 4267523 totalwrite 20938 total 4288461
	
读写个数0:1,
	writer0 26175715 
	totalread 0 totalwrite 26175715 total 26175715
	
读写个数0:2,
	writer0 7644575 writer1 8068966 
	totalread 0 totalwrite 15713541 total 15713541
	
读写个数0:4,
	writer0 429669 writer1 456541 writer2 438098 writer3 483541 
	totalread 0 totalwrite 1807851 total 1807851
	
读写个数0:8,	
	writer0 97838 writer1 135946 writer2 138639 writer3 109291 writer4 160663 writer5 85448 writer6 139960 writer7 137706 
	totalread 0 totalwrite 1005491 total 1005491
*/


#if 0 //应用层实现的读写自旋锁
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include "spin_lock.h"
#include <asm/atomic.h>
typedef struct rw_spinlock_t
{
	atomic_t writers;
	atomic_t readers;
	atomic_t writers_que_and_locked;//排队的和已持有锁的
	atomic_t readers_que;
	spinlock_t wlock;
}rw_spinlock_t;

inline int rw_spinlock_init(rw_spinlock_t *rw_spinlock, void *arg)
{
	atomic_set(&rw_spinlock->writers, 0);
	atomic_set(&rw_spinlock->readers, 0);
	atomic_set(&rw_spinlock->writers_que_and_locked, 0);
	atomic_set(&rw_spinlock->readers_que, 0);
	return 0;
}
inline int rw_spinlock_rdlock(rw_spinlock_t *rw_spinlock)
{
	atomic_inc(&rw_spinlock->readers_que);
	while(atomic_read(&rw_spinlock->writers_que_and_locked) != 0)
	{
		atomic_dec(&rw_spinlock->readers_que);
		while(atomic_read(&rw_spinlock->writers_que_and_locked) != 0);	
		atomic_inc(&rw_spinlock->readers_que);
	}
	atomic_inc(&rw_spinlock->readers);
	atomic_dec(&rw_spinlock->readers_que);
	return 0;
}

inline int rw_spinlock_wrlock(rw_spinlock_t *rw_spinlock)
{
	atomic_inc(&rw_spinlock->writers_que_and_locked);
	//写互斥
	do
	{
		if(atomic_read(&rw_spinlock->writers) != 0)
			continue;
		
		spin_lock(&rw_spinlock->wlock);
		if(atomic_read(&rw_spinlock->writers) != 0)
		{
			spin_unlock(&rw_spinlock->wlock);
			continue;
		}
		
		atomic_inc(&rw_spinlock->writers);
		spin_unlock(&rw_spinlock->wlock);
		break;
	}while(1);

	//等待无reader queue
	while(atomic_read(&rw_spinlock->readers_que) != 0);
	
	//等待无读
	while(atomic_read(&rw_spinlock->readers) != 0);

	return 0;
}

inline int rw_spinlock_rd_unlock(rw_spinlock_t *rw_spinlock)
{
	atomic_dec(&rw_spinlock->readers);
	return 0;
}
inline int rw_spinlock_wr_unlock(rw_spinlock_t *rw_spinlock)
{
	spin_lock(&rw_spinlock->wlock);
	atomic_dec(&rw_spinlock->writers);
	spin_unlock(&rw_spinlock->wlock);
	atomic_dec(&rw_spinlock->writers_que_and_locked);
	return 0;
}

inline int rw_spinlock_destroy(rw_spinlock_t *rw_spinlock)
{
	rw_spinlock_init(rw_spinlock, NULL);
	return 0;
}
inline rw_spinlock_t* shm_rw_spinlock_init(key_t iShmKey,int iNumber=1)
{
	int iFirstCreate = 1;
	int iShmID = shmget(iShmKey,sizeof(rw_spinlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
	if(iShmID < 0)
	{
		iFirstCreate = 0;
		iShmID = shmget(iShmKey,sizeof(rw_spinlock_t)*iNumber,0666);
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

		iShmID = shmget(iShmKey,sizeof(rw_spinlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
		if( iShmID < 0)
			return NULL;

		iFirstCreate = 1;
	}

	rw_spinlock_t* g_shm_rw_spinlock_t = (rw_spinlock_t*)shmat(iShmID,0,0);
	if((char*)g_shm_rw_spinlock_t == (char*)-1)
		return 0;

	struct shmid_ds s_ds;
	shmctl(iShmID,IPC_STAT,&s_ds);
	int iAttach = s_ds.shm_nattch;

	if((iAttach==1) || iFirstCreate)
	{
		for(int i=0; i<iNumber; i++)
		{
			rw_spinlock_init(g_shm_rw_spinlock_t + i, NULL);
			spin_lock_init(&g_shm_rw_spinlock_t->wlock);
		}		
	}

	return g_shm_rw_spinlock_t;
}
#endif




