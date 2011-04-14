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

#include "client_lib.h"

int transfer_file( char* server_name,
				   char* server_port,
				   char* localfile )
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
	fprintf(stderr, "Opened file %s.\n", record->filename );
	fprintf(stderr, "\tFile size is %ld bytes.\n", record->file_size );
	fprintf(stderr, "\tChunk size is %ld bytes.\n", record->chunk_size );
	fprintf(stderr, "\tNumber of chunks are %ld.\n", record->num_chunks );
	fprintf(stderr, "\tFile hash is %08X %08X %08X %08X %08X.\n",
			record->hash[0],
			record->hash[1],
			record->hash[2],
			record->hash[3],
			record->hash[4]);
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

	results = execute_transfer( record, server, &parameters );
	if( results == -1 )
		return UNACCEPTABLE_PARAMETERS;


// Execute the teardown phase of the protocol

	results = execute_teardown( record, server, &parameters, results );
	if( results == -1 )
		return UNACCEPTABLE_PARAMETERS;
	

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
FileRecord* open_file( char* filename)
{
	FileRecord* record;
	SHA1Context hashRecord;
	unsigned char chunk;
	unsigned int len;
    char* str_ptr;	
	char* old_str_ptr;
	

	record = (FileRecord*) malloc(sizeof(FileRecord));
	memset( record, 0, sizeof( FileRecord ));


		// Extract the base filename from the filepath given
	old_str_ptr = str_ptr = filename;
	while( str_ptr )
		{
			old_str_ptr = str_ptr;
			str_ptr = strchr( str_ptr+1, '/' );
			if( str_ptr )
				str_ptr = str_ptr + 1;
		}
	str_ptr = old_str_ptr;


	record->filename = (char*)malloc( strlen(str_ptr)+1 );
	strcpy( record->filename, str_ptr );


	record->fp = fopen( filename, "rb" );
	if( record->fp == NULL )
		{
			free(record);
			return NULL;
		}


		// Calculate the hash code for the file data
	SHA1Reset( &hashRecord );
	chunk = fgetc( record->fp );
	while( !feof(record->fp) )
		{
			SHA1Input( &hashRecord, &chunk, 1);			
			chunk = fgetc( record->fp );
		}


	len = SHA1Result( &hashRecord );
	if( len == 0 )
		fprintf(stderr, "Error while constructing hash. Hash is not right!\n" );

	rewind( record->fp);
	memcpy( record->hash, hashRecord.Message_Digest, 5*sizeof(int) );
	
	fseek( record->fp, 0, SEEK_END );
	record->file_size = ftell( record->fp );
	rewind( record->fp);

	record->chunk_size = 100; // Really bad hack
	
		// Count how many chunks there will be.
	record->num_chunks = record->file_size/record->chunk_size;
	if( record->file_size % record->chunk_size != 0 )
	    record->num_chunks = record->num_chunks + 1;


	return record;
}


/*

  connect_to_server

  Takes a server name and port and returns a ServerRecord structure.
  If a non-null value is returned, the client is now connected via
  a TCP session to the server.

  Returns NULL if any errors occur during the connection process.

 */

ServerRecord* connect_to_server( char* server_name, 
								 char* server_port )
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
	cftp_frame dframe;
	cftp_sif_frame* sif_frame;
	cftp_saf_frame* saf_frame;
	cftp_srf_frame* srf_frame;

	simple_parameters server_parameters;
	int parameter_length;

	int length;


    // Set up parameter structure 

	memset( parameters, 0, sizeof( simple_parameters ) );
	memcpy( parameters->filename, record->filename, strlen( record->filename));
	
	parameters->filesize = htonll(record->file_size);
	parameters->chunk_size = htonll(record->chunk_size);
	parameters->num_chunks = htonll(record->num_chunks);
	parameters->hash[0] = htonl( record->hash[0] );
	parameters->hash[1] = htonl( record->hash[1] );
	parameters->hash[2] = htonl( record->hash[2] );
	parameters->hash[3] = htonl( record->hash[3] );
	parameters->hash[4] = htonl( record->hash[4] );
	
	// Setup the message frame structure 

	memset( &dframe, 0, sizeof( cftp_frame ) );
	sif_frame = (cftp_sif_frame*)&dframe;

	sif_frame->MessageType = SIF;
	sif_frame->ErrorCode = htons(NOERROR);
	sif_frame->SessionToken = 1; //Really hacky hardcoded number.
                                 //TODO: get better session id
	sif_frame->ParameterFormat = htons(SIMPLE);
	sif_frame->ParameterLength = htons(sizeof( simple_parameters ));
	
	// Send the message

#ifdef CLIENT_DEBUG
	fprintf( stderr, "Sending SIF...  " );
#endif

	length = sizeof( cftp_frame );
    if( sendall( server->server_socket,
				 (char*)(&dframe), 
				 &length) == -1 )
		{
			fprintf( stderr, "Error sending SIF frame: %s\n",
					 strerror( errno ) );
			return -1;	
		}

#ifdef CLIENT_DEBUG
	fprintf(stderr, "and Payload.\n" );
#endif 

	length = sizeof( simple_parameters );
    if( sendall( server->server_socket,
				 (char*)(parameters), 
				 &length) == -1 )
		{
			fprintf( stderr, "Error sending SIF payload: %s\n",
					 strerror( errno ) );
			return -1;	
		}	


// Now we wait for the server to send a SAF
	
	recv( server->server_socket, 
		  &dframe,
		  sizeof( cftp_frame ),
		  MSG_WAITALL );

	if( dframe.MessageType != SAF )
		{
			fprintf( stderr, "Error: Server failed to send SAF! Aborting.\n" );
			return -1;
		}

	saf_frame = (cftp_saf_frame*)&dframe;
	parameter_length = ntohs(saf_frame->ParameterLength);
	recv( server->server_socket, 
		  &server_parameters,
		  parameter_length,
		  MSG_WAITALL );

		//TODO: We need to check the parameters the server
		// returned to confirm they are still acceptable
		// to us as a client.


//Send the Session Ready Frame

	memset( &dframe, 0, sizeof( cftp_frame ) );
	srf_frame = (cftp_srf_frame*)&dframe;

	srf_frame->MessageType = SRF;
	srf_frame->ErrorCode = htons(NOERROR);
	srf_frame->SessionToken = 1; // Again, really hacky

	length = sizeof( cftp_frame );
    if( sendall( server->server_socket,
				 (char*)(&dframe), 
				 &length) == -1 )
		{
			fprintf( stderr, "Error sending SRF frame: %s\n",
					 strerror( errno ) );
			return -1;	
		}



	return 0;


}


/*

  execute_transfer


 */

int execute_transfer( FileRecord* record,
					  ServerRecord* server,
					  simple_parameters* parameters )
{

		// The transfer phase implemented here is extremely basic
        // It will handle the retry loop, but does not wait for DAF's
        // so the retries are effectively meaningless. 
 
		// TODO: Put this somewhere else, perferably in a config of some sort
	const int MAX_RETRIES = 10;
	long chunk_id;
	int retry_count;
	int error;
	long chunk_size;
	int read_bytes;
	int length;

	cftp_frame sframe;
	cftp_frame rframe;
	cftp_dtf_frame* dtf_frame;
	cftp_daf_frame* daf_frame;
	char* chunk_data;

	chunk_size = record->chunk_size;

#ifdef CLIENT_DEBUG

	fprintf( stderr, "Preparing chunk buffer of %ld bytes.\n", chunk_size );

#endif

	chunk_data = malloc( chunk_size );
	memset( chunk_data, 0, chunk_size );

	error = 0;

	for( chunk_id = 0; ((chunk_id < record->num_chunks) && error == 0) ; chunk_id += 1 )
		{
			memset( &sframe, 0, sizeof( cftp_frame ));
			dtf_frame = (cftp_dtf_frame*)(&sframe);
			dtf_frame->MessageType = DTF;
			dtf_frame->ErrorCode = htons(NOERROR);
			dtf_frame->SessionToken = 1; //Really hacky hardcoded number. TODO: get better session id
			dtf_frame->DataSize = parameters->chunk_size; //We've already encoded this value, don't need htons
			dtf_frame->BlockNum = htonll( chunk_id );

				// Read chunk from the data file
			memset( chunk_data, 0, chunk_size );
			read_bytes = fread( chunk_data, 1, chunk_size, record->fp );
			if( read_bytes != chunk_size && !feof(record->fp) )
				{
					fprintf( stderr, "Error while reading %s. Error code %d (%s)",
							 record->filename,
							 ferror(record->fp),
							 strerror( ferror(record->fp) ) );

					free(chunk_data);
					return -1;
				}



			for( retry_count = 0; retry_count < MAX_RETRIES; retry_count += 1 )
				{

#ifdef CLIENT_DEBUG
					if( retry_count > 0 )
						fprintf( stderr, "Resending (%d) Chunk %ld/%ld of %s\n",
								 retry_count,
								 chunk_id+1,
								 record->num_chunks,
								 record->filename );
					else
						fprintf(stderr, "Sending Chunk %ld/%ld of %s\n",
								chunk_id+1,
								record->num_chunks,
								record->filename );
#endif
						//Send the DTF frame
					length = sizeof( cftp_frame );
					if( sendall( server->server_socket,
								 (char*)(&sframe), 
								 &length) == -1 )
						{
							fprintf( stderr, "Error sending DTF frame: %s\n",
									 strerror( errno ) );
							return -1;	
						}

						//Send the DTF data payload
					length = chunk_size;
					if( sendall( server->server_socket,
								 chunk_data, 
								 &length) == -1 )
						{
							fprintf( stderr, "Error sending DTF payload: %s\n",
									 strerror( errno ) );
							return -1;	
						}					




						// WAIT HERE FOR DAF BEFORE CONTINUING.


					recv( server->server_socket, 
						  &rframe,
						  sizeof( cftp_frame ),
						  MSG_WAITALL );

					if( rframe.MessageType	!= DAF )
						{
							fprintf( stderr, "Error: Server failed to send DAF! Aborting.\n" );
							return -1;
						}

					daf_frame = (cftp_daf_frame*)(&rframe);
					if( ntohll(daf_frame->BlockNum) == chunk_id )
						{
							fprintf( stderr, "Recieved DAF for block %ld. Continuing...\n",
									 chunk_id+1 );
							break;
						}
					else
						{
							fprintf( stderr, "Recieved DAF for block %ld. Correcting...\n",
									 ntohll(daf_frame->BlockNum)+1 );

						}

						// If the DAF is not received or the wrong DAF is received, then we
						// need to alter the next DTF such that its error code reflects the 
                        // previously failed DTF. Probably the error code should be set to TIMEOUT.

				}

				// If this loop finishes without a break, then we hit our retry cap
			if( retry_count == MAX_RETRIES )
				error = 1;

		}

			
	if( error == 1 )
		return -1;
	else		
		return 0;	

}


/*

    execute_teardown

*/
int execute_teardown( FileRecord* record,
					  ServerRecord* server,
					  simple_parameters* parameters,
					  int results )
{

	cftp_frame sframe;
	cftp_frame rframe;

	cftp_fff_frame* fff_frame;
	cftp_faf_frame* faf_frame;

	int length;


		// Really hacky, but it works here for now
	if( results == -1 )
		return -1;

	

		//Send FileFinished Frame to terminate the transfer
	memset( &sframe, 0, sizeof( cftp_frame ));
	fff_frame = (cftp_fff_frame*)(&sframe);

	fff_frame->MessageType = FFF;
	fff_frame->ErrorCode = htons( NOERROR );
	fff_frame->SessionToken = 1; // Still really a hack

#ifdef CLIENT_DEBUG
	fprintf( stderr, "Transfer finished, sending FFF signal.\n" );
#endif

	length = sizeof( cftp_frame );
	if( sendall( server->server_socket,
				 (char*)&sframe,
				 &length ) == -1 )
		{
			fprintf( stderr, "Error could not send FFF: %s. Aborting!\n", 
					 strerror(errno) );
			return -1;
		}

		// Now we wait for the FAF response

	recv( server->server_socket,
		  (char*)&rframe,
		  sizeof( cftp_frame ),
		  MSG_WAITALL);
	
	if( rframe.MessageType != FAF )
		{
			fprintf( stderr, "Server did not send FAF! Aborting!\n");
			return -1;
		}

	faf_frame = (cftp_faf_frame*)(&rframe);

	return 0;

}
