#ifndef CFTP_UTILITIES_H
#define CFTP_UTILITIES_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

//sendall - taken from the Beej guide to socket programming
int sendall(int s, char *buf, int *len);

//readOOB - reads a block of data from UDP socket, with timeouts
int readOOB(int s, void* buf, int len, int flags,
			struct sockaddr *from, socklen_t *fromlen,
			int timeout, char termChar);

//implement the host to network/network to host byte order 
//conversions for long datatypes
// This implementation was found from http://www.codeproject.com/KB/cpp/endianness.aspx
#define ntohll(x) (	((unsigned long)(ntohl((int)((x << 32) >> 32))) << 32) | (unsigned int)ntohl(((int)(x >> 32)))	) //By Runner

#define htonll(x) ntohll(x)

#endif
