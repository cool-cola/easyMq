#ifndef _DEFINE_MASTER_CTRL_HEADER_
#define _DEFINE_MASTER_CTRL_HEADER_

#include "frame_ctrl.h"
#include "easyMsg.h"

#define SVR_NAME "lottery bons master"
#define MAJOR "0"
#define MIDDLE "0"
#define MINOR "1"
#define VER (MAJOR "." MIDDLE "." MINOR)
using namespace ::EasyMQ;

extern CFrameCtrl* g_pFrameCtrl;

#ifndef INFO
#define INFO(format,...) \
	g_pFrameCtrl->FrequencyLog(0,DEBUGLOG, "[file:%s:%d]\r\n" format, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#ifndef WARN
#define WARN(format,...) \
	g_pFrameCtrl->FrequencyLog(0,RUNLOG, "[file:%s:%d]%s\r\n" format, __FILE__, __LINE__,__FUNCTION__, ##__VA_ARGS__)
#endif

#ifndef ERR
#define ERR(format,...) \
	g_pFrameCtrl->FrequencyLog(0,ERRORLOG, "[file:%s:%d]\r\n" format, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#ifndef FOR_WARN
#define FORCE_WARN(format,...) \
    g_pFrameCtrl->Log(0,RUNLOG, "[file:%s:%d]%s\r\n" format, __FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__)
#endif

namespace NS_LotteryBons
{
	class LotteryBonsMessage;
}

class CMasterCtrl
{
public:
	CMasterCtrl();
	~CMasterCtrl();
	int32_t Ver(FILE *pFileFp);
	int32_t Init(const char* pszConf);

	int32_t TimeTick(timeval* ptTick);
	//UserCode，不包含MQHead
	int32_t OnReqMessage(TMQHeadInfo* pMQHeadInfo, char* pUsrCode, uint32_t iUsrCodeLen);
	int32_t OnRspMessage(TMQHeadInfo* pMQHeadInfo, char* pUsrCode, uint32_t iUsrCodeLen);

	int32_t SendReq(uint32_t uIp, uint32_t uPort, char* ptReqMsg, uint32_t iLen);
	int32_t SendRsp(TMQHeadInfo* pMQHeadInfo, char *pOut, uint32_t iLen);
protected:
	int32_t ProcessInitTopic(TMQHeadInfo *pMQHeadInfo, const struct Msg &stMsg);
	int32_t ProcessMsg(TMQHeadInfo *pMQHeadInfo, const struct Msg &stMsg);

};

#endif
