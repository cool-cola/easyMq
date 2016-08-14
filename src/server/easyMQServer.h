/*
 * easyMQServer.h
 *
 *  Created on: 2015年8月27日
 *      Author: millerzhao
 */
#include "easyMsg.h"
#include <string>
namespace EasyMQ
{
    class EasyMQAgent
    {
    public:
        uint32_t ipAddr;
        uint32_t port;
    };

    class EasyMQServer
    {
        public:
            int32_t initTopic(const char * topic, const EasyMQAgent &);

            int32_t transferMsg(const struct Msg *pMsg);
        protected:
            int32_t sendMsgToAgent(const Msg *pMsg, const EasyMQAgent &agent);

    };

    //extern EasyMQServer g_easyMQServer;
}



