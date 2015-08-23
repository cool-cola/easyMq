#ifndef _SERIALIZATION_HPP_
#define _SERIALIZATION_HPP_
#include <string>
#include <map>

/**
	pBuff, temp buffer used to construct strOut
	iBufLen, temp buffer len
	mapKeyValue, input map
	strOut, output string
*/
int MapToString(char *pBuff, int iBufLen, const std::map<std::string, std::string> &mapKeyValue, std::string &strOut);

int StringToMap(const std::string &strIn, std::map<std::string, std::string> &mapKeyValue);
#endif
