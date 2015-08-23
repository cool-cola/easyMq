/******************************************************************************
  生成日期   : 2005年3月26日
  最近修改   :
  功能描述   :多线程 压力测试工具
******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <fcntl.h> 
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include "presscall.h"
#include "user_func.h"


//是否时间到
int isTimeEnd = 0;
//剩余运行时间(s)
int iSecLeft = 0;
//已运行时间
int iSecRuned = 0;

#define THREADS_ARRAY_NUM 1000	//最多1000个线程	

//互斥锁
pthread_mutex_t m_mutex_threadid;
pthread_mutex_t m_mutex_thread_user;

//总呼叫结果组,对应各个线程
struct TResult Results_History[THREADS_ARRAY_NUM];
//单位时间的呼叫结果组
struct TResult Results_Now[THREADS_ARRAY_NUM];
//单位时间结果组的互斥锁
pthread_mutex_t Results_NowLock[THREADS_ARRAY_NUM];
//线程组
pthread_t threads_array[THREADS_ARRAY_NUM];    

//#define FORTMAT_OUTPUT

#ifdef FORTMAT_OUTPUT
//单元格长度
#define CELL_LEN_LONG 30
#define CELL_LEN_SHORT 10
#endif

typedef struct
{
	char m_szDestIp[16];
	int m_iDestPort;

	int m_iThreadNum;
	int m_iThreadSleepMs;
	int m_iRunMins;
	int m_iSampleSecs;

	int m_iTimeLevel1;
	int m_iTimeLevel2;
	int m_iTimeLevel3;
}TConfig;

TConfig g_Config;

void kalSleep ( int milliSec )
{

    fd_set *readfds;
    fd_set *writefds;
    fd_set *errorfds;
    struct timeval timeout ;
    int nfds ;
    /*
    sleep (milliSec);
    */
    nfds = 0 ;
    readfds = NULL ;
    writefds = NULL ;
    errorfds = NULL ;
    timeout.tv_sec = milliSec / 1000 ;
    timeout.tv_usec = (milliSec % 1000) * 1000 ;
    select ( nfds, readfds, writefds, errorfds, &timeout);
}

//给线程编号用
int get_thread_id()
{
    static int thread_id = -1;
    pthread_mutex_lock(&m_mutex_threadid);
    thread_id++;
    if (thread_id >= THREADS_ARRAY_NUM) 
    {
        thread_id = 0;
    }
    pthread_mutex_unlock(&m_mutex_threadid);
    return thread_id;
}

void calculate();
void exit_proc();

/*****************************************************************************
 函 数 名  : thread_func
 功能描述  : 线程函数
*****************************************************************************/
void* thread_func(void *arg)
{
	int iMyID = get_thread_id();
	int iDestIP = inet_addr(g_Config.m_szDestIp);
	struct TUsrResult  stUsrResult;
	CUserFunc* pUserFunc = NULL;

	srand(iMyID);
	int iStartWaitus = rand()%(g_Config.m_iSampleSecs*1000000);
	usleep(iStartWaitus);
	
	try
	{
		pUserFunc = new CUserFunc(iMyID,iDestIP,g_Config.m_iDestPort,CFGFILE,
						&m_mutex_thread_user);
	}catch(...)
	{
		printf("ERR:thread %d can not run!\n",iMyID);
		return NULL;
	}

	timeval tBegin,tEnd;
	while(!isTimeEnd)
	{
		//执行
		memset(&stUsrResult,0,sizeof(stUsrResult));  
		gettimeofday(&tBegin,NULL);
		
		int iRet = pUserFunc->DoOnce(&stUsrResult);  
		
		gettimeofday(&tEnd,NULL);
		unsigned long long ullRspTimeUs = (tEnd.tv_sec - tBegin.tv_sec)*1000000 + 
										(tEnd.tv_usec - tBegin.tv_usec);

		//if(stUsrResult.iOkResponseNum == 0)
		//	ullRspTimeUs = 0;
		
		if(iRet != 0)
		{
			sleep(1);
		}
		
		//上报到总结果集合,完成是打印
		Results_History[iMyID].iOkResponseNum += stUsrResult.iOkResponseNum;
		Results_History[iMyID].iAllReqNum+= stUsrResult.iAllReqNum;
		Results_History[iMyID].iBadResponseNum += stUsrResult.iBadResponseNum;
		Results_History[iMyID].iNoResponseNum+= stUsrResult.iNoResponseNum;
		Results_History[iMyID].ullRspTimeUs+= ullRspTimeUs;
		if(ullRspTimeUs > Results_History[iMyID].ullMaxRspTimeUs)
			Results_History[iMyID].ullMaxRspTimeUs = ullRspTimeUs ;
		
		//上报单位时间结果,周期性打印
		pthread_mutex_lock(&Results_NowLock[iMyID]);
		Results_Now[iMyID].iOkResponseNum += stUsrResult.iOkResponseNum;
		Results_Now[iMyID].iAllReqNum+= stUsrResult.iAllReqNum;
		Results_Now[iMyID].iBadResponseNum += stUsrResult.iBadResponseNum;
		Results_Now[iMyID].iNoResponseNum+= stUsrResult.iNoResponseNum;  
		Results_Now[iMyID].ullRspTimeUs+= ullRspTimeUs;  
		if(ullRspTimeUs > Results_Now[iMyID].ullMaxRspTimeUs)
			Results_Now[iMyID].ullMaxRspTimeUs = ullRspTimeUs ;

		if(ullRspTimeUs >= (unsigned int)g_Config.m_iTimeLevel3)
		{
			Results_Now[iMyID].m_iTimeL3Num++;
		}
		if((ullRspTimeUs < (unsigned int)g_Config.m_iTimeLevel3) &&
			(ullRspTimeUs >= (unsigned int)g_Config.m_iTimeLevel2))
		{
			Results_Now[iMyID].m_iTimeL2Num++;
		}
		if((ullRspTimeUs < (unsigned int)g_Config.m_iTimeLevel2) &&
			(ullRspTimeUs >= (unsigned int)g_Config.m_iTimeLevel1))
		{
			Results_Now[iMyID].m_iTimeL1Num++;
		}
		
		pthread_mutex_unlock(&Results_NowLock[iMyID]);

		if (isTimeEnd)    
			return NULL;
		
		kalSleep(g_Config.m_iThreadSleepMs);
	}

	delete pUserFunc;
	return NULL;
}

extern "C"  void sigPIPE(int nSignal)
{
    ;
}

extern "C"  void sigINT(int nSignal)
{
    if (isTimeEnd == 0)
    {
        printf("\nWaiting to kill threads....\n");
    }
    isTimeEnd = 1;
}

extern "C"  void sigQUIT(int nSignal)
{
    isTimeEnd = 1;
    printf("\nWaiting to kill threads....\n");    
}

int main(int argc, char **argv)
{  

	//----信号处理
	int i = 0;
	for (i = 1; i <= NSIG; i++)
	{
	    signal(i, SIG_IGN);    
	}
	signal(SIGINT,  sigINT); 
	signal(SIGPIPE,  sigPIPE); 
	signal(SIGQUIT,sigQUIT);
	signal(SIGKILL,sigQUIT);
	signal(SIGTERM,sigQUIT);

	//----数据初始化
	for (int i=0;i<THREADS_ARRAY_NUM;i++)
	{
		threads_array[i] = 0;
		memset(&Results_History[i],0,sizeof(TResult));
		memset(&Results_Now[i],0,sizeof(TResult));  
		pthread_mutex_init(&Results_NowLock[i],NULL);		
	}        
	pthread_mutex_init(&m_mutex_threadid,NULL);
	pthread_mutex_init(&m_mutex_thread_user,NULL);	

	memset(&g_Config,0,sizeof(g_Config));
	//获取参数
	if (argc == 1)
	{
		TLib_Cfg_GetConfig(CFGFILE,
				"Host", CFG_STRING, g_Config.m_szDestIp, "",sizeof(g_Config.m_szDestIp),
				"Port", CFG_INT, &(g_Config.m_iDestPort),0,
				"ThreadNum", CFG_INT, &(g_Config.m_iThreadNum), 30,
				"ThreadSleepMs", CFG_INT, &(g_Config.m_iThreadSleepMs), 0,
				"RunMins", CFG_INT, &(g_Config.m_iRunMins), 0,
				"SampleSecs", CFG_INT, &(g_Config.m_iSampleSecs), 5,	
				"RspTimeLevel1", CFG_INT, &(g_Config.m_iTimeLevel1), 10000,	
				"RspTimeLevel2", CFG_INT, &(g_Config.m_iTimeLevel2), 100000,	
				"RspTimeLevel3", CFG_INT, &(g_Config.m_iTimeLevel3), 1000000,	
				NULL);	
	}
	else if (argc == 6)
	{
		strcpy(g_Config.m_szDestIp,argv[1]);
		g_Config.m_iDestPort = atoi(argv[2]);
		g_Config.m_iThreadNum = atoi(argv[3]);

		if(argv[4])
			g_Config.m_iRunMins = atoi(argv[4]);
		if(argv[5])
			g_Config.m_iSampleSecs = atoi(argv[5]);
		if(argv[6])
			g_Config.m_iTimeLevel1 = atoi(argv[6]);
		if(argv[7])		
			g_Config.m_iTimeLevel2 = atoi(argv[7]);
		if(argv[8])
			g_Config.m_iTimeLevel3 = atoi(argv[8]);
	} 
	else
	{
		 printf("presscall [ip] [port]  [th_num] \n");
		 printf("presscall [ip] [port]  [th_num] [run_min] [sample_sec] [time_l1] [time_l2] [time_l3]\n");
		 printf("example:\n");
		 printf("presscall 192.168.1.1 8080\n");
		 exit(0);
	}

	//创建线程
	for( int i = 0; i<g_Config.m_iThreadNum; i++)
	{   
	        int ret = pthread_create(&threads_array[i],NULL,thread_func,NULL); //&tattr
	        if (ret != 0)
	        {
	            printf("create thread failed :%d\n",ret);  
	            exit(1);
	        }
	}

	printf("\nOK,running...      %s:%d   %dthreads   %dmins   time level(us):%d,%d,%d\n\n",
	                                               g_Config.m_szDestIp,
	                                               g_Config.m_iDestPort,
	                                               g_Config.m_iThreadNum,
	                                               g_Config.m_iRunMins,
	                                               g_Config.m_iTimeLevel1,
	                                               g_Config.m_iTimeLevel2,
	                                               g_Config.m_iTimeLevel3
	                                               );    
	printf("TIME      OK/NO/BAD/ALL=PERCENT      TIPS        AvgTime(ms)	MaxTime(ms)	L1(%d)	L2(%d)	L3(%d)\n",
					g_Config.m_iTimeLevel1,g_Config.m_iTimeLevel2,g_Config.m_iTimeLevel3);

#ifdef FORTMAT_OUTPUT
    char pcFormatBuf[CELL_LEN_SHORT];
#endif

    //持续运行
    if (g_Config.m_iRunMins == 0)
    {
        iSecRuned = 0;
        while(1)
        {
            sleep(g_Config.m_iSampleSecs);
            iSecRuned += g_Config.m_iSampleSecs;
            
        #ifdef FORTMAT_OUTPUT
            sprintf(pcFormatBuf,"%d",iSecRuned);
            for ( int i=strlen(pcFormatBuf); i<sizeof(pcFormatBuf);i++)
            {
                pcFormatBuf[i] = ' ';
            }
            pcFormatBuf[sizeof(pcFormatBuf)-1] = '\0';
            printf("%s",pcFormatBuf);
        #else
            printf("%d        ",iSecRuned);
        #endif
            
            if (isTimeEnd)
            {
                break;
            }
            calculate();
        }
    }
    else
    {
        iSecLeft = g_Config.m_iRunMins * 60;
        iSecRuned = 0;
        while(iSecLeft>g_Config.m_iSampleSecs)
        {
            sleep(g_Config.m_iSampleSecs);
            iSecRuned += g_Config.m_iSampleSecs;
            iSecLeft -= g_Config.m_iSampleSecs;
            
       #ifdef FORTMAT_OUTPUT 
            sprintf(pcFormatBuf,"%d",iSecLeft);
            for ( int i=strlen(pcFormatBuf); i<sizeof(pcFormatBuf);i++)
            {
                pcFormatBuf[i] = ' ';
            }
            pcFormatBuf[sizeof(pcFormatBuf)-1] = '\0';
            printf("%s",pcFormatBuf);
       #else     
            printf("%d        ",iSecLeft);
       #endif
       
            if (isTimeEnd)
            {
                break;
            }            
            calculate();
        }
        if (!isTimeEnd && iSecLeft > 0)
        {
            sleep(iSecLeft);
            iSecRuned += iSecLeft;
            iSecLeft = 0;
        }
    }

    //通知线程结束
    isTimeEnd = 1;

    exit_proc();
}

/*****************************************************************************
 函 数 名  : calculate
 功能描述  : 采样函数
*****************************************************************************/
void calculate()
{
    //汇报收集单位时间结果,并清空
    struct TResult m_AllResult;
    memset(&m_AllResult,0,sizeof(m_AllResult));
    for (int i=0;i<g_Config.m_iThreadNum;i++)
    {
        pthread_mutex_lock(&Results_NowLock[i]);
		
        m_AllResult.iAllReqNum += Results_Now[i].iAllReqNum;
        m_AllResult.iOkResponseNum += Results_Now[i].iOkResponseNum;
        m_AllResult.iNoResponseNum += Results_Now[i].iNoResponseNum;
        m_AllResult.iBadResponseNum  += Results_Now[i].iBadResponseNum;
        m_AllResult.ullRspTimeUs+= Results_Now[i].ullRspTimeUs;
	if(Results_Now[i].ullMaxRspTimeUs > m_AllResult.ullMaxRspTimeUs )
			m_AllResult.ullMaxRspTimeUs = Results_Now[i].ullMaxRspTimeUs;

	m_AllResult.m_iTimeL1Num += Results_Now[i].m_iTimeL1Num;
	m_AllResult.m_iTimeL2Num += Results_Now[i].m_iTimeL2Num;
	m_AllResult.m_iTimeL3Num += Results_Now[i].m_iTimeL3Num;
	
        memset(&Results_Now[i],0,sizeof(TResult));
		
        pthread_mutex_unlock(&Results_NowLock[i]);
    }

    if (m_AllResult.iOkResponseNum > 0)
    {
        m_AllResult.ullRspTimeUs = (int)(m_AllResult.ullRspTimeUs/(float)m_AllResult.iOkResponseNum);    
    }
    else
    {
        m_AllResult.ullRspTimeUs = -1;
    }

    if (m_AllResult.iAllReqNum > 0)
    {
    #ifdef FORTMAT_OUTPUT 
        char pcFormatBufLong[CELL_LEN_LONG];
        char pcFormatBufShort[CELL_LEN_SHORT];

        //呼叫数据输出
        sprintf(pcFormatBufLong,"%d/%d/%d/%d=%.2f%%",m_AllResult.iOkResponseNum,m_AllResult.iNoResponseNum,
                                                                                m_AllResult.iBadResponseNum,m_AllResult.iAllReqNum,
                                                                                m_AllResult.iOkResponseNum*100/(float)m_AllResult.iAllReqNum);
        for ( int i=strlen(pcFormatBufLong); i<sizeof(pcFormatBufLong);i++)
        {
            pcFormatBufLong[i] = ' ';
        }
        pcFormatBufLong[sizeof(pcFormatBufLong)-1] = '\0';
        printf("%s",pcFormatBufLong);

        //tps输出
        sprintf(pcFormatBufShort,"%.2f",m_AllResult.iAllReqNum/float(g_Config.m_iSampleSecs));
        for ( int i=strlen(pcFormatBufShort); i<sizeof(pcFormatBufShort);i++)
        {
            pcFormatBufShort[i] = ' ';
        }
        pcFormatBufShort[sizeof(pcFormatBufShort)-1] = '\0';
        printf("%s",pcFormatBufShort);

        //respong time输出
        sprintf(pcFormatBufShort,"%dms",m_AllResult.ullRspTimeUs);
        for ( int i=strlen(pcFormatBufShort); i<sizeof(pcFormatBufShort);i++)
        {
            pcFormatBufShort[i] = ' ';
        }
        pcFormatBufShort[sizeof(pcFormatBufShort)-1] = '\0';
        printf("%s",pcFormatBufShort);
    #else               
        printf("%d/%d/%d/%d=%.2f%%       %.2f        %.3fms     %.3fms	%d			%d		%d\n",
                             m_AllResult.iOkResponseNum,m_AllResult.iNoResponseNum,
                             m_AllResult.iBadResponseNum,m_AllResult.iAllReqNum,
                             m_AllResult.iOkResponseNum*100/(float)m_AllResult.iAllReqNum,
                             m_AllResult.iAllReqNum/float(g_Config.m_iSampleSecs),
                             m_AllResult.ullRspTimeUs/(double)1000,
                             m_AllResult.ullMaxRspTimeUs/(double)1000,
                             m_AllResult.m_iTimeL1Num,
                             m_AllResult.m_iTimeL2Num,
                             m_AllResult.m_iTimeL3Num
                             );  
    #endif
    
    }

}
/*****************************************************************************
 函 数 名  : exit_proc
 功能描述  : 退出函数，退出时做全局统计
*****************************************************************************/
void exit_proc()
{   
    //汇报收集总结果
    struct TResult m_AllResult;
    memset(&m_AllResult,0,sizeof(m_AllResult));
    for (int i=0;i<g_Config.m_iThreadNum;i++)
    {
        m_AllResult.iAllReqNum += Results_History[i].iAllReqNum;
        m_AllResult.iOkResponseNum += Results_History[i].iOkResponseNum;
        m_AllResult.iNoResponseNum += Results_History[i].iNoResponseNum;
        m_AllResult.iBadResponseNum  += Results_History[i].iBadResponseNum;
        m_AllResult.ullRspTimeUs  += Results_History[i].ullRspTimeUs;
	if(Results_History[i].ullMaxRspTimeUs > m_AllResult.ullMaxRspTimeUs )
			m_AllResult.ullMaxRspTimeUs = Results_History[i].ullMaxRspTimeUs;		
    }
    printf("\n");
    printf("All request: %d\n",m_AllResult.iAllReqNum);
    printf("ok response: %d\n",m_AllResult.iOkResponseNum);
    printf("Bad response: %d\n",m_AllResult.iBadResponseNum);
    printf("No response: %d\n",m_AllResult.iNoResponseNum);
    if (m_AllResult.iOkResponseNum > 0)
    {
		m_AllResult.ullRspTimeUs = (int)(m_AllResult.ullRspTimeUs/(float)m_AllResult.iOkResponseNum);
		printf("Average response time: %.3fms\n",m_AllResult.ullRspTimeUs/(double)1000);
    }
	  printf("Max response time: %.3fms\n",m_AllResult.ullMaxRspTimeUs/(double)1000);
    printf("Running time(min:sec): %d:%2d\n",iSecRuned/60,iSecRuned%60);
    if (iSecRuned > 0)
    {
       printf("Tips: %f (/s)\n",m_AllResult.iAllReqNum/(float)iSecRuned);  
    }

    if (m_AllResult.iAllReqNum > 0)
    {
        float percent = m_AllResult.iOkResponseNum/(float)m_AllResult.iAllReqNum;
       printf("Percent of success: %f%%\n\n",percent*100);
    }


    pthread_mutex_destroy(&m_mutex_threadid);
    pthread_mutex_destroy(&m_mutex_thread_user);
    for ( int i=0;i< THREADS_ARRAY_NUM;i++)
    {
        pthread_mutex_destroy(&Results_NowLock[i]);
    } 
    _exit(0);   
}
