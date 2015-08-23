/*******************************************************************
名称: 基于Epoll的大容量网络连出模块

Copyright (c) 2007 

Written by nekeyzhong 

*********************************************************************/

#include "mainctrl.h"
#include "Base.hpp"
 #include <sys/resource.h>

int InitDaemon();
void sigusr1_handle( int iSigVal );
void sigusr2_handle( int iSigVal );
CMainCtrl *g_pMainCtrl;

int main( int argc, char **argv )
{
	//版本查询
	if(1 < argc && !strcasecmp(argv[1], "-v" ))
	{
		printf("%s Epoll Server Ver: 3.3 build in %s %s\n\n", SVR_NAME,  __DATE__, __TIME__);

		printf("--------------[log]-----------------\n");
		printf("[2011-9-9] add log while write socket failed.\n");
		printf("[2011-9-2] support cpu affinity.\n");
		printf("[2011-8-19] use LogMsgFrequency to control & do not close link while pipe full.\n");
		printf("[2010-11-30] delete tcp nagle,save 0.1ms rsp time.\n");
		exit(0);
	}
	
	if(argc < 2)
	{
		printf("%s  [config]\n", argv[0]);
		printf("%s  [config] -d\n", argv[0]);
		printf("%s  -v\n", argv[0]);
		exit(0);
	}

	//进程互斥锁
	int lock_fd = open(argv[1], O_RDWR|O_CREAT, 0640);
	if(lock_fd < 0 )
	{
		printf("Open Lock File Failed,Server Init Failed!\n");
		return -1;
	}

	int ret = flock(lock_fd, LOCK_EX | LOCK_NB);
	if(ret < 0 )
	{
		printf("Lock File Failed,Server is already Running!\n");
		return -1;
	}

	//后台
	if((argc>=3) && (0 == strcasecmp(argv[2], "-d")))
	{
		;
	}
	else
	{
		InitDaemon();
	}
	
	//自定义信号
	signal(SIGUSR1, sigusr1_handle);
	signal(SIGUSR2, sigusr2_handle);	

	char szProcName[128];
	GetNameFromPath(argv[0],szProcName);
	
	char szTmp[128];	
	sprintf(szTmp,"../log/%s",szProcName);
	TLib_Log_LogInit(szTmp, 0x10000000, 5);

	TLib_Log_LogMsg("%s begin Start.\n",SVR_NAME);
	g_pMainCtrl = new CMainCtrl();

	ret = g_pMainCtrl->Initialize(szProcName,argv[1]);
	if( 0 != ret )
	{
		exit(0);
	}

	TLib_Log_LogMsg("%s Started.\n",SVR_NAME);
	g_pMainCtrl->Run();
	return 0;
}


void sigusr1_handle( int iSigVal )
{
	g_pMainCtrl->SetRunFlag(Flag_ReloadCfg);
	signal(SIGUSR1, sigusr1_handle);
}

void sigusr2_handle( int iSigVal )
{
	g_pMainCtrl->SetRunFlag(Flag_Exit);
	signal(SIGUSR2, sigusr2_handle);
}

int InitDaemon()
{
	pid_t pid;

	if ((pid = fork() ) != 0 )
	{
		exit( 0);
	}

	setsid();

	signal( SIGINT,  SIG_IGN);
	signal( SIGHUP,  SIG_IGN);
	signal( SIGQUIT, SIG_IGN);
	signal( SIGPIPE, SIG_IGN);
	signal( SIGTTOU, SIG_IGN);
	signal( SIGTTIN, SIG_IGN);
	signal( SIGCHLD, SIG_IGN);
	signal( SIGTERM, SIG_IGN);

	struct sigaction sig;

	sig.sa_handler = SIG_IGN;
	sig.sa_flags = 0;
	sigemptyset( &sig.sa_mask);
	sigaction( SIGHUP,&sig,NULL);

	if ((pid = fork() ) != 0 )
	{
		exit( 0);
	}

	umask( 0);
	setpgrp();
	return 0;
}

