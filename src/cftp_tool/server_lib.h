#ifndef CFTP_SERVER_LIB_H
#define CFTP_SERVER_LIB_H

// Uncomment to enable client debug messages 
#define SERVER_DEBUG 1

#include "frames.h"
#include "simple_parameters.h"
#include "utilities.h"

void run_server(const char* server_name, const char* server_port);
ServerRecord* start_server( const char* server_name, const char* server_port );


#endif
