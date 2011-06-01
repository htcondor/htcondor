#include "send.h"

#include "send/discovery.h"
#include "send/negotiation.h"
#include "send/transfer.h"
#include "send/teardown.h"
#include "send/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>


const int STATE_NUM = 100; // Will need to change this later

void send_file( TransferArguments* args, SendResults* results)
{
	StateAction SM_States[STATE_NUM];
	TransferState* state;
    state = malloc( sizeof(TransferState) );
    memset( &state, 0, sizeof(TransferState));

	state->arguments = args;

	//-------------- Initialize SM States -----------------
	
	SM_States[S_UNKNOWN_ERROR]              = &State_UnknownError;
	SM_States[S_SEND_SESSION_CLOSE]         = &State_SendSessionClose;

	SM_States[S_SEND_SESSION_PARAMETERS]    = &State_SendSessionParameters;
	SM_States[S_RECV_ACK_SESSION]           = &State_ReceiveSessionParametersAck;
	SM_States[S_SEND_CLIENT_READY]          = &State_SendClientReady;

	SM_States[S_SEND_DATA_BLOCK]            = &State_SendDataBlock;
	SM_States[S_RECV_ACK_DATA_BLOCK]        = &State_RecvDataBlockAck;

	SM_States[S_SEND_FILE_FINISH]           = &State_SendFileFinish;
	SM_States[S_RECV_ACK_FILE_FINISH]       = &State_ReceiveFileFinishAck;

    //------------------------------------------------------- 	

    // Open file
    open_file( state->arguments->file_path, &state->local_file);

	//Prep structure for initial state
	state->state = S_SEND_SESSION_PARAMETERS;	
	state->phase = NEGOTIATION;
	state->last_error = 0;
	memset( state->error_string, 0, 256 );

	state->session_token    = 1;  //TODO: Need better token generator  
	state->parameter_length = sizeof( simple_parameters );
	state->parameter_format = SIMPLE;


	while( send_run_state_machine( SM_States, state )==0 )
	{
			// Do nothing here, we wait till the SM kills us.
	}

	// We may want some clean up based on
	// the final condition of session_state.

    free_TransferState( state ); 

}


/*


run_state_machine 

*/
int send_run_state_machine( StateAction* states, TransferState* state )
{
	int condition;


	if( state->state == -1 )
		return 1; // We are done here
	else	
		{
			condition = states[state->state]( state );
			recv_cftp_frame( state );
			state->state = send_transition_table( state, condition );
			return 0;
		}
}


/*


transition_table

*/
int send_transition_table( TransferState* state, int condition )
{

	switch( state->state )
		{


		case S_SEND_SESSION_PARAMETERS:
			if( state->frecv_buf.MessageType != SAF )
				{
					sprintf( state->error_string, 
							 "No parameter acknowledgement received from server or bad frame type.");
					return S_UNKNOWN_ERROR;
				}
			else		
				return S_RECV_ACK_SESSION;
			


		case S_RECV_ACK_SESSION:
			if( condition == -1 ) // Something bad happened here
				return S_UNKNOWN_ERROR;

				//TODO: Need to handle parameter constraint failures

			return S_SEND_CLIENT_READY;



		case S_SEND_CLIENT_READY:
            return S_SEND_DATA_BLOCK;


        case S_SEND_DATA_BLOCK:
			if( state->frecv_buf.MessageType != DAF )
				{
					sprintf( state->error_string, 
							 "Wrong frame type recieved. Resending last DTF.");
                    return S_SEND_DATA_BLOCK;
				}
            return S_RECV_ACK_DATA_BLOCK;


        case S_RECV_ACK_DATA_BLOCK:
            if( condition == 1 )
                return S_SEND_FILE_FINISH;

            return S_SEND_DATA_BLOCK;

            
        case S_SEND_FILE_FINISH:
            return S_RECV_ACK_FILE_FINISH;

			
		case S_UNKNOWN_ERROR:
			return -1; // If we have completed an unknown error, we should die.



		default:
			return S_UNKNOWN_ERROR;
			break;
		}
}



