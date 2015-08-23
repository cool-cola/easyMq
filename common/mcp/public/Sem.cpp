#include <errno.h>
#include "Sem.hpp"
#include <cstdio>

CSem::CSem()
{
	m_iSemID = -1;
	m_iSemNum = 0;
}

int CSem::Open(int iSemKey,int iSemNum/*=1*/,int iInitResNum/*=1*/)
{
    	m_iSemID = semget(iSemKey, iSemNum, 0600| IPC_CREAT | IPC_EXCL);
	if (m_iSemID < 0)
	{
		printf("[CSem]create sem %p failed! errno=%d,try attach now...\n",(void*)iSemKey,errno);
		
		m_iSemID = semget(iSemKey, iSemNum, 0600);
		if (m_iSemID < 0)
		{
			printf("[CSem]attach sem %p failed! errno=%d\n",(void*)iSemKey,errno);
			return -1;
		}	

		for (int i=0; i<iSemNum; i++)
		{
			if(GetVal(i) == ((1<<16) - 1))
			{
				SetVal(i,iInitResNum);
			}
		}
	}
	else
	{
		//初始时有1个资源
		unsigned short* init_array = new unsigned short[iSemNum];
		for(int i = 0; i < iSemNum; i++)
			init_array[i] = iInitResNum;

		semctl(m_iSemID, 0, SETALL, init_array);
		delete []init_array;
	}

	m_iSemNum = iSemNum;
	return 0;
}

void CSem::Post(unsigned index /* = 0 */)
{
/*
原子地执行在 _buf 中所包含的操作,
也就是说，只有在这些操作可以同时成功执行时，
这些操作才会被同时执行,1是_buf数组个数

SEM_UNDO: undone when the process exits.

*/

	if(m_iSemID == -1)
		return;
		
	struct sembuf _buf;
	_buf.sem_num = index;
	_buf.sem_op = 1;
	_buf.sem_flg = SEM_UNDO;	
	semop(m_iSemID, &_buf, 1);  //_buf 操作数组,1操作数组的个数
}

void CSem::Wait(unsigned index /* = 0 */)
{
	if(m_iSemID == -1)
		return;
	
	struct sembuf _buf;
	_buf.sem_num = index;
	_buf.sem_op = -1;
	_buf.sem_flg = SEM_UNDO;	
	semop(m_iSemID, &_buf, 1);
}

bool CSem::TryWait(unsigned index /* = 0 */)
{
	if(m_iSemID == -1)
		return false;
		
	errno = 0;
	
	struct sembuf _buf;
	_buf.sem_num = index;
	_buf.sem_op = -1;
	_buf.sem_flg = SEM_UNDO | IPC_NOWAIT;
	
	int ret = semop(m_iSemID, &_buf, 1);
	if (ret)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool CSem::TimeWait(unsigned index, unsigned& sec, unsigned& nanosec)
{
	if(m_iSemID == -1)
		return false;
		
	struct timespec ts;
	ts.tv_sec = sec;
	ts.tv_nsec = nanosec;

	struct sembuf _buf;
	_buf.sem_num = index;
	_buf.sem_op = -1;
	_buf.sem_flg = SEM_UNDO;
	
	for(unsigned i = 0; i < 5; i++)
	{
		errno = 0;
		int ret = semtimedop(m_iSemID, &_buf, 1, &ts);
		if (ret)
		{
			if (errno == EAGAIN)
			{
				return false;
			}
			else if (errno == EINTR)
			{
				continue;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return true;
		}
	}
	return false;
}

time_t CSem::LastOperateTime()
{
	if(m_iSemID == -1)
		return 0;
	
	struct semid_ds buf;
	int ret = semctl(m_iSemID, 0, IPC_STAT, &buf);
	if (ret < 0)
	{
		return 0;
	}
	return buf.sem_otime;
}

time_t CSem::CreateTime()
{
	if(m_iSemID == -1)
		return 0;
	
	struct semid_ds buf;
	int ret = semctl(m_iSemID, 0, IPC_STAT, &buf);
	if (ret < 0)
	{
		return 0;
	}
	return buf.sem_ctime;
}

void CSem::Destroy()
{
	if(m_iSemID == -1)
		return;
		
	semctl(m_iSemID, 0, IPC_RMID);
}

void CSem::SetVal(unsigned index /* = 0 */, unsigned short value /* = 1 */)
{
	if(m_iSemID == -1)
		return;
	
	semctl(m_iSemID, index, SETVAL, value);
}

int CSem::GetVal(unsigned index /* = 0 */)
{
	if(m_iSemID == -1)
		return 0;
	
	return semctl(m_iSemID, index, GETVAL);
}
/*
int main()
{
	CSem m_stSem;
	
	//创建进程间互斥信号量
	if(m_stSem.Open(3423434,2,3))
	{
		return -3;
	}

	//m_stSem.Post(0);
	//m_stSem.Post(0);
	//m_stSem.Post(0);
	m_stSem.Wait(0);
	m_stSem.Wait(0);
	m_stSem.Wait(0);
	return 0;
}
*/

