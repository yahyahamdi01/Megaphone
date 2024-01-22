#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/types.h>

int recv_message(int, char *, size_t);
int send_message(int, const char *, size_t);

#endif
