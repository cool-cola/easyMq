#include <sys/types.h>

#include "easyMQServer.h"
#include "master_ctrl.h"

CMasterCtrl g_tMasterCtrl;
extern EasyMQServer g_easyMQServer;
/////////////////////////////////////////////////////////////////////////////////////

using namespace EasyMQ;
void printMem(char *pMem, int len)
{
    for(int i = 0; i < len; ++i)
    {
        printf("0x%x", pMem[i]);
        if(i % 4 == 0)
        {
            printf("\n");
        }
        else
        {
            printf(" ");
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

int32_t CMasterCtrl::ProcessInitTopic(TMQHeadInfo *pMQHeadInfo, const struct Msg *pstMsg)
{
    EasyMQAgent agent;
    agent.ipAddr = pMQHeadInfo->m_unClientIP;
    agent.port = pMQHeadInfo->m_usClientPort;
	agent.socketSuffix = pMQHeadInfo->m_iSuffix;
	std::string topic(pstMsg->cBuf,pstMsg->uBufLen);
    g_easyMQServer.initTopic(topic,agent);

	return 0;
}

int32_t CMasterCtrl::ProcessMsg(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{
	Asn20Msg *pAsn20Msg = (EasyMQ::Asn20Msg *)pUsrCode;
	Msg *pMsg = &pAsn20Msg->msg;

	Asn20Msg asnResp;
	asnResp.msgLen = htonl(asnResp.len());
    asnResp.msg.type= Msg::MSG_TYPE_RESP_PUBLISH;
    asnResp.msg.retCode = Msg::MSG_RET_SUCC;
    SendRsp(pMQHeadInfo, (char *)&asnResp, sizeof(asnResp));

	std::map<std::string, std::set<EasyMQAgent> >::iterator it = g_mapTopicToAgent.find(std::string(pMsg->topic));
	if(it == g_mapTopicToAgent.end())
	{
		return -1;
	}

	for(std::set<EasyMQAgent>::iterator itr(it->second.begin()); itr != it->second.end();
				++itr)
	{
		INFO("发送msg到 socketSuffix %d",pMQHeadInfo->m_iSuffix);
		pMQHeadInfo->m_iSuffix = itr->socketSuffix;
		SendRsp(pMQHeadInfo,(char *)pUsrCode,iUsrCodeLen);
	}
	return 0;
}
int32_t CMasterCtrl::OnReqMessage(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{
	struct Asn20Msg *asnMsg = (struct Asn20Msg *)pUsrCode;
	struct Msg *pstMsg = &asnMsg->msg;

	switch(pstMsg->type)
	{
	case Msg::MSG_TYPE_REQ_INIT_TOPIC:
		ProcessInitTopic(pMQHeadInfo, pstMsg);
	    break;
	case Msg::MSG_TYPE_REQ_SUBSCRIBE:
	    pstMsg->type = Msg::MSG_TYPE_RESP_PUBLISH;
	    pstMsg->retCode = Msg::MSG_RET_SUCC;
	    break;
	case Msg::MSG_TYPE_REQ_PUBLISH:
	    pstMsg->type = Msg::MSG_TYPE_RESP_SUBSCRIBE;
	    pstMsg->retCode = Msg::MSG_RET_SUCC;
	    break;
	case Msg::MSG_TYPE_REQ_ECHO:
		pstMsg->type = Msg::MSG_TYPE_RESP_ECHO;
		pstMsg->retCode = Msg::MSG_RET_SUCC;
	default:
	    ERR("Error msg type %d", pstMsg->type);
	    pstMsg->retCode = Msg::MSG_RET_FAIL;
	}

	//保证请求的报文比响应的长，这样不会发生溢出，否则要重新分配内存，增加内存拷贝
	asnMsg->msgLen = htonl(asnMsg->len());
    SendRsp(pMQHeadInfo, pUsrCode, iUsrCodeLen);
	return 0;
}

int32_t CMasterCtrl::OnRspMessage(TMQHeadInfo *pMQHeadInfo, char *pUsrCode, uint32_t iUsrCodeLen)
{
	printMem(pUsrCode, iUsrCodeLen);

	struct Asn20Msg *asnMsg = (struct Asn20Msg *)pUsrCode;
	struct Msg *pstMsg = &asnMsg->msg;
	int32_t retType = pstMsg->type;

	switch(retType)
	{
	case Msg::MSG_RET_SUCC:
		INFO("Msg Ok!");
		break;
	case Msg::MSG_RET_FAIL:
		ERR("Msg error!");
		break;
	default:
		ERR("Error msg type %d", pstMsg->type);
	}

	return 0;
}

//pMsg不包含Header
int32_t CMasterCtrl::SendReq(uint32_t uIp, uint32_t uPort, char *pMsg, uint32_t uLen)
{
    //g_pFrameCtrl->SendReq(uIp,uPort,pMsg,uLen);
	return 0;
}

//pOut不包含Header，但是和Header连在一起
int32_t CMasterCtrl::SendRsp(TMQHeadInfo* pMQHeadInfo, char *pOut, uint32_t iLen)
{
    g_pFrameCtrl->SendRsp(pMQHeadInfo,pOut,iLen);
	return 0;
}

