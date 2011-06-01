#ifndef CFTP_SERVER_SM_TEARDOWN_H
#define CFTP_SERVER_SM_TEARDOWN_H

#include "../receive.h"

// Define State Codes
#define S_SEND_FILE_FINISH 32
#define S_RECV_ACK_FILE_FINISH 33

// Declare State Actions

//S_SEND_FILE_FINISH
int State_SendFileFinish( TransferState* state );

//S_RECV_ACK_FILE_FINISH
int State_ReceiveFileFinishAck( TransferState* state );



#endif
