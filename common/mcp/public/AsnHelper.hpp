#ifndef _ASN_HELPER_HPP_
#define _ASN_HELPER_HPP_

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dlfcn.h>
//#include <endian.h>

#if defined(LINUX) 
#include <linux/ip.h> 
#include <linux/tcp.h> 
#else 
#include <netinet/ip.h> 
#include <netinet/tcp.h> 
#endif 

#include <vector>
#include <string>
#include <set>
#include <map>
using namespace std;
#include "storage_asn.h"
inline int AsnKeyValueToMap(string &asnStr, map<string, string> &mapKeyValue, string &strErr)
{
	mapKeyValue.clear();
	if(asnStr.size() > 0)
	{
		//decode
		NameValueList asnNameValueList;
		AsnBuf inBuf;
		inBuf.InstallData((char*)asnStr.c_str(), asnStr.size());
		AsnLen bytesDecoded;


		if(asnNameValueList.BDecPdu(inBuf,bytesDecoded)!=true)
		{
			strErr = "asnNameValueList.BDecPdu failed";
			return - 1;
		}
		asnNameValueList.SetCurrToFirst();
		while(1)
		{
			NameValue *pNV = asnNameValueList.Curr();
			if(!pNV)
				break;
			string key,value;
			key.assign((char*)pNV->name, pNV->name.Len());
			value.assign((char*)pNV->value, pNV->value.Len());
			mapKeyValue[key] = value;
			asnNameValueList.GoNext();
		}
	}
	return 0;
}

inline int MapToAsnKeyValue(char *pBuff, int iBufLen, map<string, string> &mapKeyValue, string &asnStr,  string &strErr)
{
	AsnBuf outputBuf;
	size_t encodedLen;
	outputBuf.Init (pBuff,iBufLen);
	outputBuf.ResetInWriteRvsMode();
	if(mapKeyValue.size() > 0)
	{

		NameValueList note_nvl;
		map<string, string>::iterator it = mapKeyValue.begin();
		for(; it != mapKeyValue.end(); ++it)
		{
			NameValue *note_nv = note_nvl.Append();
			note_nv->name.ReSet(it->first.c_str(), it->first.size());
			note_nv->value.ReSet(it->second.c_str(), it->second.size());
		}
		if(note_nvl.BEncPdu (outputBuf, encodedLen)!=true)
		{
			strErr = "--- ERROR - Encode routines failed";
			return -1;
		}
	
		asnStr.assign(outputBuf.DataPtr(),outputBuf.DataLen());
	}
	return 0;
}
#endif
