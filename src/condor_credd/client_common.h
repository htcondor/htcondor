#ifndef __CLIENT_COMMON_H__
#define __CLIENT_COMMON_H__
class ReliSock;

int start_command_and_authenticate (const char * host, int command, ReliSock*&);


#endif
