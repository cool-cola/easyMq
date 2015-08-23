/**************************************************************************

DESC: Msg头部类

AUTHOR: nekeyzhong 

DATE: 2008年5月

PROJ: Part of MCP Project

 **************************************************************************/
#ifndef _MSG_HPP
#define _MSG_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

int32_t EncodeChar(char *pEncode, unsigned char ucSrc );
int32_t DecodeChar(char *pDecode, unsigned char &ucDest );
int32_t EncodeShort(char *pEncode, unsigned short usSrc );
int32_t DecodeShort(char *pDecode, unsigned short &usDest );
int32_t EncodeInt(char *pEncode, u_int32_t unSrc );
int32_t DecodeInt(char *pDecode, u_int32_t &unDest );
int32_t EncodeInt64(char *pEncode, u_int64_t ullSrc );
int32_t DecodeInt64(char *pDecode, u_int64_t &unDest );
int32_t EncodeString(char *pEncode,char *strSrc,const int32_t MAXSTRLEN=-1);
int32_t DecodeString(char *pDecode, char *strDest,const int32_t MAXSTRLEN=-1);
int32_t EncodeMem(char *pEncode, char *pcSrc, int32_t iMemSize );
int32_t DecodeMem(char *pDecode, char *pcDest, int32_t iMemSize );

int32_t DecodeChar(char *pDecode, char &cDest );
int32_t DecodeShort(char *pDecode, short &sDest );
int32_t DecodeInt(char *pDecode,int32_t &iDest);
int32_t DecodeInt64(char *pDecode, int64_t &llDest );

class CMsgBody
{
public:
	CMsgBody()
	{
		m_iRetCode=0;
		memset(m_szRetMsg,0,sizeof(m_szRetMsg));
	};
	virtual ~CMsgBody(){};
	virtual int32_t Encode(char *pOutBuffer,int32_t iOutBufferLen);
	virtual int32_t Decode(const char *pInBuffer,int32_t iInBufferLen);
	virtual void Print(FILE* pLogfp=stdout);

	int32_t m_iRetCode;
	char m_szRetMsg[256];
};

typedef CMsgBody* (*CREATE_MSGBODY)(int32_t iMsgID);
#define MSG_TAG		"SASN"
class CMsg
{
public:
	CMsg();
	CMsg(const CMsg &that);	
	~CMsg();
	
	int32_t Encode(char *pOutBuffer,int32_t iOutBufferLen);
	int32_t Decode(const char *pInBuffer,int32_t iInBufferLen);
	int32_t SetExtData(char* pExtData,int32_t iExtDataLen);
	
	void Print(FILE* pLogfp=stdout);
	void Print(char* pLogFile);

	static CREATE_MSGBODY m_fCreateMsgBody;

	//只解析Msg头部
	int32_t DecodeHead(const char *pOutBuffer,int32_t iOutBufferLen);
	//只复制Msg头部
	static int32_t CloneHead(CMsg *pSrcMsg,CMsg *pDestMsg);
private:	
	//头部自动填写
	int32_t m_iMsgTag;				//报文标记头	
	int32_t m_iMsgTotalLen;			//报文总长度,从MsgTag开始算起
public:
	//头部用户填写
	int32_t m_iMsgID;			//消息ID
	u_int32_t m_unMsgSeq;		//ECHO: 主seq
	u_int32_t m_unMsgSubSeq;	//ECHO: 从seq,用来在事务中并发
	timeval m_tBirthTime;			//ECHO: 本消息的诞生时间
	int32_t m_iMsgVer;			//消息版本

	//用户可控的消息路由信息
	u_int32_t m_unRouteType;	//路由目标,哪类
	u_int32_t m_unRouteIDKey;	//路由值	,哪个
	
public:	
	int32_t m_iExtDataLen;		//扩展区长度
	char* m_pExtData;			//扩展区
	
public:
	//体部
	CMsgBody* m_pMsgBody;
};

//---------------------------------------------------------
#endif

