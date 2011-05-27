#include "transfer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/*


State_ReceiveDataBlock


 */
int State_ReceiveDataBlock( TransferState* state )
{
	ENTER_STATE


	state->recv_rdy = 1;	


	LEAVE_STATE(0);
}



/*


State_AcknowledgeDataBlock


 */
int State_AcknowledgeDataBlock( TransferState* state )
{
	ENTER_STATE;

	cftp_dtf_frame* dtf_frame;
    cftp_daf_frame* daf_frame;
	char* chunk_data;
	long chunk_size;
	int recv_bytes;
	int write_bytes;
    int write_chunk = 0;
	long length;

	dtf_frame = (cftp_dtf_frame*)&(state->frecv_buf);
	daf_frame = (cftp_daf_frame*)&(state->fsend_buf);

	chunk_size = state->local_file.chunk_size;
	chunk_data = malloc( chunk_size+1 );
	chunk_data[chunk_size] = 0;
	
	state->data_buffer_size = chunk_size;
	state->recv_rdy = 1;
	recv_bytes = recv_data_frame( state );
	memcpy( chunk_data, state->data_buffer, recv_bytes );


	if( recv_bytes != chunk_size )
		{
			sprintf( state->error_string,
					 "Client sent partial data chunk. Error in receiving.");
			free( chunk_data );
			LEAVE_STATE(-1); //TODO, check the return codes here
		}

		//printf( "\tChunk Data:\n\n%s\n\n", chunk_data );

	if( ntohll(dtf_frame->BlockNum) != state->current_chunk )
		{	
			if( state->current_chunk == 0 )
				{
					sprintf( state->error_string,
							 "Client sent .");
					free( chunk_data );	
					LEAVE_STATE(-1); //TODO, check the return codes here			
				}
			else
				{
					fprintf( stderr, "Client sent the wrong block! Got %ld, but needed %ld. Acknowledging last chunk again.\n",
							 ntohll(dtf_frame->BlockNum)+1,
							 state->current_chunk+1 );
					state->current_chunk--;
				}		
		}
	else
		{
			write_chunk = 1;
		}


	if( write_chunk )
		{

			if( state->local_file.file_size - state->data_written > chunk_size )
				length = chunk_size;
			else
				length = state->local_file.file_size - state->data_written;

			write_bytes = fwrite( chunk_data, 1, length, state->local_file.fp );
			fflush( state->local_file.fp );

			if( write_bytes != length )
				{
					sprintf( state->error_string, "Error writing chunk %ld to disk: %s. Aborting!\n",
							 state->current_chunk, strerror( errno ) );
					free( chunk_data );
					LEAVE_STATE(-1);
				}
			state->data_written += write_bytes;
			if( state->data_written > state->local_file.file_size )
				{
					sprintf( state->error_string, "Too much data written to disk. Aborting!\n" );
					free( chunk_data );
					LEAVE_STATE(-1);
				}

		}

	
	memset( daf_frame, 0 , sizeof( cftp_frame ) );
	daf_frame->MessageType = DAF;
	daf_frame->ErrorCode = htons(NOERROR);
	daf_frame->SessionToken = dtf_frame->SessionToken;
	daf_frame->BlockNum = htonll( state->current_chunk );

	state->send_rdy = 1;
	send_cftp_frame( state );
	if( state->current_chunk == state->local_file.num_chunks-1)
		{
			free( chunk_data );
			LEAVE_STATE(1);
		}

	state->current_chunk ++;

	free( chunk_data );
	LEAVE_STATE(0);
}
