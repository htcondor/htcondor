#include "cftp_recieve.h"

#include "server_sm/discovery.h"
#include "server_sm/negotiation.h"
#include "server_sm/transfer.h"
#include "server_sm/teardown.h"
#include "server_sm/error.h"


StateAction SM_States[STATE_NUM];
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


CFTP_RecieveService::CFTP_RecieveService( int handle ) : Service(handle)
{}

CFTP_RecieveService::~CFTP_RecieveService()
{
}

void CFTP_RecieveService::step()
{

	int condition;


	if( _state.state == -1 )
		_finished = 1;
		return; // We are done here
	else	
		{
			condition = SM_States[_state.state]( &_state );
			recv_cftp_frame( &_state );
			_state.state = transition_table( condition );
		}
}

int CFTP_RecieveService::transition_table( int condition )
{

	switch( _state.state )
		{


		case S_RECV_SESSION_PARAMETERS:
			if( _state.frecv_buf.MessageType != SIF )
				{
					sprintf( _state.error_string, 
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
			if( _state.frecv_buf.MessageType == SRF )
				return S_RECV_CLIENT_READY;
			
			if( _state.frecv_buf.MessageType == SCF )
				return S_UNKNOWN_ERROR; // TODO: Create a state for client connection close

			return S_UNKNOWN_ERROR; 



		case S_RECV_CLIENT_READY:
			if( condition == -1 ) //Something bad happened with the file pointer
				return S_UNKNOWN_ERROR;
					
			return S_RECV_DATA_BLOCK;



		case S_RECV_DATA_BLOCK:
			if( _state.frecv_buf.MessageType == DTF )
				return S_ACK_DATA_BLOCK;
			
			if( _state.frecv_buf.MessageType == FFF )
				return S_ACK_FILE_FINISH;

			return S_UNKNOWN_ERROR;


		case S_ACK_DATA_BLOCK:
			if( condition == -1 )
				return S_UNKNOWN_ERROR;
			if( condition == 1 )
				return S_RECV_FILE_FINISH;

			return S_RECV_DATA_BLOCK;

			
		case S_RECV_FILE_FINISH:
			if( _state.frecv_buf.MessageType == FFF )
				return S_ACK_FILE_FINISH;


		case S_ACK_FILE_FINISH:
			return -1; // Probably need to do more here, but thats for later

					

			
		case S_UNKNOWN_ERROR:
			return -1; // If we have completed an unknown error, we should die.



		default:
			return S_UNKNOWN_ERROR;
			break;
		}


}
