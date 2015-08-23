#ifndef __COMPRESS_HPP__
#define __COMPRESS_HPP__

#include <string>
using namespace std;
#include "lzo/lzoconf.h"
#include "lzo/lzo1x.h"

static const char *progname = NULL;
#define WANT_LZO_MALLOC 1
#define WANT_XMALLOC 1
#include "portab.h"

#define IN_LEN      (10*1024*1024L)
#define OUT_LEN     (IN_LEN + IN_LEN / 16 + 64 + 3)
class LzoCompressor
{
public:
	LzoCompressor()
	{
		m_pOut = NULL;
		m_pWrk = NULL;
	}
	~LzoCompressor()
	{
		if(m_pWrk)
		{
			lzo_free(m_pWrk);
			m_pWrk = NULL;
		}
		if(m_pOut)
		{
			lzo_free(m_pOut);
			m_pOut = NULL;
		}

	}
	int Init()
	{
		if (lzo_init() != LZO_E_OK)
		{
			m_sLastErr = "lzo_init failed!";
			return -1;
		}

		m_pOut = (lzo_bytep) xmalloc(OUT_LEN);
		m_pWrk = (lzo_voidp) xmalloc(LZO1X_1_MEM_COMPRESS);
		if (m_pOut == NULL || m_pWrk == NULL)
		{
			m_sLastErr = "out of memory!";
			return -1;
		}
		return 0;
	}
	int Compress(const char* pInBuf, int iInBufLen, char*& pOutBuf, int &iOutLen)
	{
		pOutBuf = NULL;
		iOutLen = 0;
		if(iInBufLen > IN_LEN)
		{
			m_sLastErr = "data too large, can not compress!";
			return -1;
		}
		m_iNewLen = m_iOutLen;
		int r = lzo1x_1_compress((lzo_bytep)pInBuf, (lzo_uint)iInBufLen, m_pOut, &m_iNewLen, m_pWrk);
		if (r == LZO_E_OK)
		{
			if (m_iNewLen >= (unsigned int)iInBufLen)
			{
				pOutBuf = (char*)m_pOut;
				iOutLen = (int)m_iNewLen;
				m_sLastErr="after compress, size grow up!";
				return -1;
			}
			pOutBuf = (char*)m_pOut;
			iOutLen = (int)m_iNewLen;
			return 0;
		}
		else
		{
			m_sLastErr="lzo1x_1_compress error!!";
			return -1;
		}
		
	}
	int DeCompress(const char* pInBuf, int iInBufLen, char*& pOutBuf, int &iOutLen)
	{
		pOutBuf = NULL;
		iOutLen = 0;
		if(iInBufLen > IN_LEN)
		{
			m_sLastErr = "data too large, can not DeCompress!";
			return -1;
		}
		m_iNewLen = m_iOutLen;
		int r = lzo1x_decompress((lzo_bytep)pInBuf, (lzo_uint)iInBufLen, m_pOut, &m_iNewLen,NULL);
		if (r == LZO_E_OK)
		{
			pOutBuf = (char*)m_pOut;
			iOutLen = (int)m_iNewLen;
			return 0;
		}
		else
		{
			m_sLastErr = "lzo1x_decompress error";
			return -1;
		}
	}
private:
	lzo_bytep m_pOut;
	lzo_uint m_iOutLen;
	lzo_voidp m_pWrk;
	lzo_uint m_iNewLen;
public:
	string m_sLastErr;
};
/*
// g++ Compress.cpp -L/usr/local/lib/liblzo2.a -llzo2
int main(int argc, char *argv[])
{
	LzoCompressor test;
	test.Init();
	char *pBuf = (char*)malloc(IN_LEN+1);
	if(!pBuf)
	{
		printf("malloc failed\n");
		return -1;
	}
	for(int i = 0; i < (IN_LEN+1); ++i)
	{
		char *pOut;
		int iOutLen;
		int iRet = test.Compress(pBuf, i, pOut, iOutLen);
		
		if(iRet != 0)
		{
			printf("len %d , after compress len %d ret %d %s\n", i, iOutLen, iRet, test.m_sLastErr.c_str());
			continue;
		}
		string compressed_data;
		compressed_data.assign(pOut, iOutLen);
		iRet = test.DeCompress(compressed_data.c_str(), compressed_data.size(), pOut, iOutLen);
		
		if(iRet != 0)
		{
			printf("after compress len %d ret %d %s\n",iOutLen, iRet, test.m_sLastErr.c_str());
			continue;
		}
		if(iOutLen != i)
		{
			printf("out %d %d\n", iOutLen, i);
		}
		if(memcmp(pBuf, pOut, i))
			printf("data diff after decompress\n");
		if(i%1000 == 0)
			printf("size %d  commpress size %d\n", i, compressed_data.size());
	}
	free(pBuf);
	return 0;
}*/

#endif

