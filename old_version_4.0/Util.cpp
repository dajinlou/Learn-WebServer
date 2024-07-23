#include "Util.h"

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

ssize_t readn(int fd, void *buff, size_t n)
{
    size_t nleft = n;  //剩余的未读的字符个数
    ssize_t nread = 0; // 读到的字符数
    ssize_t readSum = 0; //已读到的字符个数

    char *ptr = (char *)buff;  //指向待存储位置
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR) // // EINTR 表示一个系统调用被中断信号所中断
            {
                nread = 0;
            }
            else if (errno == EAGAIN) //资源暂时不可用,这通常发生在非阻塞模式的系统调用中，如尝试读取一个没有数据的文件描述符
            {
                return readSum;
            }
            else
            {
                return -1;
            }
        }
        else if(nread == 0){
            break;
        }
        readSum += nread;
        nleft-=nread;
        ptr += nread;
    }
    return readSum;
}

//写操作
ssize_t writen(int fd, void *buff, size_t n) {
    size_t nleft = n;
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    char *ptr = (char *)buff;

    while(nleft>0){
        if((nwritten = write(fd,ptr,nleft))<=0){
            if(nwritten<0){
                if(errno == EINTR || errno == EAGAIN){
                    nwritten = 0;
                    continue;
                }
                else{
                    return -1;
                }
            }
        }
         writeSum+=nwritten;
         nleft -= nwritten;
         ptr+=nwritten;
    }
    return writeSum;
}

//处理信号
void handle_for_sigpipe() {
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE,&sa,NULL)){  //设置信号处理动作
        return;
    }

}

//设置非阻塞
int setSocketNonBlocking(int fd) {
    int flag = fcntl(fd,F_GETFL,0);
    if(flag == -1){
        return -1;
    }
    flag |= O_NONBLOCK;
    if(fcntl(fd,F_SETFL,flag) == -1){
        return -1;
    }
    return 0;
}
