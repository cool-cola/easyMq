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
        uint32_t msgLen;
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
            MSG_TYPE_RESP_SUBSCRIBE = 0x22
        };

        enum MsgRet
        {
            MSG_RET_SUCC = 0xe0,
            MSG_RET_FAIL = 0xe1
        };

        MsgType type;
        MsgRet retCode;
        uint32_t uBufLen;
        char topic[32];
        char cBuf[0];

        void toAsn(struct Asn20Msg &stAsnMsg)
        {
            stAsnMsg.msgLen = this->uBufLen + sizeof(struct Msg) - 4;//扣除cBuf大小
            memcpy(stAsnMsg.cBuf, stAsnMsg.cBuf, uBufLen);
        }

        void fromAsn(const struct Asn20Msg &stAsnMsg)
        {
            //TODO: 需要考虑到cBuf大小
            struct Msg *pMsg = (struct Msg*)stAsnMsg.cBuf;
            this->type = pMsg->type;
            this->retCode = pMsg->retCode;
            memcpy(this->topic, pMsg->topic, sizeof(this->topic));
        }
    };
}



#endif /* EASYMSG_H_ */
