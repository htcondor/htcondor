#ifndef CFTP_CLIENT_LIB_H
#define CFTP_CLIENT_LIB_H

// Uncomment to enable client debug messages 
#define CLIENT_DEBUG 1

#include <stdio.h>
#include "frames.h"
#include "simple_parameters.h"

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

int transfer_file( const char* server_name,
						   const char* server_port,
						   const char* localfile );

FileRecord* open_file( const char* filename );

ServerRecord* connect_to_server( const char* server_name, 
								 const char* server_port );

int execute_negotiation( FileRecord* record,
						 ServerRecord* server,
						 simple_parameters* parameters );

int execute_transfer( FileRecord* record,
					  ServerRecord* server,
					  simple_parameters* parameters );


#endif
