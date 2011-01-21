#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server_lib.h"

void run_server( char* server_name,  char* server_port)
{

	int results;
	
	FileRecord* localFileCopy = NULL;
	ServerRecord* localServer = NULL;
	ClientRecord* remoteClient = NULL;

	results = -1;

	localServer = start_server( server_name, server_port );

#ifdef SERVER_DEBUG
	if( localServer == NULL )
		fprintf( stderr, "Unable to create local server instance. Aborting.\n" );
#endif

	if( localServer == NULL )
		return;

	remoteClient = accept_client( localServer );

#ifdef SERVER_DEBUG
	if( remoteClient == NULL )
		fprintf( stderr, "Failure when attempting to connect with client. Aborting.\n" );
#endif

	if( remoteClient == NULL )
		return;	

		localFileCopy = execute_negotiation( localServer, remoteClient );

#ifdef SERVER_DEBUG
	if( localFileCopy == NULL )
		fprintf( stderr, "Failure to negotiate transfer parameters with client. Aborting.\n" );
#endif

	if( localFileCopy == NULL )
		return;	
	
		results = execute_transfer( localServer, remoteClient, localFileCopy );

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
ServerRecord* start_server( char* server_name, char* server_port )
{

//Really rusty on socket api - refered to Beej's guide for this
//http://beej.us/guide/bgnet/output/html/multipage/syscalls.html	

	int status;	
	int sock;
	int results;
	int yes;
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


	// Clear up any old binds on the port
    // lose the pesky "Address already in use" error message
    yes = 1;
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		fprintf( stderr, "Error on clearing old port bindings: %s\n", strerror( errno ) );
		freeaddrinfo(servinfo);
		free( record );	
		return NULL;;
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


ClientRecord* accept_client( ServerRecord* localServer )
{
	int results;

	struct sockaddr_in* caddr_ip4;
	struct sockaddr_in6* caddr_ip6;

	struct sockaddr_storage client_addr;
    socklen_t addr_size;

	ClientRecord* record;	

	record = (ClientRecord*) malloc(sizeof(ClientRecord));
	memset( record, 0, sizeof( ClientRecord ));

	addr_size = sizeof( client_addr );
	results = accept( localServer->server_socket,
					  (struct sockaddr *)&client_addr,
					  &addr_size );

	if( results == -1)
		{
			fprintf( stderr, "Error on accepting: %s\n", strerror( errno ) );
			free( record );
			return NULL;
		}	
	
	record->client_socket = results;
	record->client_name = NULL;
	record->client_port = NULL;

		// Test if the client is using a IP4 address
	if( client_addr.ss_family == AF_INET )
		{
#ifdef SERVER_DEBUG
			fprintf( stderr, "Client is using IP4.\n" );
#endif

			caddr_ip4 = (struct sockaddr_in*)(&client_addr);

			// Allocate enough space for an IP4 address
			record->client_name = (char*) malloc( INET_ADDRSTRLEN ); 
			memset( record->client_name, 0, INET_ADDRSTRLEN );
			
			inet_ntop( AF_INET, &(caddr_ip4->sin_addr),
					   record->client_name, INET_ADDRSTRLEN);

			// 10 chars will hold a IP4 port
			record->client_port = (char*) malloc( 10 ); 
			memset( record->client_port, 0, 10 );
			
			// extract the address from the raw value
			sprintf( record->client_port, "%d", 
					 ntohs( caddr_ip4->sin_port) );   // port number
		}

		// Test if the client is using a IP6 address
	if( client_addr.ss_family == AF_INET6 )
		{
#ifdef SERVER_DEBUG
			fprintf( stderr, "Client is using IP6.\n" );
#endif

			caddr_ip6 = (struct sockaddr_in6*)(&client_addr);

			// Allocate enough space for an IP6 address
			record->client_name = (char*) malloc( INET6_ADDRSTRLEN ); 
			memset( record->client_name, 0, INET6_ADDRSTRLEN );
			
			inet_ntop( AF_INET6, &(caddr_ip6->sin6_addr),
					   record->client_name, INET6_ADDRSTRLEN);

			// 10 chars will hold a IP6 port
			record->client_port = (char*) malloc( 10 ); 
			memset( record->client_port, 0, 10 );
			
			// extract the address from the raw value
			sprintf( record->client_port, "%d", 
					 ntohs( caddr_ip6->sin6_port) );   // port number
		}

#ifdef SERVER_DEBUG
	if( record->client_name && record->client_port )
		fprintf( stderr, "The client is %s at port %s.\n",
				 record->client_name, record->client_port );
#endif


	return record;
}

/*
    execute_negotiation
   

*/
FileRecord* execute_negotiation( ServerRecord* localServer, ClientRecord* remoteClient )
{
	FileRecord* filerecord;
	
	cftp_frame client_frame;
	cftp_sif_frame* sif_frame;
	cftp_srf_frame* srf_frame;
	simple_parameters* parameter_payload;
	int parameter_length;
	int length;
	

	recv( remoteClient->client_socket, 
		  &client_frame,
		  sizeof( cftp_frame ),
		  MSG_WAITALL );

	if( client_frame.MessageType != SIF )
		{
			fprintf( stderr, "Error: Client failed to send SIF! Aborting.\n" );
				//TODO: Need to send a SCF here
			return NULL;
		}
	
	sif_frame = (cftp_sif_frame*)&client_frame;

#ifdef SERVER_DEBUG
	fprintf( stderr, "MessageType: %d.\n", 
			 client_frame.MessageType );
	fprintf( stderr, "SIF error code: %d.\n", 
			 ntohs(sif_frame->ErrorCode) );
	fprintf( stderr, "SIF session token: %d.\n", 
			 sif_frame->SessionToken );
	fprintf( stderr, "SIF parameter format: %d.\n", 
			 ntohs(sif_frame->ParameterFormat) );
	fprintf( stderr, "SIF parameter length: %d.\n", 
			 ntohs(sif_frame->ParameterLength) );
#endif 

		// This is really hacky and hardcoded
	parameter_length = ntohs( sif_frame->ParameterLength );
	parameter_payload = malloc( sizeof( simple_parameters) );
	recv( remoteClient->client_socket, 
		  parameter_payload,
		  parameter_length,
		  MSG_WAITALL );
	
#ifdef SERVER_DEBUG
	fprintf( stderr, "Parameters:Filename %s\n", parameter_payload->filename );
	fprintf( stderr, "Parameters:FileSize %ld\n", ntohll( parameter_payload->filesize));
	fprintf( stderr, "Parameters:NumChunks %ld\n", ntohll( parameter_payload->num_chunks));
	fprintf( stderr, "Parameters:ChunkSize %ld\n", ntohll( parameter_payload->chunk_size));
	fprintf( stderr, "Parameters:Hash %08X %08X %08X %08X %08X\n",
			 ntohl(parameter_payload->hash[0]),
			 ntohl(parameter_payload->hash[1]),
			 ntohl(parameter_payload->hash[2]),
			 ntohl(parameter_payload->hash[3]),
			 ntohl(parameter_payload->hash[4]));
			 
#endif

//TODO: Perform the server side parameter verification checks
//      Will need to do this to confirm disk space, etc...
//      For now we will just spit the frame back verbatim.
//      Since the SIF and SAF frames are currently identical, no
//      work is needed here at the moment.

	client_frame.MessageType = SAF;

	length = sizeof( cftp_frame );
    if( sendall( remoteClient->client_socket,
				 (char*)(&client_frame), 
				 &length) == -1 )
		{
			fprintf( stderr, "Error sending SAF frame: %s\n",
					 strerror( errno ) );
			free( parameter_payload);
			return NULL;	
		}

	length = parameter_length;
    if( sendall( remoteClient->client_socket,
				 (char*)(parameter_payload), 
				 &length) == -1 )
		{
			fprintf( stderr, "Error sending SAF payload: %s\n",
					 strerror( errno ) );
			free( parameter_payload);	
			return NULL;	
		}	

// Now we wait for the Session Ready Frame before moving on to the transfer
// phase.

	recv( remoteClient->client_socket, 
		  &client_frame,
		  sizeof( cftp_frame ),
		  MSG_WAITALL );

	if( client_frame.MessageType != SRF )
		{
			fprintf( stderr, "Error: Client failed to send SRF! Aborting.\n" );
				//TODO: Need to send a SCF here
			return NULL;
		}

// Prepare the file record. This may need to be moved to earlier in the process

	filerecord = (FileRecord*) malloc(sizeof(FileRecord));
	memset( filerecord, 0, sizeof( FileRecord ));

	filerecord->filename = malloc(512);
	memcpy( filerecord->filename, parameter_payload->filename, 512 );

	filerecord->fp = fopen( filerecord->filename, "wb" );
	if( filerecord->fp == NULL )
		{
			free(filerecord->filename);
			free(filerecord);
			return NULL;
		}

	filerecord->file_size = ntohll(parameter_payload->filesize);
	filerecord->chunk_size = ntohll(parameter_payload->chunk_size);
	filerecord->num_chunks = ntohll(parameter_payload->num_chunks);

	filerecord->hash[0] = ntohl(parameter_payload->hash[0]);
	filerecord->hash[1] = ntohl(parameter_payload->hash[1]);
	filerecord->hash[2] = ntohl(parameter_payload->hash[2]);
	filerecord->hash[3] = ntohl(parameter_payload->hash[3]);
	filerecord->hash[4] = ntohl(parameter_payload->hash[4]);
	


	return filerecord;

}

/*

  execute_transfer

*/
int execute_transfer( ServerRecord* localServer, ClientRecord* remoteClient, FileRecord* localFileCopy )
{

		// TODO: Put this somewhere else, perferably in a config of some sort
	const int MAX_RETRIES = 10;
	
	long chunk_id;
	int retry_count;
	int error;
	long chunk_size;
	int write_bytes;
	int length;
	unsigned long data_written;

	cftp_frame sframe;
	cftp_frame rframe;
	cftp_dtf_frame* dtf_frame;
	cftp_daf_frame* daf_frame;
	char* chunk_data;

	data_written = 0;
	chunk_size = localFileCopy->chunk_size;

#ifdef SERVER_DEBUG

	fprintf( stderr, "Preparing chunk buffer of %ld bytes.\n", chunk_size );

#endif

	chunk_data = malloc( chunk_size );
	memset( chunk_data, 0, chunk_size );

	error = 0;

	for( chunk_id = 0; ((chunk_id < localFileCopy->num_chunks) && error == 0) ; chunk_id += 1 )
		{
			recv( remoteClient->client_socket, 
				  &rframe,
				  sizeof(cftp_frame),
				  MSG_WAITALL );

			if( rframe.MessageType != DTF )
				{
					fprintf( stderr, "Client did not send DTF. Aborting!\n" );
					return -1;
				}
			
			dtf_frame = (cftp_dtf_frame*)(&rframe);

			recv( remoteClient->client_socket,	
				  chunk_data,
				  chunk_size,
				  MSG_WAITALL );
			
				// TODO: replace this hack with proper DAF message requesting proper block.
			if( ntohll(dtf_frame->BlockNum) != chunk_id )
				{
					fprintf( stderr, "Client sent the wrong block! Got %ld, but needed %ld. Aborting!\n", ntohll(dtf_frame->BlockNum)+1, chunk_id+1 );
					return -1;
				}
				
			fprintf( stderr, "Received data chunk %ld/%ld.\n", chunk_id+1, 
					 localFileCopy->num_chunks );

				//
				//TODO: write the chunk to disk here
				//	

			if( localFileCopy->file_size - data_written > chunk_size )
				length = chunk_size;
			else
				length = localFileCopy->file_size - data_written;

			write_bytes = fwrite( chunk_data, 1, length, localFileCopy->fp );

			if( write_bytes != length )
				{
					fprintf( stderr, "Error writing chunk %ld to disk: %s. Aborting!\n",
							 chunk_id, strerror( errno ) );
					return -1;
				}
			data_written += write_bytes;
			if( data_written > localFileCopy->file_size )
				{
					fprintf( stderr, "Too much data written to disk. Aborting!\n" );
					return -1;
				}
	

				//
				//TODO: Send DAF response
				//

			memset( &sframe, 0 , sizeof( cftp_frame ) );
			daf_frame = (cftp_daf_frame*)(&sframe);
			daf_frame->MessageType = DAF;
			daf_frame->ErrorCode = htons(NOERROR);
			daf_frame->SessionToken = dtf_frame->SessionToken;
			daf_frame->BlockNum = htonll( chunk_id );

			fprintf( stderr, "Sending DAF for %ld/%ld.\n", chunk_id+1, 
					 localFileCopy->num_chunks );

			length = sizeof( cftp_frame );
			if( sendall( remoteClient->client_socket,
						 (char*)&sframe,
						 &length) == -1 )
				{
					fprintf( stderr, "Error sending DAF for block %ld: %s.\n", chunk_id, strerror( errno ) );
					return -1;
				}
			
		}


	if( chunk_id == localFileCopy->num_chunks )
		return 0;
	else
		return -1;

}
