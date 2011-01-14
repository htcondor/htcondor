#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "client_lib.h"

int transfer_file( const char* server_name,
						   const char* server_port,
						   const char* localfile )
{
	int results;

	FileRecord* record;
	ServerRecord* server;  
	simple_parameters parameters;

		// Open the file and examine its contents
	record = open_file( localfile );
	
	if( record == NULL )
		return UNACCEPTABLE_PARAMETERS;

	#ifdef CLIENT_DEBUG
	fprintf(stderr, "Opened file %s. File size is %d bytes.\n", record->filename, record->file_size );
	#endif

		// Initialize connection with server

	server = connect_to_server( server_name, server_port );
	if( server == NULL )
		return UNACCEPTABLE_PARAMETERS;
	
// Execute the negotiation phase of the protocol

	results = execute_negotiation( record, server, &parameters );
	if( results == -1 )
		return UNACCEPTABLE_PARAMETERS;


// Execute the transfer phase of the protocol

		//results = execute_transfer( record, server, &parameters );


// Execute the teardown phase of the protocol




		// There are rumors that this needs to be closesocket for Windows...
	close( server->server_socket);

	fclose( record->fp );
	free(record);
	return NOERROR;
}


/*
  open_file
  
  Opens a given filename and returns a FileRecord pointer.

  Returns NULL if the file is unable to be opened.

 */
FileRecord* open_file( const char* filename)
{
	FileRecord* record;
	record = (FileRecord*) malloc(sizeof(FileRecord));
	memset( record, 0, sizeof( FileRecord ));

	record->filename = filename;
	record->fp = fopen( filename, "rb" );
	if( record->fp == NULL )
		{
			free(record);
			return NULL;
		}
	
	fseek( record->fp, 0, SEEK_END );
	record->file_size = ftell( record->fp );
	rewind( record->fp);	

	return record;
}


/*

  connect_to_server

  Takes a server name and port and returns a ServerRecord structure.
  If a non-null value is returned, the client is now connected via
  a TCP session to the server.

  Returns NULL if any errors occur during the connection process.

 */

ServerRecord* connect_to_server( const char* server_name, 
								 const char* server_port )
{

//Really rusty on socket api - refered to Beej's guide for this
//http://beej.us/guide/bgnet/output/html/multipage/syscalls.html
	
	int status;	
	int sock;
	int connect_results;
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

	connect_results = connect( sock, servinfo->ai_addr, servinfo->ai_addrlen);
	if( connect_results == -1)
		{
			fprintf( stderr, "Error on connecting with server: %s\n", strerror( errno ) );
			freeaddrinfo(servinfo);	
			free( record );
			return NULL;
		}
	
	record->server_socket = sock;
	//We fill in a ServerRecord so we don't need this anymore.
	freeaddrinfo(servinfo);

	return record;
}

/*

  execute_negotiation


 */
int execute_negotiation( FileRecord* record,
						 ServerRecord* server,
						 simple_parameters* parameters )
{
	cftp_sif_frame dframe;
	int length;


    // Set up parameter structure 

	memset( parameters, 0, sizeof( simple_parameters ) );
	memcpy( parameters->filename, record->filename, strlen( record->filename));
	
	parameters->filesize = htons(record->file_size);
	
		//For now we will use a hardcoded chunk size, but this may change
	parameters->chunk_size = htons(100);

		// Count how many chunks there will be.
	parameters->num_chunks = parameters->filesize/parameters->chunk_size;
	if( parameters->filesize % parameters->chunk_size != 0 )
		parameters->num_chunks = parameters->num_chunks + 1;
	parameters->num_chunks = htons(parameters->num_chunks);

	
	// Setup the message frame structure 

	memset( &dframe, 0, sizeof( cftp_sif_frame ) );
	dframe.MessageType = htons(SIF);
	dframe.ErrorCode = htons(NOERROR);
	dframe.SessionToken = htons(1); //Really hacky hardcoded number. TODO: get better session id
	dframe.ParameterFormat = htons(SIMPLE);
	dframe.ParameterLength = htons(sizeof( simple_parameters ));
	
	// Send the message

	length = sizeof( cftp_sif_frame );
    if( sendall( server->server_socket,
				 (char*)(&dframe), 
				 &length) == -1 )
		{
			fprintf( stderr, "Error sending SIF frame: %s\n", strerror( errno ) );
			return -1;	
		}

	length = sizeof( simple_parameters );
    if( sendall( server->server_socket,
				 (char*)(parameters), 
				 &length) == -1 )
		{
			fprintf( stderr, "Error sending SIF payload: %s\n", strerror( errno ) );
			return -1;	
		}	

//
// TODO: Write the server side of the negotiation
//
	
	return 0;


}
