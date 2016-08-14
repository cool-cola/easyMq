#ifndef _DEFINE_TASK_MANAGER_HEADER_
#define _DEFINE_TASK_MANAGER_HEADER_

#include "comm_mysql.hpp"
#include "task_detector.h"
#include "task_scheduler.h"

class TiXmlElement;

namespace NS_LotteryBons
{

// ���������
class CTaskManager
{
public:
	CTaskManager();
	~CTaskManager();

	int32_t Init(const char* pszConf);

	int32_t OnTick(const timeval* ptTick);

	// ִ����������IDΪ-1ʱ�������
	int32_t DoTask(CTask* ptTask);

protected:
	// ������������
	int32_t OnTaskSchedComplete();
	// �������⵽�ɵ���ִ��
	int32_t OnTaskDetectFound();

	// ��������������ݿ⣬��������ID�͵�����Ϣ
	int32_t AddTaskToDB(CTask* ptTask);
	// ��������뵽���
	int32_t AddTaskToDetector(CTask* ptTask);
	
	//��DB�ж�ȡδ��ɵ�����
	int32_t LoadTaskFromDB();
	// ��������뵽����
	int32_t AddTaskToScheduler(CTask* ptTask);

	int32_t InitMysql(TiXmlElement* ptXmlNode);
	// ���������mysql��Ϣ
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
