#ifndef _COMMON_H
#define _COMMON_H

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <strings.h>
#include <malloc.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_FIRST_MSG_LEN 1024    //This is the max length of the message thats
                                  // expected. If you get this return value from
                                  // recvfrom, then another recvfrom has to be
                                  // done to get the full message.

enum Msgtype { OPEN, CLOSE, READ, WRITE, LSEEK, FSTAT } ;

enum { OPEN_REQ=1, SERVER_DOWN } ;
#endif
