/************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2010
Description: LV格式协议报文完整性检测
***********************************************************/

#include <arpa/inet.h>
#include <string.h>

extern "C"
{
/*
pData,unDataLen:包数据

iPkgTheoryLen: 包的理论长度,在只收到部分数据的情况下就可以知道
				包长度了，0为无法判断
				
return :
	>0 real msg len,
	=0 not complete,
	<0 error,must close link
*/
int net_complete_func(const void* pData, unsigned int unDataLen,int &iPkgTheoryLen)
{
	iPkgTheoryLen = 0;
	if (unDataLen < sizeof(int))
		return 0;

	int iMsgLen = ntohl(*(int*)pData);
	iPkgTheoryLen = iMsgLen;

	if (iPkgTheoryLen <= (int)unDataLen)
	{
		return iPkgTheoryLen;
	}

	return 0;
}

/*
能够判断出理论包长所需要的最少数据量。
*/
int msg_header_len()
{
	//需要8个字节就可以知道iPkgTheoryLen
	return sizeof(int);
}

}


