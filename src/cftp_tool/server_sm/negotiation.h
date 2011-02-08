#ifndef CFTP_SERVER_SM_NEGOTIATION_H
#define CFTP_SERVER_SM_NEGOTIATION_H

#include "../server_sm_lib.h"

// Define State Codes
#define S_RECV_SESSION_PARAMETERS 10
#define S_CHECK_SESSION_PARAMETERS 11
#define S_ACK_SESSION_PARAMETERS 12
#define S_RECV_CLIENT_READY 13

// Declare State Action


// S_RECV_SESSION_PARAMETERS
int State_ReceiveSessionParameters( ServerState* state );

// S_CHECK_SESSION_PARAMETERS
int State_CheckSessionParameters( ServerState* state );

// S_ACK_SESSION_PARAMETERS
int State_AcknowledgeSessionParameters( ServerState* state );

// S_RecieveClientReady
int State_ReceiveClientReady( ServerState* state );


#endif
