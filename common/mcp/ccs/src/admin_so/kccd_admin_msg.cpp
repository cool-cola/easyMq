/************************************************************

***********************************************************/

#include <arpa/inet.h>
#include <string.h>

#include "mainctrl.h"

#include "asn-incl.h"
#include "kccd_msg_pkt.h"

#define StoreMsgCloneHead(pMsgSrc,pMsgDst) { \
	(pMsgDst)->iVer = (pMsgSrc)->iVer;\
	(pMsgDst)->iRouteType = (pMsgSrc)->iRouteType;\
	(pMsgDst)->iRouteIDKey = (pMsgSrc)->iRouteIDKey;\
	(pMsgDst)->iSeq = (pMsgSrc)->iSeq;\
	(pMsgDst)->iSubSeq = (pMsgSrc)->iSubSeq;\
	(pMsgDst)->iBirthsec= (pMsgSrc)->iBirthsec;\
	(pMsgDst)->iBirthusec= (pMsgSrc)->iBirthusec;\
	(pMsgDst)->sExt = (pMsgSrc)->sExt;\
}

int ProcKccdAdminStat(CMainCtrl *pMainCtrl,Msg *pMsg,char* pOut, int &iOutLen)
{
	//int iCmd = pMsg->body.reqKccdAdminStat->iCmd;
	
	//rsp
	RspKccdAdminStatV2* pRspKccdAdminStatV2 = new RspKccdAdminStatV2;

	Msg rspmsg;
	StoreMsgCloneHead(pMsg,&rspmsg);
	rspmsg.body.choiceId = PKTS::rspKccdAdminStatV2Cid;
	rspmsg.body.rspKccdAdminStatV2 = pRspKccdAdminStatV2;
		
	pRspKccdAdminStatV2->iRetCode = 0;
	pRspKccdAdminStatV2->sRetMsg = "Success";
	pRspKccdAdminStatV2->sKccdVersion = "CCS";
	pRspKccdAdminStatV2->iMaxSocketNum =  ((CMainCtrl *)pMainCtrl)->m_stConfig.MAX_SOCKET_NUM;

	int iTimeAllSpanMs,iReqNum;
	int iAllReqNum = 0;
	for(int i=0;;i++)
	{
		if(!(((CMainCtrl *)pMainCtrl)->m_stConfig.m_stTcpUsedIPPort[i].m_pCLoadGrid))
			break;

		((CMainCtrl *)pMainCtrl)->m_stConfig.m_stTcpUsedIPPort[i].m_pCLoadGrid->FetchLoad(iTimeAllSpanMs,iReqNum);
		iAllReqNum+=iReqNum;
	}
	
	pRspKccdAdminStatV2->iLoadReqNum = iAllReqNum;
	pRspKccdAdminStatV2->iLoadSpanMs = iTimeAllSpanMs;

	ssize_t iBuffUsed=0,iBuffCount=0,iObjUsed=0,iObjCount=0;
	int iSendBlockSize = 0;
	for(int i=0; i<((CMainCtrl *)pMainCtrl)->m_stConfig.m_iAuxHandleNum; i++)
	{
		CAuxHandle *pAux = ((CMainCtrl *)pMainCtrl)->m_pAuxHandle[i];

		ssize_t iTmpBuffUsed,iTmpBuffCount,iTmpObjUsed,iTmpObjCount;
		pAux->m_stBuffMngSend.GetBufferUsage(iTmpBuffUsed,iTmpBuffCount,iTmpObjUsed,iTmpObjCount);

		iBuffUsed += iTmpBuffUsed;
		iBuffCount += iTmpBuffCount;
		iObjUsed += iTmpObjUsed;
		iObjCount += iTmpObjCount;

		iSendBlockSize = pAux->m_stBuffMngSend.GetBlockSize();
	}
	pRspKccdAdminStatV2->iSendBufUsed =  iBuffUsed;
	pRspKccdAdminStatV2->iSendBufCount = iBuffCount;
	pRspKccdAdminStatV2->iSendObjUsed = iObjUsed;
	pRspKccdAdminStatV2->iSendObjCount = iObjCount;
	pRspKccdAdminStatV2->iSendBlockSize = iSendBlockSize;

	iBuffUsed=0,iBuffCount=0,iObjUsed=0,iObjCount=0;
	int iRecvBlockSize = 0;
	for(int i=0; i<((CMainCtrl *)pMainCtrl)->m_stConfig.m_iAuxHandleNum; i++)
	{
		CAuxHandle *pAux = ((CMainCtrl *)pMainCtrl)->m_pAuxHandle[i];

		ssize_t iTmpBuffUsed,iTmpBuffCount,iTmpObjUsed,iTmpObjCount;
		pAux->m_stBuffMngRecv.GetBufferUsage(iTmpBuffUsed,iTmpBuffCount,iTmpObjUsed,iTmpObjCount);

		iBuffUsed += iTmpBuffUsed;
		iBuffCount += iTmpBuffCount;
		iObjUsed += iTmpObjUsed;
		iObjCount += iTmpObjCount;

		iRecvBlockSize = pAux->m_stBuffMngRecv.GetBlockSize();
	}
	pRspKccdAdminStatV2->iRecvBufUsed = iBuffUsed;
	pRspKccdAdminStatV2->iRecvBufCount = iBuffCount;
	pRspKccdAdminStatV2->iRecvObjUsed = iObjUsed;
	pRspKccdAdminStatV2->iRecvObjCount = iObjCount;
	pRspKccdAdminStatV2->iRecvBlockSize = iRecvBlockSize;


	TStatStu m_stStat;
	memset(&m_stStat,0,sizeof(m_stStat));
	TStatStu m_stAllStat;
	memset(&m_stAllStat,0,sizeof(m_stAllStat));

	int unTcpClientNum = 0;
	int m_unTcpErrClientNum = 0;
	//all aux
	for (int i=0; i<((CMainCtrl *)pMainCtrl)->m_stConfig.m_iAuxHandleNum; i++)
	{
		CAuxHandle *pAux = ((CMainCtrl *)pMainCtrl)->m_pAuxHandle[i];
		
		unTcpClientNum += pAux->m_unTcpClientNum;
		m_unTcpErrClientNum += pAux->m_unTcpErrClientNum;
		
		m_stStat.m_llTcpCSPkgNum += pAux->m_stStat.m_llTcpCSPkgNum;
		m_stStat.m_llTcpCSPkgLen += pAux->m_stStat.m_llTcpCSPkgLen;
		m_stStat.m_llTcpSCPkgNum += pAux->m_stStat.m_llTcpSCPkgNum;
		m_stStat.m_llTcpSCPkgLen += pAux->m_stStat.m_llTcpSCPkgLen;

		m_stStat.m_llUdpCSPkgNum += pAux->m_stStat.m_llUdpCSPkgNum;
		m_stStat.m_llUdpCSPkgLen += pAux->m_stStat.m_llUdpCSPkgLen;
		m_stStat.m_llUdpSCSuccPkgNum += pAux->m_stStat.m_llUdpSCSuccPkgNum;
		m_stStat.m_llUdpSCFailedPkgNum += pAux->m_stStat.m_llUdpSCFailedPkgNum;
		m_stStat.m_llUdpSCPkgLen += pAux->m_stStat.m_llUdpSCPkgLen;

		m_stAllStat.m_llTcpCSPkgNum += pAux->m_stAllStat.m_llTcpCSPkgNum;
		m_stAllStat.m_llTcpCSPkgLen += pAux->m_stAllStat.m_llTcpCSPkgLen;
		m_stAllStat.m_llTcpSCPkgNum += pAux->m_stAllStat.m_llTcpSCPkgNum;
		m_stAllStat.m_llTcpSCPkgLen += pAux->m_stAllStat.m_llTcpSCPkgLen;

		m_stAllStat.m_llUdpCSPkgNum += pAux->m_stAllStat.m_llUdpCSPkgNum;
		m_stAllStat.m_llUdpCSPkgLen += pAux->m_stAllStat.m_llUdpCSPkgLen;
		m_stAllStat.m_llUdpSCSuccPkgNum += pAux->m_stAllStat.m_llUdpSCSuccPkgNum;
		m_stAllStat.m_llUdpSCFailedPkgNum += pAux->m_stAllStat.m_llUdpSCFailedPkgNum;
		m_stAllStat.m_llUdpSCPkgLen += pAux->m_stAllStat.m_llUdpSCPkgLen;
		
	}
	
	pRspKccdAdminStatV2->iStatus = 1;
	pRspKccdAdminStatV2->iClientNum = unTcpClientNum;
	pRspKccdAdminStatV2->iTcpErrConnNum = m_unTcpErrClientNum;
	pRspKccdAdminStatV2->iTcpCSPkgNum = m_stAllStat.m_llTcpCSPkgNum;
	pRspKccdAdminStatV2->iTcpCSPkgLen = m_stAllStat.m_llTcpCSPkgLen;
	pRspKccdAdminStatV2->iTcpSCPkgNum = m_stAllStat.m_llTcpSCPkgNum;
	pRspKccdAdminStatV2->iTcpSCPkgLen = m_stAllStat.m_llTcpSCPkgLen;
	pRspKccdAdminStatV2->iUdpCSPkgNum = m_stAllStat.m_llUdpCSPkgNum;
	pRspKccdAdminStatV2->iUdpCSPkgLen = m_stAllStat.m_llUdpCSPkgLen;
	pRspKccdAdminStatV2->iUdpSCSuccPkgNum = m_stAllStat.m_llUdpSCSuccPkgNum;
	pRspKccdAdminStatV2->iUdpSCFailedPkgNum = m_stAllStat.m_llUdpSCFailedPkgNum;
	pRspKccdAdminStatV2->iUdpSCPkgLen = m_stAllStat.m_llUdpSCPkgLen;
	pRspKccdAdminStatV2->iStatCalcSecs = ((CMainCtrl *)pMainCtrl)->m_stConfig.m_iStatTimeGap;
	pRspKccdAdminStatV2->iStatTcpCSPkgNum = m_stStat.m_llTcpCSPkgNum;
	pRspKccdAdminStatV2->iStatTcpCSPkgLen = m_stStat.m_llTcpCSPkgLen;
	pRspKccdAdminStatV2->iStatTcpSCPkgNum = m_stStat.m_llTcpSCPkgNum;
	pRspKccdAdminStatV2->iStatTcpSCPkgLen = m_stStat.m_llTcpSCPkgLen;
	pRspKccdAdminStatV2->iStatUdpCSPkgNum = m_stStat.m_llUdpCSPkgNum;
	pRspKccdAdminStatV2->iStatUdpCSPkgLen = m_stStat.m_llUdpCSPkgLen;
	pRspKccdAdminStatV2->iStatUdpSCSuccPkgNum = m_stStat.m_llUdpSCSuccPkgNum;
	pRspKccdAdminStatV2->iStatUdpSCFailedPkgNum = m_stStat.m_llUdpSCFailedPkgNum;
	pRspKccdAdminStatV2->iStatUdpSCPkgLen = m_stStat.m_llUdpSCPkgLen;
	pRspKccdAdminStatV2->iParam1 = 0;
	pRspKccdAdminStatV2->iParam2 = 0;
	pRspKccdAdminStatV2->iParam3 = 0;
	pRspKccdAdminStatV2->iParam4 = 0;

	for (int i=0; i<((CMainCtrl *)pMainCtrl)->m_stConfig.m_iAuxHandleNum; i++)
	{
		CAuxHandle *pAux = ((CMainCtrl *)pMainCtrl)->m_pAuxHandle[i];
		
		MQStatNode *pMQStatNode = pRspKccdAdminStatV2->mqStatList.Append();
		pMQStatNode->iDqTotalMapSize = pAux->m_pSvr2MePipe->GetSize();
		pMQStatNode->iDqUsedSize = pAux->m_pSvr2MePipe->GetCodeLength();
		pMQStatNode->iEqTotalMapSize = pAux->m_pMe2SvrPipe->GetSize();;
		pMQStatNode->iEqUsedSize = pAux->m_pMe2SvrPipe->GetCodeLength();;
	}

	AsnLen bytesEncoded;
	AsnBuf outbuff;

	outbuff.Init(pOut,MAX_MSG_LEN);
	outbuff.ResetInWriteRvsMode();
	if(rspmsg.BEncPdu(outbuff,bytesEncoded)==false)
	{
		printf("[%s] BEncPdu failed!\n",__FUNCTION__);
		return -1;
	}

	memmove(pOut,outbuff.DataPtr(),outbuff.DataLen());
	iOutLen = outbuff.DataLen();
	return 0;
}

int ProcessLogReq(CMainCtrl *pMainCtrl,Msg *pMsg,char* pOut, int &iOutLen)
{   
	RspKccdAdminLog* rspKccdAdminLog;
	Msg rspmsg;

	rspKccdAdminLog = new RspKccdAdminLog;

	rspmsg.iVer = 1;
	rspmsg.body.choiceId = PKTS::rspKccdAdminLogCid;
	rspmsg.body.rspKccdAdminLog = rspKccdAdminLog;
	StoreMsgCloneHead(pMsg, &rspmsg);

	rspKccdAdminLog->iRetCode = 0;
	rspKccdAdminLog->iActLine = 0;
	rspKccdAdminLog->iUpdateTime = 0;
	rspKccdAdminLog->iParam1 = 0;
	rspKccdAdminLog->iParam2 = 0;
	rspKccdAdminLog->iParam3 = 0;
	rspKccdAdminLog->iParam4 = 0;

	AsnLen bytesEncoded;
	AsnBuf outbuff;

	outbuff.Init(pOut,MAX_MSG_LEN);
	outbuff.ResetInWriteRvsMode();
	if(rspmsg.BEncPdu(outbuff,bytesEncoded)==false)
	{
		printf("[%s] BEncPdu failed!\n",__FUNCTION__);
		return -1;
	}
	
	memmove(pOut,outbuff.DataPtr(),outbuff.DataLen());
	iOutLen = outbuff.DataLen();
	return 0;
}

extern "C"
{

int do_msg(void* pMainCtrl,char* pIn, int iInLen,char* pOut, int &iOutLen)
{
	CMainCtrl *MainCtrl = (CMainCtrl*)pMainCtrl;

	//decode
 	AsnBuf inBuf;
  	inBuf.InstallData(pIn,iInLen);

	AsnLen bytesDecoded;
	Msg req_msg;
	if(req_msg.BDecPdu(inBuf,bytesDecoded)!=true)
	{
		printf("--- ERROR - Decode routines failed");
		return -1;
	}

	int iRet = 0;
	switch(req_msg.body.choiceId)
	{
		case PKTS::reqKccdAdminStatCid:
			iRet = ProcKccdAdminStat(MainCtrl,&req_msg,pOut,iOutLen);
			break;

		case PKTS::reqKccdAdminLogCid:
			iRet = ProcessLogReq(MainCtrl,&req_msg,pOut,iOutLen);
			break;			
		default:
			printf("do_msg:: --- ERROR - bad choiceid %d\n",(int)req_msg.body.choiceId);
			return -1;
	}

	return iRet;
}

}



