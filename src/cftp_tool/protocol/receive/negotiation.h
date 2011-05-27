#ifndef CFTP_SERVER_SM_NEGOTIATION_H
#define CFTP_SERVER_SM_NEGOTIATION_H

#include "../receive.h"

// Define State Codes
#define S_RECV_SESSION_PARAMETERS 10
#define S_CHECK_SESSION_PARAMETERS 11
#define S_ACK_SESSION_PARAMETERS 12
#define S_RECV_CLIENT_READY 13

// Declare State Action


// S_RECV_SESSION_PARAMETERS
int State_ReceiveSessionParameters( TransferState* state );

// S_CHECK_SESSION_PARAMETERS
int State_CheckSessionParameters( TransferState* state );

// S_ACK_SESSION_PARAMETERS
int State_AcknowledgeSessionParameters( TransferState* state );

// S_RecieveClientReady
int State_ReceiveClientReady( TransferState* state );


#endif
