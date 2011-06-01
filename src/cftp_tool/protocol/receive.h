#ifndef CFTP_PROTOCOL_RECEIVE_H_
#define CFTP_PROTOCOL_RECEIVE_H_

#include "common.h"


typedef struct {
		// TODO: Fill in needed result fields
} ReceiveResults;


void receive_file( TransferArguments* args, ReceiveResults* results);
int  receive_run_state_machine( StateAction* states, TransferState* state );
int  receive_transition_table( TransferState* state, int condition );

#endif
