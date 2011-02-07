#include <stdio.h>
#include "frames.h"
#include "server_sm_lib.h"

int main( int argc, char** argv )
{

	char* server_name;
	char* server_port;

	if( argc < 3 )
		{
			printf( "Usage: %s SERVER PORT\n", argv[0] );	
			return 1;	
		}

	server_name = argv[1];
	server_port = argv[2];
	
		//TODO: Write server main loop and associated handling code.
	run_server(server_name, server_port);

	return 0;
}
