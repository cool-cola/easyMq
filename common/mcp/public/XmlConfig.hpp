/*************************************************************
Copyright (C), 1988-1999
Author:nekeyzhong
Version :1.0
Date: 2006-09
Description: xml格式配置文件读取

***********************************************************/

#ifndef _XML_CONFIG_HPP__
#define _XML_CONFIG_HPP__

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <deque>
#include <sstream>
using namespace std;

template<typename T> T from_str(const std::string& s)
{
	std::istringstream is(s);
	T t;
	is >> t;
	return t;
}

struct conf_load_error: public runtime_error{ conf_load_error(const string& s):runtime_error(s){}};
struct conf_not_find: public runtime_error{ conf_not_find(const string& s):runtime_error(s){}};
class XmlConfig
{
public:
	XmlConfig(){};
	XmlConfig(const string& filename){Init(filename);};
	~XmlConfig(){};

public:
	 //filename 配置文件名
	void Init(const string& filename) throw(conf_load_error);

	 // 取配置,支持层次结构,以'\'划分,如conf["Main\\ListenPort"]
	const string& operator [](const string& name) const throw(conf_not_find);

	//取path下所有name-value对
	const map<string,string>& GetPairs(const string& path) const;

	//取path下所有name-value或single配置
	const vector<string>& GetDomains(const string& path) const;

	 // 取path下所有subpath名(只取一层)
	const vector<string>& GetSubPath(const string& path) const;

protected:
	enum EntryType {
		T_STARTPATH = 0,
		T_STOPPATH = 1,
		T_NULL = 2,
		T_PAIR = 3,
		T_DOMAIN =4,
		T_ERROR = 5
	};
	void Load() throw (conf_load_error);
	
	string start_path(const string& s);
	string stop_path(const string& s);
	void decode_pair(const string& s,string& name,string& value);
	string trim_comment(const string& s); //trim注释和\n符号
	string path(const deque<string>& path);
	string parent_path(const deque<string>& path);
	string sub_path(const deque<string>& path);

	EntryType entry_type(const string& s);
protected:
	string _filename;

	map<string,map<string,string> > _pairs;
	map<string,vector<string> > _paths;
	map<string,vector<string> > _domains;

	map<string,string> _null_map;
	vector<string> _null_vector;
};

#endif 

