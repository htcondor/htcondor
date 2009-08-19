/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

#define	DEFAULT_XFERS						50

#define CKPT_ACCEPT_TIMEOUT					120

#define CKPT_OK                             0
#define CKPT_SVR_STORE_REQ_PORT             5651
#define CKPT_SVR_RESTORE_REQ_PORT           5652
#define CKPT_SVR_SERVICE_REQ_PORT           5653
#define CKPT_SVR_REPLICATE_REQ_PORT			5654

/* Changing this constant will result is total havoc with the backward 
	compatibility code in protocol.cpp. If you do change it, it effectively
	means a "different version" of the protocol. */
#ifdef OSF1
#define AUTHENTICATION_TCKT                 1637102411
#else
#define AUTHENTICATION_TCKT                 1637102411L
#endif

#define MAX_CONDOR_FILENAME_LENGTH          256
#define MAX_MACHINE_NAME_LENGTH             50
#define MAX_NAME_LENGTH                     50
#define MAX_PATHNAME_LENGTH                 512
#define DATA_BUFFER_SIZE                    50000
#define RECLAIM_INTERVAL                    60*5
#define CLEAN_INTERVAL						60*60*24
#define MAX_ALLOWED_XFER_TIME               60*60
#define RECLAIM_BYPASS                      100
#define STATUS_FILENAME_LENGTH              31
#define STATUS_MACHINE_NAME_LENGTH          16
#define STATUS_OWNER_NAME_LENGTH            16
#define MAX_RETRIES                         5
#define LOCAL_DRIVE_PREFIX                  "./"
#define FILE_INFO_FILENAME                  "fileinfo.dat"
#define TEMP_FILE_INFO_FILENAME             "tempinfo.dat"
#define SERVER                              "chestnut.cs.wisc.edu"
#define LOG_FILE                            "server_log"
#define OLD_LOG_FILE                        "server_log.old"
#define ADD                                 20000
#define MAX_ASCII_CODED_DECIMAL_LENGTH      15
#define REQUEST_TIMEOUT                     5
#define OVERRIDE                            1
#define LOG_EVENT_WIDTH_1                   49
#define LOG_EVENT_WIDTH_2                   47
#define MAX_PEERS							50




/* Exit/Return Codes */

#define CANNOT_WRITE_IMDS_FILE              -200
#define CANNOT_DELETE_IMDS_FILE             -201
#define CANNOT_RENAME_IMDS_FILE             -202
#define LOCAL                               -210
#define REMOTE                              -211
#define INSUFFICIENT_RESOURCES              -212
#define CHILDTERM_SUCCESS                   1
#define ILLEGAL_SERVER                      9
#define BAD_PARAMETERS                      10
#define XFER_BAD_SOCKET                     11
#define XFER_CANNOT_BIND                    12
#define BAD_AUTHENTICATION_TICKET           13
#define CHILDTERM_TOO_MANY_XFERS            14
#define CHILDTERM_STORE_BAD_ARGUMENTS       15
#define CHILDTERM_SHADOW_DEAD               16
#define CHILDTERM_READ_ERROR                17
#define BAD_REQ_PKT                         18
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
#define CHILDTERM_NO_FILE_STATUS            100
#define CHILDTERM_ERROR_ON_STATUS_WRITE     101
#define CHILDTERM_ERROR_SHORT_FILE          102
#define CHILDTERM_ERROR_INCOMPLETE_FILE     103
#define CHILDTERM_SIGNALLED                 104
#define CHILDTERM_ACCEPT_ERROR              27
#define LOG_FILE_OPEN_ERROR                 26
#define ACCEPT_ERROR                        -27
#define BIND_ERROR                          28
#define CKPT_SERVER_SOCKET_ERROR            -29
#define BAD_SOCKET_DESC_ERROR               30
#define CKPT_SERVER_TIMEOUT		            -30
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
#define CANNOT_RENAME_FILE                  46
#define BAD_CHILD_PID                       2      /* Can use since it is the
                                                        PID of the Init 
							process              */
#define BAD_TRANSFER_LIST                   48
#define BAD_RETURN_CODE                     49
#define SIGNAL_MASK_ERROR                   50
#define PARTIAL_REQ_RECVD                   70
#define NO_REQ_RECVD                        71
#define INSUFFICIENT_BANDWIDTH              72
#define DESIRED_FILE_NOT_FOUND              73
#define IMDS_INDEX_ERROR                    80
#define CANNOT_RENAME_OVER_NEWER_FILE		82
#define EXISTS                              90
#define DOES_NOT_EXIST                      91
#define CREATED                             92
#define REMOVED_FILE                        93
#define RENAMED                             94
#define FILE_ALREADY_EXISTS                 95
#define BAD_REQUEST_TYPE                    110
#define IMDS_BAD_RETURN_CODE                111
#define BAD_SERVICE_TYPE                    112
#define INSUFFICIENT_PERMISSIONS            113
#define CANNOT_LOCATE_SERVER                120
#define CONNECT_ERROR                       -121



#endif




