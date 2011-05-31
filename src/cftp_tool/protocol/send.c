#include "send.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>


const int STATE_NUM = 100; // Will need to change this later

void send_file( TransferArguments* args, SendResults* results)
{
	StateAction SM_States[STATE_NUM];
	TransferState state;
	state.arguments = (void*)args;

	//-------------- Initialize SM States -----------------
	
	SM_States[S_UNKNOWN_ERROR]              = &State_UnknownError;
	SM_States[S_SEND_SESSION_CLOSE]         = &State_SendSessionClose;

	SM_States[S_SEND_SESSION_PARAMETERS]    = &State_SendSessionParameters;
	SM_States[S_RECV_SESSION_ACK]           = &State_ReceiveSessionParametersAck;
	SM_States[S_SEND_CLIENT_READY]          = &State_SendClientReady;

	SM_States[S_RECV_DATA_BLOCK]            = &State_ReceiveDataBlock;
	SM_States[S_ACK_DATA_BLOCK]             = &State_AcknowledgeDataBlock;

	SM_States[S_RECV_FILE_FINISH]           = &State_ReceiveFileFinish;
	SM_States[S_ACK_FILE_FINISH]            = &State_AcknowledgeFileFinish;

    //------------------------------------------------------- 	


	//Prep structure for initial state
	state.state = S_SEND_SESSION_PARAMETERS;	
	state.phase = NEGOTIATION;
	state.last_error = 0;
	memset( state.error_string, 0, 256 );

	state.session_token    = 1;  //TODO: Need better token generator  
	state.parameter_length = sizeof( simple_parameters );
	state.parameter_format = SIMPLE;


	while( send_run_state_machine( SM_States, &state )==0 )
	{
			// Do nothing here, we wait till the SM kills us.
	}

	// We may want some clean up based on
	// the final condition of session_state.

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
				return S_RECV_SESSION_ACK;
			


		case S_RECV_SESSION_ACK:
			if( condition == -1 ) // Something bad happened here
				return S_UNKNOWN_ERROR;

				//TODO: Need to handle parameter constraint failures

			return S_SEND_CLIENT_READY;



		case S_SEND_CLIENT_READY:
            return S_SEND_DATA_BLOCK;





			
		case S_UNKNOWN_ERROR:
			return -1; // If we have completed an unknown error, we should die.



		default:
			return S_UNKNOWN_ERROR;
			break;
		}
}



