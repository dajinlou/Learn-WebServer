#ifndef REQUESTDATA_H_
#define REQUESTDATA_H_
#include <string>
#include <unordered_map>
#include <memory>

const int STATE_PARSE_URI = 1;     // 解析URI
const int STATE_PARSE_HEADERS = 2; // 解析头
const int STATE_RECV_BODY = 3;     // 接收消息体
const int STATE_ANALYSIS = 4;      // 分析
const int STATE_FINISH = 5;        // 完成

const int MAX_BUFF = 4096;

// 有请求出现但是读不到数据，可能是Request Aborted
// 或者来自网络的数据没有达到等原因
// 对这样的请求尝试超过一定的次数就抛弃
const int AGAIN_MAX_TIMES = 200;

const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

const int PARSE_HEADER_AGAIN = -1;  // 需要更多数据来完成解析。
const int PARSE_HEADER_ERROR = -2;  // 解析头部时发生错误。
const int PARSE_HEADER_SUCCESS = 0; // 成功解析完头部信息。

const int ANALYSIZ_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;

const int METHOD_POST = 1;
const int METHOD_GET = 2;
const int HTTP_10 = 1;
const int HTTP_11 = 2;

const int EPOLL_WAIT_TIME = 500;

class MimeType
{
private:
    static pthread_mutex_t lock;
    static std::unordered_map<std::string, std::string> mime;
    MimeType();
    MimeType(const MimeType &m);

public:
    static std::string getMime(const std::string &suffix);
};

enum Enum_HeadersState
{
    HEAD_START = 0,          // 初始状态，等待读取头部字段的开始
    HEAD_KEY,                // 正在读取头部字段的名称
    HEAD_COLON,              // 读取到冒号，等待空格
    HEAD_SPACES_AFTER_COLON, // 读取到冒号后的空格，等待头部字段的值
    HEAD_VALUE,              // 正在读取头部字段的值
    HEAD_CR,                 // 读取到回车符(\r),等待换行符(\n)        CRLF 回车换行符
    HEAD_LF,                 // 读取到换行符(\n),准备读取下一行或结束解析。
    HEAD_END_CR,             // 读取到请求结束的回车符(\r)
    HEAD_END_LF              // 读取到请求结束的换行符(\n)
};

struct MyTimer;
class RequestData;

class RequestData : public std::enable_shared_from_this<RequestData>
{

private:
    int againTimes;
    std::string path;
    int fd;
    int epollfd;
    // content的内容用完就清
    std::string content;
    int method;
    int HTTP_version;
    std::string file_name;
    int now_read_pos;
    int state;
    int head_state;
    bool isFinish;
    bool keep_alive;
    std::unordered_map<std::string, std::string> headers;
    std::weak_ptr<MyTimer> timer;

private:
    // 解析请求行
    int parse_URI();

    // 解析请求头部
    int parse_Headers();

    //响应
    int analysisRequest();

public:
    RequestData();
    RequestData(int _epollfd, int _fd, std::string _path);
    ~RequestData();

    void linkTimer(std::shared_ptr<MyTimer> mtimer);
    void reset();
    void seperateTimer();
    int getFd();
    void setFd(int _fd);
    void handleRequest();
    void handleError(int fd, int err_num, std::string short_msg);
};

#endif // REQUESTDATA_H_