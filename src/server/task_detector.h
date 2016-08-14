#ifndef _DEFINE_TASK_DETECTOR_HEADER_
#define _DEFINE_TASK_DETECTOR_HEADER_

#include <list>
#include "base_detector.h"

class TiXmlElement;

namespace NS_LotteryBons
{

// 任务检测
// OnTick时驱动所有的CTaskTimer检查是否有任务可以执行
class CTaskDetector
{
public:
	CTaskDetector();
	~CTaskDetector();

	int32_t Init(TiXmlElement* ptXmlEle);
	int32_t Reload(TiXmlElement* ptXmlEle);

	int32_t OnTick(const timeval* ptTick);

	// 将任务加入到检测器
	// 返回值：
	//		1 : 任务已经完成
	//		0 : 成功加入检测中
	//		-1: 加入失败
	int32_t Push(CTask* ptTask);
	int32_t Pop(CTask* ptTask);

	int32_t MakeTimerFromXml(TiXmlElement *ptXmlNode, CTask *pTask, int32_t taskType);
	CBaseDetector *GetDetectorFromTask(const CTask &tTask);

protected:
	void DestroyDetector(bool bClearAll = false);

	//更新任务的时钟间隔
	int32_t m_iUpdateTaskInterval;

	std::list<CBaseDetector*> m_stdNewTaskDetectorList;
	std::map<int32_t,CBaseDetector*> m_stdTaskDetectorMap;
	std::list<CTask> m_TaskList;

	std::map<int32_t, int32_t> m_stdTaskRetryTimes; //重试次数

	std::map<std::pair<std::string,std::string>,std::string> m_stdLotteryConfName;
};

}//namespace NS_LotteryBons

#endif//_DEFINE_TASK_DETECTOR_HEADER_
