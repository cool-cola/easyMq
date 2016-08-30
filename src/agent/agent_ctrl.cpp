#include <sys/types.h>
#include <stdio.h>

#include "easyMQAgent.h"
#include "agent_ctrl.h"

CAgentCtrl g_tAgentCtrl;

/////////////////////////////////////////////////////////////////////////////////////
using namespace ::EasyMQ;

CAgentCtrl::CAgentCtrl()
{
}

int32_t CAgentCtrl::Ver(FILE *pFileFp)
{
	fprintf(pFileFp,"-------- %s %s --------\n",SVR_NAME,VER);
	fprintf(pFileFp,"*2014-07-02 create by lambygao!\n");
	return 0;
}

int32_t CAgentCtrl::Init(const char* pszConf)
{
	return 0;
}

int32_t CAgentCtrl::TimeTick(timeval* ptTick)
{
	//g_tTaskManager.OnTick(ptTick);
	//g_tWorkerManager.OnTick(ptTick);
	return 0;
}

int32_t CAgentCtrl::OnReqMessage(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{
	int32_t iRet;
	struct Asn20Msg *asnMsg = (struct Asn20Msg *)pUsrCode;
	struct ::EasyMQ::Msg stMsg = asnMsg->msg;
	INFO("receive %d",stMsg.type);

	switch(stMsg.type)
	{
		case ::EasyMQ::Msg::MSG_TYPE_REQ_INIT_TOPIC:
			stMsg.type = ::EasyMQ::Msg::MSG_TYPE_REQ_INIT_TOPIC;
			uint32_t ip,port=8888;
			iRet = inet_pton(AF_INET,"192.168.1.222",&ip);
			if(iRet < 0)
			{
				ERR("covert failed");
			}
			pMQHeadInfo->m_unClientIP = ip;
			pMQHeadInfo->m_usClientPort = port;
			INFO("client is %d, port is %d",ip,port);
			SendReq(pMQHeadInfo,pUsrCode,iUsrCodeLen);
			break;
			/*
			ERR("Error msg type %d", stMsg.type);
			stMsg.retCode = ::EasyMQ::Msg::MSG_RET_FAIL;
			*/
	}
	//listMsg.push_back(stMsg);
	//SendRsp(pMQHeadInfo, pUsrCode, iUsrCodeLen);
	return 0;
}

int32_t CAgentCtrl::OnRspMessage(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{
	//printMem(pUsrCode, iUsrCodeLen);

	struct Asn20Msg *asnMsg = (struct Asn20Msg *)pUsrCode;
	struct ::EasyMQ::Msg stMsg;

	switch(stMsg.type)
	{
		case ::EasyMQ::Msg::MSG_TYPE_RESP_INIT_TOPIC:
			INFO("Msg Ok!");
			break;
		default:
			ERR("Error msg type %d", stMsg.type);
	}

	SendRsp(pMQHeadInfo,pUsrCode,iUsrCodeLen);
	return 0;
}

//pMsg不包含Header
int32_t CAgentCtrl::SendReq(TMQHeadInfo *pMQHeadInfo, char *pMsg, uint32_t uLen)
{
    g_pFrameCtrl->SendReq(pMQHeadInfo,pMsg,uLen);
	return 0;
}

//pOut不包含Header，但是和Header连在一起
int32_t CAgentCtrl::SendRsp(TMQHeadInfo* pMQHeadInfo, char *pOut, uint32_t iLen)
{
    g_pFrameCtrl->SendRsp(pMQHeadInfo,pOut,iLen);
	return 0;
}

int32_t CAgentCtrl::RecvMsg(const struct ::EasyMQ::Msg *pMsg)
{
	struct ::EasyMQ::Msg curMsg = listMsg.front();
	pMsg = &curMsg;
	listMsg.pop_front();
	return 0;
}
