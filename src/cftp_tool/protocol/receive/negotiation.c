#include "negotiation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>



/*

State_ReceiveSessionParameters

 */
int State_ReceiveSessionParameters( TransferState* state )
{
	ENTER_STATE

	state->recv_rdy = 1;

	LEAVE_STATE(0)
}

/*

State_CheckSessionParameters

 */
int State_CheckSessionParameters( TransferState* state )
{
	ENTER_STATE

	int recv_bytes;
	cftp_sif_frame* sif_frame;
	simple_parameters* recvd_params;
	simple_parameters* local_params;
	char* str_ptr;
	char* old_str_ptr;


	sif_frame = (cftp_sif_frame*)(&state->frecv_buf);
	
		// Load parameters
	state->session_token    = sif_frame->SessionToken;  
	state->parameter_length = ntohs( sif_frame->ParameterLength );
	state->parameter_format = ntohs( sif_frame->ParameterFormat );

		// Hacky as we only have one format currently
	if( state->parameter_format != SIMPLE || state->parameter_length != sizeof( simple_parameters ) )
		{
			sprintf( state->error_string,
					 "Client is sending parameters in an unacceptable format or unexpected size." );
			LEAVE_STATE(-1);
		}

		// When we add more parameter formats, this will need to be redone
	state->session_parameters = malloc( sizeof(simple_parameters) );
	memset( state->session_parameters, 0, sizeof( simple_parameters) );

	state->data_buffer_size = state->parameter_length;
	state->recv_rdy = 1;
	recv_bytes = recv_data_frame( state );

	if( recv_bytes != state->parameter_length )
		{
			sprintf( state->error_string,
					 "Client sent only partial parameters. Error in receiving.");
			LEAVE_STATE(-1);//TODO, check the return codes here
		}



		// Move data to parameters, with ntoh
	recvd_params = (simple_parameters*)(state->data_buffer);
	local_params = (simple_parameters*)(state->session_parameters);

	memcpy( local_params->filename,
			recvd_params->filename, 
			512 );

 	local_params->filesize   = ntohll( recvd_params->filesize );
	local_params->num_chunks = ntohll( recvd_params->num_chunks );
	local_params->chunk_size = ntohll( recvd_params->chunk_size );
	
	local_params->hash[0] = ntohl( recvd_params->hash[0] );
	local_params->hash[1] = ntohl( recvd_params->hash[1] );
	local_params->hash[2] = ntohl( recvd_params->hash[2] );
	local_params->hash[3] = ntohl( recvd_params->hash[3] );
	local_params->hash[4] = ntohl( recvd_params->hash[4] );	

		// Do the parameter checking here.
	
	VERBOSE( "Checking file parameters against local constraints...\n" );

		//For now we only check against the quota option.
		//There will probably be more constraints later
	if( state->arguments->quota != -1 )
	  {
	    if( local_params->filesize > state->arguments->quota*1024 )
	      {
		//We cannot accept this file. It is over quota
		VERBOSE( "\tQuota exceeded. Cannot accept file from client.\n" );
		LEAVE_STATE(1);				  
	      }
	  }
	    
	VERBOSE( "\tCheck complete.\n" );
	

//TODO: Perform the server side parameter verification checks
//      Will need to do this to confirm disk space, etc...


	

	LEAVE_STATE(0);
}


/*

State_AcknowledgeSessionParameters

 */
int State_AcknowledgeSessionParameters( TransferState* state )
{
	ENTER_STATE

	cftp_saf_frame* saf_frame;
	simple_parameters* send_parameters;
	simple_parameters* local_parameters;

		// Send the SAF control frame
	saf_frame = (cftp_saf_frame*)(&state->fsend_buf);
	saf_frame->MessageType = SAF;
	saf_frame->SessionToken = state->session_token;
	saf_frame->ErrorCode = 0;
	saf_frame->ParameterLength = htons( state->parameter_length );
	saf_frame->ParameterFormat = htons( state->parameter_format );
	
	state->send_rdy = 1;
	send_cftp_frame( state );

		// Send the data parameter payload

	if( state->data_buffer )
		free( state->data_buffer );
	
	state->data_buffer = malloc( state->parameter_length );
	send_parameters = (simple_parameters*)(state->data_buffer);
	local_parameters = (simple_parameters*)(state->session_parameters);

	memcpy( send_parameters->filename,
			local_parameters->filename,
			512 );
	
	send_parameters->filesize   = htonll( local_parameters->filesize );
	send_parameters->num_chunks = htonll( local_parameters->num_chunks );
	send_parameters->chunk_size = htonll( local_parameters->chunk_size );
	
	send_parameters->hash[0] = htonl( local_parameters->hash[0] );
	send_parameters->hash[1] = htonl( local_parameters->hash[1] );
	send_parameters->hash[2] = htonl( local_parameters->hash[2] );
	send_parameters->hash[3] = htonl( local_parameters->hash[3] );
	send_parameters->hash[4] = htonl( local_parameters->hash[4] );


    state->data_buffer_size = state->parameter_length;
	state->send_rdy = 1;
	send_data_frame( state );


	
		//Mark readiness for the Client to send SRF
	state->recv_rdy = 1;

	LEAVE_STATE(0);
}

/*

State_ReceiveClientReady

 */
int State_ReceiveClientReady( TransferState* state )
{
	ENTER_STATE
		
	simple_parameters* local_parameters;
    char* filepath;
    int pathlen;

	local_parameters = (simple_parameters*)(state->session_parameters);

	memset( &state->local_file, 0, sizeof( FileRecord ));

	state->local_file.filename = malloc(512);
	memcpy( state->local_file.filename,
			local_parameters->filename, 512 );

    state->local_file.path = malloc(strlen(state->arguments->store_path)+1);
    memset( state->local_file.path, 0, strlen(state->arguments->store_path)+1 );
	memcpy( state->local_file.path,
			state->arguments->store_path,
            strlen(state->arguments->store_path));


    pathlen = strlen(state->local_file.path)+1+strlen(state->local_file.filename)+1;
    filepath = malloc(pathlen);
    memset( filepath, 0, pathlen);
    sprintf( filepath, "%s/%s", state->local_file.path, state->local_file.filename );

	state->local_file.fp = fopen( filepath, "wb" );
	if( state->local_file.fp == NULL )
		{
			sprintf( state->error_string,
					 "Could not open file %s locally for writing. Check for permissions or disk errors.", state->local_file.filename);
			free(state->local_file.filename);
			LEAVE_STATE(-1);
		}

	state->local_file.file_size = local_parameters->filesize;
	state->local_file.chunk_size = local_parameters->chunk_size;
	state->local_file.num_chunks = local_parameters->num_chunks;

	state->local_file.hash[0] = local_parameters->hash[0];
	state->local_file.hash[1] = local_parameters->hash[1];
	state->local_file.hash[2] = local_parameters->hash[2];
	state->local_file.hash[3] = local_parameters->hash[3];
	state->local_file.hash[4] = local_parameters->hash[4];

    free( filepath );

	VERBOSE( "Client Ready. Transfer Starting.\n");

	LEAVE_STATE(0);
}
