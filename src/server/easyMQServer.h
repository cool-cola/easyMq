/*
 * easyMQServer.h
 *
 *  Created on: 2015年8月27日
 *      Author: millerzhao
 */
#include "easyMsg.h"
#include <string>
#include <map>
#include <set>
namespace EasyMQ
{
    class EasyMQAgent
    {
    public:
        uint32_t ipAddr;
        uint32_t port;
		int32_t socketSuffix;
		//set必须要重载这个操作符
		bool operator<(const EasyMQAgent &agent)const
		{
			if(ipAddr < agent.ipAddr)
			{
				return true;
			}
			else if(ipAddr == agent.ipAddr)
			{
				return port < agent.port;
			}
			return false;
		}
    };

    class EasyMQServer
    {
        public:
            int32_t initTopic(const std::string & topic, const EasyMQAgent &);

            int32_t transferMsg(const struct Msg *pMsg);
        protected:
            int32_t sendMsgToAgent(const Msg *pMsg, const EasyMQAgent &agent);

			void TimeTick(timeval *t);
    };

    //extern EasyMQServer g_easyMQServer;
}


extern std::map<std::string, std::set<EasyMQ::EasyMQAgent> > g_mapTopicToAgent;

