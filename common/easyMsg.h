/*
 * easyMsg.h
 *
 *  Created on: 2015年8月25日
 *      Author: millerzhao
 */

#ifndef EASYMSG_H_
#define EASYMSG_H_

#include <memory.h>
#include <stdint.h>
namespace EasyMQ
{
	struct Asn20Msg ;

    struct Msg
    {
        enum MsgType
        {
            MSG_TYPE_REQ_INIT_TOPIC = 0x10,
            MSG_TYPE_RESP_INIT_TOPIC = 0x20,

            MSG_TYPE_REQ_PUBLISH = 0x11,
            MSG_TYPE_RESP_PUBLISH = 0x21,

            MSG_TYPE_REQ_SUBSCRIBE = 0x12,
            MSG_TYPE_RESP_SUBSCRIBE = 0x22,

            MSG_TYPE_REQ_ECHO = 0x31,
            MSG_TYPE_RESP_ECHO = 0x32
        };

        enum MsgRet
        {
            MSG_RET_SUCC = 0xe0,
            MSG_RET_FAIL = 0xe1
        };

        MsgType type;
        MsgRet retCode;
        uint32_t uBufLen;//最后cBuf的大小
        char cBuf[0];
		int32_t getLen()
		{
			return sizeof(type) + sizeof(retCode) + sizeof(uBufLen) + uBufLen;
		}
	};

	struct Asn20Msg
    {
        uint32_t msgTag;
        uint32_t msgLen; //实际的长度，不包含包头的长度
		struct Msg msg;
    };
}



#endif /* EASYMSG_H_ */
