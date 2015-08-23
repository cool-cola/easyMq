#include <string.h>
#include <map>
#include <string>
#include "serialization.hpp"

using namespace std;

int MapToString(char *pBuff, int iBufLen, const map<string, string> &mapKeyValue, string &strOut)
{
	if (pBuff == NULL || iBufLen < sizeof(int))
	{
		return -1;
	}

	char *pFree = pBuff;
	int iFree = iBufLen;
	std::map<string, string>::const_iterator it = mapKeyValue.begin();
	for(; it != mapKeyValue.end(); ++it)
	{
		const char *pStr = it->first.c_str();
		int iSize = it->first.size();
		if (iSize + sizeof(int) > iFree)
		{
			return -1;
		}
		memcpy(pFree, (char*) &iSize, sizeof(int));
		pFree += sizeof(int);
		iFree -= sizeof(int);
		memcpy(pFree, pStr, iSize);
		pFree += iSize;
		iFree -= iSize;

		pStr = it->second.c_str();
		iSize = it->second.size();
		if (iSize + sizeof(int) > iFree)
		{
			return -1;
		}
		memcpy(pFree, &iSize, sizeof(int));
		pFree += sizeof(int);
		iFree -= sizeof(int);
		memcpy(pFree, pStr, iSize);
		pFree += iSize;
		iFree -= iSize;
	}
	strOut.assign(pBuff, iBufLen - iFree);

	return 0;
}

int StringToMap(const string &strIn, map<string, string> &mapKeyValue)
{
	const char *pBuff =  strIn.c_str();
	int iStrLen = strIn.size();
	string key, value;
	int iSize;
	while (iStrLen >= sizeof(int))
	{
		memcpy((char*) &iSize, pBuff, sizeof(int));
		pBuff += sizeof(int);
		iStrLen -= sizeof(int);
		if (iStrLen < iSize)
		{
			return -1;
		}
		key.assign(pBuff, iSize);
		pBuff += iSize;
		iStrLen -= iSize;

		if (iStrLen < sizeof(int))
		{
			return -1;
		}
		memcpy((char*) &iSize, pBuff, sizeof(int));
		pBuff += sizeof(int);
		iStrLen -= sizeof(int);
		if (iStrLen < iSize)
		{
			return -1;
		}
		value.assign(pBuff, iSize);
		pBuff += iSize;
		iStrLen -= iSize;

		mapKeyValue[key] = value;
	}

	if (iStrLen != 0)
	{
		return -1;
	}

	return 0;
}
