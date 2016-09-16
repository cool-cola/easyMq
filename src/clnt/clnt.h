#ifndef _CLNT_HEADER_H_
#define _CLNT_HEADER_H_

#define MAX_LENGTH 2048

#include <assert.h>
#include <string>
#include <sys/time.h>
#include "../../common/easyMsg.h"
#include "TSocket.h"

using std::string;
using EasyMQ::Asn20Msg;
using namespace EasyMQ;

struct MqClnt
{
	MqClnt(char *DestIp,int iDestPort)
	{
		//-----------------------------------
		if(!stTcpCltSocket.ConnectServer(inet_addr(DestIp), iDestPort))
		{
			printf("connect %s:%d failed!\n",DestIp,iDestPort);
			exit(1) ;
		}
	}
	TcpCltSocket stTcpCltSocket;
	struct InitTopicReq
	{
		string topic;
	};
	struct InitTopicResp
	{

	};
	struct timeval t1,t2;
	char DestIp[16];
	int iDestPort = 0;
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
		int iReadLen = stTcpCltSocket.TcpRead(szIn, sizeof(szIn),100000000);
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
	void initTopic(const InitTopicReq &req,InitTopicResp resp)
	{
		int32_t iRet = 0;
		char buf[MAX_LENGTH] = {0};
		Asn20Msg *asnMsg = getAsn20Msg(req.topic.c_str());
		if(asnMsg==	NULL)
		{
			assert(0);
		}
		asnMsg->msg.type = Msg::MSG_TYPE_REQ_INIT_TOPIC;
		iRet = sendMsg((char *)asnMsg,asnMsg->len());
		assert(!iRet);
		iRet = recvMsg(buf, MAX_LENGTH);
		assert(!iRet);
		//printf("receive msg len %d bytes\n",asnMsg->msgLen);
		freeAsn20Msg(asnMsg);
		printf("订阅topic %s成功!",req.topic.c_str());
	}
	struct PublishMsgReq
	{
		string topic;
		string msg;
	};
	struct PublishMsgResp
	{

	};
	void publishMsg(const PublishMsgReq &req,PublishMsgResp &resp)
	{
		char buf[MAX_LENGTH] = {0};
		int32_t iRet;
		Asn20Msg *asnMsg = getAsn20Msg(req.msg.c_str());
		assert(asnMsg!=NULL);
		asnMsg->msg.type = Msg::MSG_TYPE_REQ_PUBLISH;
		const char *topic = req.topic.c_str();
		strncpy(asnMsg->msg.topic,topic,strlen(topic)+1);
		iRet = sendMsg((char *)asnMsg,asnMsg->len());
		assert(!iRet);
		printf("生产消息 %s\n",req.msg.c_str());
		iRet = recvMsg(buf,asnMsg->len());
		assert(!iRet);
		freeAsn20Msg(asnMsg);

	}

	struct ReceiveMsgReq
	{
		string topic;
	};

	struct ReceiveMsgResp
	{
		string msg;
	};
	void receiveMsg(const ReceiveMsgReq &req,ReceiveMsgResp &resp,void (*f)(string msg))
	{
		char buf[MAX_LENGTH] = {0};
		int32_t iRet = 0;
		InitTopicReq initReq;
		InitTopicResp initResp;
		initReq.topic = req.topic;


		initTopic(initReq,initResp);
		while(1)
		{
			iRet = recvMsg(buf,MAX_LENGTH);
			assert(!iRet);
			Asn20Msg *pAsnMsg = (Asn20Msg *)buf;
			printf("收到订阅的消息 %s\n",pAsnMsg->msg.cBuf);
			f(string(pAsnMsg->msg.cBuf));
		}
	}
};

#endif
