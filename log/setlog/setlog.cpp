/******************************************************************************

  文 件 名   : Setlog.cpp
  版 本 号   : V1.0
  作    者   : nekeyzhong
  生成日期   : 2005年11月30日
  功能描述   : 通过共享内存设置日志开关
******************************************************************************/
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <errno.h>

typedef struct  
{
	int m_iLogLevel;
	int m_iLogKey;
}TShmLog;

char* CreateShareMem( const char * szFile, char byProjID, int iSize)
{
	key_t iKey;
	int iShmID;

	if( !szFile )
	{
		return NULL;
	}

	iKey = ftok( szFile, byProjID );

	if( iKey < 0 )
	{
		printf("Error in ftok, %s.\n", strerror(errno));
		exit(-1);
	}

	iShmID = shmget( iKey, iSize, IPC_CREAT|IPC_EXCL|0666 );

	if( iShmID < 0 )
	{
		if( errno != EEXIST )
		{
			printf("Alloc share memory failed, %s\n", strerror(errno));
			exit(-1);
		}

		iShmID = shmget( iKey, iSize, 0666 );
		if( iShmID < 0 )
		{
			iShmID = shmget( iKey, 0, 0666 );
			if( iShmID < 0 )
			{
				printf("Fatel error, touch to shm failed, %s.\n", strerror(errno));
				exit(-1);
			}
			else
			{
				if( shmctl(iShmID, IPC_RMID, NULL) )
				{
					printf("Remove share memory failed, %s\n", strerror(errno));
					exit(-1);
				}
				iShmID = shmget( iKey, iSize, IPC_CREAT|IPC_EXCL|0666 );
				if( iShmID < 0 )
				{
					printf("Fatal error, alloc share memory failed, %s\n", strerror(errno));
					exit(-1);
				}
			}
		}

	}

	return  (char *)shmat(iShmID, NULL, 0);
}

//日志级别
enum emHandleLogLevel
{	   
	KEYLOG = -1,
	NOLOG = 0, 
	ERRORLOG,	
	RUNLOG,
	DEBUGLOG
};

void ShowUsage(TShmLog* pShmLog)
{
	switch(pShmLog->m_iLogLevel)
	{
		case KEYLOG:
			 printf("Log key %d\n",pShmLog->m_iLogKey);
			 break;	
		case NOLOG:
			 printf("Log off\n");
			 break;
		case RUNLOG:
			 printf("Log run\n");
			 break;
		case ERRORLOG:
			 printf("Log error\n");
			 break;
		case DEBUGLOG:
			 printf("Log debug\n");
			 break;
		default:
			  printf("Bad Log level value %d\n",pShmLog->m_iLogLevel);
			 break;						 
	};	
	
	return;
}
int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage:setlog [procname] [off/run/error/debug]\n");
		printf("usage:setlog [procname] -key [key]\n");
		return 0;
	}
	
	char *pProName = argv[1];
	char szTmpStr[256];
	sprintf(szTmpStr,"./.%s_ctrllog",pProName);
	
	if (0 != access(szTmpStr,F_OK))
	{
		sprintf(szTmpStr,"touch ./.%s_ctrllog",pProName);
		system(szTmpStr);
	}
	sprintf(szTmpStr,"./.%s_ctrllog",pProName);
	   
	TShmLog* pShmLog = (TShmLog*)CreateShareMem(szTmpStr, 'L', sizeof(TShmLog));
	if( !pShmLog )
	{
		printf("Alloc shared memory for Log failed check %s file.\n",szTmpStr);
		exit(-1);
	}

	if (argc < 3)
	{
		ShowUsage(pShmLog);
		return 0;
	}

	if (0 == strcasecmp(argv[2],"off"))
	{
		pShmLog->m_iLogLevel= NOLOG;
		printf("Log off\n");
	}
	else if (0 == strcmp(argv[2],"run"))
	{
		pShmLog->m_iLogLevel = RUNLOG;
		printf("Log run\n");
	}	
	else if (0 == strcmp(argv[2],"error"))
	{
		pShmLog->m_iLogLevel = ERRORLOG;
		printf("Log error\n");
	}	
	else if (0 == strcmp(argv[2],"debug"))
	{
		pShmLog->m_iLogLevel = DEBUGLOG;
		printf("Log debug\n");
	}		
	else if (0 == strcmp(argv[2],"-key") || 0 == strcmp(argv[2],"-k"))
	{
		if (argc < 4)
		{
			ShowUsage(pShmLog);
			return 0;
		}
		pShmLog->m_iLogLevel = KEYLOG;
		pShmLog->m_iLogKey= atoi(argv[3]);
		printf("Log Key %d\n",pShmLog->m_iLogKey);
	}		
	else
	{
		ShowUsage(pShmLog);
		return 0;
	}
	
	shmdt( (char*)pShmLog );          //      禁止本进程使用这块内存
	return 0;
}

