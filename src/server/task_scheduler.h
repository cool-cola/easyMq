#ifndef _DEFINE_TASK_SCHEDULER_HEADER_
#define _DEFINE_TASK_SCHEDULER_HEADER_

#include <vector>
#include <list>
#include <map>

class TiXmlElement;

namespace NS_LotteryBons
{

struct TaskInfo
{
	TaskInfo():iSuccWorker(0),iFailWorker(0) {}
	int32_t iSuccWorker;
	int32_t iFailWorker;
	CTask tTask;
	std::vector<WORKERUID> stdScheVec; // 任务调度索引
};

// 任务调度器
class CTaskScheduler
{
public:
	CTaskScheduler();
	~CTaskScheduler();

	int32_t Init(TiXmlElement* ptXmlEle);

	// 轮询所有任务调度情况
	// 返回值：0 - 没有调度完成的任务，>0 - 当次调度完成的任务数，<0 - 失败
	int32_t OnTick(const timeval* ptTick);

	// 将任务加入到调度列表
	int32_t Push(CTask* ptTask);
	// 将执行完成任务取回
	int32_t Pop(CTask* ptTask);
	// 终止正在调度的任务
	int32_t Terminate(CTask* ptTask);

protected:
	int32_t ScheTask(TaskInfo& tTaskInfo);
	int32_t CheckTask(TaskInfo& tTaskInfo);

	int32_t SaveScheduledTask();
	int32_t LoadScheduledTask();
	int32_t SaveScheInfo();
	int32_t LoadScheInfo();

	int32_t m_iUpdateTaskInterval;
	int32_t m_iMaxTaskTime;
	std::string m_strScheTaskFile;
	std::string m_strScheInfoFile;//

	std::list<TaskInfo> m_stdTaskList;// 正在调度的任务
	std::list<TaskInfo> m_stdSchedTaskList;// 调度已完成，待manager取回的任务
	std::map<int32_t,std::vector<WORKERUID> > m_stdSchedTaskInfoMap;// 完成了调度的任务存档信息，用于下次调度到同一台机器上，暂定为保存3000条
};

}//namespace NS_LotteryBons

#endif//_DEFINE_TASK_SCHEDULER_HEADER_
