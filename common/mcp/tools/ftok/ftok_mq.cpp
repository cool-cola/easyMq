#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <assert.h>

int main(int argc ,char **argv)
{
	if(argc < 2)
	{
		printf("%s [mq_file]\n",argv[0]);
		return 0;
	}
	
	//create shm for mq
	key_t iShmKeyQ = ftok(argv[1],'Q');
	
	printf("Ftok,Key:%p\n",(void*)iShmKeyQ);
	return 0;
}

