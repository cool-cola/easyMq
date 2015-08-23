#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "Base.hpp"

main(int argc,char** argv)
{
	if(argc <2)
	{
		printf("%s [port]\n",argv[0]);
		return 0;
	}
	int port = atoi(argv[1]);
	int sockfd;
	char buf[1024];
	struct sockaddr_in server = {AF_INET,port,INADDR_ANY};
	struct sockaddr_in client;
	int iclientlen = sizeof(struct sockaddr_in);

	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	server.sin_port = htons(port);
	bind(sockfd,(struct sockaddr *)&server,sizeof(struct sockaddr_in));

	while(1)
	{
		memset (buf,0,1024);
		int len = recvfrom(sockfd,buf,1024,0,(struct sockaddr *)&client,(socklen_t*)&iclientlen);		//阻塞点

		sendto(sockfd,buf,len,0,(struct sockaddr *)&client,sizeof(struct sockaddr_in));


		PrintBin(stdout,buf,len);
		//fprintf(stderr,"\nRevv:  %s  , from %s ,port %d",buf,inet_ntoa(client.sin_addr),client.sin_port);
		            //  sleep(5);
		
	}	
}

