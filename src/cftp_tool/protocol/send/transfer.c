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

State_SendDataBlock

*/


int State_SendDataBlock( TransferState* state )
{
	ENTER_STATE;

	long chunk_size;
	int read_bytes;

	cftp_dtf_frame* dtf_frame;

    if( state->retry_count >= 10 )
        {
            LEAVE_STATE(-2);
        }
    
    
    if( state->retry_count == 0 )
        {
            VERBOSE( "Sending Chunk %ld/%ld of %s\n",
                     state->current_chunk+1,
                     state->local_file.num_chunks,
                     state->local_file.filename );
        }
    else
        {
            VERBOSE( "Resending (%d) Chunk %ld/%ld of %s\n",
                     state->retry_count,
                     state->current_chunk+1,
                     state->local_file.num_chunks,
                     state->local_file.filename );
        }

    chunk_size = state->local_file.chunk_size;

    // Prep DTF header
    dtf_frame = (cftp_dtf_frame*)(&state->fsend_buf);
    memset( dtf_frame, 0, sizeof( cftp_dtf_frame ));

    dtf_frame->MessageType = DTF;
    dtf_frame->ErrorCode = htons(NOERROR);
    dtf_frame->SessionToken =  htons(state->session_token);
    dtf_frame->DataSize = htons(chunk_size);
    dtf_frame->BlockNum = htonll(state->current_chunk);

    state->send_rdy = 1;
    send_cftp_frame( state );

    // And Data payload
    if( state->data_buffer )
        free( state->data_buffer );
    state->data_buffer = malloc( chunk_size );
    state->data_buffer_size = chunk_size;

    memset( state->data_buffer, 0, chunk_size );

    //Move to chunk position
    fseek( state->local_file.fp,
           state->current_chunk*chunk_size,
           SEEK_SET );

    read_bytes = fread( state->data_buffer, 1, chunk_size, state->local_file.fp );
    if( read_bytes != chunk_size && !feof(state->local_file.fp) )
        {
            fprintf( stderr, "Error while reading %s. Error code %d (%s)",
                     state->local_file.filename,
                     ferror(state->local_file.fp),
                     strerror( ferror(state->local_file.fp)));

            LEAVE_STATE(-1);
        }
    
    state->send_rdy = 1;
    send_data_frame( state );




    // Wait for server acknowledgement
    state->retry_count += 1;
    state->net_timeout = 30;
    fprintf( stderr, "Ready for recv  " );
	state->recv_rdy = 1;	
    fprintf( stderr, "Set.\n" );


	LEAVE_STATE(0);
}



/*


State_RecvDataBlockAck


 */
int State_RecvDataBlockAck( TransferState* state )
{
	ENTER_STATE;

    cftp_daf_frame* daf_frame;

    daf_frame = (cftp_daf_frame*)(&state->frecv_buf);
    
    if( ntohll(daf_frame->BlockNum) == state->current_chunk )
        {
            VERBOSE( "Recieved DAF for block %ld/%ld. Continuing...\n",
                     state->current_chunk+1,state->local_file.num_chunks  );
            
            // Move onto the next block
            state->current_chunk++;
            state->retry_count = 0;
            if( state->current_chunk >= state->local_file.num_chunks )
                {
                    LEAVE_STATE(1);
                }
        }
    else
        {
            VERBOSE( "Recieved DAF for block %ld. Preparing to send block %d.\n",
                     ntohll(daf_frame->BlockNum)+1,
                     ntohll(daf_frame->BlockNum)+2);


            state->current_chunk = ntohll(daf_frame->BlockNum)+1;
            state->retry_count = 0;
            
        }

    
    // If the DAF is not received or the wrong DAF is received, then we
    // need to alter the next DTF such that its error code reflects the 
    // previously failed DTF. Probably the error code should be set to TIMEOUT.


    // Send the next needed piece;
    state->recv_rdy = 0;

	LEAVE_STATE(0);
}
