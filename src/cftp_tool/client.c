#include <stdio.h>
#include <string.h>
#include "send.h"

int main( int argc, char** argv )
{

	int ret_code;

	char* server_name;
	int   server_port;

	SendResults transfer_results;   
    TransferArguments args;

	if( argc < 4 )
		{
			printf( "Usage: %s SERVER PORT FILENAME\n", argv[0] );	
			return 1;	
		}

    memset( &transfer_results, 0, sizeof( SendResults ));
    memset( &args, 0, sizeof(TransferArguments)); 

    args.debug = 1;
    args.verbose = 1;

	server_name = argv[1];
	sscanf( argv[2], "%d", &server_port);
	args.file_path = argv[3];
	
    ret_code = makeConnection( server_name, server_port, &args.local_socket);
    
    if( ret_code )
        {
            printf("Unable to connect to server at %s:%d.\n", server_name, server_port);
            perror( ret_code );
            closeConnection( &args.local_socket );
            return 1;
        }

	send_file( &args, &transfer_results);

    closeConnection( &args.local_socket );

    return 0;
}
