#ifndef _DEFINE_MASTER_CTRL_HEADER_
#define _DEFINE_MASTER_CTRL_HEADER_

#include "frame_ctrl.h"

#define SVR_NAME "lottery bons master"
#define MAJOR "0"
#define MIDDLE "0"
#define MINOR "1"
#define VER (MAJOR "." MIDDLE "." MINOR)

extern CFrameCtrl* g_pFrameCtrl;

#define INFO(format,...) \
	g_pFrameCtrl->FrequencyLog(0,DEBUGLOG, "[file:%s:%d]\r\n" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(format,...) \
	g_pFrameCtrl->FrequencyLog(0,RUNLOG, "[file:%s:%d]%s\r\n" format, __FILE__, __LINE__,__FUNCTION__, ##__VA_ARGS__)
#define ERR(format,...) \
	g_pFrameCtrl->FrequencyLog(0,ERRORLOG, "[file:%s:%d]\r\n" format, __FILE__, __LINE__, ##__VA_ARGS__)

#define FORCE_WARN(format,...) \
    g_pFrameCtrl->Log(0,RUNLOG, "[file:%s:%d]%s\r\n" format, __FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__)

namespace NS_LotteryBons
{
	class LotteryBonsMessage;
}

#define MAKE_LOTTERYBONS_ASN_REQ(req_msg, p_var_sub_msg, sub_msg_type, seq) {   \
	(req_msg).iVer = 1; \
	(req_msg).iSeq = seq; \
	(req_msg).body = new NS_LotteryBons::BODYPKTS;    \
	(req_msg).body->choiceId = NS_LotteryBons::BODYPKTS::req##sub_msg_type##Cid;  \
	(req_msg).body->req##sub_msg_type = new NS_LotteryBons::Req##sub_msg_type;   \
	(p_var_sub_msg) = (req_msg).body->req##sub_msg_type; \
}

#define MAKE_LOTTERYBONS_ASN_RSP(rsp_msg, p_var_sub_msg, sub_msg_type, seq) {   \
	(rsp_msg).iVer = 1; \
	(rsp_msg).iSeq = seq; \
	(rsp_msg).body = new NS_LotteryBons::BODYPKTS;    \
	(rsp_msg).body->choiceId = NS_LotteryBons::BODYPKTS::rsp##sub_msg_type##Cid;  \
	(rsp_msg).body->rsp##sub_msg_type = new NS_LotteryBons::Rsp##sub_msg_type;   \
	(p_var_sub_msg) = (rsp_msg).body->rsp##sub_msg_type; \
}


class CMasterCtrl
{
public:
	CMasterCtrl();
	~CMasterCtrl();
	int32_t Ver(FILE *pFileFp);
	int32_t Init(const char* pszConf);

	int32_t TimeTick(timeval* ptTick);
	int32_t OnReqMessage(TMQHeadInfo* pMQHeadInfo, char* pUsrCode, uint32_t iUsrCodeLen);
	int32_t OnRspMessage(TMQHeadInfo* pMQHeadInfo, char* pUsrCode, uint32_t iUsrCodeLen);

	int32_t SendReq(uint32_t uIp, uint32_t uPort, NS_LotteryBons::LotteryBonsMessage* ptReqMsg);
	int32_t SendRsp(TMQHeadInfo* pMQHeadInfo, char *pOut, uint32_t iLen);

protected:
	int32_t ProcessMasterHeartBeat(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptReqMsg);
	int32_t ProcessMasterDoTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptReqMsg);
};

#endif
