#ifndef CFTP_PROTOCOL_RECEIVE_H_
#define CFTP_PROTOCOL_RECEIVE_H_

#include "common.h"

#include "send/discovery.h"
#include "send/negotiation.h"
#include "send/transfer.h"
#include "send/teardown.h"
#include "send/error.h"


typedef struct {
    int success;
		// TODO: Fill in needed result fields
} SendResults;


void send_file( TransferArguments* args, SendResults* results);
int  send_run_state_machine( StateAction* states, TransferState* state );
int  send_transition_table( TransferState* state, int condition );

#endif
