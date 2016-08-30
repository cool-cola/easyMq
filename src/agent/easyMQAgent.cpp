/*
 * easyMQAgent.cpp
 *
 *  Created on: 2015年9月26日
 *      Author: millerzhao
 */
//using namespace EasyMQ;

#include "easyMQAgent.h"
#include "agent_ctrl.h"

extern CAgentCtrl g_tAgentCtrl;

int32_t EasyMQAgent::recvMsg(struct ::EasyMQ::Msg &msg)
{
	return 0;
}

int32_t EasyMQAgent::sendMsg(const struct ::EasyMQ::Msg &msg)
{
	return 0;
}

