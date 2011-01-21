#ifndef CFTP_SERVER_LIB_H
#define CFTP_SERVER_LIB_H

// Uncomment to enable client debug messages 
#define SERVER_DEBUG 1

#include "frames.h"
#include "simple_parameters.h"
#include "utilities.h"

void run_server( char* server_name,  char* server_port);
ServerRecord* start_server(  char* server_name,  char* server_port );
ClientRecord* accept_client( ServerRecord* localServer );
FileRecord* execute_negotiation( ServerRecord* localServer, ClientRecord* remoteClient );
int execute_transfer( ServerRecord* localServer, ClientRecord* remoteClient, FileRecord* localFileCopy );


#endif
