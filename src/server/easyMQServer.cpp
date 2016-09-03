/*
 * easyMQServer.cpp
 *
 *  Created on: 2015年8月27日
 *      Author: millerzhao
 */

#include "master_ctrl.h"
#include "easyMQServer.h"
#include <set>
using ::EasyMQ::EasyMQServer;
using ::EasyMQ::EasyMQAgent;
std::map<std::string, std::set<EasyMQAgent> > g_mapTopicToAgent;

extern CMasterCtrl g_tMasterCtrl;
EasyMQServer g_easyMQServer;
int32_t EasyMQServer::initTopic(const string &topic, const EasyMQAgent &agent)
{
	INFO("receive msg %s from ip %d port %d socketSuffix %d",topic.c_str(),agent.ipAddr,agent.port,agent.socketSuffix);
    std::set<EasyMQAgent> setAgent;
    auto it = g_mapTopicToAgent.find(topic);
    if(it == g_mapTopicToAgent.end())
    {
		setAgent.insert(agent);
        g_mapTopicToAgent[topic] = setAgent;
		INFO("First initialize the topic, insert directly,插入后map的大小 %d",g_mapTopicToAgent.size());
    }
    else
    {
		if(it->second.end() == it->second.find(agent))
		{
			INFO("Not find agent,now insert,插入后的大小 %d",it->second.size());
			it->second.insert(agent);
		}
    }

    return 0;
}

int32_t EasyMQServer::transferMsg(const Msg *pMsg)
{
	std::map<std::string, std::set<EasyMQAgent> >::iterator it = g_mapTopicToAgent.find(std::string(pMsg->topic));
    if(it == g_mapTopicToAgent.end())
    {
        return -1;
    }

    for(std::set<EasyMQAgent>::iterator itr(it->second.begin()); itr != it->second.end();
            ++itr)
    {
        sendMsgToAgent(pMsg, *itr);
    }

    return 0;
}

int32_t EasyMQServer::sendMsgToAgent(const Msg *pMsg, const EasyMQAgent &agent)
{
    INFO("Send msg to agent!");
	//return g_tMasterCtrl.SendRsp(agent.ipAddr, agent.port, (char *)pMsg, sizeof(*pMsg));
}



