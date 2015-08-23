#include "Msg.hpp"

int64_t _MSG_NTOHL64(int64_t N)
{
	const int n = 1;
	bool bLittleEndian = *((char*)&n) ? true : false;
	if(!bLittleEndian)
		return N;
	
	u_int64_t	H = 0;
	u_int32_t iTmp = 0;
	
	iTmp = ntohl((u_int32_t)(N>>32));
	memcpy(&H,&iTmp,sizeof(u_int32_t));
	
	iTmp = ntohl((u_int32_t)(N));
	memcpy(((char*)&H)+sizeof(u_int32_t),&iTmp,sizeof(u_int32_t));
	
	return H;
}

int64_t _MSG_HTONL64(int64_t H)
{
	const int n = 1;
	bool bLittleEndian = *((char*)&n) ? true : false;
	if(!bLittleEndian)
		return H;
	
	u_int64_t	N = 0;
	u_int32_t iTmp = 0;
	
	iTmp = htonl((u_int32_t)(H>>32));
	memcpy(&N,&iTmp,sizeof(u_int32_t));
	
	iTmp = htonl((u_int32_t)(N));
	memcpy(((char*)&N)+sizeof(u_int32_t),&iTmp,sizeof(u_int32_t));
	
	return N;
}

int32_t EncodeChar(char *pEncode,unsigned char ucSrc)
{
	*pEncode = (char)ucSrc;
	return (int32_t)sizeof(unsigned char);
}
int32_t DecodeChar(char *pDecode,unsigned char &ucDest)
{
	ucDest = *pDecode;
	return (int32_t)sizeof(unsigned char);
}
int32_t EncodeShort(char *pEncode,unsigned short usSrc)
{
	unsigned short usTemp = htons(usSrc);
	memcpy(pEncode,&usTemp,sizeof(unsigned short));
	return (int32_t)sizeof(unsigned short);
}
int32_t DecodeShort(char *pDecode,unsigned short &usDest)
{
	memcpy(&usDest,pDecode,sizeof(unsigned short));
	usDest = ntohs(usDest);
	return (int32_t)sizeof(unsigned short);
}
int32_t EncodeInt(char *pEncode,u_int32_t unSrc)
{
	u_int32_t unTemp = htonl(unSrc);
	memcpy(pEncode,&unTemp,sizeof(u_int32_t));
	return (int32_t)sizeof(u_int32_t);
}
int32_t DecodeInt(char *pDecode,u_int32_t &unDest)
{
	memcpy(&unDest,pDecode,sizeof(u_int32_t));
	unDest = ntohl(unDest);
	return (int32_t)sizeof(u_int32_t);
}
int32_t EncodeInt64(char *pEncode,u_int64_t ullSrc)
{
	u_int64_t usTemp = _MSG_HTONL64(ullSrc);
	memcpy(pEncode,&usTemp,sizeof(u_int64_t));
	return (int32_t)sizeof(u_int64_t);
}
int32_t DecodeInt64(char *pDecode,u_int64_t  &ullDest)
{
	memcpy(&ullDest,pDecode,sizeof(u_int64_t));
	ullDest = _MSG_HTONL64(ullDest);
	return (int32_t)sizeof(u_int64_t);
}
int32_t EncodeString(char *pEncode,char *strSrc,const int32_t MAXSTRLEN/*=-1*/)
{
	int32_t iStrLen = strlen(strSrc);
	if((MAXSTRLEN>=0) && (iStrLen > MAXSTRLEN-1))
	{
		iStrLen = MAXSTRLEN-1;
	}

	memcpy(pEncode,strSrc,iStrLen);
	pEncode[iStrLen] = 0;
	return (int32_t)(iStrLen+1);
}
int32_t DecodeString(char *pDecode,char *strDest,const int32_t MAXSTRLEN/*=-1*/)
{
	if(MAXSTRLEN>=0)
		strncpy(strDest,pDecode,(size_t)MAXSTRLEN);
	else		
    		strcpy(strDest,pDecode);
	
    return (int32_t)(strlen(pDecode)+1);
}

int32_t EncodeMem(char *pEncode,char *pcSrc,int32_t iMemSize)
{
	memcpy(pEncode,pcSrc,iMemSize);
	return iMemSize;
}
int32_t DecodeMem(char *pDecode,char *pcDest,int32_t iMemSize)
{
    memcpy(pcDest,pDecode,iMemSize);
    return iMemSize;
}

//---------------重载----------------------------
int32_t DecodeChar(char *pDecode, char &cDest )
{
	unsigned char ucDest = 0;
	int32_t iRet = DecodeChar(pDecode,ucDest);
	cDest = (char)ucDest;
	return iRet;
}
int32_t DecodeShort(char *pDecode, short &sDest )
{
	unsigned short usDest = 0;
	int32_t iRet = DecodeShort(pDecode,usDest);
	sDest = (short)usDest;
	return iRet;
}
int32_t DecodeInt(char *pDecode,int32_t &iDest)
{
	u_int32_t unDest = 0;
	int32_t iRet = DecodeInt(pDecode,unDest);
	iDest = (int32_t)unDest;
	return iRet;
}
int32_t DecodeInt64(char *pDecode, int64_t &llDest )
{
	u_int64_t ullDest = 0;
	int32_t iRet = DecodeInt64(pDecode,ullDest);
	llDest = (int64_t)ullDest;
	return iRet;
}
void _PrintBin(FILE* pLogfp,char *pBuffer, int32_t iLength)
{
    char tmpBuffer[16384];
    char strTemp[32];
    if( iLength <= 0 || iLength > 4096 || pBuffer == NULL )
        return;

    tmpBuffer[0] = '\0';
    for(int32_t i = 0; i < iLength; i++ )
    {
        if( !(i%16) )
        {
            sprintf(strTemp, "\n%04d>    ", i/16+1);
            strcat(tmpBuffer, strTemp);
        }
        sprintf(strTemp, "%02X ", (unsigned char)pBuffer[i]);
        strcat(tmpBuffer, strTemp);
    }
    strcat(tmpBuffer, "\n");
    fprintf(pLogfp,"Print Hex:%s", tmpBuffer);
}

//--------------------------------------------------------------------------
int32_t CMsgBody::Encode(char *pOutBuffer,int32_t iOutBufferLen)
{
	if(iOutBufferLen < (int32_t)sizeof(int32_t))
		return -1;
	
	int32_t iOutLength = 0;
	iOutLength += EncodeInt(pOutBuffer+iOutLength,(u_int32_t)m_iRetCode);
	iOutLength += EncodeString(pOutBuffer+iOutLength, m_szRetMsg,sizeof(m_szRetMsg));
	return iOutLength>iOutBufferLen?-1:iOutLength;
}
int32_t CMsgBody::Decode(const char *pInBuffer,int32_t iInBufferLen)
{
	if(iInBufferLen < (int32_t)sizeof(int32_t))
		return -1;
	
	char *pInBufferPos = (char *)pInBuffer;
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_iRetCode);
	pInBufferPos += DecodeString(pInBufferPos, m_szRetMsg,sizeof(m_szRetMsg));
	
	return (int32_t)(pInBufferPos-pInBuffer)>iInBufferLen ? 
					-1:(pInBufferPos-pInBuffer);
}

void CMsgBody::Print(FILE* pLogfp)
{
	fprintf(pLogfp,"CMsgBody==>>\nRetCode=%d,RetMsg=%s\n",m_iRetCode,m_szRetMsg);
}
//------------------------------------------------------
CMsg::CMsg()
{
	m_iMsgTag = 0;
	m_iMsgTotalLen = 0;
	m_iMsgID = -1;
	m_unMsgSeq = 0;
	m_unMsgSubSeq = 0;
	m_pMsgBody = NULL;
	m_iMsgVer = 0;
	m_unRouteType = 0;
	m_unRouteIDKey = 0;
	m_iExtDataLen = 0;
	m_pExtData = NULL;
	gettimeofday(&m_tBirthTime,NULL);
};

CMsg::CMsg(const CMsg &that)
{
	m_pMsgBody = NULL;
	m_pExtData = NULL;
	
	*this = that;
}

CMsg::~CMsg()
{
	if(m_pExtData)
		delete []m_pExtData;

	if(m_pMsgBody)
		delete m_pMsgBody;
};

int32_t CMsg::Encode(char *pOutBuffer,int32_t iOutBufferLen)
{
	if(!pOutBuffer || iOutBufferLen<=(int32_t)sizeof(int32_t))
		return -100;
	
	int32_t iOutLength = 0;

	memcpy(&m_iMsgTag,MSG_TAG,sizeof(int32_t));
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)m_iMsgTag);

	char* pMsgLenPos = pOutBuffer+iOutLength;
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)m_iMsgTotalLen);
	
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)m_iMsgID);
	iOutLength += EncodeInt(pOutBuffer+iOutLength, m_unMsgSeq);
	iOutLength += EncodeInt(pOutBuffer+iOutLength, m_unMsgSubSeq);
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)(m_tBirthTime.tv_sec));
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)(m_tBirthTime.tv_usec));
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)m_iMsgVer);

	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)m_unRouteType);
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)m_unRouteIDKey);
	
	iOutLength += EncodeInt(pOutBuffer+iOutLength, (u_int32_t)m_iExtDataLen);
	if(m_iExtDataLen > iOutBufferLen-iOutLength)
		return -101;
	
	iOutLength += EncodeMem(pOutBuffer+iOutLength, m_pExtData,m_iExtDataLen);
	
	int32_t iBodyLen = m_pMsgBody->Encode(pOutBuffer+iOutLength,iOutBufferLen-iOutLength);
	if(iBodyLen < 0)
		return -102;
	
	iOutLength += iBodyLen; 

	//编码长度大于数据长度
	if(iOutLength > iOutBufferLen)
		return -103;
	
	//消息总长度
	m_iMsgTotalLen = iOutLength;
	EncodeInt(pMsgLenPos,m_iMsgTotalLen);

	return iOutLength;
}
int32_t CMsg::Decode(const char *pInBuffer,int32_t iInBufferLen)
{
	//头部
	int32_t iDecLen = DecodeHead(pInBuffer,iInBufferLen);
	if(iDecLen <= 0)
		return iDecLen;

	char * pInBufferPos = (char *)pInBuffer + iDecLen;

	//体部
	if(m_pMsgBody)
	{
		delete m_pMsgBody;
		m_pMsgBody = NULL;
	}	
	
	m_pMsgBody = m_fCreateMsgBody(m_iMsgID);
	if(!m_pMsgBody)
		return -103;

	int32_t iBodyLen = m_pMsgBody->Decode(pInBufferPos,
								iInBufferLen-(pInBufferPos-pInBuffer));
	if(iBodyLen < 0)
		return -104;

	pInBufferPos += iBodyLen;
	
	return (int32_t)(pInBufferPos-pInBuffer)>iInBufferLen ? 
					-1:(pInBufferPos-pInBuffer);
}

int32_t CMsg::SetExtData(char* pExtData,int32_t iExtDataLen)
{
	if(m_pExtData)
		delete []m_pExtData;
	m_pExtData = NULL;
	m_iExtDataLen = 0;

	if(!pExtData || iExtDataLen<=0)
		return 0;

	m_iExtDataLen = iExtDataLen;
	m_pExtData = new char[m_iExtDataLen];
	memcpy(m_pExtData,pExtData,m_iExtDataLen);
	return 0;
}
	
void CMsg::Print(FILE* pLogfp)
{
	fprintf(pLogfp,"CMsg==>>\nMsgID=%d,Seq=%u,SubSeq=%u,BirthTime=%d.%d,Ver=%d\n",
		m_iMsgID,m_unMsgSeq,m_unMsgSubSeq,
		(int32_t)m_tBirthTime.tv_sec,(int32_t)m_tBirthTime.tv_usec,
		m_iMsgVer);

	fprintf(pLogfp,"RouteInfo==>>\nRouteType=%u,RouteIDKey=%u\n",
		m_unRouteType,m_unRouteIDKey);
	
	fprintf(pLogfp,"ExtLen=%d\n",m_iExtDataLen);
	if(m_iExtDataLen>0)
	{
		fprintf(pLogfp,"Ext:\n");
		_PrintBin(pLogfp,m_pExtData,m_iExtDataLen);
	}
	
	if(m_pMsgBody)
		m_pMsgBody->Print(pLogfp);
}

void CMsg::Print(char* pLogFile)
{
	FILE* pLogfp = fopen(pLogFile,"a+");
	if(!pLogfp)
	{
		return;
	}
	Print(pLogfp);
	fclose(pLogfp);
}

int32_t CMsg::DecodeHead(const char *pInBuffer,int32_t iInBufferLen)
{
	if(!pInBuffer || iInBufferLen<(int32_t)sizeof(int32_t))
		return -106;
	
	char *pInBufferPos = (char *)pInBuffer;

	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_iMsgTag);
	if(0 != memcmp(&m_iMsgTag,MSG_TAG,sizeof(m_iMsgTag)))
		return -107;
	
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_iMsgTotalLen);
	if(m_iMsgTotalLen > iInBufferLen)
		return -108;
	
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_iMsgID);
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_unMsgSeq);
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_unMsgSubSeq);
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)(m_tBirthTime.tv_sec));
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)(m_tBirthTime.tv_usec));
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_iMsgVer);

	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_unRouteType);
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_unRouteIDKey);
	
	pInBufferPos += DecodeInt(pInBufferPos,(u_int32_t &)m_iExtDataLen);
	if(m_iExtDataLen > iInBufferLen-(pInBufferPos-pInBuffer))
		return -109;
	
	if(m_pExtData)
	{
		delete []m_pExtData;
		m_pExtData = NULL;
	}	

	if(m_iExtDataLen > 0 )
	{
		m_pExtData = new char[m_iExtDataLen];
		pInBufferPos += DecodeMem(pInBufferPos,m_pExtData,m_iExtDataLen);	
	}
	
	return (int32_t)(pInBufferPos-pInBuffer)>iInBufferLen ? 
					-1:(pInBufferPos-pInBuffer);
}

int32_t CMsg::CloneHead(CMsg *pSrcMsg,CMsg *pDestMsg)
{
	pDestMsg->m_iMsgID = pSrcMsg->m_iMsgID;
	pDestMsg->m_unMsgSeq = pSrcMsg->m_unMsgSeq;
	pDestMsg->m_unMsgSubSeq = pSrcMsg->m_unMsgSubSeq;
	pDestMsg->m_tBirthTime.tv_sec = pSrcMsg->m_tBirthTime.tv_sec;
	pDestMsg->m_tBirthTime.tv_usec = pSrcMsg->m_tBirthTime.tv_usec;
	pDestMsg->m_iMsgVer = pSrcMsg->m_iMsgVer;
	pDestMsg->m_unRouteType = pSrcMsg->m_unRouteType;
	pDestMsg->m_unRouteIDKey = pSrcMsg->m_unRouteIDKey;
	pDestMsg->SetExtData(pSrcMsg->m_pExtData,pSrcMsg->m_iExtDataLen);
	return 0;
}
//-------------------------------------------------------------------
/*
#include "MyMsg.hpp"
int32_t main()
{
CMsg stMsg1;

stMsg1.m_iMsgID = 1;
stMsg1.m_iMsgVer = 2;
stMsg1.SetExtData("aaaaa",5);

CReqGetMapBody* pp =new CReqGetMapBody();
pp->m_unSuffix1 = 99;
pp->m_unSuffix2 = 88;
        stMsg1.m_pMsgBody = (CMsgBody*)pp;


CMsg stMsg2=stMsg1;

printf("end\n");
return 0;
}
*/

/*

	CMsg stReqMsg;
	stReqMsg.m_iMsgID = MSG_REQ_GET_MAP;
	stReqMsg.m_iMsgVer = 1;
	gettimeofday(&stReqMsg.m_tBirthTime,NULL);
	stReqMsg.m_unMsgSeq=9988;
	stReqMsg.m_unMsgSubSeq=8877;
	stReqMsg.m_iExtLen=10;
	strcpy(stReqMsg.m_szExt,"123456789");

	CReqGetMapBody* pCReqGetMapBody = new CReqGetMapBody();
	pCReqGetMapBody->m_unSuffix1 = 10;
	pCReqGetMapBody->m_unSuffix2 = 20;
	
	stReqMsg.m_pMsgBody = (CMsgBody*)pCReqGetMapBody;
	stReqMsg.Print(stdout);
	
	char szBuff[1024];
	int32_t iRet = stReqMsg.Encode(szBuff);
	printf("encode len %d\n",iRet);

	printf("g_kkk=%p\n",CMsg::m_fCreateMsgBody);
	
	CMsg stRspMsg;
	iRet = stRspMsg.Decode(szBuff);
	printf("decode len %d\n",iRet);

	stRspMsg.Print(stdout);
	return 0;
*/
