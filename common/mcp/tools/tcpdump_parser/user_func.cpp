#include "user_func.h"

//tcpdump -X -s 0 -w ./msg_file tcp  dst port 5555 and dst host 172.25.38.83 

void PrintBin(FILE* pLogfp,char *pBuffer, int32_t iLength)
{
    if( iLength <= 0 || pBuffer == NULL )
        return;

    for(int i = 0; i < iLength; i++ )
    {
        if( !(i%16) )
        {
		fprintf(pLogfp,"\n%04d>    ", i/16+1);
        }
	fprintf(pLogfp,"%02x ", (unsigned char)pBuffer[i]);
    }
    fprintf(pLogfp,"\n");
}


int CUserFunc::ParseMsg(char* pData ,int iDataLen)
{
	return 0;
	
}

CUserFunc::~CUserFunc()
{
}
