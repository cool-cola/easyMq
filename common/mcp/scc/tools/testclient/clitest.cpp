#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include "TSocket.h"
#include "Base.hpp"

int main(int argc,char** argv)
{
	if(argc <2)
	{
		printf("%s [tcp/udp] [port]\n",argv[0]);
		return 0;
	}
	int iCodeLength = 3*1024*1024+1024;
	char *m_szTempBuff = new char[iCodeLength+4];

	TcpSrvSocket stTcpSrvSocket;	
	stTcpSrvSocket.openServer(NULL,atoi(argv[2]));
	int iClientSocket = stTcpSrvSocket.acceptClient();
	printf("accept connect!\n");
	while(1)
	{
	/*
		int ret = read(iClientSocket,m_szTempBuff,8);
		if (ret != 8)
		{
			printf("disconnect!\n");
			close(iClientSocket);
			iClientSocket = stTcpSrvSocket.acceptClient();
			continue;
		}

		iCodeLength = *(int*)(m_szTempBuff+4);
		iCodeLength = ntohl(iCodeLength);

		int rlen = read(iClientSocket,m_szTempBuff+8,iCodeLength-8);
		if (rlen <=0)
		{
			printf("may be close!\n");
			iClientSocket = stTcpSrvSocket.acceptClient();
			sleep(1);
			continue;
		}
*/
	int ret = read(iClientSocket,m_szTempBuff,3*1024*1024);
	if(ret <= 0)continue;
	
		PrintBin(stdout,m_szTempBuff,ret);

		int ret2 = write(iClientSocket,m_szTempBuff,ret);

		printf("write ret %d\n",ret2);
		PrintBin(stdout,m_szTempBuff,ret);

	}

}

