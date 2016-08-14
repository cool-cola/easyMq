#ifndef _DEFINE_TASK_MANAGER_HEADER_
#define _DEFINE_TASK_MANAGER_HEADER_

#include "comm_mysql.hpp"
#include "task_detector.h"
#include "task_scheduler.h"

class TiXmlElement;

namespace NS_LotteryBons
{

// 任务管理器
class CTaskManager
{
public:
	CTaskManager();
	~CTaskManager();

	int32_t Init(const char* pszConf);

	int32_t OnTick(const timeval* ptTick);

	// 执行任务，任务ID为-1时添加任务
	int32_t DoTask(CTask* ptTask);

protected:
	// 有任务调度完成
	int32_t OnTaskSchedComplete();
	// 有任务检测到可调度执行
	int32_t OnTaskDetectFound();

	// 检查后将任务加入数据库，生成任务ID和调度信息
	int32_t AddTaskToDB(CTask* ptTask);
	// 将任务加入到检查
	int32_t AddTaskToDetector(CTask* ptTask);
	
	//从DB中读取未完成的任务
	int32_t LoadTaskFromDB();
	// 将任务加入到调度
	int32_t AddTaskToScheduler(CTask* ptTask);

	int32_t InitMysql(TiXmlElement* ptXmlNode);
	// 更新任务的mysql信息
	int32_t UpdateMysql(CTask* ptTask);

	std::string		m_strDBIP;
	std::string		m_strDBUser;
	std::string		m_strDBPasswd;
	std::string		m_strDBName;
	std::string		m_strTableName;
	std::string		m_strCharSet;
	uint32_t		m_uDBPort;

	char m_szDbCmd[2048];
	CMysqlConn m_tDbConn;

	CTaskScheduler m_tTaskScheduler;
	CTaskDetector m_tTaskDetector;
};

}//namespace NS_LotteryBons

#endif//_DEFINE_TASK_MANAGER_HEADER_
