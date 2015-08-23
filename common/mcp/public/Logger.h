#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace boost;
using namespace boost::io;
using boost::format;

#define LOG_TRACE(logger, msg) { \
        if ((logger)->isLogEnable(Logger::TRACE)) { \
            (logger)->append((neformat("[%1%] [TRACE] [%2%:%3%] %4%") % currentTime() % __FILE__ % __LINE__ % (msg)).str()); }}

#define LOG_DEBUG(logger, msg) { \
        if ((logger)->isLogEnable(Logger::DEBUG)) { \
            (logger)->append((neformat("[%1%] [DEBUG] [%2%:%3%] %4%") % currentTime() % __FILE__ % __LINE__ % (msg)).str()); }}

#define LOG_INFO(logger, msg) { \
        if ((logger)->isLogEnable(Logger::INFO)) { \
            (logger)->append((neformat("[%1%] [INFO] [%2%:%3%] %4%") % currentTime() % __FILE__ % __LINE__ % (msg)).str()); }}

#define LOG_WARN(logger, msg) { \
        if ((logger)->isLogEnable(Logger::WARN)) { \
            (logger)->append((neformat("[%1%] [WARN] [%2%:%3%] %4%") % currentTime() % __FILE__ % __LINE__ % (msg)).str()); }}

#define LOG_ERROR(logger, msg) { \
        if ((logger)->isLogEnable(Logger::ERROR)) { \
            (logger)->append((neformat("[%1%] [ERROR] [%2%:%3%] %4%") % currentTime() % __FILE__ % __LINE__ % (msg)).str()); }}

#define LOG_FATAL(logger, msg) { \
        if ((logger)->isLogEnable(Logger::FATAL)) { \
            (logger)->append((neformat("[%1%] [FATAL] [%2%:%3%] %4%") % currentTime() % __FILE__ % __LINE__ % (msg)).str()); }}

format neformat(const std::string& f_string);

string currentTime();

class Logger
{
    public:
        enum Level
        {
            TRACE = 0,
            DEBUG = 1,
            INFO  = 2,
            WARN  = 3,
            ERROR = 4,
            FATAL = 5
        };

    public:
        Logger(const string& base_name, int64_t max_file_size, int32_t max_file_num);
        virtual ~Logger();

        int32_t append(const string& msg);

        void setLogLevel(Level level);

        int32_t setLogLevel(const string& level_name);

        bool isLogEnable(Level level);

    private:
        string _base_name;        // filename: _base_name.log

        int64_t _max_file_size;   // every file have _max_file_size, can have _max_file_num files
        int32_t _max_file_num;

        int64_t _cur_file_size;
        int32_t _cur_file_id;     // current filename: _base_name##_cur_file_id##.log

        Level _level;             // default: Level::INFO
        
        FILE* _file;
};

typedef shared_ptr<Logger> LoggerPtr;

#endif // _LOGGER_H_

