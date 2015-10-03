#include <sys/types.h>

#include "easyMQAgent.h"
#include "agent_ctrl.h"

CAgentCtrl g_tAgentCtrl;

/////////////////////////////////////////////////////////////////////////////////////

CAgentCtrl::CAgentCtrl()
{
}

CAgentCtrl::~CAgentCtrl()
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
	struct Asn20Msg *asnMsg = (struct Asn20Msg *)pUsrCode;
	struct Msg stMsg;
	stMsg.fromAsn(*asnMsg);

	switch(stMsg.type)
	{
	case Msg::MSG_TYPE_RESP_PUBLISH:
	    stMsg.type = Msg::MSG_TYPE_RESP_SUBSCRIBE;
	    stMsg.retCode = Msg::MSG_RET_SUCC;
	    break;
	default:
	    ERR("Error msg type %d", stMsg.type);
	    stMsg.retCode = Msg::MSG_RET_FAIL;
	}
	listMsg.push_back(stMsg);
	SendRsp(pMQHeadInfo, pUsrCode, iUsrCodeLen);
	return 0;
}

int32_t CAgentCtrl::OnRspMessage(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{
	printMem(pUsrCode, iUsrCodeLen);

	struct Asn20Msg *asnMsg = (struct Asn20Msg *)pUsrCode;
	struct Msg stMsg;
	stMsg.fromAsn(*asnMsg);

	switch(stMsg.type)
	{
	case Msg::MSG_RET_SUCC:
		INFO("Msg Ok!");
		break;
	case Msg::MSG_RET_FAIL:
		ERR("Msg error!");
		break;
	default:
		ERR("Error msg type %d", stMsg.type);
	}

	return 0;
}

//pMsg不包含Header
int32_t CAgentCtrl::SendReq(uint32_t uIp, uint32_t uPort, char *pMsg, uint32_t uLen)
{
    g_pFrameCtrl->SendReq(uIp,uPort,pMsg,uLen);
	return 0;
}

//pOut不包含Header，但是和Header连在一起
int32_t CAgentCtrl::SendRsp(TMQHeadInfo* pMQHeadInfo, char *pOut, uint32_t iLen)
{
    g_pFrameCtrl->SendRsp(pMQHeadInfo,pOut,iLen);
	return 0;
}

int32_t CAgentCtrl::RecvMsg(const struct Msg *pMsg)
{
	struct Msg curMsg(listMsg.pop_front());
	pMsg = &curMsg;
	return 0;
}
