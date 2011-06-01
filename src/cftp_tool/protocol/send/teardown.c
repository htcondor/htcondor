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

State_SendFileFinish

*/
int State_SendFileFinish( TransferState* state )
{
	ENTER_STATE;

    cftp_fff_frame* fff_frame;

    fff_frame = (cftp_fff_frame*)(&state->fsend_buf);
    memset( fff_frame, 0, sizeof(cftp_frame));

	fff_frame->MessageType = FFF;
	fff_frame->ErrorCode = htons( NOERROR );
	fff_frame->SessionToken = htons( state->session_token ); 

    //Send file finish
    state->send_rdy = 1;
    send_cftp_frame( state );


    // Wait for Acknowledgement
	state->recv_rdy = 1;

	LEAVE_STATE(0);
}


/*

State_ReceiveFileFinishAck

 */
int State_ReceiveFileFinishAck( TransferState* state )
{
	ENTER_STATE;	

    VERBOSE( "File Finish Acknowledgement received. Send complete." );

	LEAVE_STATE(0);
}
