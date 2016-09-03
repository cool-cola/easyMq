#include "./easyMsg.h"
namespace EasyMQ
{
	Asn20Msg *getAsn20Msg(const char *p)
	{
		int32_t iMsgLen = sizeof(EasyMQ::Msg)+strlen(p)+1;
		int32_t iAsnMsgLen = sizeof(EasyMQ::Asn20Msg)-sizeof(EasyMQ::Msg)+iMsgLen;
		EasyMQ::Asn20Msg *asnMsg = (EasyMQ::Asn20Msg *)malloc(iAsnMsgLen);
		if(asnMsg == NULL)
		{
			return NULL;
		}
		EasyMQ::Msg *msg = &asnMsg->msg;
		msg->uBufLen = strlen(p)+1;
		strncpy(msg->cBuf,p,strlen(p)+1);
		//PrintBin((char *)msg,iMsgLen);

		//asnMsg->msgTag =0x4E534153;
		asnMsg->msgTag =0x5341534E;
		asnMsg->msgLen = htonl(iAsnMsgLen);
		memcpy(&asnMsg->msg,msg,iMsgLen);
		//PrintBin((char *)asnMsg,iAsnMsgLen);
		return asnMsg;
	}
	void freeAsn20Msg(Asn20Msg *pMsg)
	{
		if(pMsg != NULL)
		{
			free(pMsg);
		}
	}
}
