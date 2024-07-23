#ifndef UTIL_H_
#define UTIL_H_
#include <cstdlib>

ssize_t readn(int fd,void *buff,size_t n);
ssize_t writen(int fd,void *buff,size_t n);

void handle_for_sigpipe();
int setSocketNonBlocking(int fd);

#endif //UTIL_H_