#ifndef CFTP_SERVER_SM_NEGOTIATION_H
#define CFTP_SERVER_SM_NEGOTIATION_H

#include "../send.h"

// Define State Codes
#define S_SEND_SESSION_PARAMETERS 14
#define S_RECV_SESSION_ACK 15
#define S_SEND_CLIENT_READY 16

// Declare State Action


// S_SEND_SESSION_PARAMETERS
int State_SendSessionParameters( TransferState* state );

// S_RECV_SESSION_ACK
int State_ReceiveSessionParametersAck( TransferState* state );

// S_SEND_CLIENT_READY
int State_SendClientReady( TransferState* state );

#endif
