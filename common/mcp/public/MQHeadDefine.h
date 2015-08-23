#ifndef _MQHEADDEFINE_H__
#define _MQHEADDEFINE_H__

#include <time.h>
#include <sys/types.h>

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN	(16*1024*1024)
#endif

#define MQ_NAME_LEN	(8)
#define MQ_ECHO_DATA_LEN	(12)

//系统保留的MQ名称
#define NEXT_MQ		"NEXT"
#define CCS_MQ		"CCS"
#define SCC_MQ		"SCC"

//所有管道的通用头部
#pragma pack(1)
typedef struct
{
	u_int8_t m_ucCmd;						/*命令*/
	enum
	{
		//通用
		CMD_DATA_TRANS = 0,			//数据传输包

		//CCS使用
		CMD_CCS_NOTIFY_DISCONN,		//通知断开连接

		//SCC使用，S->C方向
		CMD_REQ_SCC_CONN,			//请求连接
		CMD_REQ_SCC_CLOSE,			//请求关闭
		//SCC使用，C->S方向
		CMD_SCC_RSP_CONNSUCC,		//通知连接成功
		CMD_SCC_RSP_CONNFAIL,		//通知连接失败
		CMD_SCC_RSP_DISCONN,		//通知连接断裂	

		//内部使用
		CMD_NEW_TCP_SOCKET,
		CMD_NEW_UDP_SOCKET,

		//CCS使用
		CMD_CCS_MCP_NOTIFY_DISCONN,	//特殊协议关闭连接，thread收到后广播给所有线程
		CMD_CCS_THREAD_NOTIFY_DISCONN,	//其它thread发出关闭连接
	};	
	u_int8_t m_ucDataType;
	enum
	{
		DATA_TYPE_TCP = 0,
		DATA_TYPE_UDP
	};	

	// SCC/UDP 使用定位部分	
	u_int16_t m_usClientPort;		/*客户端端口,本地字节序*/	
	u_int32_t m_unClientIP;			/*客户端IP地址,网络字节序*/

	//CCS使用定位部分
	int32_t m_iSuffix;              /*唯一标记*/
	char m_szEchoData[MQ_ECHO_DATA_LEN];	/*回射数据,使用者必须回带*/

	char m_szSrcMQ[MQ_NAME_LEN];	/*本次来源MQ,字串类型,0结尾*/
	u_int16_t m_usSrcListenPort;	/*本次来源的监听端口*/
	u_int16_t m_usFromCQID;			/*来源CQ管道*/

	char m_szDstMQ[MQ_NAME_LEN];	/*本次目标MQ,字串类型,0结尾*/	
	u_int16_t m_usClosePort;			/*2013-03-14 第三方消息关闭端口连接*/
	u_int16_t m_usRserve3;			/*保留*/
	
	//管道时间戳记,用来得知消息在管道中的留存时间
	u_int32_t  m_tTimeStampSec;
	u_int32_t  m_tTimeStampuSec;
} TMQHeadInfo;
#pragma pack()

#define INIT_MQHEAD(PMQHD)	{\
	memset((PMQHD),0,sizeof(TMQHeadInfo));\
	(PMQHD)->m_ucCmd = TMQHeadInfo::CMD_DATA_TRANS;\
	(PMQHD)->m_iSuffix = -1;\
}

#define PRINT_MQHEAD(PMQHD)	{\
	printf("=== PRINT MQHEAD ===\n");\
	printf("m_ucCmd	%d\n",(int)PMQHD->m_ucCmd);\
	printf("m_ucDataType	%d\n",(int)PMQHD->m_ucDataType);\
	printf("m_usClientPort	%d\n",(int)PMQHD->m_usClientPort);\
	printf("m_unClientIP	%d\n",(int)PMQHD->m_unClientIP);\
	printf("m_iSuffix	%d\n",(int)PMQHD->m_iSuffix);\
	printf("m_usSrcListenPort	%d\n",(int)PMQHD->m_usSrcListenPort);\
	printf("m_usFromCQID	%d\n",(int)PMQHD->m_usFromCQID);\
	printf("m_tTimeStampSec	%d\n",(int)PMQHD->m_tTimeStampSec);\
	printf("m_tTimeStampuSec	%d\n",(int)PMQHD->m_tTimeStampuSec);\
	printf("=== END PRINT ===\n");\
}
#endif

