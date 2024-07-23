#ifndef DEBUG_H_
#define DEBUG_H_
#include <iostream>

using namespace std;


#define debug() cout <<"[ " << get_filename(__FILE__) << " : "<<__LINE__ <<" ] "


// 获取文件名 为避免多重定义，可以将其定义成inline 或 static
inline std::string get_filename(const char* filepath) {
    std::string path(filepath);
    size_t pos = path.rfind('/');
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path; // 如果没有找到'/'，返回整个路径
}
#endif //DEBUG_H_