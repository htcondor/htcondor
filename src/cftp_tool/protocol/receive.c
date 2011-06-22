#include "receive.h"

#include "receive/discovery.h"
#include "receive/negotiation.h"
#include "receive/transfer.h"
#include "receive/teardown.h"
#include "receive/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>


const int STATE_NUM = 100; // Will need to change this later

void receive_file( TransferArguments* args, ReceiveResults* results)
{
	StateAction SM_States[STATE_NUM];
	TransferState* state;
    state = (TransferState*)malloc( sizeof(TransferState) );
    memset( state, 0, sizeof(TransferState));
	state->arguments = (void*)args;

	//-------------- Initialize SM States -----------------
	
	SM_States[S_UNKNOWN_ERROR]              = &State_UnknownError;
	SM_States[S_SEND_SESSION_CLOSE]         = &State_SendSessionClose;

	SM_States[S_RECV_SESSION_PARAMETERS]    = &State_ReceiveSessionParameters;
	SM_States[S_CHECK_SESSION_PARAMETERS]   = &State_CheckSessionParameters;
	SM_States[S_ACK_SESSION_PARAMETERS]     = &State_AcknowledgeSessionParameters;
	SM_States[S_RECV_CLIENT_READY]          = &State_ReceiveClientReady;

	SM_States[S_RECV_DATA_BLOCK]            = &State_ReceiveDataBlock;
	SM_States[S_ACK_DATA_BLOCK]             = &State_AcknowledgeDataBlock;

	SM_States[S_RECV_FILE_FINISH]           = &State_ReceiveFileFinish;
	SM_States[S_ACK_FILE_FINISH]            = &State_AcknowledgeFileFinish;

    //------------------------------------------------------- 	


	//Prep structure for initial state
	state->state = S_RECV_SESSION_PARAMETERS;	
	state->phase = NEGOTIATION;
	state->last_error = 0;
	memset( state->error_string, 0, 256 );


	while( receive_run_state_machine( SM_States, state )==0 )
	{
			// Do nothing here, we wait till the SM kills us.
	}

    // We finished with success
    if( state->state == -2 && state->last_error == 0 )
        {
            results->success = 1;
            results->filename = malloc( strlen( state->local_file.filename) +1);
            strcpy(results->filename,
                   state->local_file.filename );

        }
    else
        {
            results->success = 0;
        }
    


	// We may want some clean up based on
	// the final condition of session_state.

}


/*


run_state_machine 

*/
int receive_run_state_machine( StateAction* states, TransferState* state )
{
	int condition;


	if( state->state < 0 )
		return 1; // We are done here
	else	
		{
			condition = states[state->state]( state );
			recv_cftp_frame( state );
			state->state = receive_transition_table( state, condition );
			return 0;
		}
}


/*


transition_table

*/
int receive_transition_table( TransferState* state, int condition )
{

	switch( state->state )
		{


		case S_RECV_SESSION_PARAMETERS:
			if( state->frecv_buf.MessageType != SIF )
				{
					sprintf( state->error_string, 
							 "No frame received from client or bad frame type.");
					return S_UNKNOWN_ERROR;
				}
			else		
				return S_CHECK_SESSION_PARAMETERS;
			


		case S_CHECK_SESSION_PARAMETERS:
			if( condition == -1 ) // Something bad happened here
				return S_UNKNOWN_ERROR;

				//TODO: Need to handle parameter constraint failures

			return S_ACK_SESSION_PARAMETERS;



		case S_ACK_SESSION_PARAMETERS:
			if( state->frecv_buf.MessageType == SRF )
				return S_RECV_CLIENT_READY;
			
			if( state->frecv_buf.MessageType == SCF )
				return S_UNKNOWN_ERROR; // TODO: Create a state for client connection close

			return S_UNKNOWN_ERROR; 



		case S_RECV_CLIENT_READY:
			if( condition == -1 ) //Something bad happened with the file pointer
				return S_UNKNOWN_ERROR;
					
			return S_RECV_DATA_BLOCK;



		case S_RECV_DATA_BLOCK:
			if( state->frecv_buf.MessageType == DTF )
				return S_ACK_DATA_BLOCK;
			
			if( state->frecv_buf.MessageType == FFF )
				return S_ACK_FILE_FINISH;

			return S_UNKNOWN_ERROR;


		case S_ACK_DATA_BLOCK:
			if( condition == -1 )
				return S_UNKNOWN_ERROR;
			if( condition == 1 )
				return S_RECV_FILE_FINISH;

			return S_RECV_DATA_BLOCK;

			
		case S_RECV_FILE_FINISH:
			if( state->frecv_buf.MessageType == FFF )
				return S_ACK_FILE_FINISH;


		case S_ACK_FILE_FINISH:
            state->last_error = 0;
			return -2; // Probably need to do more here, but thats for later

					

			
		case S_UNKNOWN_ERROR:
			return -1; // If we have completed an unknown error, we should die.



		default:
			return S_UNKNOWN_ERROR;
			break;
		}
}



