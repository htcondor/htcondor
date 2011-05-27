#ifndef CFTP_SERVER_SM_ERROR_H
#define CFTP_SERVER_SM_ERROR_H

#include "../receive.h"

// Define State Codes
#define S_UNKNOWN_ERROR 0
#define S_SEND_SESSION_CLOSE 1

// Declare State Actions

//S_UNKNOWN_ERROR
int State_UnknownError( TransferState* state );

//S_SEND_SESSION_CLOSE
int State_SendSessionClose( TransferState* state );



#endif
