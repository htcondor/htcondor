#ifndef CFTP_UTILITIES_H
#define CFTP_UTILITIES_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

typedef struct _FileRecord 
{
    char*          path;
	char*          filename;
	FILE*          fp;
	unsigned long  file_size;
	unsigned long  chunk_size;
	unsigned long  num_chunks;   
	unsigned int   hash[5];
} FileRecord;

typedef struct {
    int socket;
    char* address;
	int port;
} Connection;

typedef struct {
    int socket;
    char* address;
	int port;
} Listener;

typedef struct {
    int socket;
    char* address;
	int port;
} Endpoint;

int establishListener( const char* address, int port, Listener* l );
int acceptConnection( Listener* l, Connection* c, int timeout );
int makeConnection( const char* address, int port, Connection* c);
int startConnection( const char* address, int port, Connection* c);
int finishConnection( Connection* c, int timeout, int finish);
int closeConnection( Connection* c );

int makeUDPEndpoint( const char* address, int port, Endpoint* e );
int readUDPEndpoint( Endpoint* e, void* buffer, int len,
                     char* address, int* port, int timeout );
int writeUDPEndpoint( Endpoint* e, void* buffer, int len,
                     const char* address, int port);

void getAddressPort( struct sockaddr_storage* ss, char* address, int* port );

//sendall - taken from the Beej guide to socket programming
int sendall(int s, char *buf, int *len);

//readOOB - reads a block of data from UDP socket, with timeouts
int readOOB(int s, void* buf, int len, int flags,
			struct sockaddr *from, socklen_t *fromlen,
			int timeout, char termChar);

int open_file(  char* filename, FileRecord* record );

//implement the host to network/network to host byte order 
//conversions for long datatypes
// This implementation was found from http://www.codeproject.com/KB/cpp/endianness.aspx
#define ntohll(x) (	((unsigned long)(ntohl((int)((x << 32) >> 32))) << 32) | (unsigned int)ntohl(((int)(x >> 32)))	) //By Runner

#define htonll(x) ntohll(x)

#endif
