#ifndef _USERFUNC_H_
#define _USERFUNC_H_

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h> 

#include <map>
using namespace std;

/*
用户实现DoMsg接口即可
*/
class CUserFunc
{
public:
	int ParseMsg(char* pData ,int iDataLen);
	~CUserFunc();
private:
public:
public:

};

#endif

