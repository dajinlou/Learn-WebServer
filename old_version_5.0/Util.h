#ifndef UTIL_H_
#define UTIL_H_
#include <cstdlib>
#include <string>

ssize_t readn(int fd,void *buff,size_t n);
ssize_t readn(int fd, std::string &inBuffer);
ssize_t writen(int fd,void *buff,size_t n);
ssize_t writen(int fd,std::string &sbuff);

void handle_for_sigpipe();
int setSocketNonBlocking(int fd);

#endif //UTIL_H_