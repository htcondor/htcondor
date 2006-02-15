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
#ifndef SERVER2_H
#define SERVER2_H


#include "condor_common.h"
#include "constants2.h"
#include "typedefs2.h"
#include "gen_lib.h"
#include "network2.h"
#include "imds2.h"
#include "xferstat2.h"
#include "replication.h"
#include "list.h"
#include "classad_collection.h"


class Server
{
  private:
    int            more;
    int            req_ID;
    int            store_req_sd;
    int            restore_req_sd;
    int            service_req_sd;
	int            replicate_req_sd;
	int            socket_bufsize;
    int            max_xfers;
    int            max_store_xfers;
    int            max_restore_xfers;
	int            max_replicate_xfers;
    int            max_req_sd_plus1;
    IMDS           imds;
    TransferState  transfers;
    ofstream       log_file;
    int            num_store_xfers;
    int            num_restore_xfers;
	int            num_replicate_xfers;
    struct in_addr server_addr;
	struct sockaddr_in peer_addr_list[MAX_PEERS];
	int            num_peers;
	int		       replication_level;
	int			   reclaim_interval;
	int			   clean_interval;
	ClassAdCollection	*CkptClassAds;
    int SetUpPort(u_short port);
	void SetUpPeers();
	ReplicationSchedule replication_schedule;
	void ScheduleReplication(struct in_addr shadow_IP, char *owner,
							 char *filename, int level);
	void Replicate();
    void HandleRequest(int          req_sd,
		       request_type req);
    void ProcessServiceReq(int            req_id,
			   int            req_sd,
			   struct in_addr shadow_IP,
			   service_req_pkt service_req);
    void ProcessStoreReq(int            req_id,
			 int            req_sd,
			 struct in_addr shadow_IP,
			 store_req_pkt  store_req);
    void ProcessRestoreReq(int             req_id,
			   int             req_sd,
			   struct in_addr  shadow_IP,
			   restore_req_pkt restore_req);
    void ReceiveCheckpointFile(int         data_conn_sd,
			       const char* pathname,
			       int         file_size);
    void TransmitCheckpointFile(int         data_conn_sd,
				const char* pathname,
				int         file_size);
    void SendStatus(int data_conn_sd);

  public:
    Server();
    ~Server();
    void Init();
    void Execute();
    void ChildComplete();
    void NoMore();
    void ServerDump();
    void Log(int         request,
	     const char* event);
    void Log(const char* event);
};


void InstallSigHandlers();             // Four signals are caught: SIGALRM,
                                       //   SIGCHLD, SIGUSR1, and SIGUSR2

void BlockSignals();                   // Blocks all three signals to
                                       //   prevent corruption of the
                                       //   internal data structure

void UnblockSignals();                 // Unblocks the three signals


void SigAlarmHandler(int);


void SigChildHandler(int);


void SigUser1Handler(int);


void SigTermHandler(int);
void SigHupHandler(int);




#endif


