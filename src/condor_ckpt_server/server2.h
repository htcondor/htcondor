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
#include "protocol.h"
#include "directory.h"


class Server
{
  private:
  	char		   *ckpt_server_dir;
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
    std::ofstream       log_file;
    int            num_store_xfers;
    int            num_restore_xfers;
	int            num_replicate_xfers;
    struct in_addr server_addr;
	struct sockaddr_in peer_addr_list[MAX_PEERS];
	int            num_peers;
	int		       replication_level;
	int			   reclaim_interval;
	int			   clean_interval;
	int            check_parent_interval;

	time_t         remove_stale_ckptfile_interval;
	time_t         next_time_to_remove_stale_ckpt_files;
	time_t         stale_ckptfile_age_cutoff;

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
				FDContext *fdc,
				struct in_addr shadow_IP,
				service_req_pkt service_req);
    void ProcessStoreReq(int            req_id,
				FDContext *fdc,
				struct in_addr shadow_IP,
				store_req_pkt  store_req);
    void ProcessRestoreReq(int             req_id,
				FDContext *fdc,
				struct in_addr  shadow_IP,
				restore_req_pkt restore_req);
    void ReceiveCheckpointFile(int         data_conn_sd,
			       const char* pathname,
			       int         file_size);
    void TransmitCheckpointFile(int         data_conn_sd,
				const char* pathname,
				int         file_size);
    void SendStatus(int data_conn_sd);

	void RemoveStaleCheckpointFiles(const char *directory);
	void RemoveStaleCheckpointFilesRecurse(const char *path, Directory *dir,
		time_t cutoff_time, time_t a_time);

  public:
    Server();
    ~Server();
    void Init();
    void Execute();
    void ChildComplete();
    void NoMore(char const *reason);
    void ServerDump();
    static void Log(int         request,
	     const char* event);
    static void Log(const char* event);
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


void SigQuitHandler(int);
void SigTermHandler(int);
void SigHupHandler(int);




#endif


