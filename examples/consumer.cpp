#include "../src/clnt/clnt.h"
#include <stdio.h>
void f(std::string msg)
{
	printf("处理消息 %s",msg.c_str());
}

int main()
{
	MqClnt clnt("127.0.0.1",8888);
	MqClnt::ReceiveMsgReq req;
	MqClnt::ReceiveMsgResp resp;
	req.topic = "yafngzh";

	clnt.receiveMsg(req,resp,f);
}
