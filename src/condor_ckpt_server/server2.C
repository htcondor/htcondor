#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_fdset.h"
#include "xfer_summary.h"
#include "string_list.h"
#include "internet.h"

#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <iomanip.h>

#include "server2.h"
#include "gen_lib.h"
#include "network2.h"
#include "signal2.h"
#include "alarm2.h"
#include "condor_uid.h"

#undef DEBUG


XferSummary	xfer_summary;

Server server;
Alarm  rt_alarm;

extern "C" {
ssize_t stream_file_xfer( int src_fd, int dst_fd, size_t n_bytes );
long random( void );
int srandom( unsigned seed );
}


Server::Server()
{
	char*          temp;
	struct in_addr temp_addr;
	
	more = 1;
	req_ID = 0;
	temp = gethostaddr();
	if (temp == NULL) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: unable to obtain host's IP address" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(HOSTNAME_ERROR);
    }
	memcpy((char*) &server_addr, temp, sizeof(struct in_addr));
	memcpy((char*) &temp_addr, temp, sizeof(struct in_addr));
	if (memcmp(&server_addr, &temp_addr, sizeof(struct in_addr)) != 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: checkpoint server running on illegal server" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(ILLEGAL_SERVER);
    }
	store_req_sd = -1;
	restore_req_sd = -1;
	service_req_sd = -1;
	replicate_req_sd = -1;
	max_xfers = 0;
	max_store_xfers = 0;
	max_restore_xfers = 0;
	max_replicate_xfers = 0;
	num_peers = 0;
}


Server::~Server()
{
	if (store_req_sd >= 0)
		close(store_req_sd);
	if (restore_req_sd >=0)
		close(restore_req_sd);
	if (service_req_sd >=0)
		close(service_req_sd);
	if (replicate_req_sd >=0)
		close(replicate_req_sd);
}


void Server::Init(int max_new_xfers,
				  int max_new_store_xfers,
				  int max_new_restore_xfers)
{
	struct stat log_stat;
	char        log_filename[100];
	char        old_log_filename[100];
	char		*ckpt_server_dir, *level;
#ifdef DEBUG
	char        log_msg[256];
	char        hostname[100];
	int         buf_size, len;
#endif
	
	max_xfers = max_new_xfers;
	max_store_xfers = max_new_store_xfers;
	max_restore_xfers = max_new_restore_xfers;
	max_replicate_xfers = max_store_xfers/5;
	num_store_xfers = 0;
	num_restore_xfers = 0;
	num_replicate_xfers = 0;
#if DEBUG
	sprintf(log_filename, "%s%s", LOCAL_DRIVE_PREFIX, LOG_FILE);
	if (stat(log_filename, &log_stat) == 0) {
		sprintf(old_log_filename, "%s%s", LOCAL_DRIVE_PREFIX, OLD_LOG_FILE);
		rename(log_filename, old_log_filename);
    }
	log_file.open(log_filename);
	if (log_file.fail()) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: unable to open checkpoint server log file" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(LOG_FILE_OPEN_ERROR);
    }
#endif

	config( 0 );
	dprintf_config( "CKPT_SERVER", 2 );

	ckpt_server_dir = param( "CKPT_SERVER_DIR" );
	if (ckpt_server_dir) {
		if (chdir(ckpt_server_dir) < 0) {
			dprintf(D_ALWAYS, "Failed to chdir() to %s\n", ckpt_server_dir);
			exit(1);
		}
		dprintf(D_ALWAYS, "CKPT_SERVER running in directory %s\n", 
				ckpt_server_dir);
	}

	level = param( "CKPT_SERVER_REPLICATION_LEVEL" );
	if (level)
		replication_level = atoi(level);
	else
		replication_level = 0;

	store_req_sd = SetUpPort(CKPT_SVR_STORE_REQ_PORT);
	restore_req_sd = SetUpPort(CKPT_SVR_RESTORE_REQ_PORT);
	service_req_sd = SetUpPort(CKPT_SVR_SERVICE_REQ_PORT);
	replicate_req_sd = SetUpPort(CKPT_SVR_REPLICATE_REQ_PORT);
	max_req_sd_plus1 = store_req_sd;
	if (restore_req_sd > max_req_sd_plus1)
		max_req_sd_plus1 = restore_req_sd;
	if (service_req_sd > max_req_sd_plus1)
		max_req_sd_plus1 = service_req_sd;
	if (replicate_req_sd > max_req_sd_plus1)
		max_req_sd_plus1 = replicate_req_sd;
	max_req_sd_plus1++;
	InstallSigHandlers();
	SetUpPeers();
	xfer_summary.init();
#ifdef DEBUG
	Log(req_ID, "Server Initializing:");
	Log("Server:                            ");
	if (gethostname(hostname, 99) == 0) {
		hostname[99] = '\0';
		Log(hostname);
    }
	else
		Log("Cannot resolve host name");
	sprintf(log_msg, "%s%d", "Store Request Port:                ", 
			CKPT_SVR_STORE_REQ_PORT);
	Log(log_msg);
	sprintf(log_msg, "%s%d", "Store Request Socket Descriptor:   ",
			store_req_sd);
	Log(log_msg);
	len = sizeof(buf_size);
	if (getsockopt(store_req_sd, SOL_SOCKET, SO_RCVBUF, (char*) &buf_size,
				   &len) < 0)
		sprintf(log_msg, "%s%s", "Store Request Buffer Size:         ",
				"Cannot access buffer size");
	else
		sprintf(log_msg, "%s%d", "Store Request Buffer Size:         ", 
				buf_size);
	Log(log_msg);
	sprintf(log_msg, "%s%d", "Restore Request Port:              ",
			CKPT_SVR_RESTORE_REQ_PORT);
	Log(log_msg);
	sprintf(log_msg, "%s%d", "Restore Request Socket Descriptor: ",
			restore_req_sd);
	Log(log_msg);
	len = sizeof(buf_size);
	if (getsockopt(restore_req_sd, SOL_SOCKET, SO_RCVBUF, (char*) &buf_size,
				   &len) < 0)
		sprintf(log_msg, "%s%s", "Restore Request Buffer Size:       ",
				"Cannot access buffer size");
	else
		sprintf(log_msg, "%s%d", "Restore Request Buffer Size:       ", buf_size);
	Log(log_msg);
	sprintf(log_msg, "%s%d", "Service Request Port:              " ,
			CKPT_SVR_SERVICE_REQ_PORT);
	Log(log_msg);
	sprintf(log_msg, "%s%d", "Service Request Socket Descriptor: ",
			service_req_sd);
	Log(log_msg);
	len = sizeof(buf_size);
	if (getsockopt(service_req_sd, SOL_SOCKET, SO_RCVBUF, (char*) &buf_size,
				   &len) < 0)
		sprintf(log_msg, "%s%s", "Service Request Buffer Size:       ", 
				"Cannot access buffer size");
	else
		sprintf(log_msg, "%s%d", "Service Request Buffer Size:       ",
				buf_size);
	Log(log_msg);
	Log("Signal handlers installed:         SIGCHLD");
	Log("                                   SIGUSR1");
	Log("                                   SIGUSR2");
	Log("                                   SIGALRM");
	sprintf(log_msg, "%s%d", "Total allowable transfers:         ", max_xfers);
	Log(log_msg);
	sprintf(log_msg, "%s%d", "Number of storing transfers:       ",
			max_store_xfers);
	Log(log_msg);
	sprintf(log_msg, "%s%d", "Number of restoring transfers:     ",
			max_restore_xfers);
	Log(log_msg);
	log_file << endl << endl << endl;
	log_file << "   Local Time    Request ID   Event" << endl;
	log_file << "   ----------    ----------   -----" << endl;
#endif
}


int Server::SetUpPort(u_short port)
{
  struct sockaddr_in socket_addr;
  int                temp_sd;
  char               log_msg[256];
  int                ret_code;

  temp_sd = I_socket();
  if (temp_sd == INSUFFICIENT_RESOURCES) {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: insufficient resources for new socket" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      Log(0, "ERROR: insufficient resources for new socket");
      exit(INSUFFICIENT_RESOURCES);
    }
  else if (temp_sd == CKPT_SERVER_SOCKET_ERROR) {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot open a server request socket" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      Log(0, "ERROR: cannot open a server request socket");
      exit(CKPT_SERVER_SOCKET_ERROR);
  }
  memset((char*) &socket_addr, 0, sizeof(struct sockaddr_in));
  socket_addr.sin_family = AF_INET;
  socket_addr.sin_port = htons(port);
  memcpy((char*) &socket_addr.sin_addr, (char*) &server_addr, 
	 sizeof(struct in_addr));
  if ((ret_code=I_bind(temp_sd, &socket_addr)) != CKPT_OK) {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: I_bind() returns an error (#" << ret_code << ")" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      sprintf(log_msg, "ERROR: I_bind() returns an error (#%d)", ret_code);
      Log(0, log_msg);
      exit(ret_code);
  }
  if (I_listen(temp_sd, 5) != CKPT_OK) {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot listen on a socket" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      Log(0, "ERROR: I_listen() fails");
      exit(LISTEN_ERROR);
  }
  if (ntohs(socket_addr.sin_port) != port) {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot use Condor well-known port " << port << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      Log(0, "ERROR: cannot use Condor well-known port");
      exit(BIND_ERROR);
  }
  return temp_sd;
}

void Server::SetUpPeers()
{
	char *peers, *peer, peer_addr[256];
	StringList peer_name_list;

	if ((peers = param("CKPT_SERVER_HOSTS")) == NULL) {
		return;
	}

	peer_name_list.initializeFromString(peers);
	peer_name_list.rewind();
	while ((peer = peer_name_list.next()) != NULL) {
		if (strcmp(peer, param("CKPT_SERVER_HOST"))) {
			sprintf(peer_addr, "<%s:%d>", peer, CKPT_SVR_REPLICATE_REQ_PORT);
			string_to_sin(peer_addr, peer_addr_list+(num_peers++));
		}
	}
}

void Server::Execute()
{
	fd_set         req_sds;
	int            num_sds_ready;
	time_t         current_time;
	time_t         start_interval_time;
	struct timeval poll;
	
	start_interval_time = time(NULL);
	while (more) {                          // Continues until SIGUSR2 signal
		poll.tv_sec = RECLAIM_INTERVAL;
		poll.tv_usec = 0;
		errno = 0;
		FD_ZERO(&req_sds);
		FD_SET(store_req_sd, &req_sds);
		FD_SET(restore_req_sd, &req_sds);
		FD_SET(service_req_sd, &req_sds);
		FD_SET(replicate_req_sd, &req_sds);
		// Can reduce the number of calls to the reclaimer by blocking until a
		//   request comes in (change the &poll argument to NULL).  The 
		//   philosophy is that one does not need to reclaim a child process
		//   until another is ready to start
		while ((more) && ((num_sds_ready=select(max_req_sd_plus1,
#if defined(HPUX9)
												(int *)&req_sds, 
#else
												&req_sds, 
#endif
												NULL, NULL, &poll)) > 0)) {
			if (FD_ISSET(service_req_sd, &req_sds))
				HandleRequest(service_req_sd, SERVICE_REQ);
			if (FD_ISSET(store_req_sd, &req_sds))
				HandleRequest(store_req_sd, STORE_REQ);
			if (FD_ISSET(restore_req_sd, &req_sds))
				HandleRequest(restore_req_sd, RESTORE_REQ);
			if (FD_ISSET(replicate_req_sd, &req_sds))
				HandleRequest(replicate_req_sd, REPLICATE_REQ);
			FD_ZERO(&req_sds);
			FD_SET(store_req_sd, &req_sds);
			FD_SET(restore_req_sd, &req_sds);
			FD_SET(service_req_sd, &req_sds);	  
			FD_SET(replicate_req_sd, &req_sds);
		}
		if (num_sds_ready < 0)
			if (errno != EINTR) {
				cerr << endl << "ERROR:" << endl;
				cerr << "ERROR:" << endl;
				cerr << "ERROR: cannot select from request ports (errno = " 
					<< errno << ")" << endl;
				cerr << "ERROR:" << endl;
				cerr << "ERROR:" << endl << endl;
				exit(SELECT_ERROR);
			}
		current_time = time(NULL);
		if (((unsigned int) current_time - (unsigned int) start_interval_time) 
			>= RECLAIM_INTERVAL) {
			BlockSignals();
			transfers.Reclaim(current_time);
			UnblockSignals();
			start_interval_time = time(NULL);
			if (replication_level)
				Replicate();
			Log("Sending ckpt server ad to collector...");
			xfer_summary.time_out(current_time);
		}
    }
	close(service_req_sd);
	close(store_req_sd);
	close(restore_req_sd);
	close(replicate_req_sd);
#if DEBUG
	log_file.close();
#endif
}


void Server::HandleRequest(int req_sd,
						   request_type req)
{
	struct sockaddr_in shadow_sa;
	int                shadow_sa_len;
	int                req_len;
	int                new_req_sd;
	service_req_pkt    service_req;
	store_req_pkt      store_req;
	restore_req_pkt    restore_req;
	replicate_req_pkt  replicate_req;
	store_reply_pkt    store_reply;
	restore_reply_pkt  restore_reply;
	char*              buf_ptr;
	int                bytes_recvd=0;
	int                temp_len;
	char               log_msg[256];
	
	shadow_sa_len = sizeof(shadow_sa);
	if ((new_req_sd=I_accept(req_sd, &shadow_sa, &shadow_sa_len)) == 
		ACCEPT_ERROR) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: I_accept() fails" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(ACCEPT_ERROR);
    }
	BlockSignals();
	req_ID++;
	switch (req) {
        case SERVICE_REQ:
		    sprintf(log_msg, "%s%s", "Receiving service request from ", 
					inet_ntoa(shadow_sa.sin_addr));
			break;
		case STORE_REQ:
			sprintf(log_msg, "%s%s", "Receiving store request from ", 
					inet_ntoa(shadow_sa.sin_addr));
			break;
		case RESTORE_REQ:
			sprintf(log_msg, "%s%s", "Receiving restore request from ", 
					inet_ntoa(shadow_sa.sin_addr));
			break;
		case REPLICATE_REQ:
			sprintf(log_msg, "%s%s", "Receiving replicate request from ", 
					inet_ntoa(shadow_sa.sin_addr));
			break;
		default:
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: invalid request type encountered ("
				<< (int) req << ")" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			exit(BAD_REQUEST_TYPE);
		}
	Log(req_ID, log_msg);
	sprintf(log_msg, "%s%d%s", "Using descriptor ", new_req_sd, 
			" to handle request");
	Log(log_msg);
	UnblockSignals();
	if ((req == STORE_REQ) || (req == RESTORE_REQ) || (req == REPLICATE_REQ)) {
		BlockSignals();
		if ((num_store_xfers+num_restore_xfers == max_xfers) ||
			((req == STORE_REQ) && (num_store_xfers == max_store_xfers)) ||
			((req == RESTORE_REQ) && (num_restore_xfers == max_store_xfers)) ||
			((req == REPLICATE_REQ) &&
			 (num_replicate_xfers == max_replicate_xfers))) {
			UnblockSignals();
			if (req == STORE_REQ || req == REPLICATE_REQ) {
				store_reply.server_name.s_addr = htonl(0);
				store_reply.port = htons(0);
				store_reply.req_status = htons(INSUFFICIENT_BANDWIDTH);
				net_write(new_req_sd, (char*) &store_reply, 
						  sizeof(store_reply_pkt));
				sprintf(log_msg, "%s", 
						"Request to store a file has been DENIED");
			} else {
				UnblockSignals();
				restore_reply.server_name.s_addr = htonl(0);
				restore_reply.port = htons(0);
				restore_reply.file_size = htonl(0);
				restore_reply.req_status = htons(INSUFFICIENT_BANDWIDTH);
				net_write(new_req_sd, (char*) &restore_reply, 
						  sizeof(restore_reply_pkt));	      
				sprintf(log_msg, "%s", 
						"Request to restore a file has been DENIED");
			}
			BlockSignals();
			Log(req_ID, log_msg);
			Log("Insufficient bandwidth for file transfer");
			UnblockSignals();
			close(new_req_sd);
			return;
		} else
			UnblockSignals();
    }

	switch (req) {
        case SERVICE_REQ:
		    req_len = sizeof(service_req_pkt);
			buf_ptr = (char*) &service_req;
			break;
		case STORE_REQ:
			req_len = sizeof(store_req_pkt);
			buf_ptr = (char*) &store_req;
			break;
		case RESTORE_REQ:
			req_len = sizeof(restore_req_pkt);
			buf_ptr = (char*) &restore_req;
			break;
		case REPLICATE_REQ:
			req_len = sizeof(replicate_req_pkt);
			buf_ptr = (char*) &replicate_req;
			break;
	}

	BlockSignals();
	rt_alarm.SetAlarm(new_req_sd, REQUEST_TIMEOUT);

	while (bytes_recvd < req_len) {
		errno = 0;
		temp_len = read(new_req_sd, buf_ptr+bytes_recvd, req_len-bytes_recvd);
		if (temp_len <= 0) {
			if (errno != EINTR) {
				rt_alarm.ResetAlarm();
				close(new_req_sd);
				BlockSignals();
				sprintf(log_msg, "Request from %s REJECTED:", 
						inet_ntoa(shadow_sa.sin_addr));
				Log(req_ID, log_msg);
				Log("Incomplete request packet");
				sprintf(log_msg, "DEBUG: %d bytes sent; %d bytes expected", 
						bytes_recvd, req_len);
				Log(-1, log_msg);	     
				UnblockSignals();
				return;
			}
		} else
			bytes_recvd += temp_len;
    }
	rt_alarm.ResetAlarm();
	UnblockSignals();
	switch (req) {
    case SERVICE_REQ:
		ProcessServiceReq(req_ID, new_req_sd, shadow_sa.sin_addr, service_req);
		break;
    case STORE_REQ:
		ProcessStoreReq(req_ID, new_req_sd, shadow_sa.sin_addr, store_req);
		break;
    case RESTORE_REQ:
		ProcessRestoreReq(req_ID, new_req_sd, shadow_sa.sin_addr, restore_req);
		break;
	case REPLICATE_REQ:
		store_req.file_size = replicate_req.file_size;
		store_req.ticket = replicate_req.ticket;
		store_req.priority = replicate_req.priority;
		store_req.time_consumed = replicate_req.time_consumed;
		store_req.key = replicate_req.key;
		strcpy(store_req.filename, replicate_req.filename);
		strcpy(store_req.owner, replicate_req.owner);
		ProcessStoreReq(req_ID, new_req_sd, replicate_req.shadow_IP,
						store_req);
		break;
    }  
	
}


void Server::ProcessServiceReq(int             req_id,
							   int             req_sd,
							   struct in_addr  shadow_IP,
							   service_req_pkt service_req)
{  
	service_reply_pkt  service_reply;
	char               log_msg[256];
	struct sockaddr_in server_sa;
	int                data_conn_sd;
	int                lock_status;
	struct stat        chkpt_file_status;
	char               pathname[MAX_PATHNAME_LENGTH];
	int                num_files;
	int                child_pid;
	int                ret_code;

	service_req.ticket = ntohl(service_req.ticket);
	if (service_req.ticket != AUTHENTICATION_TCKT) {
		service_reply.server_addr.s_addr = htonl(0);
		service_reply.port = htons(0);
		service_reply.req_status = htons(BAD_AUTHENTICATION_TICKET);
		net_write(req_sd, (char*) &service_reply, sizeof(service_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Request for service from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("Invalid authentication ticket used");
		UnblockSignals();
		close(req_sd);
		return;
    }
	service_req.service = ntohs(service_req.service);
	service_req.key = ntohl(service_req.key);
	BlockSignals();
#if defined(DEBUG)
	switch (service_req.service) {
        case CKPT_SERVER_SERVICE_STATUS:
		    sprintf(log_msg, "Service CKPT_SERVER_SERVICE_STATUS request from %s", 
					service_req.owner_name);
			break;
		case SERVICE_RENAME:
			sprintf(log_msg, "Service SERVICE_RENAME request from %s", 
					service_req.owner_name);
			break;
		case SERVICE_DELETE:
			sprintf(log_msg, "Service SERVICE_DELETE request from %s", 
					service_req.owner_name);
			break;
		case SERVICE_EXIST:
			sprintf(log_msg, "Service SERVICE_EXIST request from %s", 
					service_req.owner_name);
			break;
		}
	Log(req_id, log_msg);  
#endif
	switch (service_req.service) {
        case CKPT_SERVER_SERVICE_STATUS:
		    num_files = imds.GetNumFiles();
			service_reply.num_files = htonl(num_files);
			if (num_files > 0) {
				data_conn_sd = I_socket();
				if (data_conn_sd == INSUFFICIENT_RESOURCES) {
					service_reply.server_addr.s_addr = htonl(0);
					service_reply.port = htons(0);
					service_reply.req_status = 
						htons(abs(INSUFFICIENT_RESOURCES));
					net_write(req_sd, (char*) &service_reply, 
							  sizeof(service_reply));
					sprintf(log_msg, "Service request from %s DENIED:", 
							inet_ntoa(shadow_IP));
					Log(req_id, log_msg);
					Log("Insufficient buffers/ports to handle request");
					UnblockSignals();
					close(req_sd);
					return;
				} else if (data_conn_sd == CKPT_SERVER_SOCKET_ERROR) {
					service_reply.server_addr.s_addr = htonl(0);
					service_reply.port = htons(0);
					service_reply.req_status = htons(abs(CKPT_SERVER_SOCKET_ERROR));
					net_write(req_sd, (char*) &service_reply, 
							  sizeof(service_reply));
					sprintf(log_msg, "Service request from %s DENIED:", 
							inet_ntoa(shadow_IP));
					Log(req_id, log_msg);
					Log("Cannot obtain a new socket from server");
					UnblockSignals();
					close(req_sd);
					return;
				}
				memset((char*) &server_sa, 0, sizeof(server_sa));
				server_sa.sin_family = AF_INET;
				server_sa.sin_addr = server_addr;
				server_sa.sin_port = htons(0);
				if ((ret_code=I_bind(data_conn_sd, &server_sa)) != CKPT_OK) {
					cerr << endl << "ERROR:" << endl;
					cerr << "ERROR:" << endl;
					cerr << "ERROR: I_bind() returns an error (#" << ret_code
						<< ")" << endl;
					cerr << "ERROR:" << endl;
					cerr << "ERROR:" << endl << endl;
					sprintf(log_msg, "ERROR: I_bind() returns an error (#%d)", 
							ret_code);
					Log(0, log_msg);
					exit(ret_code);
				}
				if (I_listen(data_conn_sd, 1) != CKPT_OK) {
					cerr << endl << "ERROR:" << endl;
					cerr << "ERROR:" << endl;
					cerr << "ERROR: I_listen() fails to listen" << endl;
					cerr << "ERROR:" << endl;
					cerr << "ERROR:" << endl << endl;
					sprintf(log_msg, "ERROR: I_listen() fails to listen");
					Log(0, log_msg);
					exit(LISTEN_ERROR);
				}
				service_reply.server_addr = server_addr;
		  // From the I_bind() call, the port should already be in network-byte
		  //   order
				service_reply.port = server_sa.sin_port;
			} else {
				service_reply.server_addr.s_addr = htonl(0);
				service_reply.port = htons(0);
			}
			service_reply.req_status = htons(CKPT_OK);
			strcpy(service_reply.capacity_free_ACD, "0");
			break;

		case SERVICE_RENAME:
			service_reply.server_addr.s_addr = htonl(0);
			service_reply.port = htons(0);
			service_reply.num_files = htons(0);
			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0) || 
				(strlen(service_req.new_file_name) == 0)) {
				service_reply.req_status = htons(BAD_REQ_PKT);
			} else {
				service_req.key = ntohl(service_req.key);
				if (imds.LockStatus(shadow_IP, service_req.owner_name, 
									service_req.file_name) == EXCLUSIVE_LOCK)
					if (transfers.GetKey(shadow_IP, service_req.owner_name, 
								 service_req.file_name) == service_req.key) {
						transfers.SetOverride(shadow_IP, 
											  service_req.owner_name, 
											  service_req.file_name);
						imds.Unlock(shadow_IP, service_req.owner_name,
									service_req.file_name);
					}
				if (imds.LockStatus(shadow_IP, service_req.owner_name, 
								service_req.new_file_name) == EXCLUSIVE_LOCK)
					if (transfers.GetKey(shadow_IP, service_req.owner_name, 
							 service_req.new_file_name) == service_req.key) {
						transfers.SetOverride(shadow_IP, 
											  service_req.owner_name, 
											  service_req.new_file_name);
						imds.Unlock(shadow_IP, service_req.owner_name,
									service_req.new_file_name);
					}
				service_reply.req_status = 
					htons(imds.RenameFile(shadow_IP, 
										  service_req.owner_name, 
										  service_req.file_name,
										  service_req.new_file_name));
				if (replication_level) {
					ScheduleReplication(shadow_IP,
										service_req.owner_name,
										service_req.new_file_name,
										replication_level);
				}
			}
			break;

		case SERVICE_COMMIT_REPLICATION:
			service_reply.server_addr.s_addr = htonl(0);
			service_reply.port = htons(0);
			service_reply.num_files = htons(0);
			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0) || 
				(strlen(service_req.new_file_name) == 0)) {
				service_reply.req_status = htons(BAD_REQ_PKT);
			} else {
				service_req.key = ntohl(service_req.key);
				if (imds.LockStatus(service_req.shadow_IP,
									service_req.owner_name, 
									service_req.file_name) == EXCLUSIVE_LOCK)
					if (transfers.GetKey(service_req.shadow_IP,
										 service_req.owner_name, 
								 service_req.file_name) == service_req.key) {
						transfers.SetOverride(service_req.shadow_IP, 
											  service_req.owner_name, 
											  service_req.file_name);
						imds.Unlock(service_req.shadow_IP,
									service_req.owner_name,
									service_req.file_name);
					}
				if (imds.LockStatus(service_req.shadow_IP,
									service_req.owner_name, 
								service_req.new_file_name) == EXCLUSIVE_LOCK)
					if (transfers.GetKey(service_req.shadow_IP,
										 service_req.owner_name, 
							 service_req.new_file_name) == service_req.key) {
						transfers.SetOverride(service_req.shadow_IP, 
											  service_req.owner_name, 
											  service_req.new_file_name);
						imds.Unlock(service_req.shadow_IP,
									service_req.owner_name,
									service_req.new_file_name);
					}
				service_reply.req_status = 
					htons(imds.RenameFile(service_req.shadow_IP, 
										  service_req.owner_name, 
										  service_req.file_name,
										  service_req.new_file_name));
			}
			break;

		case SERVICE_ABORT_REPLICATION:
			shadow_IP = service_req.shadow_IP;
			// no break == fall through to SERVICE_DELETE

		case SERVICE_DELETE:
			service_reply.server_addr.s_addr = htonl(0);
			service_reply.port = htons(0);
			service_reply.num_files = htons(0);
			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0))
				service_reply.req_status = htons(BAD_REQ_PKT);
			else {
				lock_status = imds.LockStatus(shadow_IP, 
											  service_req.owner_name, 
											  service_req.file_name);
				switch (lock_status) {
				    case UNLOCKED:
					    service_reply.req_status = 
							htons(imds.RemoveFile(shadow_IP, 
												  service_req.owner_name, 
												  service_req.file_name));
						break;
					case EXCLUSIVE_LOCK:
						service_req.key = ntohl(service_req.key);
						if (transfers.GetKey(shadow_IP, service_req.owner_name,
								 service_req.file_name) == service_req.key) {
							transfers.SetOverride(shadow_IP, 
												  service_req.owner_name,
												  service_req.file_name);
							imds.Unlock(shadow_IP, service_req.owner_name,
										service_req.file_name);
							service_reply.req_status = 
								htons(imds.RemoveFile(shadow_IP, 
													  service_req.owner_name, 
													  service_req.file_name));
						} else
							service_reply.req_status = htons(FILE_LOCKED);
						break;
					case DOES_NOT_EXIST:
						sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, 
								inet_ntoa(shadow_IP), service_req.owner_name, 
								service_req.file_name);
						errno = 0;
						if (stat(pathname, &chkpt_file_status) == 0) {
							if (remove(pathname) == 0)
								service_reply.req_status = htons(CKPT_OK);
							else
								service_reply.req_status = 
									htons(CANNOT_DELETE_FILE);
						} else if (errno == EACCES)
							service_reply.req_status = 
								htons(INSUFFICIENT_PERMISSIONS);
						else
							service_reply.req_status = htons(DOES_NOT_EXIST);
						break;

					default:
						close(req_sd);
						cerr << endl << "ERROR:" << endl;
						cerr << "ERROR:" << endl;
						cerr << "ERROR: IMDS file has ambiguous lock state" <<
							endl;
						cerr << "ERROR:" << endl << endl;
						exit(IMDS_BAD_RETURN_CODE);
					}
			}
			break;

		case SERVICE_EXIST:
			service_reply.server_addr.s_addr = htonl(0);
			service_reply.port = htons(0);
			service_reply.num_files = htons(0);
			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0))
				service_reply.req_status = htons(BAD_REQ_PKT);
			else {
				lock_status = imds.LockStatus(shadow_IP, 
											  service_req.owner_name, 
											  service_req.file_name);
				if (lock_status == DOES_NOT_EXIST) {
					sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, 
							inet_ntoa(shadow_IP), service_req.owner_name, 
							service_req.file_name);
					if (stat(pathname, &chkpt_file_status) == 0) {
						// File exists but 
						//   is not in 
						//   in-memory
						//   data structure
						
						imds.AddFile(shadow_IP, service_req.owner_name, 
									 service_req.file_name, 
									 chkpt_file_status.st_size, ON_SERVER);
/*						service_reply.req_status = htons(EXISTS); */
						service_reply.req_status = htons(0);
					} else
						service_reply.req_status = htons(DOES_NOT_EXIST);
				} else
/*					service_reply.req_status = htons(EXISTS); */
					service_reply.req_status = htons(0);
			}
			break;
			
		default:
			service_reply.server_addr.s_addr = htonl(0);
			service_reply.port = htons(0);
			service_reply.num_files = htons(0);
			service_reply.req_status = htons(BAD_SERVICE_TYPE);
		}
			
	if (net_write(req_sd, (char*) &service_reply, sizeof(service_reply)) < 0) {
		close(req_sd);
		sprintf(log_msg, "Service request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("Cannot sent IP/port to shadow (socket closed)");
    } else {
		close(req_sd);
		if (service_req.service == CKPT_SERVER_SERVICE_STATUS)
			if (num_files == 0) {
				Log(req_id, "Request for server status GRANTED:");
				Log("No files on checkpoint server");
			} else {
				child_pid = fork();
				if (child_pid < 0) {
					close(data_conn_sd);
					Log(req_id, "Unable to honor status service request:");
					Log("Cannot fork child processes");	  
				} else if (child_pid != 0)  {
					// Parent (Master) process
					transfers.Insert(child_pid, req_id, shadow_IP,
									 "Server Status",
									 "Condor", 0, service_req.key, 0,
									 FILE_STATUS);
					Log(req_id, "Request for server status GRANTED");
				} else {
					// Child process
#if DEBUG
					log_file.close();
#endif
					close(store_req_sd);
					close(restore_req_sd);
					close(service_req_sd);
					SendStatus(data_conn_sd);
					exit(CHILDTERM_SUCCESS);
				}
			} else if (service_req.service == SERVICE_RENAME)
				if (service_reply.req_status == htons(CKPT_OK))
					Log(req_id, "Rename file request successfully completed");
				else {
					Log(req_id, "Rename file request cannot be completed:");
					sprintf(log_msg, "Attempt returns error #%d", 
							ntohs(service_reply.req_status));
					Log(log_msg);
				}
			else if (service_reply.req_status == htons(CKPT_OK))
				Log(req_id, "Delete file request successfully completed");
			else {
				Log(req_id, "Delete file request cannot be completed:");
				sprintf(log_msg, "Attempt returns error #%d", 
						ntohs(service_reply.req_status));
				Log(log_msg);
			}
	}
	UnblockSignals();
}


void Server::ScheduleReplication(struct in_addr shadow_IP, char *owner,
								 char *filename, int level)
{
	char        log_msg[256];
	static bool first_time = true;

	if (first_time) {
		first_time = false;
		srandom(time(NULL));
	}

	int first_peer = (int)(random()%(long)num_peers);
	for (int i=0; i < level && i < num_peers; i++) {
		ReplicationEvent *e = new ReplicationEvent();
		e->Prio(i);
		e->ServerAddr(peer_addr_list[(first_peer+i)%num_peers]);
		e->ShadowIP(shadow_IP);
		e->Owner(owner);
		e->File(filename);
		sprintf(log_msg, "Scheduling Replication: Prio=%d, Serv=%s, File=%s",
				i, sin_to_string(peer_addr_list+((first_peer+i)%num_peers)),
				filename);
		Log(log_msg);
		replication_schedule.InsertReplicationEvent(e);
	}
}

void Server::Replicate()
{
	int 				child_pid, bytes_recvd=0, bytes_read;
	int					bytes_sent=0, bytes_written, bytes_left;
	char        		log_msg[256], buf[10240];
	char				pathname[MAX_PATHNAME_LENGTH];
	int 				server_sd, ret_code, fd;
	struct sockaddr_in 	server_sa;
	struct stat 		chkpt_file_status;
	time_t				file_timestamp;
	replicate_req_pkt 	req;
	replicate_reply_pkt	reply;

	Log("Checking replication schedule...");
	ReplicationEvent *e = replication_schedule.GetNextReplicationEvent();
	if (e) {
		sprintf(log_msg, "Replicating: Prio=%d, Serv=%s, File=%s",
				e->Prio(), sin_to_string(&(e->ServerAddr())), e->File());
		Log(log_msg);
		sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX,
				inet_ntoa(e->ShadowIP()), e->Owner(), e->File());
		if (stat(pathname, &chkpt_file_status) < 0) {
			exit(DOES_NOT_EXIST);
		}
		req.file_size = htonl(chkpt_file_status.st_size);
#undef AVOID_FORK
#if !defined(AVOID_FORK)
		child_pid = fork();
		if (child_pid < 0) {
			Log("Unable to perform replication.  Cannot fork child process.");
		} else if (child_pid == 0) {
#endif
			server_sa = e->ServerAddr();
			req.ticket = htonl(AUTHENTICATION_TCKT);
			req.priority = htonl(0);
			req.time_consumed = htonl(0);
			req.key = htonl(getpid()); // from server_interface.c
			strcpy(req.owner, e->Owner());
//			strcpy(req.filename, e->File());
			sprintf(req.filename, "%s.rep", e->File());
			req.shadow_IP = e->ShadowIP();
			file_timestamp = chkpt_file_status.st_mtime;
			if ((server_sd = I_socket()) < 0) {
				exit(server_sd);
			}
			ret_code = net_write(server_sd, (char *)&req, sizeof(req));
			if (ret_code != sizeof(req))
				exit(CHILDTERM_CANNOT_WRITE);
			if (connect(server_sd, (struct sockaddr*) &server_sa,
						sizeof(server_sa)) < 0) {
				exit(CONNECT_ERROR);
			}
			if (net_write(server_sd, (char *) &req,
						  sizeof(req)) != sizeof(req)) {
				exit(CHILDTERM_CANNOT_WRITE);
			}
			while (bytes_recvd != sizeof(reply)) {
				errno = 0;
				bytes_read = read(server_sd, ((char *) &reply)+bytes_recvd,
								  sizeof(reply)-bytes_recvd);
				assert(bytes_read >= 0);
				if (bytes_read == 0)
					assert(errno == EINTR);
				else
					bytes_recvd += bytes_read;
			}
			close(server_sd);
			if ((server_sd = I_socket()) < 0) {
				exit(server_sd);
			}
			memset((char*) &server_sa, 0, sizeof(server_sa));
			server_sa.sin_family = AF_INET;
			memcpy((char *) &server_sa.sin_addr.s_addr,
				   (char *) &reply.server_name, sizeof(reply.server_name));
			server_sa.sin_port = reply.port;
			if (connect(server_sd, (struct sockaddr*) &server_sa,
						sizeof(server_sa)) < 0) {
				exit(CONNECT_ERROR);
			}
			if ((fd = open(pathname, O_RDONLY)) < 0) {
				exit(CHILDTERM_CANNOT_OPEN_CHKPT_FILE);
			}
			bytes_read = 1;
			while (bytes_sent != req.file_size && bytes_read > 0) {
				errno = 0;
				bytes_read = read(fd, &buf, sizeof(buf));
				bytes_left = bytes_read;
				while (bytes_left) {
					bytes_written = write(server_sd, &buf, bytes_read);
					assert(bytes_written >= 0);
					if (bytes_written == 0)
						assert(errno == EINTR);
					else {
						bytes_sent += bytes_written;
						bytes_left -= bytes_written;
					}
				}
				sleep(1);		// slow pipe
			}
			close(server_sd);
			close(fd);
			chkpt_file_status.st_mtime = 0;
			stat(pathname, &chkpt_file_status);
			if ((server_sd = I_socket()) < 0) {
				exit(server_sd);
			}
			server_sa.sin_port = htons(CKPT_SVR_SERVICE_REQ_PORT);
			if (connect(server_sd, (struct sockaddr*) &server_sa,
						sizeof(server_sa)) < 0) {
				exit(CONNECT_ERROR);
			}
			service_req_pkt s_req;
			service_reply_pkt s_rep;
			s_req.shadow_IP = e->ShadowIP();
			s_req.ticket = htonl(AUTHENTICATION_TCKT);
			s_req.key	= htonl(getpid());
			strcpy(s_req.owner_name, e->Owner());
			sprintf(s_req.file_name, "%s.rep", e->File());
			sprintf(s_req.new_file_name, "%s", e->File());
			if (chkpt_file_status.st_mtime == file_timestamp) {
				s_req.service = htons(SERVICE_COMMIT_REPLICATION);
			}
			else {
				s_req.service = htons(SERVICE_ABORT_REPLICATION);
			}
			ret_code = net_write(server_sd, (char *)&s_req, sizeof(s_req));
			if (ret_code != sizeof(s_req))
				exit(CHILDTERM_CANNOT_WRITE);
			bytes_recvd = 0;
			while (bytes_recvd != sizeof(s_rep)) {
				errno = 0;
				bytes_read = read(server_sd, ((char *) &s_rep)+bytes_recvd,
								  sizeof(s_rep)-bytes_recvd);
				assert(bytes_read >= 0);
				if (bytes_read == 0)
					assert(errno == EINTR);
				else
					bytes_recvd += bytes_read;
			}
#if !defined(AVOID_FORK)
			exit(CHILDTERM_SUCCESS);
		}
		transfers.Insert(child_pid, -1, e->ShadowIP(), e->File(), e->Owner(),
						 req.file_size, getpid(), 0, REPLICATE);
#endif
		num_replicate_xfers++;
		delete e;
	}
}

void Server::SendStatus(int data_conn_sd)
{
  struct sockaddr_in chkpt_addr;
  int                chkpt_addr_len;
  int                xfer_sd;
  int                buf_size=DATA_BUFFER_SIZE;

  chkpt_addr_len = sizeof(struct sockaddr_in);
  if ((xfer_sd=I_accept(data_conn_sd, &chkpt_addr, &chkpt_addr_len)) ==
      ACCEPT_ERROR)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: I_accept() fails" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(CHILDTERM_ACCEPT_ERROR);
    }
  close(data_conn_sd);
  // Changing buffer size may fail
  setsockopt(xfer_sd, SOL_SOCKET, SO_SNDBUF, (char*) &buf_size, 
	     sizeof(buf_size));
  imds.TransferFileInfo(xfer_sd);
}


void Server::ProcessStoreReq(int            req_id,
							 int            req_sd,
							 struct in_addr shadow_IP,
							 store_req_pkt  store_req)
{
	int                ret_code;
	store_reply_pkt    store_reply;
	int                data_conn_sd;
	struct sockaddr_in server_sa;
	int                child_pid;
	char               pathname[MAX_PATHNAME_LENGTH];
	char               log_msg[256];
	int                err_code;

	store_req.ticket = ntohl(store_req.ticket);
	if (store_req.ticket != AUTHENTICATION_TCKT) {
		store_reply.server_name.s_addr = htonl(0);
		store_reply.port = htons(0);
		store_reply.req_status = htons(BAD_AUTHENTICATION_TICKET);
		net_write(req_sd, (char*) &store_reply, sizeof(store_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("Invalid authentication ticket used");
		UnblockSignals();
		close(req_sd);
		return;
    }
	if ((strlen(store_req.filename) == 0) || (strlen(store_req.owner) == 0)) {
		store_reply.server_name.s_addr = htonl(0);
		store_reply.port = htons(0);
		store_reply.req_status = htons(BAD_REQ_PKT);
		net_write(req_sd, (char*) &store_reply, sizeof(store_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("Incomplete request packet");
		UnblockSignals();
		close(req_sd);
		return;
    }

	BlockSignals();
#if defined(DEBUG)
	sprintf(log_msg, "STORE request from %s", store_req.owner);
	Log(req_id, log_msg);
	sprintf(log_msg, "File name: %s", store_req.filename);
	Log(log_msg);
	sprintf(log_msg, "File size: %d", ntohl(store_req.file_size));
	Log(log_msg);
#endif
	ret_code = imds.LockStatus(shadow_IP, store_req.owner, store_req.filename);
	UnblockSignals();
	store_req.file_size = ntohl(store_req.file_size);
	if (ret_code == EXCLUSIVE_LOCK) {
		store_reply.server_name.s_addr = htonl(0);
		store_reply.port = htons(0);
		store_reply.req_status = htons(ret_code);
		net_write(req_sd, (char*) &store_reply, sizeof(store_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("File is currently LOCKED");
		close(req_sd);
		UnblockSignals();
    } else if ((ret_code == UNLOCKED) || (ret_code == DOES_NOT_EXIST)) {
		data_conn_sd = I_socket();
		if (data_conn_sd == INSUFFICIENT_RESOURCES) {
			store_reply.server_name.s_addr = htonl(0);
			store_reply.port = htons(0);
			store_reply.req_status = htons(abs(INSUFFICIENT_RESOURCES));
			net_write(req_sd, (char*) &store_reply, sizeof(store_reply_pkt));
			BlockSignals();
			sprintf(log_msg, "Store request from %s DENIED:", 
					inet_ntoa(shadow_IP));
			Log(req_id, log_msg);
			Log("Insufficient buffers/ports to handle request");
			UnblockSignals();
			close(req_sd);
			return;
		} else if (data_conn_sd == CKPT_SERVER_SOCKET_ERROR) {
			store_reply.server_name.s_addr = htonl(0);
			store_reply.port = htons(0);
			store_reply.req_status = htons(abs(CKPT_SERVER_SOCKET_ERROR));
			net_write(req_sd, (char*) &store_reply, sizeof(store_reply_pkt));
			BlockSignals();
			sprintf(log_msg, "Store request from %s DENIED:", 
					inet_ntoa(shadow_IP));
			Log(req_id, log_msg);
			Log("Cannot obtain a new socket from server");
			UnblockSignals();
			close(req_sd);
			return;
		}

		memset((char*) &server_sa, 0, sizeof(server_sa));
		server_sa.sin_family = AF_INET;
		server_sa.sin_port = htons(0);
		server_sa.sin_addr = server_addr;
		if ((err_code=I_bind(data_conn_sd, &server_sa)) != CKPT_OK) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: I_bind() returns an error (#" << ret_code
				<< ")" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			sprintf(log_msg, "ERROR: I_bind() returns an error (#%d)", 
					err_code);
			Log(0, log_msg);
			exit(ret_code);
		}

		if (I_listen(data_conn_sd, 1) != CKPT_OK) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: I_listen() fails to listen" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			sprintf(log_msg, "ERROR: I_listen() fails to listen");
			Log(0, log_msg);
			exit(LISTEN_ERROR);
		}

		BlockSignals();
		imds.AddFile(shadow_IP, store_req.owner, store_req.filename, 
					 store_req.file_size, NOT_PRESENT);
		imds.Lock(shadow_IP, store_req.owner, store_req.filename, LOADING);
		UnblockSignals();
		store_reply.server_name = server_addr;
      // From the I_bind() call, the port should already be in network-byte
      //   order
		store_reply.port = server_sa.sin_port;  
		store_reply.req_status = htons(CKPT_OK);
		if (net_write(req_sd, (char*) &store_reply, 
					  sizeof(store_reply_pkt)) < 0) {
			close(req_sd);
			BlockSignals();
			sprintf(log_msg, "Store request from %s DENIED:", 
					inet_ntoa(shadow_IP));
			Log(req_id, log_msg);
			Log("Cannot send IP/port to shadow (socket closed)");
			imds.RemoveFile(shadow_IP, store_req.owner, store_req.filename);
			UnblockSignals();
			close(data_conn_sd);
		} else {
			close(req_sd);
			BlockSignals();

			child_pid = fork();

			if (child_pid < 0) {
				close(data_conn_sd);
				Log(req_id, "Unable to honor store request:");
				Log("Cannot fork child processes");
			} else if (child_pid != 0) {
				// Parent (Master) process
				close(data_conn_sd);
				num_store_xfers++;
				transfers.Insert(child_pid, req_id, shadow_IP, 
								 store_req.filename, store_req.owner,
								 store_req.file_size, store_req.key,
								 store_req.priority, RECV);
				Log(req_id, "Request to store checkpoint file GRANTED");
			} else {
				// Child process
#if DEBUG
				log_file.close();
#endif
				close(store_req_sd);
				close(restore_req_sd);
				close(service_req_sd);
				sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, 
						inet_ntoa(shadow_IP), store_req.owner, 
						store_req.filename);
				ReceiveCheckpointFile(data_conn_sd, pathname,
									  (int) store_req.file_size);
				exit(CHILDTERM_SUCCESS);
			}
			UnblockSignals();
		}
    } else {
		close(req_sd);
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: IMDS file has an ambiguous lock state" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(IMDS_BAD_RETURN_CODE);
    }
}


void Server::ReceiveCheckpointFile(int         data_conn_sd,
								   const char* pathname,
								   int         file_size)
{
	struct sockaddr_in chkpt_addr;
	int                chkpt_addr_len;
	int                xfer_sd;
	ssize_t            bytes_recvd=0;
	int                buf_size=DATA_BUFFER_SIZE;
	int				   file_fd;
	
#if 1
	file_fd = open(pathname, O_WRONLY|O_CREAT|O_TRUNC,0664);
#else
	file_fd = open("/dev/null", O_WRONLY, 0);
#endif

	if (file_fd < 0) {
		exit(CHILDTERM_CANNOT_OPEN_CHKPT_FILE);
	}
	chkpt_addr_len = sizeof(struct sockaddr_in);
	// May cause an ACCEPT_ERROR error
	do {
		xfer_sd = tcp_accept_timeout(data_conn_sd,
									 (struct sockaddr *)&chkpt_addr, 
									 &chkpt_addr_len, CKPT_ACCEPT_TIMEOUT);
	} while (xfer_sd == -3 || (xfer_sd == -1 && errno == EINTR));
#if 0
	if ((xfer_sd=I_accept(data_conn_sd, &chkpt_addr, &chkpt_addr_len)) ==
		ACCEPT_ERROR) {
#else
	if (xfer_sd < 0) {
#endif
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: I_accept() fails" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(CHILDTERM_ACCEPT_ERROR);
    }
	close(data_conn_sd);
	// Changing buffer size may fail
	setsockopt(xfer_sd, SOL_SOCKET, SO_RCVBUF, (char*) &buf_size, 
			   sizeof(buf_size));
	bytes_recvd = stream_file_xfer(xfer_sd, file_fd, file_size);
	bytes_recvd = htonl(bytes_recvd);
	net_write(xfer_sd, (char*) &bytes_recvd, sizeof(bytes_recvd));
	close(xfer_sd);
	fsync( file_fd );
	close(file_fd);
}


void Server::ProcessRestoreReq(int             req_id,
							   int             req_sd,
							   struct in_addr  shadow_IP,
							   restore_req_pkt restore_req)
{
	struct stat        chkpt_file_status;
	struct sockaddr_in server_sa;
	int                ret_code;
	restore_reply_pkt  restore_reply;
	int                data_conn_sd;
	int                child_pid;
	char               pathname[MAX_PATHNAME_LENGTH];
	int                preexist;
	char               log_msg[256];
	int                err_code;
	
	restore_req.ticket = ntohl(restore_req.ticket);
	if (restore_req.ticket != AUTHENTICATION_TCKT) {
		restore_reply.server_name.s_addr = htonl(0);
		restore_reply.port = htons(0);
		restore_reply.file_size = htonl(0);
		restore_reply.req_status = htons(BAD_AUTHENTICATION_TICKET);
		net_write(req_sd, (char*) &restore_reply, sizeof(restore_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Restore request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("Invalid authentication ticket used");
		UnblockSignals();
		close(req_sd);
		return;
    }
	if ((strlen(restore_req.filename) == 0) || 
		(strlen(restore_req.owner) == 0)) {
		restore_reply.server_name.s_addr = htonl(0);
		restore_reply.port = htons(0);
		restore_reply.file_size = htonl(0);
		restore_reply.req_status = htons(BAD_REQ_PKT);
		net_write(req_sd, (char*) &restore_reply, sizeof(restore_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Restore request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("Incomplete request packet");
		UnblockSignals();
		close(req_sd);
		return;
    }
	BlockSignals();
#ifdef DEBUG  
	sprintf(log_msg, "RESTORE request from %s", restore_req.owner);
	Log(req_id, log_msg);
	sprintf(log_msg, "File name: %s", restore_req.filename);
	Log(log_msg);
#endif
	sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, inet_ntoa(shadow_IP),
			restore_req.owner, restore_req.filename);
	ret_code = imds.LockStatus(shadow_IP, restore_req.owner, 
							   restore_req.filename);
	UnblockSignals();
	if (ret_code == EXCLUSIVE_LOCK) {
		restore_reply.server_name.s_addr = htonl(0);
		restore_reply.port = htons(0);
		restore_reply.file_size = htonl(0);
		restore_reply.req_status = htons(ret_code);
		net_write(req_sd, (char*) &restore_reply, sizeof(restore_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Restore request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("File is currently LOCKED");
		UnblockSignals();
		close(req_sd);
		return;
    }
	else if (((preexist=stat(pathname, &chkpt_file_status)) != 0) && 
			 (ret_code == DOES_NOT_EXIST)) {
		restore_reply.server_name.s_addr = htonl(0);
		restore_reply.port = htons(0);
		restore_reply.file_size = htonl(0);
		restore_reply.req_status = htons(DESIRED_FILE_NOT_FOUND);
		net_write(req_sd, (char*) &restore_reply, sizeof(restore_reply_pkt));
		BlockSignals();
		sprintf(log_msg, "Restore request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(req_id, log_msg);
		Log("Requested file does not exist on server");
		UnblockSignals();
		close(req_sd);
		return;
    } else if ((ret_code == UNLOCKED) || (preexist == 0)) {
      if (preexist == 0) {                  // File exists but is not in
                                           //   in-memory data structure
		  BlockSignals();
		  imds.AddFile(shadow_IP, restore_req.owner, restore_req.filename, 
					   chkpt_file_status.st_size, ON_SERVER);
		  UnblockSignals();
	  }
      data_conn_sd = I_socket();
      if (data_conn_sd == INSUFFICIENT_RESOURCES) {
		  restore_reply.server_name.s_addr = htonl(0);
		  restore_reply.port = htons(0);
		  restore_reply.file_size = htonl(0);
		  restore_reply.req_status = htons(abs(INSUFFICIENT_RESOURCES));
		  net_write(req_sd, (char*) &restore_reply, sizeof(restore_reply_pkt));
		  BlockSignals();
		  sprintf(log_msg, "Restore request from %s DENIED:", 
				  inet_ntoa(shadow_IP));
		  Log(req_id, log_msg);
		  Log("Insufficient buffers/ports to handle request");
		  UnblockSignals();
		  close(req_sd);
		  return;
	  } else if (data_conn_sd == CKPT_SERVER_SOCKET_ERROR) {
		  restore_reply.server_name.s_addr = htonl(0);
		  restore_reply.port = htons(0);
		  restore_reply.file_size = htonl(0);
		  restore_reply.req_status = htons(abs(CKPT_SERVER_SOCKET_ERROR));
		  net_write(req_sd, (char*) &restore_reply, sizeof(restore_reply_pkt));
		  BlockSignals();
		  sprintf(log_msg, "Restore request from %s DENIED:", 
				  inet_ntoa(shadow_IP));
		  Log(req_id, log_msg);
		  Log("Cannot botain a new socket from server");
		  UnblockSignals();
		  close(req_sd);
		  return;
	  }
      memset((char*) &server_sa, 0, sizeof(server_sa));
      server_sa.sin_family = AF_INET;
      server_sa.sin_port = htons(0);
      server_sa.sin_addr = server_addr;
      if ((err_code=I_bind(data_conn_sd, &server_sa)) != CKPT_OK) {
		  cerr << endl << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR: I_bind() returns an error (#" << ret_code
			  << ")" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR:" << endl << endl;
		  sprintf(log_msg, "ERROR: I_bind() returns an error (#%d)", err_code);
		  Log(0, log_msg);
		  exit(ret_code);
	  }
      if (I_listen(data_conn_sd, 1) != CKPT_OK) {
		  cerr << endl << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR: I_listen() fails to listen" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR:" << endl << endl;
		  sprintf(log_msg, "ERROR: I_listen() fails to listen");
		  Log(0, log_msg);
		  exit(LISTEN_ERROR);
	  }
      BlockSignals();
      imds.Lock(shadow_IP, restore_req.owner, restore_req.filename, XMITTING);
      UnblockSignals();
      restore_reply.server_name = server_addr;
      // From the I_bind() call, the port should already be in network-byte
      //   order
      restore_reply.port = server_sa.sin_port;  
      restore_reply.file_size = htonl(chkpt_file_status.st_size);
      restore_reply.req_status = htons(CKPT_OK);
      if (net_write(req_sd, (char*) &restore_reply, 
					sizeof(restore_reply_pkt)) < 0) {
		  close(req_sd);
		  BlockSignals();
		  sprintf(log_msg, "Restore request from %s DENIED:", 
				  inet_ntoa(shadow_IP));
		  Log(req_id, log_msg);
		  Log("Cannot send IP/port to shadow (socket closed)");
		  UnblockSignals();
		  close(data_conn_sd);
	  } else {
		  close(req_sd);
		  BlockSignals();
		  child_pid = fork();
		  if (child_pid < 0) {
			  close(data_conn_sd);
			  Log(req_id, "Unable to honor restore request:");
			  Log("Cannot fork child processes");
		  } else if (child_pid != 0) {            // Parent (Master) process
			  close(data_conn_sd);
			  num_restore_xfers++;
			  transfers.Insert(child_pid, req_id, shadow_IP, 
							   restore_req.filename, restore_req.owner,
							   chkpt_file_status.st_size, restore_req.key,
							   restore_req.priority, XMIT);
			  Log(req_id, "Request to restore checkpoint file GRANTED");
		  } else {
			  // Child process
#if DEBUG
			  log_file.close();
#endif
			  close(store_req_sd);
			  close(restore_req_sd);
			  close(service_req_sd);
			  TransmitCheckpointFile(data_conn_sd, pathname,
									 chkpt_file_status.st_size);
			  exit(CHILDTERM_SUCCESS);
		  }
		  UnblockSignals();
	  }
  } else {
      close(req_sd);
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: IMDS file has an ambiguous lock state" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(IMDS_BAD_RETURN_CODE);
  }
}


void Server::TransmitCheckpointFile(int         data_conn_sd,
									const char* pathname,
									int         file_size)
{
	struct sockaddr_in chkpt_addr;
	int                chkpt_addr_len;
	int                xfer_sd;
	u_lint             bytes_recvd=0;
	int                bytes_read;
	int                bytes_to_read;
	u_lint             bytes_sent=0;
	u_lint             check;
	int                temp;
	int                buf_size=DATA_BUFFER_SIZE;
	int					file_fd;
	
	file_fd = open(pathname, O_RDONLY);
	if (file_fd < 0) {
		exit(CHILDTERM_CANNOT_OPEN_CHKPT_FILE);
	}

	chkpt_addr_len = sizeof(struct sockaddr_in);
	do {
		xfer_sd = tcp_accept_timeout(data_conn_sd,
									 (struct sockaddr *)&chkpt_addr, 
									 &chkpt_addr_len, CKPT_ACCEPT_TIMEOUT);
	} while (xfer_sd == -3 || (xfer_sd == -1 && errno == EINTR));
#if 0
	if ((xfer_sd=I_accept(data_conn_sd, &chkpt_addr, &chkpt_addr_len)) ==
		ACCEPT_ERROR) {
#else
	if (xfer_sd < 0) {
#endif
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: I_accept() fails" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(CHILDTERM_ACCEPT_ERROR);
    }
	close(data_conn_sd);

	// Changing buffer size may fail
	setsockopt(xfer_sd, SOL_SOCKET, SO_SNDBUF, (char*) &buf_size, 
			   sizeof(buf_size));

	bytes_sent = stream_file_xfer(file_fd, xfer_sd, file_size);

	close(xfer_sd);
	close(file_fd);
	if (bytes_sent != file_size) {
		exit(CHILDTERM_BAD_FILE_SIZE);
	}
}


void Server::ChildComplete()
{
	int           child_pid;
	int           exit_status;
	int           exit_code;
	int           xfer_type;
	transferinfo* ptr;
	char          pathname[MAX_PATHNAME_LENGTH];
	char          log_msg[256];
	int           temp;
	
	BlockSignals();
	while ((child_pid=waitpid(-1, &exit_status, WNOHANG)) > 0) {
		if (WIFEXITED(exit_status)) {
			exit_code = WEXITSTATUS(exit_status);
		} else 	{
			// Must trap the SIGKILL signal explicitly since the reclaimation
			//   process may kill a process after it has completed its work.
			//   However, other signals will indicate some failure indicating
			//   abnormal termination
			if (WTERMSIG(exit_status) == SIGTERM)
				exit_code = CHILDTERM_KILLED;
			else
				exit_code = CHILDTERM_SIGNALLED;
		}
		xfer_type = transfers.GetXferType(child_pid);
		if (xfer_type == BAD_CHILD_PID) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: cannot resolve child pid (#" << child_pid << ")" 
				<< endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			exit(BAD_TRANSFER_LIST);
		}
		if ((ptr=transfers.Find(child_pid)) == NULL) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: cannot resolve child pid (#" << child_pid << ")" 
				<< endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			exit(BAD_TRANSFER_LIST);
		}
		switch (xfer_type) {
		    case RECV:
			    num_store_xfers--;
				imds.Unlock(ptr->shadow_addr, ptr->owner, ptr->filename);
				if (exit_code == CHILDTERM_SUCCESS)
					Log(ptr->req_id, "File successfully received");
				else if ((exit_code == CHILDTERM_KILLED) && 
						 (ptr->override == OVERRIDE)) {
					Log(ptr->req_id, "File successfully received; kill signal");
					Log("overridden");
				} else {
					if (exit_code == CHILDTERM_SIGNALLED) {
						sprintf(log_msg, 
								"File transfer terminated due to signal #%d",
								WTERMSIG(exit_status));
						Log(ptr->req_id, log_msg);
						if (WTERMSIG(exit_status) == SIGKILL)
							Log("User killed the peer process");
					}
					else if (exit_code == CHILDTERM_KILLED) {
						Log(ptr->req_id, 
							"File reception terminated by reclamation process");
					} else {
						sprintf(log_msg, 
								"File reception self-terminated; error #%d",
								exit_code);
						Log(ptr->req_id, log_msg);
						Log("occurred during file reception");
					}
					sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX,
							inet_ntoa(ptr->shadow_addr), ptr->owner,
							ptr->filename);
					// Attempt to remove file
					if (remove(pathname) != 0)
						Log("Unable to remove file from physical storage");
					else
						Log("Partial file removed from physical storage");
					// Remove file information from in-memory data structure
					if ((temp=imds.RemoveFile(ptr->shadow_addr, ptr->owner, 
											  ptr->filename)) != REMOVED_FILE){
						sprintf(log_msg,
								"Unable to remove file from imds (%d)",
								temp);
						Log(log_msg);
					}
				}
				break;
			case XMIT:
				num_restore_xfers--;
				imds.Unlock(ptr->shadow_addr, ptr->owner, ptr->filename);
				if (exit_code == CHILDTERM_SUCCESS)
					Log(ptr->req_id, "File successfully transmitted");
				else if ((exit_code == CHILDTERM_KILLED) && 
						 (ptr->override == OVERRIDE)) {
					Log(ptr->req_id,
						"File successfully transmitted; kill signal");
					Log("overridden");
				} else if (exit_code == CHILDTERM_SIGNALLED) {
					sprintf(log_msg, 
							"File transfer terminated due to signal #%d",
							WTERMSIG(exit_status));
					Log(ptr->req_id, log_msg);
					if (WTERMSIG(exit_status) == SIGPIPE)
						Log("Socket on receiving end closed");
					else if (WTERMSIG(exit_status) == SIGKILL)
						Log("User killed the peer process");
				} else if (exit_code == CHILDTERM_KILLED) {
					Log(ptr->req_id, 
						"File transmission terminated by reclamation");
					Log("process");
				} else {
					sprintf(log_msg, 
							"File transmission self-terminated; error #%d", 
							exit_code);
					Log(ptr->req_id, log_msg);
					Log("occurred during file transmission");
				}
				break;
			case FILE_STATUS:
				if (exit_code == CHILDTERM_SUCCESS)
					Log(ptr->req_id, "Server status successfully transmitted");
				else if (exit_code == CHILDTERM_KILLED)
					Log(ptr->req_id, 
						"Server status terminated by reclamation process");
				else if (exit_code == CHILDTERM_SIGNALLED) {
					sprintf(log_msg, 
							"File transfer terminated due to signal #%d",
							WTERMSIG(exit_status));
					Log(ptr->req_id, log_msg);
					if (WTERMSIG(exit_status) == SIGPIPE)
						Log("Socket on receiving end closed");
					else if (WTERMSIG(exit_status) == SIGKILL)
						Log("User killed the peer process");
				} else {
					sprintf(log_msg, 
							"Server status self-terminated; error #%d",
							exit_code);
					Log(ptr->req_id, log_msg);
					Log("occurred during status transmission");
				}
				break;
			case REPLICATE:
				num_replicate_xfers--;
				if (exit_code == CHILDTERM_SUCCESS)
					Log(ptr->req_id, "File successfully replicated");
				else if ((exit_code == CHILDTERM_KILLED) && 
						 (ptr->override == OVERRIDE)) {
					Log(ptr->req_id,
						"File successfully replicated; kill signal");
					Log("overridden");
				} else if (exit_code == CHILDTERM_SIGNALLED) {
					sprintf(log_msg, 
							"File replication terminated due to signal #%d",
							WTERMSIG(exit_status));
					Log(ptr->req_id, log_msg);
					if (WTERMSIG(exit_status) == SIGPIPE)
						Log("Socket on receiving end closed");
					else if (WTERMSIG(exit_status) == SIGKILL)
						Log("User killed the peer process");
				} else if (exit_code == CHILDTERM_KILLED) {
					Log(ptr->req_id, 
						"File replication terminated by reclamation");
					Log("process");
				} else {
					sprintf(log_msg, 
							"File replication self-terminated; error #%d", 
							exit_code);
					Log(ptr->req_id, log_msg);
					Log("occurred during file transmission");
				}
				break;
			default:
				cerr << endl << "ERROR:" << endl;
				cerr << "ERROR:" << endl;
				cerr<<"ERROR: illegal transfer type used; terminating" << endl;
				cerr << "ERROR:" << endl;
				cerr << "ERROR:" << endl << endl;
				exit(BAD_RETURN_CODE);	  
			}
		transfers.Delete(child_pid);
    }
	UnblockSignals();
}


void Server::NoMore()
{
  more = 0;
  Log(0, "SIGUSR2 trapped; shutting down checkpoint server");
}


void Server::ServerDump()
{
  imds.DumpInfo();
  transfers.Dump();
}


void Server::Log(int         request,
				 const char* event)
{
	dprintf(D_ALWAYS, "\t");
#if 0
	PrintTime(log_file);
	log_file << setw(11) << request << "   ";
#endif
	if (strlen(event) > LOG_EVENT_WIDTH_1) {
		dprintf(D_ALWAYS | D_NOHEADER, "%s\n", event);
#if 0
		log_file.write(event, LOG_EVENT_WIDTH_1);
		log_file << endl;
#endif
		Log(event+LOG_EVENT_WIDTH_1);
    } else {
		dprintf(D_ALWAYS | D_NOHEADER, "%s\n", event);
#if 0
		log_file << event << endl;
#endif
	}
}


void Server::Log(const char* event)
{
	int   num_lines;
	int   count;
	char* ptr;
	
#if 0
	ptr = (char*) event;
	num_lines = (strlen(event)+LOG_EVENT_WIDTH_2-1)/LOG_EVENT_WIDTH_2;
	for (count=1; count<num_lines; count++) {
		log_file << "\t\t\t\t";
		log_file.write(ptr, LOG_EVENT_WIDTH_2);
		log_file << endl;
		ptr += LOG_EVENT_WIDTH_2;
    }
	log_file << "\t\t\t\t" << ptr << endl;
#endif
	dprintf(D_ALWAYS | D_NOHEADER, "\t\t\t%s\n", event);
}


void InstallSigHandlers()
{
	struct sigaction sig_info;
	
	sig_info.sa_handler = (SIG_HANDLER) SigChildHandler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGCHLD, &sig_info, NULL) < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot install SIGCHLD signal handler" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigUser1Handler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGUSR1, &sig_info, NULL) < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot install SIGUSR1 signal handler" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigUser2Handler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGUSR2, &sig_info, NULL) < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot install SIGUSR2 signal handler" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SIG_IGN;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGPIPE, &sig_info, NULL) < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot install SIGPIPE signal handler (SIG_IGN)" << 
			endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigAlarmHandler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGALRM, &sig_info, NULL) < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot install SIGALRM signal handler" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(SIGNAL_HANDLER_ERROR);
    }
}


void SigAlarmHandler(int)
{
  rt_alarm.Expired();
}


void SigChildHandler(int)
{
  BlockSignals();
  server.ChildComplete();
  UnblockSignals();
}


void SigUser1Handler(int)
{
  BlockSignals();
  cout << "SIGUSR1 trapped; this handler is incomplete" << endl << endl;
  server.ServerDump();
  UnblockSignals();
}


void SigUser2Handler(int)
{
  BlockSignals();
  cout << "SIGUSR2 trapped; this handler is incomplete" << endl << endl;
  server.NoMore();
  UnblockSignals();
}


void BlockSignals()
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot obtain current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if ((sigaddset(&mask, SIGCHLD) + sigaddset(&mask, SIGUSR1) +
       sigaddset(&mask, SIGUSR2) + sigaddset(&mask, SIGPIPE)) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot add signals to signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot set new signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
}


void UnblockSignals()
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot obtain current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if ((sigdelset(&mask, SIGCHLD) + sigdelset(&mask, SIGUSR1) +
       sigdelset(&mask, SIGUSR2) + sigdelset(&mask, SIGPIPE)) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot remove signals from signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot set new signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl << endl;
      exit(SIGNAL_MASK_ERROR);
    }
}


int main(int   argc,
	 char* argv[])
{
	int xfers, sends, recvs;
	int	wait_for_debug;

	/* Consume a '-f' flag which the condor_master always gives to its
	   children */
	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'f') {
		argc--;
		argv++;
	}

	set_condor_priv();

	if ((argc != 1) && (argc != 2) && (argc != 4)) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: incorrect number of parameters" << endl;
		cerr << "\tUsage: " << argv[0] << " <MAX TRANSFERS> {<MAX RECEIVE> "
			<< "<MAX TRANSMIT>}" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(BAD_PARAMETERS);
	}
	
	if (argc > 1) {
		xfers = atoi(argv[1]);
	} else {
		xfers = DEFAULT_XFERS;
	}

	if (xfers <= 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: the maximum number of transfers must be greater than " 
			<< "zero" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(BAD_PARAMETERS);
	}
	if (argc == 2 || argc == 1) {
		sends = xfers;
		recvs = xfers;
	} else {
		recvs = atoi(argv[2]);
		if ((recvs < 0) || (recvs > xfers)) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: illegal maximum number of storing transfers" 
				<< endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			exit(BAD_PARAMETERS);
		}
		sends = atoi(argv[3]);
		if ((sends < 0) || (sends > xfers)) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: illegal maximum number of restoring transfers" 
				<< endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			exit(BAD_PARAMETERS);
		}      
	}

	server.Init(xfers, recvs, sends);
	server.Execute();
	return 0;                            // Should never execute
}
