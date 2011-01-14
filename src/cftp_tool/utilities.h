#ifndef CFTP_UTILITIES_H
#define CFTP_UTILITIES_H

typedef struct _FileRecord 
{
	const char* filename;
	FILE* fp;
	long  file_size;
} FileRecord;

typedef struct _ServerRecord
{
	const char* server_name;
	const char* server_port;
	int         server_socket;
} ServerRecord;

typedef struct _ClientRecord
{
	const char* client_name;
	const char* client_port;
	int         client_socket;
} ClientRecord;


//sendall - taken from the Beej guide to socket programming
int sendall(int s, char *buf, int *len);

#endif
