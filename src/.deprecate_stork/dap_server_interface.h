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
#include "condor_daemon_core.h"

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







