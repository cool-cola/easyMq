/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: epoll封转

EPOLLIN ：表示对应的文件描述符可以读
EPOLLOUT：表示对应的文件描述符可以写
EPOLLPRI：表示对应的文件描述符有紧急的数据可读
EPOLLERR：表示对应的文件描述符发生错误
EPOLLHUP：表示对应的文件描述符被挂断
EPOLLET：表示对应的文件描述符有事件发生

***********************************************************/

#ifndef _EPOLL_FLOW_HPP_
#define _EPOLL_FLOW_HPP_

#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <assert.h>
#include <string>
#include <stdexcept>
#include <iostream>

class CEPollFlow
{
public:
	CEPollFlow();
	~CEPollFlow();
	
	int Create(int iMaxFD);
	int Wait(int iTimeMs);
	int GetEvents(long long &llKey, unsigned int &iEvent);
	
	int Add(int iFd,long long llKey,int iFlag);
	int Modify(int iFd,long long llKey,int iFlag);
	int Del(int iFd);

private:
	int Ctl(int iFd,long long llKey,int iEpollAction, int iFlag);
	
	int m_iEpollFD;
	epoll_event* m_pEpollEvents;
	int m_iMaxFD;

	int m_iEventNum;
	int m_iCurrEvtIdx;
};

#endif


