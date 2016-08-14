#ifndef _DEFINE_BASE_DETECTOR_HEADER_
#define _DEFINE_BASE_DETECTOR_HEADER_

namespace NS_LotteryBons
{

class CBaseDetector
{
#define MAX_ERR_MSG_LENGTH	999
public:
	// ����Ҫ����������Ϣ����
	CBaseDetector(const CTask& tTask){m_szErrMsg[MAX_ERR_MSG_LENGTH]=0;}
	virtual ~CBaseDetector(){}

	virtual int32_t InitDetector(const char* pszConf){return -1;}

	// ���detector���е������Ƿ��б仯
	// ����ֵ��
	//		0 : ����û�б仯
	//		>0: ������Ե����ˣ�tTask���ؿɵ��ȵ�����
	//		<0: �����¸�tick����
	virtual int32_t Check(CTask* ptTask){return -1;}

	// ��������Ⱥ��������
	// ����ֵ��
	//		0 : ������ɣ�����ɾ��detector��
	//		1 : ����δ��ɣ������������У��豣��detector
	//		-1: ��������������detector
	virtual int32_t OnTaskDone(const CTask* ptTask){return -1;}

	const char* GetErrMsg(){return m_szErrMsg;}

protected:
	char	m_szErrMsg[MAX_ERR_MSG_LENGTH+1];
};


}//NS_LotteryBons

#endif//_DEFINE_BASE_DETECTOR_HEADER_
