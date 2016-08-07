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
    struct Asn20Msg
    {
        uint32_t msgTag;
        uint32_t msgLen; //实际的长度，不包含包头的长度
        char cBuf[0];
    };

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

        void toAsn(struct Asn20Msg &stAsnMsg)
        {
        	//计算变长结构体数组的大小
            stAsnMsg.msgLen = sizeof(type) + sizeof(retCode) + sizeof(uBufLen) + uBufLen;
            memcpy(stAsnMsg.cBuf, (char *)this, stAsnMsg.msgLen);
        }

        void fromAsn(const struct Asn20Msg &stAsnMsg)
        {
            memcpy((char *)this, stAsnMsg.cBuf, stAsnMsg.msgLen);
        }
    };
}



#endif /* EASYMSG_H_ */
