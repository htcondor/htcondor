#include <stdio.h>
#include "frames.h"
#include "client_lib.h"

int main( int argc, char** argv )
{

	int ret_code;

	char* server_name;
	char* server_port;
	char* local_filename;

	int transfer_results;

	if( argc < 4 )
		{
			printf( "Usage: %s SERVER PORT FILENAME\n", argv[0] );	
			return 1;	
		}

	server_name = argv[1];
	server_port = argv[2];
	local_filename = argv[3];
	
	transfer_results = transfer_file(server_name, server_port, local_filename);

	ret_code = 1; //Assume failure

	switch( transfer_results )
		{
		case NOERROR:
			printf("Transfer completed successfully.\n");
			ret_code = 0;
			break;
		case CLIENT_TIMEOUT:
			printf("Transfer Failed. Timeout occured when communicating with Server.\n");
			break;
		case SERVER_TIMEOUT:
			printf("Transfer Failed. Timeout occured when communicating with Client.\n");
			break;
		case UNKNOWN_FORMAT:
			printf("Transfer Failed. UNKNOWN_FORMAT\n");
			break;
		case OVERLOADED:
			printf("Transfer Failed. OVERLOADED\n");
			break;
		case TIMEOUT:
			printf("Transfer Failed. TIMEOUT\n");
			break;
		case DUPLICATE_SESSION_TOKEN:
			printf("Transfer Failed. DUPLICATE_SESSION_TOKEN\n");
			break;
		case MISSING_PARAMETER:
			printf("Transfer Failed. MISSING_PARAMETER\n");
			break;
		case NO_SESSION:
			printf("Transfer Failed. NO_SESSION\n");
			break;
		case UNACCEPTABLE_PARAMETERS:
			printf("Transfer Failed. UNACCEPTABLE_PARAMETERS\n");
			break;
		case NO_DISK_SPACE:
			printf("Transfer Failed. NO_DISK_SPACE\n");
			break;
		}
		

	return ret_code;

}
