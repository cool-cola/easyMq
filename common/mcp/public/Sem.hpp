/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: 信号量封装

***********************************************************/

#ifndef _SYSV_SEM_HPP_
#define _SYSV_SEM_HPP_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string>

class CSem
{
public:
	CSem();
	int Open(int iSemKey,int iSemNum=1,int iInitResNum=1);

	//operation
	int GetSemID(){return m_iSemID;}	
	void Post(unsigned index = 0);
	void Wait(unsigned index = 0);
	bool TryWait(unsigned index = 0);
	bool TimeWait(unsigned index, unsigned& sec, unsigned& nanosec);

	time_t LastOperateTime();
	time_t CreateTime();
	
private:
	void SetVal(unsigned index = 0, unsigned short value = 1);
	int GetVal(unsigned index = 0);
	void Destroy();
	
	int m_iSemID;
	int m_iSemNum;
};

#endif

