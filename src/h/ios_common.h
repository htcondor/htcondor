#ifndef _IOS_COMMON_H
#define _IOS_COMMON_H

#define MAX_FIRST_MSG_LEN 1024    //This is the max length of the message thats
                                  // expected. If you get this return value from
                                  // recvfrom, then another recvfrom has to be
                                  // done to get the full message.

enum Msgtype { OPEN, CLOSE, READ, WRITE, LSEEK, FSTAT } ;

enum { OPEN_REQ=1, SERVER_DOWN } ;

#endif
