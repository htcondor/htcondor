#ifndef CFTP_CLIENT_LIB_H
#define CFTP_CLIENT_LIB_H

// Uncomment to enable client debug messages 
#define CLIENT_DEBUG 1

#include <stdio.h>
#include "frames.h"
#include "simple_parameters.h"
#include "utilities.h"
#include "sha1-c/sha1.h"

int transfer_file( char* server_name,
				   char* server_port,
				   char* localfile );

ServerRecord* connect_to_server(  char* server_name, 
								  char* server_port );

int execute_negotiation( FileRecord* record,
						 ServerRecord* server,
						 simple_parameters* parameters );

int execute_transfer( FileRecord* record,
					  ServerRecord* server,
					  simple_parameters* parameters );

int execute_teardown( FileRecord* record,
					  ServerRecord* server,
					  simple_parameters* parameters,
					  int results );

#endif
