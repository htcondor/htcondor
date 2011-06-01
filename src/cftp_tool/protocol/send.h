#ifndef CFTP_PROTOCOL_SEND_H_
#define CFTP_PROTOCOL_SEND_H_

#include "common.h"

typedef struct {
    int success;
		// TODO: Fill in needed result fields
} SendResults;


void send_file( TransferArguments* args, SendResults* results);
int  send_run_state_machine( StateAction* states, TransferState* state );
int  send_transition_table( TransferState* state, int condition );

#endif
