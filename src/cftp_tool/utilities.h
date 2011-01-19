#ifndef CFTP_UTILITIES_H
#define CFTP_UTILITIES_H

typedef struct _FileRecord 
{
	char* filename;
	FILE* fp;
	long  file_size;
} FileRecord;

typedef struct _ServerRecord
{
	char* server_name;
	char* server_port;
	int         server_socket;
} ServerRecord;

typedef struct _ClientRecord
{
	char* client_name;
	char* client_port;
	int         client_socket;
} ClientRecord;


//sendall - taken from the Beej guide to socket programming
int sendall(int s, char *buf, int *len);

//implement the host to network/network to host byte order 
//conversions for long datatypes
// This implementation was found from http://www.codeproject.com/KB/cpp/endianness.aspx
#define ntohll(x) (	((unsigned long)(ntohl((int)((x << 32) >> 32))) << 32) | (unsigned int)ntohl(((int)(x >> 32)))	) //By Runner

#define htonll(x) ntohll(x)

#endif
