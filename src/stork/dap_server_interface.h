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







