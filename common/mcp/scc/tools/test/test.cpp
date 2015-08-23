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
#include <sys/time.h>

#include "MQHeadDefine.h"
#include "tlib_cfg.h"
#include "Base.hpp"
#include "CodeQueue.hpp"

int main(int argc,char** argv)
{
	if(argc < 3)	    
	{	        
		printf("%s  [mcp_to_scc] [scc_to_mcp]  [ip] [port] [datalen]\n",argv[0]);
		return 0;			    
	}
	
	CCodeQueue          m_Me2SvrPipe;                      /*输出队列*/
	CCodeQueue          m_Svr2MePipe;                      /*输入队列*/ 	
	
	if(CCodeQueue::CreateMQByFile(argv[1],&m_Me2SvrPipe))
	{
		printf("CreateShmMQ %s failed!\n",argv[1]);
		return -1;	
	}
	
	if(CCodeQueue::CreateMQByFile(argv[2],&m_Svr2MePipe))
	{
		printf("CreateShmMQ %s failed!\n",argv[2]);
		return -1;	
	}

	 int iCodeLength = 1024*1024*3+1024;
	 char *m_szTempBuff = new char[iCodeLength];	
	 memset(m_szTempBuff,'c',iCodeLength);

	int usrlen = strlen(argv[5]);
	while(1)
	{
		int iTag = 0;
		memcpy(&iTag,"MPMG",4);
		iTag = htonl(iTag);
		memcpy(m_szTempBuff + sizeof(TMQHeadInfo),&iTag,4);
		
		int tmp = htonl(usrlen+8);
		memcpy(m_szTempBuff + sizeof(TMQHeadInfo)+4,&tmp,4);
		strcpy(m_szTempBuff + sizeof(TMQHeadInfo)+8,argv[3]);

		TMQHeadInfo* pstMQHeadInfo = (TMQHeadInfo*)m_szTempBuff;
		pstMQHeadInfo->m_ucCmd = TMQHeadInfo::CMD_DATA_TRANS;
		pstMQHeadInfo->m_unClientIP =  inet_addr(argv[3]);
		strcpy(pstMQHeadInfo->m_szSrcMQ,"sss");
		pstMQHeadInfo->m_usClientPort = atoi(argv[4]);
		pstMQHeadInfo->m_ucDataType = TMQHeadInfo::DATA_TYPE_TCP;

		int ret = m_Me2SvrPipe.AppendOneCode((char*)m_szTempBuff,sizeof(TMQHeadInfo)+usrlen+8);
			if (ret <0)
			{
				usleep(10);
			}
			

		printf("SEND: user len=%d\n",sizeof(TMQHeadInfo)+usrlen+8);
		PrintBin(stdout,(char*)m_szTempBuff,sizeof(TMQHeadInfo)+usrlen+8);

		int pkgnum = 0;
		int pkglen = 0;
		int is = time(0);
		while(1)
		{
			iCodeLength = 1024*1024*3+1024;
			int ilen2 = iCodeLength;
			int iRet2 = m_Svr2MePipe.GetHeadCode(m_szTempBuff, &ilen2);
			if(iRet2 < 0 || ilen2 <= 0)
			{
				usleep(10000);
				continue;
			}
			PrintBin(stdout,(char*)m_szTempBuff,ilen2);
			
			pkgnum++;

			pkglen += ilen2;
			
			int in = time(0);	
			if (in-is >=1)
			{
				printf("pkgnum=%d,pkglen=%d\n",pkgnum,(int)(pkglen/1000.0f));
				is = in;
				pkgnum = 0;
				pkglen = 0;
			
			}
		}
		//usleep(300000);
	}
}


