#include "Logger.h"

#include <sys/stat.h>
#include <errno.h>
#include <time.h>

string currentTime()
{
    time_t tt = time(0);
    struct tm* t = localtime(&tt);
    char buf[64];
    snprintf(buf, 64, "%04d-%02d-%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    return buf;
}

format neformat(const std::string& f_string)
{
    format fm(f_string);
    fm.exceptions(no_error_bits);
    return fm;
}

Logger::Logger(const string& base_name, int64_t max_file_size, int32_t max_file_num): _base_name(base_name), _max_file_size(max_file_size), _max_file_num(max_file_num), _cur_file_size(0), _cur_file_id(0), _level(TRACE), _file(NULL)
{
    FILE* fp = NULL;
    try
    {
        char buf[64];
        snprintf(buf, 64, "%s.logid", _base_name.c_str());
        fp = fopen(buf, "r");
        if (fp != NULL)
            fscanf(fp, "%d", &_cur_file_id);
        if (_cur_file_id >= max_file_num)
            _cur_file_id = 0;
    }
    catch (exception e)
    {
        ;
    }
    if (fp)
        fclose(fp);
}

Logger::~Logger()
{
    if (_file)
        fclose(_file);
}

void Logger::setLogLevel(Level level)
{
    _level = level;
}

int32_t Logger::setLogLevel(const string& level_name)
{
    if (level_name == "TRACE")
        _level = TRACE;
    else if (level_name == "DEBUG")
        _level = DEBUG;
    else if (level_name == "INFO")
        _level = INFO;
    else if (level_name == "WARN")
        _level = WARN;
    else if (level_name == "ERROR")
        _level = ERROR;
    else if (level_name == "FATAL")
        _level = FATAL;
    else
        return -1;
    return 0;
}

bool Logger::isLogEnable(Level level)
{
    return level >= _level;
}

int32_t Logger::append(const string& msg)
{
    if (NULL == _file)
    {
        char filename[1024];
        snprintf(filename, 1024, "%s_%d.log", _base_name.c_str(), _cur_file_id);
        struct stat s;
        if (0 == lstat(filename, &s))
        {
            if (s.st_size < _max_file_size)
                _file = fopen(filename, "a");
            else
                _file = fopen(filename, "w");
        }
        else
        {
            _file = fopen(filename, "w");
        }
        if (NULL == _file)
        {
            fprintf(stderr, "[%s] fopen fail,file:%s errno:%d\n", currentTime().c_str(), filename, errno);
            return 0;
        }
        _cur_file_size = 0;
    }
    int ret = 0;
    ret = fprintf(_file, "%s\n", msg.c_str());
    if (ret < int(msg.length()) + 1)
    {
        fprintf(stderr, "[%s] fprintf fail,msg:%s fileid:%d ret:%d\n", currentTime().c_str(), msg.c_str(), _cur_file_id, ret);
        return ret;
    }
    fflush(_file);  // TODO
    _cur_file_size += ret;
    if (_cur_file_size >= _max_file_size)
    {
        ret = fclose(_file);
        if (ret != 0)
        {
            fprintf(stderr, "[%s] fclose fail,fileid:%d errno:%d\n", currentTime().c_str(), _cur_file_id, errno);
            return ret;
        }
        _file = NULL;
        _cur_file_id = (_cur_file_id + 1) % _max_file_num;

        char filename[1024];
        snprintf(filename, 1024, "%s.logid", _base_name.c_str());
        FILE* fp = fopen(filename, "w");
        if (fp != NULL)
        {
            fprintf(fp, "%d", _cur_file_id);
            fclose(fp);
        }
    }
    return ret;
}

