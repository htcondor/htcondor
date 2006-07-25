/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef _DAP_SERVER_INTERFACE_H
#define _DAP_SERVER_INTERFACE_H

#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include <strings.h>
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

//dap_server_run
#define SERV_SUBMIT_TCP_PORT      34044  //listening to dap_submit requests
#define SERV_STATUS_TCP_PORT      34045  //listening to dap_status requests
#define SERV_REMOVE_TCP_PORT      34046  //listening to dap_rm     requests
#define CLI_AGENT_LOG_TCP_PORT    34047
#define CLI_AGENT_SUBMIT_TCP_PORT 34048


//#define SERV_SUBMIT_TCP_PORT  34047  //listening to dap_submit requests
//#define SERV_STATUS_TCP_PORT  34048  //listening to dap_status requests
//#define SERV_REMOVE_TCP_PORT  34049  //listening to dap_rm     requests

#define MAX_TCP_BUF 2048
#define LISTENQ 5

#endif







