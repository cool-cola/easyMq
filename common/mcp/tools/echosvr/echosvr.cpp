#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
using namespace std;

void PrintBin(char *pBuffer, int iLength)
{
    int i;

    char tmpBuffer[16384];
    char strTemp[32];

    if( iLength <= 0 || iLength > 4096 || pBuffer == NULL )
    {
        return;
    }

    tmpBuffer[0] = '\0';
    for( i = 0; i < iLength; i++ )
    {
        if( !(i%16) )
        {
            sprintf(strTemp, "\n%04d>    ", i/16+1);
            strcat(tmpBuffer, strTemp);
        }
        sprintf(strTemp, "%02X ", (unsigned char)pBuffer[i]);
        strcat(tmpBuffer, strTemp);
    }

    strcat(tmpBuffer, "\n");
    printf("Print Hex:%s", tmpBuffer);
    return;
}

int main(int argc,char** argv)
{
	if(argc <3)
	{
		printf("%s [IP] [PORT]\n",argv[0]);
		printf("%s [IP] [PORT] echo\n",argv[0]);
		return 0;
	}

	int echo=0;
	if((argc >=4)&&(0==strcmp(argv[3],"echo")))
	{
		echo = 1;
	}
	char szReadBuffer[100*1024];
	
	struct sockaddr_in server; 
	server.sin_addr.s_addr = inet_addr(argv[1]);
	server.sin_port = htons(atoi(argv[2]));
	server.sin_family = AF_INET;
	int iListenSocketFD = socket(AF_INET,SOCK_STREAM,0);
	if (iListenSocketFD < 0)
	{
		printf("create socket failed!\n");
		return 0;
	}

	//设置地址重用
	int iSetResult;       
	int option_on = 1;
	setsockopt( iListenSocketFD, SOL_SOCKET, SO_REUSEADDR, (char*)&option_on, sizeof(option_on) );
	if (0>bind(iListenSocketFD,(struct sockaddr *)&server,sizeof(struct sockaddr_in)))
	{	
		printf("bind failed!\n");
		return 0;
	}
	if (0>listen(iListenSocketFD,1024))
	{
		printf("listen failed!\n");
	    	return 0;
	}
	vector<int> vec_cli_sock;
	
	struct sockaddr_in stConnAddr; 
	socklen_t iAddrLength = sizeof(stConnAddr);

while(1)
{
	int iSelRet = 0;
	fd_set fds_read;
	do{
		FD_ZERO( &fds_read );
		FD_SET( iListenSocketFD, &fds_read );
		for (int i=0; i<vec_cli_sock.size();i++)
		{
			FD_SET(vec_cli_sock[i], &fds_read);
		}		
		timeval tvListen;
		tvListen.tv_sec = 1;
		tvListen.tv_usec = 0;	
		iSelRet = select( FD_SETSIZE, &fds_read, NULL, NULL, &tvListen );
	}while(iSelRet<=0);

	if (FD_ISSET( iListenSocketFD, &fds_read ))
	{
		int iNewSocketFD = accept(iListenSocketFD, (struct sockaddr *)&stConnAddr, &iAddrLength);
		if(iNewSocketFD>0)
		{
			vec_cli_sock.push_back(iNewSocketFD);
			printf("accept connect from %s\n",inet_ntoa(stConnAddr.sin_addr));
		}
	}
	
	for (int i=0; i<vec_cli_sock.size();i++)
	{
		memset(szReadBuffer,0,sizeof(szReadBuffer));
		int sockfd = vec_cli_sock[i];
		if (FD_ISSET( sockfd, &fds_read ))
		{
			int irLen = read (sockfd, szReadBuffer,sizeof(szReadBuffer));
			if(irLen <= 0)
			{
			       getpeername(sockfd,(struct sockaddr *)&stConnAddr,&iAddrLength); 		
				printf("close connect from %s\n",inet_ntoa(stConnAddr.sin_addr));
				
				close(sockfd);
				std::vector<int>::iterator it = vec_cli_sock.begin();
				for(;it!=vec_cli_sock.end();it++)
				{
					if(*it == sockfd)
					{
						vec_cli_sock.erase(it);
						i--;
						break;
					}
				}
			}
			else
			{
				printf("RECV:%s ",szReadBuffer);
				PrintBin(szReadBuffer,irLen);
				if(echo)
				{
					write(sockfd,szReadBuffer,irLen);
				}
			}
		}
	}
}
	
}
