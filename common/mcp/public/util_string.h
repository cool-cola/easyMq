#ifndef _UTIL_STRING_H_
#define _UTIL_STRING_H_

#include <string>

const std::string c_stWhiteSpaces(" \t\n\r");

inline void trimRight(std::string &str, const std::string &spaces = c_stWhiteSpaces)
{
	// when str only contains space, then find_last_not_of return string::npos
	// We have tested it using g++, string::npos + 1 == 0.
	// But for safe so that trimRight does not depend on npos implement, we add an if clause.
	std::string::size_type pos = str.find_last_not_of(spaces);
	if (pos != std::string::npos)
	{
		pos = pos + 1;
	}
	else
	{
		pos = 0;
	}
		
	str.erase(pos);
}

inline void trimLeft(std::string &str, const std::string &spaces = c_stWhiteSpaces)
{
	std::string::size_type pos = str.find_first_not_of(spaces);
	str.erase(0, pos);
}

inline void trim(std::string &str, const std::string &spaces = c_stWhiteSpaces)
{
	trimRight(str, spaces);
	trimLeft(str, spaces);
}

#endif
