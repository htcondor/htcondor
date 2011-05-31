#ifndef CFTP_SERVER_SM_TEARDOWN_H
#define CFTP_SERVER_SM_TEARDOWN_H

#include "../receive.h"

// Define State Codes
#define S_RECV_FILE_FINISH 30
#define S_ACK_FILE_FINISH 31

// Declare State Actions

//S_RECV_FILE_FINISH
int State_ReceiveFileFinish( TransferState* state );

//S_ACK_FILE_FINISH
int State_AcknowledgeFileFinish( TransferState* state );



#endif
