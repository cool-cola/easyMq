#ifndef _DEFINE_WORKER_MANAGER_HEADER_
#define _DEFINE_WORKER_MANAGER_HEADER_

namespace NS_LotteryBons
{

// 以pair<IP,PORT>作为worker的UID
typedef std::pair<uint32_t,uint32_t> WORKERUID;

enum WORKER_STATE
{
	em_init_state = 0,
	em_dead_state,
	em_idle_state,
	em_work_doing,
	em_work_finished
};

struct WorkerInfo
{
	WorkerInfo()
		:	iState(em_init_state),iLastAckTime(0),iTaskBeginTime(0),iTaskEndTime(0),uAllocTimes(0)
	{
	}

	int32_t iState;//WORKER_STATE
	int32_t iLastAckTime;// 最后一次应答时间
	int32_t iTaskBeginTime;// 本次任务开始时间
	int32_t iTaskEndTime;// 本次任务结束时间
	uint32_t uAllocTimes;// 被分配出去的次数
	CTask tTask;
};

class CWorkerManager
{
public:
	CWorkerManager();
	~CWorkerManager();

	int32_t Init(const char* pszConf);

	int32_t OnTick(const timeval* ptTick);

	WORKERUID GetWorker();
	WORKERUID GetIdleWorker();
	const WorkerInfo& GetWorkerInfo(const WORKERUID& workerUID) const;
	int32_t ResetWorker(const WORKERUID& workerUID);

	int32_t ReqHeartBeat(const WORKERUID& workerUID);
	int32_t ReqQueryTask(const WORKERUID& workerUID);
	int32_t ReqSetTask(const WORKERUID& workerUID, const CTask* ptTask);
	int32_t ReqStopTask(const WORKERUID& workerUID, const CTask* ptTask);

	int32_t RspHeartBeat(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg);
	int32_t RspQueryTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg);
	int32_t RspSetTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg);
	int32_t RspStopTask(TMQHeadInfo *pMQHeadInfo, NS_LotteryBons::LotteryBonsMessage* ptRspMsg);

protected:
	int32_t m_iHeartBeatInterval;
	int32_t m_iAliveToDeadTime;
	int32_t m_iSeqID;

	std::map<WORKERUID,WorkerInfo> m_stdWorkerMap;
};

}//namespace NS_LotteryBons

#endif//_DEFINE_TASK_MANAGER_HEADER_
