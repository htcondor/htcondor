#ifndef CFTP_SERVER_SM_TRANSFER_H
#define CFTP_SERVER_SM_TRANSFER_H

#include "../send.h"

// Define State Codes
#define S_SEND_DATA_BLOCK 22
#define S_RECV_ACK_DATA_BLOCK 23


// TODO: Put this somewhere else, perferably in a config of some sort
const int MAX_RETRIES = 10;


// Declare State Actions

//S_SEND_DATA_BLOCK
int State_SendDataBlock( TransferState* state );

//S_RECV_DATA_BLOCK_ACK
int State_RecvDataBlockAck( TransferState* state );



#endif
