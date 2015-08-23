/*  
 *  定义协议扩展字段
 */

#ifndef _ASN_EXTBLOB_H_
#define _ASN_EXTBLOB_H_

// 建议格式:生成消息的模块_协议_扩展段名称


// cmem access get req消息的扩展段字段定义

// get时检查数据的长度有无变化
#define ACCESS_GET_CHECK_VALUE_LEN	"get_check_len"
// get时是否需要返回数据
#define ACCESS_GET_CHECK_TIMESTAMP	"get_check_timestamp"



// cache setrsp消息的扩展段字段定义

// set的value的长度
#define CACHE_SET_VALUE_LEN		"value_len"
// set的value : 成功时为新的value，失败时为set中的value
#define CACHE_SET_VAULE_ECHO	"value_echo"




#endif
