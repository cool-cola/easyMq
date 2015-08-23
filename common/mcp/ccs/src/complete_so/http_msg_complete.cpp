/************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2010
Description: http格式协议报文完整性检测
***********************************************************/


#ifndef _HTTP_MSG_COMPLETE_
#define _HTTP_MSG_COMPLETE_

#include <string.h>
#include <stdlib.h>

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
	char *pMsgData = (char*)pData;

	//no head
	char *pHeadEnd = strstr(pMsgData,"\r\n\r\n");
	if((pHeadEnd == NULL) || (pHeadEnd+4-pMsgData > (int)unDataLen))
		return 0;

	pHeadEnd += 4;	

	//no body,return head
	char *pContentLength = strcasestr(pMsgData,"Content-Length:");
	if((pContentLength == NULL) || (pContentLength > pHeadEnd))
		return pHeadEnd - pMsgData;

	int iHeadLen = pHeadEnd - pMsgData;
	int iBodyLen = unDataLen - iHeadLen;

	int iContentLength = atol(pContentLength+15);
	
	if(iBodyLen < iContentLength)
	{
		iPkgTheoryLen = iHeadLen + iContentLength;
		return 0;
	}
	else
	{
		//多余的也带上
		return iHeadLen+iBodyLen;
	}
}

int msg_header_len()
{
	return 1024;
}


}

#endif

