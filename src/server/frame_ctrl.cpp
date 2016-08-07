#include "frame_ctrl.h"
#include "master_ctrl.h"

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#ifdef INFO
#undef INFO
#undef WARN
#undef ERR
#endif

#define INFO(format,...) \
	Log(0,DEBUGLOG, "[file:%s:%d]\r\n" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(format,...) \
	Log(0,RUNLOG, "[file:%s:%d]\r\n" format, __FILE__, __LINE__, ##__VA_ARGS__)
#define ERR(format,...) \
	Log(0,ERRORLOG, "[file:%s:%d]\r\n" format, __FILE__, __LINE__, ##__VA_ARGS__)

///////////////////////////////////////////////////////////////////

extern CMasterCtrl g_tMasterCtrl;

///////////////////////////////////////////////////////////////////

CFrameCtrl::CFrameCtrl()
    :	m_iMcpIdx(0), m_iRunFlag(0), m_CCSToMeMQ(NULL), m_MeToCCSMQ(NULL),
        m_SCCToMeMQ(NULL), m_MeToSCCMQ(NULL), m_pRecvBuffer(NULL), m_pSendBuffer(NULL)
{
    memset(&m_stConfig, 0, sizeof(m_stConfig));
    m_pRecvBuffer = (char *)malloc(FRAME_MAX_BUFF_LEN);
    m_pSendBuffer = (char *)malloc(FRAME_MAX_BUFF_LEN);
}

CFrameCtrl::~CFrameCtrl()
{
    if (m_pSendBuffer != NULL)
    {
        free(m_pSendBuffer);
        m_pSendBuffer = NULL;
    }
    if (m_pRecvBuffer != NULL)
    {
        free(m_pRecvBuffer);
        m_pRecvBuffer = NULL;
    }
}

void CFrameCtrl::Ver(FILE *pFileFp)
{
    fprintf(pFileFp, "build in %s %s\n", __DATE__, __TIME__);
	fprintf(pFileFp, "frame ctrl create in 2014-07-02\n");
	g_tMasterCtrl.Ver(pFileFp);
    fprintf(pFileFp, "\n");

    return;
}

int32_t CFrameCtrl::Initialize(char *pProName, char *pConfigFile)
{
    //������
    strcpy(m_stConfig.m_szSvrName, pProName);
    LoadMcpIdx();
    if (ReadCfgFile(pConfigFile))
    {
        printf("ReadCfgFile %s failed!\n", pConfigFile);
        return -1;
    }

    if (LogInit())
    {
        printf("LogInit Failed!\n");
        return -1;
    }

    char baseStatLogName[300];
    snprintf(baseStatLogName, sizeof(baseStatLogName), "../log/%s_stat", m_stConfig.m_szSvrName);
    m_pStatistic = new CStatistic();
    if (m_pStatistic == NULL || m_pStatistic->Inittialize(baseStatLogName))
    {
        printf("Statistic Log Inittialize Failed!\n");
        return -1;
    }

    // ��mcp0��ʱ��ʼ��
    if (m_iMcpIdx != 0)
    {
        sleep(2);
    }

    if (m_tEpoll.Create(512))
    {
        printf("Create epoll failed!\n");
        return -1;
    }
    if (InitCodeQueue())
    {
        printf("InitCodeQueue Failed!\n");
        return -1;
    }
    m_tEpoll.Add(m_CCSToMeMQ->GetReadNotifyFD(), m_CCSToMeMQ->GetReadNotifyFD(), EPOLLIN);
    m_tEpoll.Add(m_SCCToMeMQ->GetReadNotifyFD(), m_SCCToMeMQ->GetReadNotifyFD(), EPOLLIN);


    if (m_stConfig.m_iBindCpu >= 0)
    {
        cpu_set_t mask;
        int32_t iCpuId = m_stConfig.m_iBindCpu;
        CPU_ZERO(&mask);
        CPU_SET(iCpuId, &mask);
        sched_setaffinity(0, sizeof(mask), &mask);
        printf("Main Thread set cpu affinity %d\n", iCpuId);
    }
	if(g_tMasterCtrl.Init(m_stConfig.m_szCtrlConf))
	{
		printf("Init MasterCtrl Failed!\n");
		return -1;
	}

    return 0;
}

int32_t CFrameCtrl::LoadMcpIdx()
{
    char *pos = NULL;
    pos = strstr(m_stConfig.m_szSvrName, "_mcp");
    if (NULL == pos || strlen(pos) == strlen("_mcp"))
    {
        m_iMcpIdx = 0;
    }
    else
    {
        pos += strlen("_mcp");
        m_iMcpIdx = atoi(pos);
    }
    return 0;
}

int32_t CFrameCtrl::InitCodeQueue()
{
    int ret;
    static CCodeQueueMutil ccs2meCodeQueueMutil, me2ccsCodeQueueMutil;
    static CCodeQueue scc2meCodeQueue, me2sccCodeQueue;

    ret = CCodeQueueMutil::CreateMQByFile(m_stConfig.m_szCCSToMeMQ, &ccs2meCodeQueueMutil);
    if (ret)
    {
        ERR("CCSToMeQueue init failed, ret[%d]!\n", ret);
        return -1;
    }
    ret = CCodeQueueMutil::CreateMQByFile(m_stConfig.m_szMeToCCSMQ, &me2ccsCodeQueueMutil);
    if (ret)
    {
        ERR("MeToCCSQueue init failed, ret[%d]!\n", ret);
        return -2;
    }

    if (CCodeQueue::CreateMQByFile(m_stConfig.m_szSCCToMeMQ, &scc2meCodeQueue))
    {
        ERR("SCC To Me pipe create failed!\n");
        return -3;
    }
    if (CCodeQueue::CreateMQByFile(m_stConfig.m_szMeToSCCMQ, &me2sccCodeQueue))
    {
        ERR("Me To SCC pipe create failed!\n");
        return -4;
    }

    if (ccs2meCodeQueueMutil.GetCodeQueueNum() != me2ccsCodeQueueMutil.GetCodeQueueNum()
        || m_iMcpIdx >= ccs2meCodeQueueMutil.GetCodeQueueNum())
    {
        ERR("CodeQueueMutil Error!\n");
        return -5;
    }

    m_CCSToMeMQ = ccs2meCodeQueueMutil.GetCodeQueue(m_iMcpIdx);
    m_MeToCCSMQ = me2ccsCodeQueueMutil.GetCodeQueue(m_iMcpIdx);
    m_SCCToMeMQ = &scc2meCodeQueue;
    m_MeToSCCMQ = &me2sccCodeQueue;
    return 0;
}

int32_t CFrameCtrl::LogInit()
{
    char szTmp[256];

    sprintf(szTmp, "if [ -f './.%s_ctrllog' ];then echo '.%s_ctrllog exist';else touch ./.%s_ctrllog;fi",
            m_stConfig.m_szSvrName, m_stConfig.m_szSvrName, m_stConfig.m_szSvrName);
    system(szTmp);
    sprintf(szTmp, "./.%s_ctrllog", m_stConfig.m_szSvrName);

    int32_t iErrNo = 0;
    bool bNewCreate = false;
    m_stConfig.m_pShmLog = (TShmLog *)CreateShm(ftok(szTmp, 'L'), sizeof(TShmLog), iErrNo, bNewCreate, true);
    if (!m_stConfig.m_pShmLog)
    {
        printf("Alloc shared memory for Log failed ErrorNo=%d, ERR=%s\n", iErrNo, strerror(iErrNo));
        return -1;
    }
    if (bNewCreate)
    {
        memset((char *)m_stConfig.m_pShmLog, 0, sizeof(TShmLog));
        m_stConfig.m_pShmLog->m_iLogLevel = RUNLOG;
    }
	i-3
    sprintf(m_stConfig.m_szLogFileBase, "../log/%s", m_stConfig.m_szSvrName);

    TLib_Log_LogInit(m_stConfig.m_szLogFileBase, m_stConfig.m_iMaxLogSize, m_stConfig.m_iMaxLogNum);

    return 0;
}

void CFrameCtrl::Log(int32_t iKey, int32_t iLevel, const char *sFormat, ...)
{
    int32_t iWriteLog = 0;
    if ((iKey != 0) && (m_stConfig.m_pShmLog->m_iLogKey == iKey))
    {
        iWriteLog = 1;
    }

	//日志的打印级别
    if (iLevel <= m_stConfig.m_pShmLog->m_iLogLevel)
    {
        iWriteLog = 1;
    }

    if (!iWriteLog)
    {
        return;
    }

    char buf[1024];
    int32_t len = 0;
    switch (iLevel)
    {
        case KEYLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[KEYLOG]");
            break;
        case NOLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[NOLOG]");
            break;
        case ERRORLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[ERRORLOG]");
            break;
        case RUNLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[RUNLOG]");
            break;
        case DEBUGLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[DEBUGLOG]");
            break;
        default:
            len += snprintf(buf, sizeof(buf) - 1, "[UNKOWN%d]", iLevel);
            break;
    }
    va_list ap;
    va_start(ap, sFormat);
    len += vsnprintf(buf + len, sizeof(buf) - len - 1, sFormat, ap);
    va_end(ap);
    //���û���Ի��н�����׷��һ������
    if (len > 0 && buf[len - 1] != '\n')
    {
        strcat(buf, "\n");
    }
    TLib_Log_WriteLog(m_stConfig.m_szLogFileBase, m_stConfig.m_iMaxLogSize, m_stConfig.m_iMaxLogNum, buf);
    return;
}

void CFrameCtrl::FrequencyLog(int32_t iKey, int32_t iLevel, const char *sFormat, ...)
{
    int32_t iWriteLog = 0;
    if ((iKey != 0) && (m_stConfig.m_pShmLog->m_iLogKey == iKey))
    {
        iWriteLog = 1;
    }

    //���μ������Ҫ��ļ���
    if (iLevel <= m_stConfig.m_pShmLog->m_iLogLevel)
    {
        iWriteLog = 1;
    }

    if (!iWriteLog)
    {
        return;
    }

    static time_t last = m_tNow.tv_sec;
    static int32_t frq = 0;
    time_t now = m_tNow.tv_sec;
    if (now - last >= 5)
    {
        last = now;
        frq = 0;
    }
    if (frq > 10)
    {
        return;
    }
    frq++;

    char buf[1024];
    int32_t len = 0;
    switch (iLevel)
    {
        case KEYLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[KEYLOG]");
            break;
        case NOLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[NOLOG]");
            break;
        case ERRORLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[ERRORLOG]");
            break;
        case RUNLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[RUNLOG]");
            break;
        case DEBUGLOG:
            len += snprintf(buf, sizeof(buf) - 1, "[DEBUGLOG]");
            break;
        default:
            len += snprintf(buf, sizeof(buf) - 1, "[UNKOWN%d]", iLevel);
            break;
    }
    va_list ap;
    va_start(ap, sFormat);
    len += vsnprintf(buf + len, sizeof(buf) - len - 1, sFormat, ap);
    va_end(ap);
    //���û���Ի��н�����׷��һ������
    if (len > 0 && buf[len - 1] != '\n')
    {
        strcat(buf, "\n");
    }
    TLib_Log_WriteLog(m_stConfig.m_szLogFileBase, m_stConfig.m_iMaxLogSize, m_stConfig.m_iMaxLogNum, buf);
    return;
}

int32_t CFrameCtrl::WriteStat()
{
    time_t tNow = m_tNow.tv_sec;
    static time_t siLastStatTime = time(NULL);

    if (likely(tNow - siLastStatTime < (time_t)m_stConfig.m_iStatTimeGap))
    {
        return 0;
    }

    m_pStatistic->WriteToFile();
    m_pStatistic->ClearStat();

    siLastStatTime = tNow;
    return 0;
}

int32_t CFrameCtrl::ReadCfgFile(char *szCfgFile)
{
    int32_t iRet = 0;

    strncpy(m_stConfig.m_szCfgFileName, szCfgFile, sizeof(m_stConfig.m_szCfgFileName) - 1);
    iRet = TLib_Cfg_GetConfig(szCfgFile,
                              "CCSToMeMQ", CFG_STRING, m_stConfig.m_szCCSToMeMQ, "", sizeof(m_stConfig.m_szCCSToMeMQ),
                              "MeToCCSMQ", CFG_STRING, m_stConfig.m_szMeToCCSMQ, "", sizeof(m_stConfig.m_szMeToCCSMQ),

                              "SCCToMeMQ", CFG_STRING, m_stConfig.m_szSCCToMeMQ, "", sizeof(m_stConfig.m_szSCCToMeMQ),
                              "MeToSCCMQ", CFG_STRING, m_stConfig.m_szMeToSCCMQ, "", sizeof(m_stConfig.m_szMeToSCCMQ),

                              "BindCpu", CFG_INT, &(m_stConfig.m_iBindCpu), -1,

							  "CtrlConfigure", CFG_STRING, m_stConfig.m_szCtrlConf, "", sizeof(m_stConfig.m_szCtrlConf),

                              "StatisticGap", CFG_INT, &(m_stConfig.m_iStatTimeGap), 300,
                              "MaxLogSize", CFG_INT, &(m_stConfig.m_iMaxLogSize), 10000000,
                              "MaxLogNum", CFG_INT, &(m_stConfig.m_iMaxLogNum), 10,

                              NULL);

    m_stConfig.Print();

    return 0;
}

int32_t CFrameCtrl::SetRunFlag(int32_t iRunFlag)
{
    m_iRunFlag = iRunFlag;
    return 0;
}

int32_t CFrameCtrl::WaitData()
{
    long long llKey;
    unsigned int unEvents;
    char szTempBuf[1024];

    long long ccsToMeFd = m_CCSToMeMQ->GetReadNotifyFD();
    long long sccToMeFd = m_SCCToMeMQ->GetReadNotifyFD();

    if (0 != m_CCSToMeMQ->GetCodeLength() || 0 != m_SCCToMeMQ->GetCodeLength())
    {
        m_tEpoll.Wait(0);
    }
    else
    {
        m_tEpoll.Wait(1);
    }

    while (m_tEpoll.GetEvents(llKey, unEvents))
    {
        if (ccsToMeFd == llKey	|| sccToMeFd == llKey)
        {
            //���֪ͨ
            if (EPOLLIN == unEvents)
            {
                read((int32_t)llKey, szTempBuf, sizeof(szTempBuf));
            }
        }
    }

    return 0;
}

int32_t CFrameCtrl::TimeTick()
{
    gettimeofday(&m_tNow, NULL);

	g_tMasterCtrl.TimeTick(&m_tNow);
    //дͳ��
    WriteStat();
    return 0;
}

int32_t CFrameCtrl::Run()
{
    TLib_Log_LogMsg("===== Server Started. =====\n");
    printf("Server Start Success!\n");

    int32_t iCodeLength = 0;
    while (1)
    {
        if (unlikely(Flag_Exit == m_iRunFlag))
        {
            TLib_Log_LogMsg("Server Exit!\n");
            return 0;
        }

        if (unlikely(Flag_ReloadCfg == m_iRunFlag))
        {
            ReadCfgFile(m_stConfig.m_szCfgFileName);
            m_iRunFlag = 0;
        }

        TimeTick();

        int32_t iHaveMQCode = 0;
        int32_t iDoMsgCnt = 0;
        int32_t iDoMsgLen = 0;
        char *pMsgBuff = NULL;

        //CCS
        short sCQID = 0;
        while ((iDoMsgLen < 100 * 1024) && (iDoMsgCnt < 100))
        {
            int32_t iGetPtrLen = m_CCSToMeMQ->GetHeadCodePtr(sCQID, pMsgBuff, &iCodeLength);
            if (iGetPtrLen < 0)
            {
                iCodeLength = FRAME_MAX_BUFF_LEN;
                int32_t iRet = m_CCSToMeMQ->GetHeadCode(sCQID, m_pRecvBuffer, &iCodeLength);
                if (iRet < 0 || iCodeLength <= 0)
                {
                    break;
                }
                pMsgBuff = m_pRecvBuffer;
            }
            else if (iGetPtrLen == 0)
            {
                break;
            }

            iDoMsgCnt++;
            iDoMsgLen += iCodeLength;

            //
            g_tMasterCtrl.OnReqMessage((TMQHeadInfo *)pMsgBuff, pMsgBuff + sizeof(TMQHeadInfo), iCodeLength - sizeof(TMQHeadInfo));

            if (iGetPtrLen > 0)
            {
                m_CCSToMeMQ->SkipHeadCodePtr(sCQID);
            }

            iHaveMQCode = 1;
        }

        //SCC
        iDoMsgCnt = 0;
        iDoMsgLen = 0;
        while ((iDoMsgLen < 100 * 1024) && (iDoMsgCnt < 100))
        {
            int32_t iGetPtrLen = m_SCCToMeMQ->GetHeadCodePtr(pMsgBuff, &iCodeLength);
            if (iGetPtrLen < 0)
            {
                iCodeLength = FRAME_MAX_BUFF_LEN;
                int32_t iRet = m_SCCToMeMQ->GetHeadCode(m_pRecvBuffer, &iCodeLength);
                if (iRet < 0 || iCodeLength <= 0)
                {
                    break;
                }
                pMsgBuff = m_pRecvBuffer;
            }
            else if (iGetPtrLen == 0)
            {
                break;
            }

            iDoMsgCnt++;
            iDoMsgLen += iCodeLength;

            //
            g_tMasterCtrl.OnRspMessage((TMQHeadInfo *)pMsgBuff, pMsgBuff + sizeof(TMQHeadInfo), iCodeLength - sizeof(TMQHeadInfo));

            if (iGetPtrLen > 0)
            {
                m_SCCToMeMQ->SkipHeadCodePtr();
            }

            iHaveMQCode = 1;
        }

        //
        if (!iHaveMQCode)
        {
            WaitData();
        }
    }

    return 0;
}

int32_t CFrameCtrl::SendReq(uint32_t uIp, uint32_t uPort, char *pReq, uint32_t reqLen)
{
    TMQHeadInfo *pstMQHeadInfo = (TMQHeadInfo *)(pReq - sizeof(TMQHeadInfo));
    INIT_MQHEAD(pstMQHeadInfo);

    pstMQHeadInfo->m_ucCmd = TMQHeadInfo::CMD_DATA_TRANS;
    pstMQHeadInfo->m_unClientIP = htonl(uIp);
    pstMQHeadInfo->m_usClientPort = uPort;
    pstMQHeadInfo->m_ucDataType = TMQHeadInfo::DATA_TYPE_TCP;

    memcpy(pstMQHeadInfo->m_szDstMQ, SCC_MQ, sizeof(SCC_MQ));
    pstMQHeadInfo->m_tTimeStampSec = m_tNow.tv_sec;
    pstMQHeadInfo->m_tTimeStampuSec = m_tNow.tv_usec;

    return m_MeToSCCMQ->AppendOneCode((char *)pstMQHeadInfo, reqLen + sizeof(TMQHeadInfo));
}

int32_t CFrameCtrl::SendRsp(TMQHeadInfo *pMQHeadInfo, char *pRsp, uint32_t rspLen)
{
	TMQHeadInfo *rspMQHead = NULL;

	if(pRsp != NULL && 0 != rspLen)
	{
		rspMQHead = (TMQHeadInfo *)(pRsp - sizeof(TMQHeadInfo));
	}
	else
	{
		rspMQHead = (TMQHeadInfo *)m_pSendBuffer;
		pMQHeadInfo->m_ucCmd = TMQHeadInfo::CMD_CCS_NOTIFY_DISCONN;
	}

	memcpy(rspMQHead, pMQHeadInfo, sizeof(TMQHeadInfo));
	rspMQHead->m_tTimeStampSec = m_tNow.tv_sec;
	rspMQHead->m_tTimeStampuSec = m_tNow.tv_usec;

    return m_MeToCCSMQ->AppendOneCode((char*)rspMQHead, rspLen + sizeof(TMQHeadInfo));
}

