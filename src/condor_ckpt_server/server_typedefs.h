/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/******************************************************************************
*                                                                             *
*   Author:  Hsu-lin Tsao                                                     *
*   Project: Condor Checkpoint Server                                         *
*   Date:    May 1, 1995                                                      *
*   Version: 0.5 Beta                                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Module:  server_typedefs.h                                                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains type definitions of packets used to communicate with *
*   the Checkpoint Server.  Currently, only two types of requests are         *
*   supported: storing a checkpoint file, and restoring a checkpoint file.    *
*   For each request, a reply must be sent to the requesting shadow process.  *
*                                                                             *
*   In a later release, it will be necessary to support a message to the      *
*   Checkpoint Server to indicate that a Condor job has completed, and all    *
*   related checkpoint files may be deleted.                                  *
*                                                                             *
******************************************************************************/


#ifndef SERVER_TYPEDEFS_H
#define SERVER_TYPEDEFS_H


/* Header Files */

#include "server_network.h"
#include "server_constants.h"




/* Type Definitions */


typedef unsigned int u_lint;


typedef struct recv_req_pkt
{
  u_lint  file_size;
  u_lint  ticket;
  char    filename[MAX_CONDOR_FILENAME_LENGTH];
  char    owner[MAX_NAME_LENGTH];
  u_lint  priority;                                    
  u_lint  time_consumed;                             
} recv_req_pkt;




typedef struct recv_reply_pkt
{
  struct in_addr server_name;
  u_short        port;
  u_short        req_status;
} recv_reply_pkt;


typedef struct xmit_req_pkt
{
  u_lint ticket;
  char   filename[MAX_CONDOR_FILENAME_LENGTH];
  char   owner[MAX_NAME_LENGTH];
  u_lint priority;
} xmit_req_pkt;




typedef struct xmit_reply_pkt
{
  struct in_addr server_name;
  u_short        port;
  u_lint         file_size;
  u_short        req_status;
} xmit_reply_pkt;


typedef recv_req_pkt   store_req_pkt;
typedef recv_reply_pkt store_reply_pkt;
typedef xmit_req_pkt   restore_req_pkt;
typedef xmit_reply_pkt restore_reply_pkt;


#endif
