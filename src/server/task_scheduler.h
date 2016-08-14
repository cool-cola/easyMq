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
	std::vector<WORKERUID> stdScheVec; // �����������
};

// ���������
class CTaskScheduler
{
public:
	CTaskScheduler();
	~CTaskScheduler();

	int32_t Init(TiXmlElement* ptXmlEle);

	// ��ѯ��������������
	// ����ֵ��0 - û�е�����ɵ�����>0 - ���ε�����ɵ���������<0 - ʧ��
	int32_t OnTick(const timeval* ptTick);

	// ��������뵽�����б�
	int32_t Push(CTask* ptTask);
	// ��ִ���������ȡ��
	int32_t Pop(CTask* ptTask);
	// ��ֹ���ڵ��ȵ�����
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

	std::list<TaskInfo> m_stdTaskList;// ���ڵ��ȵ�����
	std::list<TaskInfo> m_stdSchedTaskList;// ��������ɣ���managerȡ�ص�����
	std::map<int32_t,std::vector<WORKERUID> > m_stdSchedTaskInfoMap;// ����˵��ȵ�����浵��Ϣ�������´ε��ȵ�ͬһ̨�����ϣ��ݶ�Ϊ����3000��
};

}//namespace NS_LotteryBons

#endif//_DEFINE_TASK_SCHEDULER_HEADER_
