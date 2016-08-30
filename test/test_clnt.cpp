/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2010
Description:
***********************************************************/

#include <assert.h>
#include "TSocket.h"
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include "../common/easyMsg.h"

TcpCltSocket stTcpCltSocket;
timeval t1,t2;
char DestIp[16];
int iDestPort = 0;
#define SHOWUSAGE \
{\
printf("%s [data]\n",argv[0]);\
}

#define MAX_LENGTH 2048

int32_t sendMsg(char *buf, int32_t iLen);
int32_t recvMsg(char *buf, int32_t iLen);

void PrintBin(char *pBuffer, int iLength )
{
	int i;
	char tmpBuffer[4096];
	char strTemp[32];
	if( iLength <= 0 || iLength > 1024 || pBuffer == NULL )
	{
		return;
	}

	tmpBuffer[0] = '\0';
	for( i = 0; i < iLength; i++ )
	{
		if(!( i%16 ) )
		{
			sprintf( strTemp, "\n%04d>    ", i/16+1);
			strcat(tmpBuffer, strTemp);
		}
		sprintf( strTemp, "%02X ", (unsigned char )pBuffer[i]);
		strcat(tmpBuffer, strTemp);
	}
	strcat(tmpBuffer, "\n");
	printf("Print Hex:%s", tmpBuffer);
	return;
}

int ReadCfg( char *DestIp, int &iDestPort)
{
	char m_pCurLine[1024];
	FILE* m_fpFile = fopen("./conf.cfg","r+"); //w+
	if (m_fpFile == NULL)
	{
		printf("open conf.cfg failed!\n");
		return -1;
	}

	char pCfgName[512];
	int i;
	while(NULL != fgets(m_pCurLine,sizeof(m_pCurLine)-1,m_fpFile))
	{
		i=0;
		while(m_pCurLine[i] == ' ') {i++;}
		if(m_pCurLine[i] == '#' || m_pCurLine[i] == '\0' || m_pCurLine[i] == '\r' ||m_pCurLine[i] == '\n' || m_pCurLine[i] == '\t')
		{
			continue;
		}
		if (0 == memcmp(&m_pCurLine[i],"DestIP",strlen("DestIP")))
		{
			if (3 != sscanf(m_pCurLine,"%s %s %d",pCfgName,DestIp,&iDestPort))
			{
				printf("reading conf.cfg failed!\n");
				return -1;
			}
		}
		else
		{
			continue;
		}
	}
	fclose(m_fpFile);
	return 0;
}

int32_t testInitTopic(char *pTopic)
{
	int32_t iRet = 0;
	char buf[MAX_LENGTH] = {0};
	int32_t iMsgLen = sizeof(EasyMQ::Msg)+strlen(pTopic);
	EasyMQ::Msg *msg = (EasyMQ::Msg *)malloc(iMsgLen);
	msg->type = EasyMQ::Msg::MSG_TYPE_REQ_INIT_TOPIC;
	msg->uBufLen = strlen(pTopic);
	memcpy(msg->cBuf,pTopic,strlen(pTopic));
	PrintBin((char *)msg,iMsgLen);

	int32_t iAsnMsgLen = sizeof(EasyMQ::Asn20Msg)-sizeof(EasyMQ::Msg)+iMsgLen;
	EasyMQ::Asn20Msg *asnMsg = (EasyMQ::Asn20Msg *)malloc(iAsnMsgLen);
	//asnMsg->msgTag =0x4E534153;
	asnMsg->msgTag =0x5341534E;
	asnMsg->msgLen = htonl(iAsnMsgLen);
	memcpy(&asnMsg->msg,msg,iMsgLen);
	PrintBin((char *)asnMsg,iAsnMsgLen);
	iRet = sendMsg((char *)asnMsg,iAsnMsgLen);
	if(iRet != 0)
	{
	    return iRet;
	}
	iRet = recvMsg(buf, MAX_LENGTH);
	return iRet;
}

int32_t testTransferMsg()
{
	int32_t iRet = 0;
	return iRet;
}

int32_t testInitTopicMulti()
{
	return 0;
}

int32_t testTransferMsgMulti()
{
	return 0;
}

int32_t testTransferSpeed()
{
	return 0;
}

int32_t testRecvMsgSpeed()
{
	return 0;
}

int32_t sendMsg(char *buf,int32_t iMsgLen)
{
	printf("SEND=>>\n");
	PrintBin(buf,iMsgLen);
	gettimeofday(&t1,NULL);
	stTcpCltSocket.TcpWrite(buf,iMsgLen);
	return 0;
}

int32_t recvMsg(char *buf, int32_t iLen)
{
	printf("\n\nRECV=>>\n");
	char szIn[65535];
	int iReadLen = stTcpCltSocket.TcpRead(szIn, sizeof(szIn));
	if (iReadLen<=0)
	{
		printf("read from %s:%d failed!\n",DestIp,iDestPort);
		return -1;
	}
	gettimeofday(&t2,NULL);

	PrintBin(szIn,iReadLen);

	printf("\ncost %ld us\n",(t2.tv_sec-t1.tv_sec)*1000000+
		t2.tv_usec-t1.tv_usec);
	memcpy(buf, szIn, iReadLen);
	return 0;
}

void init()
{
	ReadCfg(DestIp,iDestPort);
	//-----------------------------------
	if(!stTcpCltSocket.ConnectServer(inet_addr(DestIp), iDestPort))
	{
		printf("connect %s:%d failed!\n",DestIp,iDestPort);
		exit(1) ;
	}
}
int main(int argc, char* argv[])
{
	int32_t iRet = 0;
	init();
	iRet = testInitTopic("yafngzh");
	assert(!iRet);
	sleep(3);
	return 0;

}




