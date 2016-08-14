#ifndef _DEFINE_TASK_DETECTOR_HEADER_
#define _DEFINE_TASK_DETECTOR_HEADER_

#include <list>
#include "base_detector.h"

class TiXmlElement;

namespace NS_LotteryBons
{

// ������
// OnTickʱ�������е�CTaskTimer����Ƿ����������ִ��
class CTaskDetector
{
public:
	CTaskDetector();
	~CTaskDetector();

	int32_t Init(TiXmlElement* ptXmlEle);
	int32_t Reload(TiXmlElement* ptXmlEle);

	int32_t OnTick(const timeval* ptTick);

	// ��������뵽�����
	// ����ֵ��
	//		1 : �����Ѿ����
	//		0 : �ɹ���������
	//		-1: ����ʧ��
	int32_t Push(CTask* ptTask);
	int32_t Pop(CTask* ptTask);

	int32_t MakeTimerFromXml(TiXmlElement *ptXmlNode, CTask *pTask, int32_t taskType);
	CBaseDetector *GetDetectorFromTask(const CTask &tTask);

protected:
	void DestroyDetector(bool bClearAll = false);

	//���������ʱ�Ӽ��
	int32_t m_iUpdateTaskInterval;

	std::list<CBaseDetector*> m_stdNewTaskDetectorList;
	std::map<int32_t,CBaseDetector*> m_stdTaskDetectorMap;
	std::list<CTask> m_TaskList;

	std::map<int32_t, int32_t> m_stdTaskRetryTimes; //���Դ���

	std::map<std::pair<std::string,std::string>,std::string> m_stdLotteryConfName;
};

}//namespace NS_LotteryBons

#endif//_DEFINE_TASK_DETECTOR_HEADER_
