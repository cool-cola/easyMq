/*******************************************************************
名称: 基于Epoll的大容量网络接入模块

Copyright (c) 2007 

Written by nekeyzhong 

Develop from zengyu's code and CCD module (system architectural dept.)

*********************************************************************/

#include "mainctrl.h"
#include "Base.hpp"

int InitDaemon();
void sigusr1_handle( int iSigVal );
void sigusr2_handle( int iSigVal );
CMainCtrl *g_pMainCtrl;

int main( int argc, char **argv )
{
	//版本查询
	if(1 < argc && !strcasecmp(argv[1], "-v" ))
	{
		printf("%s Epoll Server Ver: 3.0.9 build in %s %s\n\n",SVR_NAME,  __DATE__, __TIME__);

		printf("--------------[log]-----------------\n");
		printf("[2014-02-08] v3.0.9 [f] aux thread close socket when it does not monitor the socket\n");
		printf("[2013-04-03] v3.0.8 [a]patxiao frequency log\n");
		printf("[2013-03-15] v3.0.7 [a]patxiao support close port link\n");
		printf("[2012-11-06] add level log\n");
		printf("[2012-11-06] add error msg for exeption close link\n");
		printf("[2012-04-10] support uniqueID for mq file: key={proj_id+FF+uniqueID}\n");
		printf("[2011-09-01] support ip table access/deny\n");
		printf("[2011-07-20] support admin so\n");
		printf("[2011-05-11] if setrlimit failed, exit!\n");
		printf("[2011-03-16] add mutil thread, mutil cq ,mutil port.\n");
		printf("[2010-11-30] delete tcp nagle to solve 'the last pkg slow' problem.\n");
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

	TLib_Log_LogMsg("===== %s begin Start... =====\n",SVR_NAME);
	g_pMainCtrl = new CMainCtrl();

	ret = g_pMainCtrl->Initialize(szProcName,argv[1]);
	if( 0 != ret )
	{
		exit(0);
	}
	
	TLib_Log_LogMsg("===== %s Started. =====\n",SVR_NAME);
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

