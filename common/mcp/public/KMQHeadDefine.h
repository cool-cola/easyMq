#ifndef _KMQHEADDEFINE_H__
#define _KMQHEADDEFINE_H__
#include <sys/types.h>

enum KCCD_CMD_TYPE
{
	CMD_DATA_TRANS = 0, 	//数据传输包, 通用
	CMD_CCD_NOTIFY_DISCONN, //通知断开连接, CCD使用
};


/* MQ消息头的连接类型 */
enum KCCD_CONN_TYPE
{
	DATA_TYPE_TCP = 0,
	DATA_TYPE_UDP
};

/* MQ消息接口 */
typedef struct
{
	//通用部分
	u_int8_t m_ucCmd;				/*命令，见KCCD_CMD_TYPE 定义*/
	u_int8_t m_ucConnType;        	/*连接类型，UDP或者TCP */

	// SCC/UDP 使用定位部分
	u_int16_t m_usClientPort;		/*客户端端口,本地字节序*/
	u_int32_t m_unClientIP;			/*客户端IP地址,网络字节序*/
	u_int32_t m_uiServPortID;		/* 服务器侦听端口Index */


	//CCS使用定位部分
	int32_t m_iSuffix;             	/*socketNode下标*/
	int32_t m_FlowID;            	/* socketNode对应的Flow值*/

	//管道时间戳记,用来得知消息在管道中的留存时间
	u_int32_t  m_tTimeStampSec;
	u_int32_t  m_tTimeStampuSec;
}KMQHeadInfo;


#endif

