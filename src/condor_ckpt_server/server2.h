#ifndef SERVER2_H
#define SERVER2_H


#include "constants2.h"
#include "typedefs2.h"
#include "gen_lib.h"
#include "network2.h"
#include "imds2.h"
#include "xferstat2.h"
#include "replication.h"
#include "list.h"
#include <sys/types.h>
#include <fstream.h>


class Server
{
  private:
    int            more;
    int            req_ID;
    int            store_req_sd;
    int            restore_req_sd;
    int            service_req_sd;
	int            replicate_req_sd;
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
    void Init(int max_new_xfers,
	      int max_new_store_xfers,
	      int max_new_restore_xfers);
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




#endif


