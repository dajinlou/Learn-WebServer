#include "RequestData.h"
#include "MyTimer.h"
#include "Util.h"
#include "Epoll.h"
#include "MutexLockGuard.h"
#include "Debug.h"

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <vector>
#include <queue>

#include <opencv/cv.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;



pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
// 类外声明，类内初始化
pthread_mutex_t MimeType::lock = PTHREAD_MUTEX_INITIALIZER;
std::unordered_map<std::string, std::string> MimeType::mime;
priority_queue<shared_ptr<MyTimer>, deque<shared_ptr<MyTimer>>, TimerCmp> myTimerQueue;

std::string MimeType::getMime(const std::string &suffix)
{
    if (mime.size() == 0)
    {
        pthread_mutex_lock(&lock);
        mime[".html"] = "text/html";
        mime[".avi"] = "video/x-msvideo";
        mime[".bmp"] = "image/bmp";
        mime[".c"] = "text/plain";
        mime[".doc"] = "application/msword";
        mime[".gif"] = "image/gif";
        mime[".gz"] = "application/x-gzip";
        mime[".htm"] = "text/html";
        mime[".ico"] = "application/x-ico";
        mime[".jpg"] = "image/jpeg";
        mime[".png"] = "image/png";
        mime[".txt"] = "text/plain";
        mime[".mp3"] = "audio/mp3";
        mime["default"] = "text/html";
    }
    pthread_mutex_unlock(&lock);
    if (mime.find(suffix) == mime.end())
    {
        return mime["default"];
    }
    else
    {
        return mime[suffix];
    }
}

int RequestData::parse_URI()
{
    string &str = content;
    // 读到完整的请求行再开始解析请求
    int pos = str.find('\r', now_read_pos);
    if (pos < 0)
    {
        return PARSE_HEADER_AGAIN;
    }
    // 去掉请求行所占空间，节省空间
    string request_line = str.substr(0, pos);
    if (str.size() > pos + 1)
    {
        str = str.substr(pos + 1);
    }
    else
    {
        str.clear();
    }
    // Method
    pos = request_line.find("GET");
    if (pos < 0)
    {
        pos = request_line.find("POST");
        if (pos < 0)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
            method = METHOD_POST;
            debug()<<"是 post 请求......."<<endl;
        }
    }
    else
    {
        method = METHOD_GET;
    }

    // 文件名
    pos = request_line.find('/', pos);
    if (pos < 0)
    {
        return PARSE_URI_ERROR;
    }
    else
    {
        int _pos = request_line.find(' ', pos);
        if (_pos < 0)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
            if (_pos - pos > 1)
            {
                file_name = request_line.substr(pos + 1, _pos - pos - 1);
                int __pos = file_name.find('?');
                if (__pos >= 0)
                {
                    file_name = file_name.substr(0, __pos);
                }
            }
            else
            {
                file_name = "index.html";
            }
        }
        debug() << "file_name:" << file_name << endl;
        pos = _pos;
    }
    // HTTP 版本号
    pos = request_line.find("/", pos);
    if (pos < 0)
    {
        return PARSE_URI_ERROR;
    }
    else
    {
        if (request_line.size() - pos <= 3)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
            string ver = request_line.substr(pos + 1, 3);
            if (ver == "1.0")
            {
                HTTP_version = HTTP_10;
            }
            else if (ver == "1.1")
            {
                HTTP_version = HTTP_11;
            }
            else
            {
                return PARSE_URI_ERROR;
            }
        }
    }
    state = STATE_PARSE_HEADERS;
    return PARSE_URI_SUCCESS;
}

// 解析头部
int RequestData::parse_Headers()
{
    string &str = content;
    int key_start = -1;
    int key_end = -1;
    int value_start = -1;
    int value_end = -1;
    int now_read_line_begin = 0;
    bool notFinish = true;
    for (int i = 0; i < str.size() && notFinish; ++i)
    {
        switch (head_state)
        {
        case HEAD_START:
        {
            if (str[i] == '\n' || str[i] == '\r')
            {
                break;
            }
            head_state = HEAD_KEY;
            key_start = i;
            now_read_line_begin = i;
            break;
        }
        case HEAD_KEY:
        {
            if (str[i] == ':')
            {
                key_end = i;
                if (key_end - key_start <= 0)
                {
                    return PARSE_HEADER_ERROR;
                }
                head_state = HEAD_COLON;
            }
            else if (str[i] == '\n' || str[i] == '\r')
            {
                return PARSE_HEADER_ERROR;
            }
            break;
        }
        case HEAD_COLON:
        {
            if (str[i] == ' ')
            {
                head_state = HEAD_SPACES_AFTER_COLON;
            }
            else
            {
                return PARSE_HEADER_ERROR;
            }
            break;
        }
        case HEAD_SPACES_AFTER_COLON:
        {
            head_state = HEAD_VALUE;
            if (str[i] == '\n' || str[i] == '\r')
                break;
            value_start = i;
            break;
        }
        case HEAD_VALUE:
        {
            if (str[i] == '\r')
            {
                head_state = HEAD_CR;
                value_end = i;
                if (value_end - value_start <= 0)
                {
                    return PARSE_HEADER_ERROR;
                }
            }
            else if (i - value_start > 255)
            {
                return PARSE_HEADER_ERROR;
            }
            break;
        }
        case HEAD_CR:
        {
            if (str[i] == '\n')
            {
                head_state = HEAD_LF;
                string key(str.begin() + key_start, str.begin() + key_end);
                string value(str.begin() + value_start, str.begin() + value_end);
                headers[key] = value;
                now_read_line_begin = i;
            }
            else
            {
                return PARSE_HEADER_ERROR;
            }
            break;
        }
        case HEAD_LF:
        {
            if (str[i] == '\r')
            {
                head_state = HEAD_END_CR;
            }
            else
            {
                key_start = i;
                head_state = HEAD_KEY;
            }
            break;
        }
        case HEAD_END_CR:
        {
            if (str[i] == '\n')
            {
                head_state = HEAD_END_LF;
            }
            else
            {
                return PARSE_URI_ERROR;
            }
            break;
        }
        case HEAD_END_LF:
        {
            notFinish = false;
            key_start = i;
            now_read_line_begin = i;
            break;
        }
        }
    }
    if (head_state == HEAD_END_LF)
    {
        str = str.substr(now_read_line_begin);
        debug()<<"用户名和密码:"<<str<<endl;
        return PARSE_HEADER_SUCCESS;
    }
    str = str.substr(now_read_line_begin);
    return PARSE_HEADER_AGAIN;
}

int RequestData::analysisRequest()
{
    debug()<<"method: "<<method<<endl;
    if (method == METHOD_POST)
    {
        // get content
        char header[MAX_BUFF];
        sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
        if (headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
        {
            keep_alive = true;
            sprintf(header, "%sConnection: keep-alive\r\n", header);
            sprintf(header, "%skeep-Alive: timeout=%d", header, EPOLL_WAIT_TIME);
        }
        char *send_content = "I have receiced this.";

        sprintf(header, "%sContent-length: %zu\r\n", header, strlen(send_content));
        sprintf(header, "%s\r\n", header);
        size_t send_len = (size_t)writen(fd, header, strlen(header));
        if (send_len != strlen(header))
        {
            perror("Send header failed");

            return ANALYSIZ_ERROR;
        }
        send_len = (size_t)writen(fd, send_content, strlen(send_content));
        if (send_len != strlen(send_content))
        {
            perror("Send content failed");
            return ANALYSIZ_ERROR;
        }
        debug() << "content size ==" << content.size() << endl;
        vector<char> data(content.begin(), content.end());
        // imdecode 函数用于从内存中的二进制数据 data 解码图像。
        Mat test = imdecode(data, CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR);
        // imwrite 函数用于将 Mat 对象 test 写入文件。
        imwrite("receive.bmp", test);
        return ANALYSIS_SUCCESS;
    }
    else if (method == METHOD_GET)
    {
        char header[MAX_BUFF];

        sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
        if (headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
        {
            keep_alive = true;
            sprintf(header, "%sConnection: keep-alive\r\n", header);
            sprintf(header, "%skeep-Alive: timeout=%d", header, EPOLL_WAIT_TIME);
        }
        int dot_pos = file_name.find('.');
        const char *filetype;
        if (dot_pos < 0)
        {
            filetype = MimeType::getMime("default").c_str();
        }
        else
        {
            filetype = MimeType::getMime(file_name.substr(dot_pos)).c_str();
        }
        struct stat sbuf;
        if (stat(file_name.c_str(), &sbuf) < 0)
        {
            handleError(fd, 404, "Not Found!");
            return ANALYSIZ_ERROR;
        }

        sprintf(header, "%sContent-type: %s\r\n", header, filetype);
        // 通过Content-length返回文件大小
        sprintf(header, "%sContent-length: %ld\r\n", header, sbuf.st_size);

        sprintf(header, "%s\r\n", header);
        size_t send_len = (size_t)writen(fd, header, strlen(header));
        if (send_len != strlen(header))
        {
            perror("Send header failed");
            return ANALYSIZ_ERROR;
        }
        int src_fd = open(file_name.c_str(), O_RDONLY, 0);
        // mmap,它用于将文件或者其他对象映射到进程的地址空间,这样，进程就可以像访问普通内存一样访问这些对象
        // addr:通常为NULL,让系统选择映射区域   offset:文件映射的其实位置
        char *src_addr = static_cast<char *>(mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
        close(src_fd);

        // 发送文件并检验完整性
        send_len = writen(fd, src_addr, sbuf.st_size);
        if (send_len != sbuf.st_size)
        {
            perror("Send file failed");
            return ANALYSIZ_ERROR;
        }
        munmap(src_addr, sbuf.st_size);
        return ANALYSIS_SUCCESS;
    }
    else
    {
        return ANALYSIZ_ERROR;
    }
}

RequestData::RequestData()
    : now_read_pos(0),
      state(STATE_PARSE_URI),
      head_state(HEAD_START),
      keep_alive(false),
      againTimes(0)
{
    debug() << "requestData constructed !" << endl;
}

RequestData::RequestData(int _epollfd, int _fd, std::string _path)
    : now_read_pos(0),
      state(STATE_PARSE_URI),
      head_state(HEAD_START),
      keep_alive(false),
      againTimes(0),
      path(_path),
      fd(_fd),
      epollfd(_epollfd)
{
}

RequestData::~RequestData()
{
#if DEBUG
    debug() << "~requestData()" << endl;
#endif
    // struct epoll_event ev;
    // 超时的一定都是读请求，没有“被动”写.
    // ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    // ev.data.ptr = (void *)this;
    // epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
    // if (timer != NULL)
    // {
    //     timer->clearReq();
    //     timer = NULL;
    // }
    close(fd);
}

void RequestData::addTimer(std::shared_ptr<MyTimer> mtimer)
{
        timer = mtimer;
}

void RequestData::reset()
{
    againTimes = 0;
    content.clear();
    file_name.clear();
    path.clear();
    now_read_pos = 0;
    state = STATE_PARSE_URI;
    head_state = HEAD_START;
    headers.clear();
    keep_alive = false;
}

void RequestData::seperateTimer()
{
    //1. 获取强引用 lock()寒素是用于从std::weak_ptr获取一个std::shared_ptr,如果不存在返回一个空的std::shared_ptr
    if (timer.lock())
    {
        shared_ptr<MyTimer> my_timer(timer.lock());
        my_timer->clearReq();
        my_timer.reset();
    }
}

int RequestData::getFd()
{
    return fd;
}

void RequestData::setFd(int _fd)
{
    fd = _fd;
}

void RequestData::handleRequest()
{
    char buff[MAX_BUFF];
    bool isError = false;
    while (true)
    {
        int read_num = readn(fd, buff, MAX_BUFF);
        if (read_num < 0)
        {
            perror("1");
            isError = true;
            break;
        }
        else if (read_num == 0)
        {
            // 有请求出现但是读不到数据，可能是Request Aborted，或者来自网络的数据没有达到等原因
            perror("read_num == 0");
            if (errno == EAGAIN)
            {
                if (againTimes > AGAIN_MAX_TIMES)
                    isError = true;
                else
                    ++againTimes;
            }
            else if (errno != 0)
                isError = true;
            break;
        }
        string now_read(buff, buff + read_num);
        content += now_read;
        debug() <<"\n"<< content << " --->" << againTimes << endl;
        if (state == STATE_PARSE_URI)
        {
            int flag = this->parse_URI();
            if (flag == PARSE_URI_AGAIN)
            {
                break;
            }
            else if (flag == PARSE_URI_ERROR)
            {
                perror("2");
                isError = true;
                break;
            }
        }
        if (state == STATE_PARSE_HEADERS)
        {
            int flag = this->parse_Headers();
            if (flag == PARSE_HEADER_AGAIN)
            {
                break;
            }
            else if (flag == PARSE_HEADER_ERROR)
            {
                perror("3");
                isError = true;
                break;
            }
            if (method == METHOD_POST)
            {
                state = STATE_RECV_BODY;
            }
            else
            {
                state = STATE_ANALYSIS;
            }
        }
        if (state == STATE_RECV_BODY)
        {
            int content_length = -1;
            if (headers.find("Content-Length") != headers.end())
            {
                content_length = stoi(headers["Content-Length"]);
               
            }
            else
            {
                isError = true;
                break;
            }
             debug()<<"content_length: "<<content.size()<<endl;
            if (content.size() < content_length)
                continue;
            state = STATE_ANALYSIS;
        }
        if (state == STATE_ANALYSIS)
        {
            int flag = this->analysisRequest();
            if (flag < 0)
            {
                isError = true;
                break;
            }
            else if (flag == ANALYSIS_SUCCESS)
            {

                state = STATE_FINISH;
                break;
            }
            else
            {
                isError = true;
                break;
            }
        }
    }

    if (isError)
    {
        // delete this;
        debug() << "handleRequest遇见错误" <<endl;
        return;
    }
    // 加入epoll继续
    if (state == STATE_FINISH)
    {
        if (keep_alive)
        {
            printf("ok\n");
            this->reset();
        }
        else
        {
            // delete this;
            return;
        }
    }
    // 一定要先加时间信息，否则可能会出现刚加进去，下个in触发来了，然后分离失败后，又加入队列，最后超时被删，然后正在线程中进行的任务出错，double free错误。
    // 新增时间信息

    //TODO:注意这里不能用this
    // shared_ptr<MyTimer> mtimer(new MyTimer(this, 500));
    shared_ptr<MyTimer> mtimer(new MyTimer(shared_from_this(), 500));
    this->addTimer(mtimer);
    {
        //TODO: 很妙 直接缩小作用域
        MutexLockGuard lock;
        myTimerQueue.push(mtimer);
    }
    
       

    __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    int ret = Epoll::epoll_mod(fd, shared_from_this(), _epo_event);
    if (ret < 0)
    {
        // 返回错误处理
        // delete this;
        return;
    }
}

void RequestData::handleError(int fd, int err_num, std::string short_msg)
{
    short_msg = " " + short_msg;
    char send_buff[MAX_BUFF];
    string body_buff, header_buff;
    body_buff += "<html><title>TKeed Error</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += to_string(err_num) + short_msg;
    body_buff += "<hr><em> JinJunHui's Web Server</em>\n</body></html>";

    header_buff += "HTTP/1.1 " + to_string(err_num) + short_msg + "\r\n";
    header_buff += "Content-type: text/html\r\n";
    header_buff += "Connection: close\r\n";
    header_buff += "Content-length: " + to_string(body_buff.size()) + "\r\n";
    header_buff += "\r\n";
    sprintf(send_buff, "%s", header_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
    sprintf(send_buff, "%s", body_buff.c_str());
    writen(fd, send_buff, strlen(send_buff));
}

bool TimerCmp::operator()(std::shared_ptr<MyTimer> &a,std::shared_ptr<MyTimer> &b) const
{
    return a->getExpTime() > b->getExpTime();
}
