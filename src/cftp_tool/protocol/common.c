
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>


/*


recv_cftp_frame


*/
int recv_cftp_frame( TransferState* state )
{
    ENTER_STATE;

    int flags;
    int status;

	if( !state->recv_rdy )
		return -1;
	state->recv_rdy = 0;

	memset( &state->frecv_buf, 0, sizeof( cftp_frame ) );


    flags = fcntl(state->arguments->local_socket.socket, F_GETFL, 0);
    DEBUG( "We are in non-blocking mode: %d\n", flags & O_NONBLOCK );

    status = recv( state->arguments->local_socket.socket, 	
                   &state->frecv_buf,	
                   sizeof( cftp_frame ),
                   MSG_WAITALL );
    if( status == -1 )
        {
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not recv CFTP frame: %s", strerror(errno));
            DEBUG("Could not recv CFTP frame: %s\n", strerror(errno));
			LEAVE_STATE(0);
        }
    if( status == 0 )
        {
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Client Closed connection unexpectedly.");
            DEBUG("Client Closed connection unexpectedly.\n");
			LEAVE_STATE(-1);
        }


	DEBUG("Read %d bytes from client on frame_recv.\n", status );
	desc_cftp_frame(state, 0 );

	LEAVE_STATE(status);
}



/*


recv_data_frame


*/
int recv_data_frame( TransferState* state )
{
ENTER_STATE;
	if( !state->recv_rdy )
		return -1;
	state->recv_rdy = 0;

	int recv_bytes;
		int i;

	if( state->data_buffer_size <= 0 )
		return 0;

	if( state->data_buffer )
		free( state->data_buffer );

	state->data_buffer = (char*) malloc( state->data_buffer_size );
	memset( state->data_buffer, 0, state->data_buffer_size );

	recv_bytes = recv( state->arguments->local_socket.socket, 	
					   state->data_buffer,	
					   state->data_buffer_size,
					   MSG_WAITALL );

    state->data_buffer_size = 0;
    if( recv_bytes == -1 )
        {
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not recv CFTP frame: %s", strerror(errno));
            DEBUG("Could not recv CFTP frame: %s\n", strerror(errno));
			LEAVE_STATE(0);
        }
    if( recv_bytes == 0 )
        {
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Client Closed connection unexpectedly.");
            DEBUG("Client Closed connection unexpectedly.\n");
			LEAVE_STATE(-1);
        }

	DEBUG("Read %d bytes from client on data_recv.\n", recv_bytes );
		
    for( i = 0; i < recv_bytes; i ++ )
		{
			if( i % 32 == 0 )
				fprintf( stderr, "\n" );
			fprintf( stderr, "%X", (char)*(state->data_buffer+i));
		}
	fprintf( stderr, "\n" );
		
	
	LEAVE_STATE(recv_bytes);
}




/*


send_cftp_frame

*/
int send_cftp_frame( TransferState* state )
{
ENTER_STATE;
	int length = 0;
	int status = 0;

	if( !state->send_rdy )
		return -1;
	state->send_rdy = 0;

	length = sizeof( cftp_frame );
	status = sendall( state->arguments->local_socket.socket,
					  (char*)(&state->fsend_buf),
					  &length );

	if( status == -1 )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send CFTP frame: %s", strerror(errno));
            DEBUG("Could not send CFTP frame: %s\n", strerror(errno));
			LEAVE_STATE(0);
		}
	else
		{
			DEBUG("Sent %d bytes to client on frame_send.\n", length );
			desc_cftp_frame(state, 1 );
			LEAVE_STATE(length);
		}
LEAVE_STATE(0);
}


/*


send_data_frame

*/
int send_data_frame( TransferState* state )
{
ENTER_STATE;
	int length = 0;
	int status = 0;
    int i;

	if( !state->send_rdy )
		return -1;
	state->send_rdy = 0;

	length = state->data_buffer_size;
    state->data_buffer_size = 0;
	if( length <= 0 )
		return 0;
	
	if( !state->data_buffer )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send data: Data buffer null");
			LEAVE_STATE(1);
		}

	status = sendall( state->arguments->local_socket.socket,
					  (char*)(state->data_buffer),
					  &length );

	if( status == -1 )
		{
			state->last_error = 1;
			sprintf( state->error_string, 
					 "Could not send data: %s", strerror(errno));
            DEBUG("Could not send data: %s\n", strerror(errno));
			LEAVE_STATE(0);
		}
	else
		{ 
			DEBUG("Sent %d bytes to client on data_send.\n", length );
			LEAVE_STATE( length );
		}
LEAVE_STATE(0);
}




/*

desc_cftp_frame




 */
void desc_cftp_frame( TransferState* state, int send_or_recv)
{
	cftp_frame* frame;

	if( send_or_recv == 1 )
		{
			frame = &state->fsend_buf;
			DEBUG("\tMessage type of Send Frame is " );
		}
	else
		{
			frame = &state->frecv_buf;
			DEBUG("\tMessage type of Recv Frame is " );
		}
	
	switch( frame->MessageType )
		{
		case DSF:
			DEBUG("(%d) DSF frame.\n", frame->MessageType );
			break;
		case DRF:
			DEBUG("(%d) DRF frame.\n", frame->MessageType );
			break;
		case SIF:
			DEBUG("(%d) SIF frame.\n", frame->MessageType );
            DEBUG( "SIF error code: %d.\n",
                   ntohs(((cftp_sif_frame*)frame)->ErrorCode) );
            DEBUG( "SIF session token: %d.\n",
                   ((cftp_sif_frame*)frame)->SessionToken );
            DEBUG( "SIF parameter format: %d.\n",
                   ntohs(((cftp_sif_frame*)frame)->ParameterFormat) );
            DEBUG( "SIF parameter length: %d.\n",
                   ntohs(((cftp_sif_frame*)frame)->ParameterLength) );
			break;
		case SAF:
			DEBUG("(%d) SAF frame.\n", frame->MessageType );
			break;
		case SRF:
			DEBUG("(%d) SRF frame.\n", frame->MessageType );
			break;
		case SCF:
			DEBUG("(%d) SCF frame.\n", frame->MessageType );
			break;
		case DTF:
			DEBUG("(%d) DTF frame.\n", frame->MessageType );
			DEBUG("\tChunk Number: %ld\n" , ntohll(((cftp_dtf_frame*)frame)->BlockNum) );
			break;
		case DAF:
			DEBUG("(%d) DAF frame.\n", frame->MessageType );
			DEBUG("\tChunk Number: %ld\n" , ntohll(((cftp_daf_frame*)frame)->BlockNum) );	
			break;
		case FFF:
			DEBUG("(%d) FFF frame.\n", frame->MessageType );
			break;
		case FAF:
			DEBUG("(%d) FAF frame.\n", frame->MessageType );
			break;
		default:
			DEBUG("(%d) Unknown frame.\n" , frame->MessageType );
			break;
		}


}


/*

free_TransferState

*/
void free_TransferState( TransferState* state )
{
    if( state == NULL )
        return;

    if( state->session_parameters )
        free( state->session_parameters );

    if( state->data_buffer )
        free( state->data_buffer );

    free( state );

}
