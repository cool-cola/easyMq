#include <iostream>
#include "util_string.h"

using namespace std;

int main()
{
	string null_str;
	string space_str = "	 ";
	string left_str = "  192:sd=s  si";
	string right_str = "_123:23:23:2    ";
	string full_str = "		ok	 ";
	string nospace_str = "haha";
	
	trim(null_str);
	cout << "null_str:" << null_str << "|" << endl;
	
	cout << "space_str.find_last_not_of(\" \\t\")=" << space_str.find_last_not_of(" \t") << endl;
	trim(space_str);
	cout << "space_str:" << space_str << "|" << endl;

	trim(left_str);
	cout << "left_str:" << left_str << "|" << endl;

	trim(right_str);
	cout << "right_str:" << right_str << "|" << endl;

	trim(full_str);
	cout << "full_str:" << full_str << "|" << endl;

	trim(nospace_str);
	cout << "nospace_str:" << nospace_str << "|" << endl;

	cout << endl;
	cout << "string::npos=" << string::npos << " 0x" << hex << string::npos << endl;
	cout << "string::npos + 1=" << string::npos + 1<< endl;

	return 0;
}
