#ifndef _DEFINE_BASE_DETECTOR_HEADER_
#define _DEFINE_BASE_DETECTOR_HEADER_

namespace NS_LotteryBons
{

class CBaseDetector
{
#define MAX_ERR_MSG_LENGTH	999
public:
	// 将需要检测的任务信息传入
	CBaseDetector(const CTask& tTask){m_szErrMsg[MAX_ERR_MSG_LENGTH]=0;}
	virtual ~CBaseDetector(){}

	virtual int32_t InitDetector(const char* pszConf){return -1;}

	// 检查detector持有的任务是否有变化
	// 返回值：
	//		0 : 任务没有变化
	//		>0: 任务可以调度了，tTask返回可调度的任务
	//		<0: 出错，下个tick重试
	virtual int32_t Check(CTask* ptTask){return -1;}

	// 将任务调度后继续传入
	// 返回值：
	//		0 : 任务完成，可以删除detector了
	//		1 : 任务未完成，继续加入检测中，需保留detector
	//		-1: 出错，继续保留到detector
	virtual int32_t OnTaskDone(const CTask* ptTask){return -1;}

	const char* GetErrMsg(){return m_szErrMsg;}

protected:
	char	m_szErrMsg[MAX_ERR_MSG_LENGTH+1];
};


}//NS_LotteryBons

#endif//_DEFINE_BASE_DETECTOR_HEADER_
