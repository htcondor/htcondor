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

   State_SendSessionParameters

*/
int State_SendSessionParameters( TransferState* state )
{
    ENTER_STATE;

	cftp_sif_frame* sif_frame;
	simple_parameters* parameters;


    // Set up parameter structure 
    if( state->data_buffer )
		free( state->data_buffer );
	state->data_buffer = malloc( state->parameter_length );
    parameters = (simple_parameters*)state->data_buffer;


	memset( parameters, 0, sizeof( simple_parameters ) );
	memcpy( parameters->filename, 
            state->local_file.filename,
            strlen( state->local_file.filename));
	
	parameters->filesize = htonll(state->local_file.file_size);
	parameters->chunk_size = htonll(state->local_file.chunk_size);
	parameters->num_chunks = htonll(state->local_file.num_chunks);
	parameters->hash[0] = htonl( state->local_file.hash[0] );
	parameters->hash[1] = htonl( state->local_file.hash[1] );
	parameters->hash[2] = htonl( state->local_file.hash[2] );
	parameters->hash[3] = htonl( state->local_file.hash[3] );
	parameters->hash[4] = htonl( state->local_file.hash[4] );
	
	// Setup the message frame structure 

	sif_frame = (cftp_sif_frame*)(&state->fsend_buf);
	sif_frame->MessageType = SIF;
	sif_frame->ErrorCode = htons(NOERROR);
	sif_frame->SessionToken =    htons(state->session_token);
    sif_frame->ParameterFormat = htons(state->parameter_length);
    sif_frame->ParameterLength = htons(state->parameter_format);


    // Send message frame
    state->send_rdy = 1;
    send_cftp_frame( state );

    // Send data frame
	state->send_rdy = 1;
	send_data_frame( state );

    //Mark receive ready for parameter acks
    state->recv_rdy = 1;

    LEAVE_STATE(0);
}



/*

State_ReceiveSessionParametersAck

 */
int State_ReceiveSessionParametersAck( TransferState* state )
{
	ENTER_STATE;

	cftp_saf_frame* saf_frame;



	saf_frame = (cftp_saf_frame*)(&state->frecv_buf);

    //TODO: We need to check the parameters the server
    // returned to confirm they are still acceptable
    // to us as a client.


    //Move directly to sending client ready
    state->recv_rdy = 0;

	LEAVE_STATE(0);
}


/*

State_SendClientReady

 */
int State_SendClientReady( TransferState* state )
{
	ENTER_STATE;
		
    cftp_srf_frame* srf_frame;

    srf_frame = (cftp_srf_frame*)(&state->fsend_buf);
    srf_frame->MessageType = SRF;
	srf_frame->ErrorCode = htons(NOERROR);
	srf_frame->SessionToken = htons(state->session_token);

    // Send message frame
    state->send_rdy = 1;
    send_cftp_frame( state );

    //Send first file chunk; 
    state->recv_rdy = 0;

	LEAVE_STATE(0);
}
