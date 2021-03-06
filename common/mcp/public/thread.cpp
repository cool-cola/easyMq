#include "thread.hpp"

void* ThreadProc( void *pvArgs )
{
	if( !pvArgs )
	{
		return NULL;
	}

	CThread *pThread = (CThread *)pvArgs;
	pThread->Run();
	return NULL;
}

CThread::CThread()
{
	m_iRunStatus = rt_init;
}

CThread::~CThread()
{
}

int CThread::CreateThread()
{
    pthread_attr_init( &m_stAttr );
    pthread_attr_setscope( &m_stAttr, PTHREAD_SCOPE_SYSTEM );
    pthread_attr_setdetachstate( &m_stAttr, PTHREAD_CREATE_JOINABLE );
    pthread_cond_init( &m_stCond, NULL );
    pthread_mutex_init( &m_stMutex, NULL );
    m_iRunStatus = rt_running;

    pthread_create( &m_hTrd, &m_stAttr, ThreadProc, (void *)this );
    return 0;
}

int CThread::IsToBeBlocked()
{
	return 0;
}
	
int CThread::CondBlock()
{
	pthread_mutex_lock( &m_stMutex );
	while(IsToBeBlocked() || (m_iRunStatus == rt_stopped))
	{
		if( m_iRunStatus == rt_stopped )
		{
			pthread_exit( (void *)m_abyRetVal );
		}
		m_iRunStatus = rt_blocked;
		pthread_cond_wait( &m_stCond, &m_stMutex );
	}
	m_iRunStatus = rt_running;
	pthread_mutex_unlock( &m_stMutex );
	return 0;
}

int CThread::WakeUp()
{
	pthread_mutex_lock( &m_stMutex );
	if((!IsToBeBlocked()) && (m_iRunStatus == rt_blocked))
	{
		pthread_cond_signal( &m_stCond );
	}
	pthread_mutex_unlock( &m_stMutex );
	return 0;
}

int CThread::StopThread()
{
	pthread_mutex_lock( &m_stMutex );
	m_iRunStatus = rt_stopped;
	pthread_cond_signal( &m_stCond );
	pthread_mutex_unlock( &m_stMutex );

	pthread_join( m_hTrd, NULL );
	return 0;
}

int CThread::GetThreadStatus()
{
	int iTempStatus;
	pthread_mutex_lock( &m_stMutex );
	iTempStatus = m_iRunStatus;
	pthread_mutex_unlock( &m_stMutex );
	return iTempStatus;
}

