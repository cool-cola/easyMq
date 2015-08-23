#include <iostream>
#include <string>
#include <map>
#include <assert.h>
#include "serialization.hpp"

using namespace std;

#define BUFFER_LEN (2*1024*1024)
char buffer[BUFFER_LEN];

int main()
{
	map<string, string>::iterator it;
	string strBuf;
	map<string, string> mapIn, mapOut;
	int ret;

	cout << "--normal test" << endl;
	mapIn["s1"] = "hello";
	mapIn["s2"] = "world";

	ret = MapToString(buffer, BUFFER_LEN, mapIn, strBuf);
	assert(ret == 0);
	cout << "to string:" << strBuf << endl;

	ret = StringToMap(strBuf, mapOut);
	assert(ret == 0);
	it = mapOut.begin();
	for(; it!=mapOut.end(); ++it)
	{
		cout << it->first << ":" << it->second << endl;
	}

	cout << "--null test" <<  endl;
	mapIn.clear();
	mapOut.clear();
	ret = MapToString(buffer, BUFFER_LEN, mapIn, strBuf);
	assert(ret == 0);
	cout << "to string.size:" << strBuf.size() << endl;
	ret = StringToMap(strBuf, mapOut);
	assert(ret == 0);
	cout << "map.Out.size:" << mapOut.size() << endl;
	it = mapOut.begin();
	for(; it!=mapOut.end(); ++it)
	{
		cout << it->first << ":" << it->second << endl;
	}


	cout<< "--zero len value test" << endl;
	mapIn.clear();
	mapOut.clear();
	mapIn["s1"] = "";
	mapIn["s2"] = "";
	ret = MapToString(buffer, BUFFER_LEN, mapIn, strBuf);
	assert(ret == 0);
	cout << "to string.size:" << strBuf.size() << endl;
	ret = StringToMap(strBuf, mapOut);
	assert(ret == 0);
	cout << "map.Out.size:" << mapOut.size() << endl;
	it = mapOut.begin();
	for(; it!=mapOut.end(); ++it)
	{
		cout << it->first << ":" << it->second << endl;
	}

	cout << "--0 in the middle of string buffer test" << endl;
	mapIn.clear();
	mapOut.clear();
	char szTmp[5];
	memset(szTmp, 'a', sizeof(szTmp));
	szTmp[2] = 0;
	string strTmp;
	strTmp.assign(szTmp, sizeof(szTmp));
	mapIn["s1"] = strTmp;
	mapIn["s2"] = "end";
	ret = MapToString(buffer, BUFFER_LEN, mapIn, strBuf);
	assert(ret == 0);
	cout << "to string.size:" << strBuf.size() << endl;
	ret = StringToMap(strBuf, mapOut);
	assert(ret == 0);
	cout << "map.Out.size:" << mapOut.size() << endl;
	it = mapOut.begin();
	for(; it!=mapOut.end(); ++it)
	{
		cout << it->first << ":" << it->second << endl;
	}

	return 0;
}
