/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
