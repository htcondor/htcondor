#ifndef CFTP_SERVER_SM_TEARDOWN_H
#define CFTP_SERVER_SM_TEARDOWN_H

#include "../server_sm_lib.h"

// Define State Codes
#define S_RECV_FILE_FINISH 30
#define S_ACK_FILE_FINISH 31

// Declare State Actions

//S_RECV_FILE_FINISH
int State_ReceiveFileFinish( ServerState* state );

//S_ACK_FILE_FINISH
int State_AcknowledgeFileFinish( ServerState* state );



#endif
