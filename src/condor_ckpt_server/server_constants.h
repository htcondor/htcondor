/******************************************************************************
*                                                                             *
*   Author:  Hsu-lin Tsao                                                     *
*   Project: Condor Checkpoint Server                                         *
*   Date:    May 1, 1995                                                      *
*   Version: 0.5 Beta                                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Module:  server_constants.h                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains constants used by the Condor Checkpoint Server and   *
*   by shadow processes.  Although the Checkpoint Server is primarily written *
*   in C++, many other modules in Condor are written in C.  Therefore,        *
*   preprocessor symbolic constants are used.                                 *
*                                                                             *
******************************************************************************/


#ifndef SERVER_CONSTANTS_H
#define SERVER_CONSTANTS_H


/* Symbolic Constants */

#define OK                                  0
#define CKPT_SVR_STORE_REQ_PORT             5651
#define CKPT_SVR_RESTORE_REQ_PORT           5652
#define AUTHENTICATION_TCKT                 1637102411L
#define MAX_CONDOR_FILENAME_LENGTH          256
#define MAX_MACHINE_NAME_LENGTH             50
#define MAX_NAME_LENGTH                     50
#define NIL                                 -1
#define DATA_BUFFER_SIZE                    50000
#define RECLAIM_INTERVAL                    300
#define MAX_ALLOWED_XFER_TIME               600
#define RECLAIM_BYPASS                      100
#define AFS_PREFIX                          "~tsao"
#define SERVER                              "chestnut.cs.wisc.edu"
#define LOG_FILE                            "/tmp/server_log"
#define OLD_LOG_FILE                        "/tmp/server_log.old"


/* Exit/Return Codes */

#define CHILDTERM_SUCCESS                   1
#define BAD_PARAMETERS                      10
#define XFER_BAD_SOCKET                     11
#define XFER_CANNOT_BIND                    12
#define BAD_AUTHENTICATION_TICKET           13
#define CHILDTERM_TOO_MANY_XFERS            14
#define CHILDTERM_STORE_BAD_ARGUMENTS       15
#define CHILDTERM_SHADOW_DEAD               16
#define CHILDTERM_READ_ERROR                17
#define CHILDTERM_BAD_REQ_PKT               18
#define CHILDTERM_CANNOT_WRITE              19
#define CHILDTERM_CANNOT_CREATE_CHKPT_FILE  20
#define CHILDTERM_CANNOT_OPEN_CHKPT_FILE    61
#define CHILDTERM_ERROR_ON_CHKPT_FILE_WRITE 21
#define CHILDTERM_ERROR_ON_CHKPT_FILE_READ  62
#define CHILDTERM_ERROR_ON_CHKPT_READING    22
#define CHILDTERM_EXEC_KILLED_SOCKET        23
#define CHILDTERM_BAD_FILE_SIZE             24
#define CHILDTERM_WRONG_FILE_SIZE           25
#define CHILDTERM_KILLED                    60
#define LOG_FILE_OPEN_ERROR                 26
#define ACCEPT_ERROR                        27
#define BIND_ERROR                          28
#define SOCKET_ERROR                        29
#define BAD_SOCKET_DESC_ERROR               30
#define NOT_TCPIP                           31
#define LISTEN_ERROR                        32
#define SELECT_ERROR                        33
#define FORK_ERROR                          34
#define MKDIR_ERROR                         35
#define SIGNAL_HANDLER_ERROR                36
#define HOSTNAME_ERROR                      37
#define GETHOSTBYNAME_ERROR                 38
#define GETHOSTBYADDR_ERROR                 39
#define DYNAMIC_ALLOCATION                  40
#define CANNOT_FIND_MACHINE                 41
#define CANNOT_FIND_OWNER                   42
#define CANNOT_FIND_FILE                    43
#define CANNOT_DELETE_FILE                  44
#define CANNOT_DELETE_DIRECTORY             45
#define BAD_CHILD_PID                       46
#define BAD_TRANSFER_LIST                   47
#define BAD_RETURN_CODE                     48
#define SIGNAL_MASK_ERROR                   49
#define PARTIAL_REQ_RECVD                   70
#define NO_REQ_RECVD                        71
#define INSUFFICIENT_BANDWIDTH              72
#define DESIRED_FILE_NOT_FOUND              73
#define RECV                                100
#define XMIT                                101


#endif



