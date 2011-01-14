#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "server_lib.h"

void run_server(const char* server_name, const char* server_port)
{

	int results;
	
	FileRecord* localFileCopy = NULL;
	ServerRecord* localServer = NULL;
	ClientRecord* remoteClient = NULL;

		//localServer = start_server( server_name, server_port );

#ifdef SERVER_DEBUG
	if( localServer == NULL )
		fprintf( stderr, "Unable to create local server instance. Aborting.\n" );
#endif

	if( localServer == NULL )
		return;

		//remoteClient = accept_client( localServer );

#ifdef SERVER_DEBUG
	if( remoteClient == NULL )
		fprintf( stderr, "Failure when attempting to connect with client. Aborting.\n" );
#endif

	if( remoteClient == NULL )
		return;	

		//localFileCopy = execute_negotiation( localServer, remoteClient );

#ifdef SERVER_DEBUG
	if( localFileCopy == NULL )
		fprintf( stderr, "Failure to negotiate transfer parameters with client. Aborting.\n" );
#endif

	if( localFileCopy == NULL )
		return;	
	
		//results = execute_transfer( localServer, remoteClient, localFileCopy );

#ifdef SERVER_DEBUG
	if( results == -1 )
		fprintf( stderr, "Failure to transfer file from client. Aborting.\n" );
#endif

	if( results == -1 )
		return;		

		//results  = execute_teardown( localServer, remoteClient, localFileCopy );

#ifdef SERVER_DEBUG
	if( results == -1 )
		fprintf( stderr, "Failure to teardown after transfer. Aborting.\n" );
#endif

	if( results == -1 )
		return;	

	
	close( remoteClient->client_socket );
	close( localServer->server_socket );
	fclose( localFileCopy->fp );
	free( remoteClient );
	free( localServer );
	free( localFileCopy );

	return;
}
