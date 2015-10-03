/*
 * easyMQAgent.h
 *
 *  Created on: 2015年8月27日
 *      Author: millerzhao
 */

#ifndef SRC_AGENT_EASYMQAGENT_H_
#define SRC_AGENT_EASYMQAGENT_H_

#include "easyMsg.h"
#include <string>
class EasyMQAgent
{
public:
	int32_t initTopic(const std::string &topic);
	int32_t recvMsg(struct Msg &msg);
	int32_t sendMsg(const struct Msg &msg);
};



#endif /* SRC_AGENT_EASYMQAGENT_H_ */
