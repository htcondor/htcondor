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

	localServer = start_server( server_name, server_port );

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


/*

  start_server


 */
ServerRecord* start_server( const char* server_name, const char* server_port )
{

//Really rusty on socket api - refered to Beej's guide for this
//http://beej.us/guide/bgnet/output/html/multipage/syscalls.html	

	int status;	
	int sock;
	int results;
	struct addrinfo hints;
	struct addrinfo *servinfo;  // will point to the results
	ServerRecord* record;	

	record = (ServerRecord*) malloc(sizeof(ServerRecord));
	memset( record, 0, sizeof( ServerRecord ));

	record->server_name = server_name;
	record->server_port = server_port;

	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	if ((status = getaddrinfo(server_name,
							  server_port,
							  &hints,
							  &servinfo)) != 0) {
		fprintf(stderr, "Unable to resolve server: %s\n", gai_strerror(status));
		free( record );
		return NULL;
	}

	sock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if( sock == -1)
		{
			fprintf( stderr, "Error on creating socket: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);
			free( record );	
			return NULL;
		}	

	results = bind( sock, servinfo->ai_addr, servinfo->ai_addrlen);
	if( results == -1)
		{
			fprintf( stderr, "Error on binding socket: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);	
			free( record );
			return NULL;
		}
	
	results = listen( sock, 1 ); // Using a backlog of 1 for this simple server
	if( results == -1)
		{
			fprintf( stderr, "Error on listening: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);	
			free( record );
			return NULL;
		}	

	record->server_socket = sock;
	//We fill in a ServerRecord so we don't need this anymore.
	freeaddrinfo(servinfo);

	return record;

}
