#ifndef CFTP_SERVER_SM_TRANSFER_H
#define CFTP_SERVER_SM_TRANSFER_H

#include "../server_sm_lib.h"

// Define State Codes
#define S_RECV_DATA_BLOCK 20
#define S_ACK_DATA_BLOCK 21

// Declare State Actions

//S_RECV_DATA_BLOCK
int State_ReceiveDataBlock( ServerState* state );

//S_ACK_DATA_BLOCK
int State_AcknowledgeDataBlock( ServerState* state );



#endif
