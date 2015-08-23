#include <sys/time.h>
#include "CodeQueueMutil.hpp"

#define SHOWUSAGE \
{\
printf("%s [mqfile]\n",argv[0]);\
}

int main(int argc, char* argv[])
{
	if(argc < 2)    
	{        
		SHOWUSAGE	 
		return 0;    
	}
     
	CCodeQueueMutil m_CheckPipe;

	//创建管道
	if(CCodeQueueMutil::CreateMQByFile(argv[1],&m_CheckPipe))
	{
		printf("CreateMQByFile %s failed!\n",argv[1]);
		return -1;	
	}
	printf("CodeLen %d,Size %d\n",m_CheckPipe.GetCodeLength(),m_CheckPipe.GetSize());
	
	m_CheckPipe.Print(stdout);
	
	return 0;
}

