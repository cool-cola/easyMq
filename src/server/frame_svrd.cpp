
#include "frame_ctrl.h"
#include <signal.h>
#include <execinfo.h>
#include <sys/file.h>

int InitDaemon();
void sigusr1_handle( int iSigVal );
void sigusr2_handle( int iSigVal );
void sigsegv_handle (int sig);
CFrameCtrl *g_pFrameCtrl;

int main( int argc, char **argv )
{

	if(1 < argc && !strcasecmp(argv[1], "-v" ))
	{
		g_pFrameCtrl = new CFrameCtrl();
		g_pFrameCtrl->Ver(stdout);
		exit(0);
	}

	if(argc < 2)
	{
		printf("%s  [config]\n", argv[0]);
		printf("%s  [config] -d\n", argv[0]);
		exit(0);
	}
	
	char *pCfgFile = argv[1];
	if(0!=access(pCfgFile,R_OK))
	{
		printf("%s does not exist!\n",pCfgFile);
		exit(0);
	}
	
	if(!((argc >= 3)&&(strcmp(argv[2],"-d")==0)))
	{
		//后台进程
		InitDaemon();
	}	
	
	//文件锁
	int lock_fd = open(pCfgFile, O_RDWR|O_CREAT, 0640);
	if(lock_fd < 0 )
	{
		printf("Open Lock File %s Failed,  Server Init Failed!\n",pCfgFile);
		return -1;
	}

	int ret = flock(lock_fd, LOCK_EX | LOCK_NB);
	if(ret < 0 )
	{
		printf("Lock File Failed,  Server is already Running!\n");
		return -1;
	}
	
	//reload signal
	signal(SIGUSR1, sigusr1_handle);
	//stop signal
	signal(SIGUSR2, sigusr2_handle);	

	char szTmp[128];
	char szProcName[128];
	GetNameFromPath(argv[0],szProcName);
	
	sprintf(szTmp,"../log/%s",szProcName);
	TLib_Log_LogInit(szTmp, 0x10000000, 5);
//	signal(SIGSEGV, sigsegv_handle);
//	signal(SIGBUS, sigsegv_handle);
	g_pFrameCtrl = new CFrameCtrl();

	ret = g_pFrameCtrl->Initialize(szProcName,pCfgFile);
	if( 0 != ret )
	{
		exit(0);
	}

	TLib_Log_LogMsg("Start services ......\n");
	
	g_pFrameCtrl->Run();

	delete g_pFrameCtrl;

	flock(lock_fd, LOCK_UN);
	
	return 0;
}


void sigusr1_handle( int iSigVal )
{
	g_pFrameCtrl->SetRunFlag(Flag_ReloadCfg);
	signal(SIGUSR1, sigusr1_handle);
}

void sigusr2_handle( int iSigVal )
{
	g_pFrameCtrl->SetRunFlag(Flag_Exit);
	signal(SIGUSR2, sigusr2_handle);
}

void sigsegv_handle (int sig)
{
	static int first = 0;
    	void * array[25] = {0};
    	int nSize = 0;
    	char ** symbols = NULL;
    	int i = 0;
	
 	if (!first)
	{
	 	 first++;//�����ֹ����Ĵ����ٴη������������ѭ��
	  
		TLib_Log_LogMsg("segv memory error.\n");
	  	nSize = backtrace(array, 25);
	  	symbols = backtrace_symbols(array, nSize);
	  	for (i = 0; i < nSize; i++)
	  	{   
			TLib_Log_LogMsg("stack %d: %s\n", i, symbols[i]);
		} 
		
	  	free(symbols);
	}
	exit( 1);
}
int InitDaemon()
{
	pid_t pid;
	
	// 1.ת��Ϊ��̨����
	if ((pid = fork() ) != 0 )
		exit( 0);

	// 2.�뿪ԭ�ȵĽ�����
	setsid();

	// 3.��ֹ�ٴδ򿪿����ն�
	if ((pid = fork() ) != 0 )
		exit( 0);

	// 4.�رմ򿪵��ļ��������������˷�ϵͳ��Դ
	/*
	rlimit rlim;
	if(getrlimit(RLIMIT_NOFILE,&rlim) == 0)
	{
		for(int fd=3; fd<=(int)rlim.rlim_cur; fd++)
		{
			close(fd);
		}
	}
	*/

	// 5.�ı䵱ǰ�Ĺ���Ŀ¼������ж�ز����ļ�ϵͳ
	//if (chdir("/") == -1) exit(1);
 
	// 6.�����ļ����룬��ֹĳЩ���Ա�����������
	umask(0);
	setpgrp();

	// 7.�ض����׼���룬���������������Ϊ�ػ�����û�п����ն�
	/*
	if ((fd = open("/dev/null", O_RDWR)) == -1) 
		exit(1); 

	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	close(fd);
	*/

	// 8.�����ź�
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
	
	return 0;
}

