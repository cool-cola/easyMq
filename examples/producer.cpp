#include "../src/clnt/clnt.h"
#include <stdio.h>
#include <unistd.h>

int main()
{
	MqClnt clnt("127.0.0.1",8888);
	MqClnt::PublishMsgReq req;
	MqClnt::PublishMsgResp resp;
	req.topic = "yafngzh";
	while(1)
	{
		req.msg = "current time is "+std::to_string(time(0));
		clnt.publishMsg(req,resp);
		sleep(1);
	}
	pause();
}
