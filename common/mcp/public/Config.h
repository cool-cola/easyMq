#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <tr1/unordered_map>
#include <boost/shared_ptr.hpp>

using namespace std;
using namespace boost;

class Config
{
    public:
        Config(const string& cmd, const string& filename);
        virtual ~Config();

        // every derived class should implement this interface
        // @return: 0  --> ok
        //          -1 --> fail
        virtual int32_t init();
        
        string getConfStr(const string& name);  // if not exist: return ""
        int32_t getConfInt(const string& name); // if not exist: return -1

        /**
         *  @return:    0   --> yes
         *              -1  --> no
         * */
        int32_t setConfStr(const string& name, const string& default_value = "");
        int32_t setConfInt(const string& name, int32_t default_value = -1);

        string toString();

        string getFilename();
        string getProcName();
        string getDirname();

    private:
        string readItem(const string& name); // if read fail or not exist: return ""

        tr1::unordered_map<string, string> _conf_str;   // store all the config: [key, value]
        tr1::unordered_map<string, int32_t> _conf_int;

        string _filename;
        string _proc_name;
        string _dirname;
};

typedef shared_ptr<Config> ConfigPtr;

#endif // _CONFIG_H_

