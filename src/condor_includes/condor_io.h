#ifndef CONDOR_IO_H
#define CONDOR_IO_H


#include "condor_common.h"
#include "condor_constants.h"

#if !defined(WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#if defined(OSF1) && defined(__cplusplus)

extern "C" {
void bzero(char *, int);
int gethostname(char *, int);

unsigned int htonl(unsigned int);
unsigned int ntohl(unsigned int);
unsigned short htons(unsigned short);
unsigned short ntohs(unsigned short);
}

#endif

/*
#if defined(__cplusplus)

extern "C" {
int accept(int, void *, int *);
int bind(int, void *, int);
int connect(int, void *, int);
int listen(int, int);
int socket(int, int, int);
int getsockname(int, struct sockaddr *,int *);
}

#endif
*/

#include "stream.h"
#include "buffers.h"
#include "sock.h"
#include "reli_sock.h"
#include "safe_sock.h"


#endif
