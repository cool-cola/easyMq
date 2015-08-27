#include <sys/types.h>

#include "easyMsg.h"
#include "master_ctrl.h"


using namespace ::EasyMQ;

CMasterCtrl g_tMasterCtrl;

/////////////////////////////////////////////////////////////////////////////////////

 void printMem(char *pMem, int len)
{
    for(int i = 0; i < len; ++i)
    {
        INFO("0x%x", pMem[i]);
        if(i % 4 == 0)
        {
            INFO("\n");
        }
        else
        {
            INFO(" ");
        }
    }
}

CMasterCtrl::CMasterCtrl()
{
}

CMasterCtrl::~CMasterCtrl()
{
}

int32_t CMasterCtrl::Ver(FILE *pFileFp)
{
	fprintf(pFileFp,"-------- %s %s --------\n",SVR_NAME,VER);
	fprintf(pFileFp,"*2014-07-02 create by lambygao!\n");
	return 0;
}

int32_t CMasterCtrl::Init(const char* pszConf)
{
	return 0;//(g_tTaskManager.Init(pszConf) | g_tWorkerManager.Init(pszConf));
}

int32_t CMasterCtrl::TimeTick(timeval* ptTick)
{
	//g_tTaskManager.OnTick(ptTick);
	//g_tWorkerManager.OnTick(ptTick);
	return 0;
}

int32_t CMasterCtrl::OnReqMessage(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{
	printMem(pUsrCode, iUsrCodeLen);

	struct Asn20Msg *asnMsg = (struct Asn20Msg *)pUsrCode;
	struct Msg stMsg;
	stMsg.fromAsn(*asnMsg);

	switch(stMsg.type)
	{
	case Msg::MSG_TYPE_REQ_INIT_TOPIC:
	    stMsg.type = Msg::MSG_TYPE_RESP_INIT_TOPIC;
	    stMsg.retCode = Msg::MSG_RET_SUCC;
	    break;
	case Msg::MSG_TYPE_REQ_SUBSCRIBE:
	    stMsg.type = Msg::MSG_TYPE_RESP_PUBLISH;
	    stMsg.retCode = Msg::MSG_RET_SUCC;
	    break;
	case Msg::MSG_TYPE_RESP_PUBLISH:
	    stMsg.type = Msg::MSG_TYPE_RESP_SUBSCRIBE;
	    stMsg.retCode = Msg::MSG_RET_SUCC;
	default:
	    ERR("Error msg type %d", stMsg.type);
	    stMsg.retCode = Msg::MSG_RET_FAIL;
	}
	SendRsp(pMQHeadInfo, pUsrCode, iUsrCodeLen);
	return 0;
}

int32_t CMasterCtrl::OnRspMessage(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{

	return 0;
}

//pMsg不包含Header
int32_t CMasterCtrl::SendReq(uint32_t uIp, uint32_t uPort, char *pMsg, uint32_t uLen)
{
    g_pFrameCtrl->SendReq(uIp,uPort,pMsg,uLen);
	return 0;
}

//pOut不包含Header，但是和Header连在一起
int32_t CMasterCtrl::SendRsp(TMQHeadInfo* pMQHeadInfo, char *pOut, uint32_t iLen)
{
    g_pFrameCtrl->SendRsp(pMQHeadInfo,pOut,iLen);
	return 0;
}

int32_t CMasterCtrl::ProcessMasterHeartBeat(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptReqMsg)
{


	return 0;//SendRsp(pMQHeadInfo,&rspMsg);
}

int32_t CMasterCtrl::ProcessMasterDoTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptReqMsg)
{

	return 0;//SendRsp(pMQHeadInfo,&rspMsg);
}
