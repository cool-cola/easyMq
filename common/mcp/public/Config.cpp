#include "Config.h"

#include <libgen.h>

bool isNumber(const char* p)
{
    int len = strlen(p);
    if (0 == len)
        return -1;
    for (int i = 0; i < len; ++i)
        if (p[i] < '0' || p[i] > '9')
            return -1;
    return 0;
}

Config::Config(const string& cmd, const string& filename): _filename(filename)
{
    char buf[128];
    snprintf(buf, 128, "%s", cmd.c_str());
    _proc_name = basename(buf);
    _dirname = dirname(buf);
}

Config::~Config()
{
}

int32_t Config::init()
{
    return 0;
}

string Config::getFilename()
{
    return _filename;
}

string Config::getProcName()
{
    return _proc_name;
}

string Config::getDirname()
{
    return _dirname;
}

string Config::getConfStr(const string& name)
{
    tr1::unordered_map<string, string>::iterator it = _conf_str.find(name);
    return it == _conf_str.end() ? "" : it->second;
}

int32_t Config::getConfInt(const string& name)
{
    tr1::unordered_map<string, int32_t>::iterator it = _conf_int.find(name);
    return it == _conf_int.end() ? -1 : it->second;
}

int32_t Config::setConfStr(const string& name, const string& default_value)
{
    string value = readItem(name);
    if (value.empty() && default_value.empty())
        return -1;
    else if (value.empty() && !default_value.empty())
        value = default_value;
    _conf_str[name] = value;
    return 0;
}

int32_t Config::setConfInt(const string& name, int32_t default_value)
{
    string  value = readItem(name);

    int32_t v;
    if (value.empty())
        v = default_value;
    else if (0 == isNumber(value.c_str()))
        v = atoi(value.c_str());
    else
        return -1;
    if (v < 0)
        return -1;
    _conf_int[name] = v;

    return 0;
}

string Config::readItem(const string& name)
{
    string content;

    FILE* file = fopen(_filename.c_str(), "r");
    if (NULL == file)
        return content;

    char buf[1024], key[1024], value[1024];
    while (fgets(buf, 1024, file) != NULL)
    {
        int len = strlen(buf);
        if (buf[len - 1] == '\n')
        {
            buf[len - 1] = '\0';
            len--;
        }
        // line starting with # is comments
        if (0 == len || buf[0] == '#')
            continue;
        
        try
        {
            sscanf(buf, "%s%s", key, value);
            if (name == key) 
            {
                content = value;
                break;
            }
        }
        catch (exception e)
        {
            ;
        }
    }

    fclose(file);

    return content;
}

string Config::toString()
{
    string ret;
    tr1::unordered_map<string, string>::iterator it1;
    for (it1 = _conf_str.begin(); it1 != _conf_str.end(); ++it1)
    {
        if (!ret.empty())
            ret.append("\n");
        ret.append(it1->first).append(":").append(it1->second);
    }
    tr1::unordered_map<string, int32_t>::iterator it2;
    for (it2 = _conf_int.begin(); it2 != _conf_int.end(); ++it2)
    {
        if (!ret.empty())
            ret.append("\n");
        char buf[16];
        snprintf(buf, 16, "%d", it2->second);
        ret.append(it2->first).append(":").append(buf);
    }
    return ret;
}

