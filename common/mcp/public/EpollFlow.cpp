#include <unistd.h>
#include "EpollFlow.hpp"

CEPollFlow::CEPollFlow() 
{
	m_iEpollFD = -1;
	m_pEpollEvents = NULL;
}
CEPollFlow::~CEPollFlow() 
{
	if (m_pEpollEvents) 
		delete []m_pEpollEvents;

	if (m_iEpollFD >= 0)
		close(m_iEpollFD);
}
int CEPollFlow::Create(int iMaxFD)
{
	m_iMaxFD = iMaxFD;
	m_iEpollFD = epoll_create(iMaxFD);
	if(m_iEpollFD < 0)
	{
		return -1;
	}	
	m_pEpollEvents = new epoll_event[iMaxFD];
	return 0;
}

int CEPollFlow::Add(int iFd,long long llKey,int iFlag)
{
	return Ctl(iFd,llKey,EPOLL_CTL_ADD,iFlag);
}
int CEPollFlow::Del(int iFd)
{	
	return Ctl(iFd,0,EPOLL_CTL_DEL,0);
}
int CEPollFlow::Modify(int iFd,long long llKey,int iFlag)
{
	return Ctl(iFd,llKey,EPOLL_CTL_MOD,iFlag);
}
int CEPollFlow::Wait(int iTimeMs)
{
	m_iEventNum = epoll_wait(m_iEpollFD,m_pEpollEvents,m_iMaxFD,iTimeMs);
	m_iCurrEvtIdx = 0;
	return m_iEventNum;
}
/*
typedef union epoll_data {
	void *ptr;
	int fd;
	__uint32_t u32;
	__uint64_t u64;
} epoll_data_t;

struct epoll_event {
	__uint32_t events;	
	epoll_data_t data;	
};
*/
int CEPollFlow::GetEvents(long long &llKey, unsigned int &iEvent)
{
	if (m_iCurrEvtIdx >= m_iEventNum)
		return 0;

	epoll_event* curr_event = &m_pEpollEvents[m_iCurrEvtIdx++];
	llKey = curr_event->data.u64;
	iEvent = curr_event->events;
	return 1;	
}
	
int CEPollFlow::Ctl(int iFd,long long llKey,int iEpollAction, int iFlag)
{
	epoll_event evt;
	evt.events = iFlag;
	evt.data.u64 = llKey;

	if (iFd < 0)
		return -1;
		
	int ret = epoll_ctl(m_iEpollFD, iEpollAction, iFd, &evt);
	if (ret < 0)
	{
		return -2;
	}	
	return 0;
}

