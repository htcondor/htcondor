#include "teardown.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../sha1-c/sha1.h"

/*

State_ReceiveFileFinish


*/
int State_ReceiveFileFinish( ServerState* state )
{
	ENTER_STATE;

	state->recv_rdy = 1;


	LEAVE_STATE(0);

}


/*


State_AcknowledgeFileFinish

 */
int State_AcknowledgeFileFinish( ServerState* state )
{
	ENTER_STATE;	

	cftp_faf_frame* faf_frame;
	SHA1Context     hashRecord;	
	unsigned char   chunk;
	int             len;

	faf_frame = (cftp_faf_frame*)(&state->fsend_buf);

		//No matter what happens next, we close the file now and reopen it for reading.

	fclose( state->local_file.fp );
	state->local_file.fp = fopen( state->local_file.filename, "r" );
	if( !state->local_file.fp )	
		{
			sprintf( state->error_string, 
					 "Failure to reopen file after chunks written out." );
			LEAVE_STATE(-1);
		}


		//If we have not gotten all chunks, we have a big problem
	if( state->current_chunk != state->local_file.num_chunks-1)
		{
			sprintf( state->error_string, 
					 "Entered teardown, but not all chunks were recieved. Aborting." );
			fclose( state->local_file.fp );
			LEAVE_STATE(-1);
		}
	

		// Check to confirm that the file is correct
	
	SHA1Reset( &hashRecord );
	chunk = fgetc( state->local_file.fp );
	while( !feof(state->local_file.fp) )
		{
			SHA1Input( &hashRecord, &chunk, 1);			
			chunk = fgetc( state->local_file.fp );
		}


	len = SHA1Result( &hashRecord );
	if( len == 0 )
		{
			sprintf(state->error_string,
					"Error while constructing hash. Hash is not right!" );
			LEAVE_STATE(-1);
		}

	if( state->local_file.hash[0] != hashRecord.Message_Digest[0] &&
		state->local_file.hash[1] != hashRecord.Message_Digest[1] &&
		state->local_file.hash[2] != hashRecord.Message_Digest[2] &&
		state->local_file.hash[3] != hashRecord.Message_Digest[3] &&
		state->local_file.hash[4] != hashRecord.Message_Digest[4] )
		{
			sprintf( state->error_string,
					 "Local file does not match hash value expected. Aborting." );
			LEAVE_STATE(-1);
		}
	else
		{
			VERBOSE( "Local file hash value matches. File transfer is successful." );
		}

		// Everything looks good, send FAF
	
	memset( faf_frame, 0 , sizeof( cftp_frame ) );
	faf_frame->MessageType = FAF;
	faf_frame->ErrorCode = htons(NOERROR);
	faf_frame->SessionToken = state->session_token;

	state->send_rdy = 1;
	send_cftp_frame( state );

	fclose( state->local_file.fp );


	LEAVE_STATE(0);
}
