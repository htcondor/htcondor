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
#include "server_network.h"
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include "server_typedefs.h"
#include "server_filestat.h"
#include "server_xferstat.h"
#include <fstream.h>
#include <signal.h>
#include <unistd.h>


#define DEBUG


extern "C" int getpid();
extern "C" int gethostname();
extern "C" int close();
extern "C" int fork();
extern "C" int select();
extern "C" int socket();
extern "C" int bind();
extern "C" int accept();
extern "C" int listen();
extern "C" int getsockname();
extern "C" int getpid();
extern "C" int gethostname();
extern "C" int getsockopt();
extern "C" int setsockopt();


#define DEBUG

typedef void (*SIG_HANDLER)(int);


class Server
{
private:
  int                recv_req_sd;
  int                xmit_req_sd;
  int                max_transfers;
  int                max_recv_xfers;
  int                max_xmit_xfers;
  fd_set             fd_req_sds;
  int                max_req_fdp1;
  time_t             start_interval_time;
  struct timeval     req_pkt_timeout;
  xmit_req_pkt       xmit_req_info;
  xmit_reply_pkt     xmit_reply_info;
  struct sockaddr_in server_addr;
  struct sigaction   sigchild;
  void SetupReqPorts();
  void ProcessStoreReq();
  void ProcessRestoreReq();
  void StoreCheckpointFile(char* pathname, int size, int data_conn_sd);
  void RestoreCheckpointFile(char* pathname, int size, int data_conn_sd);
  void StripFilename(char* pathname);

public:
  Server();
  ~Server();
  void Init(int max_xfers, int max_recv, int max_send);
  void Execute();
};


void SigChildHandler(int);
void BlockSigChild(void);
void UnblockSigChild(void);




























