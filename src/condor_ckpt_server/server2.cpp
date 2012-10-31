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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_adtypes.h"
#include "condor_attributes.h"
#include "xfer_summary.h"
#include "string_list.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_version.h"
#include "condor_socket_types.h"
#include "subsystem_info.h"
#include "condor_netdb.h"
#include <iostream>
#include <fstream>
#include "condor_open.h"
#include "directory.h"
#include "basename.h"
#include "stat_info.h"

#include "server2.h"
#include "gen_lib.h"
#include "network2.h"
#include "signal2.h"
#include "alarm2.h"
#include "condor_uid.h"
#include "classad_collection.h"
#include "daemon.h"
#include "protocol.h"
#include "internet_obsolete.h"
#include "condor_sockfunc.h"
using namespace std;

XferSummary	xfer_summary;

Server server;
Alarm  rt_alarm;

char* myName = NULL;

extern "C" {
ssize_t stream_file_xfer( int src_fd, int dst_fd, size_t n_bytes );
int tcp_accept_timeout( int, struct sockaddr*, int*, int );
}

/* Ensure the checkpoint's filename I'm about to read or write stays in
	the checkpointing directory. */
int ValidateNoPathComponents(char *path);

/* Attempt a write.  If the write fails, exit with READWRITE_ERROR.
This code might in the future retry, especially on EAGAIN or EINTR, but
no gaurantees are made.  Really, you should call write() and check your
return, retrying if necessary.  However, in light of the anticipiated
short remaining life on the checkpoint server, this is an acceptable
solution for existing code.  DO NOT USE FOR NETWORK CONNECTIONS.
*/
static ssize_t write_or_die(int fd, const void * buf, size_t count) {
	ssize_t ret = write(fd, buf, count);
	if(ret < 0) {
		dprintf(D_ALWAYS, "Error: error trying to write %d bytes to FD %d. %d: %s\n",
			(int)count, (int)fd, (int)errno, strerror(errno));
		exit(READWRITE_ERROR);
	}
	if(((size_t)ret) != count) {
		dprintf(D_ALWAYS, "Error: only wrote %d bytes out of %d to FD %d. %d: %s\n",
			(int)ret, (int)count, (int)fd, (int)errno, strerror(errno));
		exit(READWRITE_ERROR);
	}
	return ret;
}

/* Attempt a read.  If the read fails, exit with READWRITE_ERROR.
This code might in the future retry, especially on EAGAIN or EINTR, but
no gaurantees are made.  Really, you should call read() and check your
return, retrying if necessary.  However, in light of the anticipiated
short remaining life on the checkpoint server, this is an acceptable
solution for existing code.  DO NOT USE FOR NETWORK CONNECTIONS.
*/
static ssize_t read_or_die(int fd, void * buf, size_t count) {
	ssize_t ret = read(fd, buf, count);
	if(ret < 0) {
		dprintf(D_ALWAYS, "Error: error trying to read %d bytes to FD %d. %d: %s\n",
			(int)count, (int)fd, (int)errno, strerror(errno));
		exit(READWRITE_ERROR);
	}
	if(((size_t)ret) != count) {
		dprintf(D_ALWAYS, "Error: only read %d bytes out of %d to FD %d. %d: %s\n",
			(int)ret, (int)count, (int)fd, (int)errno, strerror(errno));
		exit(READWRITE_ERROR);
	}
	return ret;
}


Server::Server()
{
	ckpt_server_dir = NULL;
	more = 1;
	req_ID = 0;
		// We can't initialize this until Server::Init(), after
		// config() has been called, so we get NETWORK_INTERFACE. 
	server_addr.s_addr = 0;
	store_req_sd = -1;
	restore_req_sd = -1;
	service_req_sd = -1;
	replicate_req_sd = -1;
	socket_bufsize = 0;
	max_xfers = 0;
	max_store_xfers = 0;
	max_restore_xfers = 0;
	max_replicate_xfers = 0;
	num_peers = 0;
	CkptClassAds = NULL;
	clean_interval = 0;
	memset(peer_addr_list, 0, sizeof(peer_addr_list));
	max_req_sd_plus1 = 0;
	num_replicate_xfers = 0;
	num_restore_xfers = 0;
	num_store_xfers = 0;
	reclaim_interval = 0;
	replication_level = 0;
	check_parent_interval = 0;

	next_time_to_remove_stale_ckpt_files = 0;
	remove_stale_ckptfile_interval = 0;
	stale_ckptfile_age_cutoff = 0;
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
	if (ckpt_server_dir != NULL) {
		free(ckpt_server_dir);
		ckpt_server_dir = NULL;
	}
	delete CkptClassAds;
}


void Server::Init()
{
	char		*collection_log;
	char        log_msg[256];
	char        hostname[100];
	int         buf_size;
	SOCKET_LENGTH_TYPE	len;
	static bool first_time = true;
	
	num_store_xfers = 0;
	num_restore_xfers = 0;
	num_replicate_xfers = 0;

	config();
	dprintf_config( get_mySubSystem()->getName() );

	set_condor_priv();

		// We have to do this after we call config, not in the Server
		// constructor, or we won't have NETWORK_INTERFACE yet.
        // Commented out the following line so that server_addr is determined after
        // the socket is bound to an address. -- Sonny 5/18/2005
	//server_addr.s_addr = htonl( my_ip_addr() );

	dprintf( D_ALWAYS,
			 "******************************************************\n" );
	dprintf( D_ALWAYS, "** %s (CONDOR_%s) STARTING UP\n", myName, 
			 get_mySubSystem()->getName() );
	dprintf( D_ALWAYS, "** %s\n", CondorVersion() );
	dprintf( D_ALWAYS, "** %s\n", CondorPlatform() );
	dprintf( D_ALWAYS, "** PID = %lu\n", (unsigned long) getpid() );
	dprintf( D_ALWAYS,
			 "******************************************************\n" );

	ckpt_server_dir = param( "CKPT_SERVER_DIR" );
	if( ckpt_server_dir ) {
		if (chdir(ckpt_server_dir) < 0) {
			dprintf(D_ALWAYS, "Failed to chdir() to %s\n", ckpt_server_dir);
			exit(1);
		}
		dprintf(D_ALWAYS, "CKPT_SERVER running in directory %s\n", 
				ckpt_server_dir);
	}

	replication_level = param_integer( "CKPT_SERVER_REPLICATION_LEVEL",0 );
	reclaim_interval = param_integer( "CKPT_SERVER_INTERVAL",RECLAIM_INTERVAL );

	collection_log = param( "CKPT_SERVER_CLASSAD_FILE" );
	if ( collection_log ) {
		delete CkptClassAds;
		CkptClassAds = new ClassAdCollection(collection_log);
		free(collection_log);
	} else {
		delete CkptClassAds;
		CkptClassAds = NULL;
	}

	// How long between periods of checking ckpt files for staleness and
	// removing them.
	// Defaults to one day (86400) seconds.
	// This is used because A) in general, the ckpt server sucks,
	// and B) the new timeout code in the server interface. If suppose
	// the checkpoint server goes down and the schedd can't remove the ckpt
	// files, they are simply leaked into the checkpoint server, forever to
	// consume space. This will prevent unbounded leakage.
	// It was faster to implement this on a dying feature of Condor than
	// to implement the checkpoint server calling back schedds to see if the
	// job is still around.
	remove_stale_ckptfile_interval =
		param_integer("CKPT_SERVER_REMOVE_STALE_CKPT_INTERVAL", 86400, 0, INT_MAX);

	// How long a ckptfile's a_time hasn't been updated to be considered stale
	// Defaults to 60 days.
	stale_ckptfile_age_cutoff =
		param_integer("CKPT_SERVER_STALE_CKPT_AGE_CUTOFF", 5184000, 1, INT_MAX);

	// This has the effect of checking for stale stuff right away on start up 
	next_time_to_remove_stale_ckpt_files = 0;

	clean_interval = param_integer( "CKPT_SERVER_CLEAN_INTERVAL",CLEAN_INTERVAL );
	check_parent_interval = param_integer( "CKPT_SERVER_CHECK_PARENT_INTERVAL",120);
	socket_bufsize = param_integer( "CKPT_SERVER_SOCKET_BUFSIZE",0 );
	max_xfers = param_integer( "CKPT_SERVER_MAX_PROCESSES",DEFAULT_XFERS );
	max_store_xfers = param_integer( "CKPT_SERVER_MAX_STORE_PROCESSES",max_xfers );
	max_restore_xfers = param_integer( "CKPT_SERVER_MAX_RESTORE_PROCESSES",max_xfers );

	max_replicate_xfers = max_store_xfers/5;

	if (first_time) {
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
	}
	SetUpPeers();
	xfer_summary.init();
	Log("Server Initializing");
	Log("Server:                            ");
	if (condor_gethostname(hostname, 99) == 0) {
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

	first_time = false;
}


int Server::SetUpPort(u_short port)
{
	condor_sockaddr socket_addr;
  int                temp_sd;
  int                ret_code;

  temp_sd = I_socket();
  if (temp_sd == INSUFFICIENT_RESOURCES) {
      dprintf(D_ALWAYS, "ERROR: insufficient resources for new socket\n");
      exit(INSUFFICIENT_RESOURCES);
    }
  else if (temp_sd == CKPT_SERVER_SOCKET_ERROR) {
      dprintf(D_ALWAYS, "ERROR: cannot open a server request socket\n");
      exit(CKPT_SERVER_SOCKET_ERROR);
  }
  socket_addr.set_ipv4();
  socket_addr.set_addr_any();
  socket_addr.set_port(port);
  //memset((char*) &socket_addr, 0, sizeof(struct sockaddr_in));
  //socket_addr.sin_family = AF_INET;
  //socket_addr.sin_port = htons(port);
  // Let OS choose address --Sonny 5/18/2005
  //memcpy((char*) &socket_addr.sin_addr, (char*) &server_addr, 
  //	 sizeof(struct in_addr));
  if ((ret_code=I_bind(temp_sd, socket_addr, TRUE)) != CKPT_OK) {
      dprintf(D_ALWAYS, "ERROR: I_bind() returned an error (#%d)\n", ret_code);
      exit(ret_code);
  }
  if (I_listen(temp_sd, 5) != CKPT_OK) {
      dprintf(D_ALWAYS, "ERROR: I_listen() failed\n");
      exit(LISTEN_ERROR);
  }
  if (socket_addr.get_port() != port) {
      dprintf(D_ALWAYS, "ERROR: cannot use Condor well-known port\n");
      exit(BIND_ERROR);
  }
  // Now we can set server_addr with the address that temp_sd is bound to
  struct sockaddr_in *tmp = getSockAddr(temp_sd);
  if (tmp == NULL) {
      dprintf(D_ALWAYS, "failed getSockAddr(%d)\n", temp_sd);
      exit(BIND_ERROR);
  }
  memcpy( (char*) &server_addr, (char*) &tmp->sin_addr.s_addr, 
		  sizeof(struct in_addr) );

  return temp_sd;
}

void Server::SetUpPeers()
{
	char *peers, *peer, *ckpt_host;
	//char peer_addr[256];
	StringList peer_name_list;

	if ((peers = param("CKPT_SERVER_HOSTS")) == NULL) {
		return;
	}

	if( (ckpt_host = param("CKPT_SERVER_HOST")) == NULL ) {
		return;
	}

	peer_name_list.initializeFromString(peers);
	free( peers );
	peer_name_list.rewind();
	while ((peer = peer_name_list.next()) != NULL) {
		if( strcmp(peer, ckpt_host) ) {
			condor_sockaddr addr;
			if( ! addr.from_ip_string(peer) ) {
				EXCEPT("Unable to parse IP string %s from CKPT_SERVER_HOST/CKPT_SERVER_HOSTS", peer);
			}
			addr.set_port(CKPT_SVR_REPLICATE_REQ_PORT);
			peer_addr_list[num_peers++] = addr.to_sin();
			//sprintf(peer_addr, "<%s:%d>", peer, CKPT_SVR_REPLICATE_REQ_PORT);
			//string_to_sin(peer_addr, peer_addr_list+(num_peers++));
		}
	}
	free( ckpt_host );
}

void Server::Execute()
{
	fd_set         req_sds;
	int            num_sds_ready = 0;
	time_t         current_time;
	time_t         last_reclaim_time;
	time_t		   last_clean_time;
	time_t         last_check_parent_time;
	time_t         last_remove_stale_time;
	struct timeval poll;
	int            ppid;
	
	current_time = last_reclaim_time = last_clean_time = last_check_parent_time = time(NULL);

	ppid = getppid();

	dprintf( D_ALWAYS, "Sending initial ckpt server ad to collector\n" );
    struct sockaddr_in *tmp = getSockAddr(store_req_sd);
    if (tmp == NULL) {
        EXCEPT("Can't get the address that store_req_sd is bound to");
    }
    char *canon_name = strdup( inet_ntoa( server_addr) );
	if ( canon_name == NULL ) {
        EXCEPT("strdup() of server address failed");
	}
    xfer_summary.time_out(current_time, canon_name);

	// determine if I need to clean up any files upon start up. This'll.
	// ensure I don't get into a "start, leak, crash" problem which will
	// consume the disk.
	RemoveStaleCheckpointFiles(ckpt_server_dir);
	// take into account that I grab the time AFTER I removed the state files
	// this ensures I don't count the time it took to remove them.
	last_remove_stale_time = time(NULL);

	while (more) {                          // Continues until SIGUSR2 signal
		poll.tv_sec = reclaim_interval - ((unsigned int)current_time -
										  (unsigned int)last_reclaim_time);

		poll.tv_usec = 0;

		if( check_parent_interval > 0 ) {
			if( last_check_parent_time > current_time ) {
					// time has jumped back
				last_check_parent_time = current_time;
			}
			if( poll.tv_sec > check_parent_interval -
				(current_time - last_check_parent_time) )
			{
				poll.tv_sec = check_parent_interval;
			}
		}

		// if for whatever reason the user wants a very fast polling time to
		// check for stale checkpoints, then allow it. This isn't exactly
		// correct in that occasionally an event could be miscalculated because
		// I'm not being more careful about the time left up until the next
		// remove stale cycle. But this code is a hack, we don't have real
		// timers, and I need to get it done. This should be "good enough" for
		// now.
		if ((remove_stale_ckptfile_interval > 0) &&
			(remove_stale_ckptfile_interval < poll.tv_sec)) 
		{
			poll.tv_sec = remove_stale_ckptfile_interval;
		}

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
		UnblockSignals();
		while( (more) && 
			   ((num_sds_ready=select(max_req_sd_plus1,
									  (SELECT_FDSET_PTR)&req_sds, 
									  NULL, NULL, &poll)) > 0)) {
		        BlockSignals();
	   
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

			UnblockSignals();
		}
		BlockSignals();
		if (num_sds_ready < 0) {
			if (errno == ECHILD) {
				// Note: we shouldn't really get an ECHILD here, but
				// we did (see condor-admin 14075).
				dprintf(D_ALWAYS, "WARNING: got ECHILD from select()\n");
			} else if (errno != EINTR) {
				dprintf(D_ALWAYS, 
						"ERROR: cannot select from request ports, errno = "
						"%d (%s)\n", errno, strerror(errno));
				exit(SELECT_ERROR);
			}
		}
		current_time = time(NULL);
		if (((unsigned int) current_time - (unsigned int) last_reclaim_time) 
				>= (unsigned int) reclaim_interval) {
			transfers.Reclaim(current_time);
			last_reclaim_time = current_time;
			if (replication_level)
				Replicate();
			dprintf(D_ALWAYS, "Sending ckpt server ad to collector...\n"); 
			xfer_summary.time_out(current_time, canon_name);
		}
		if (((unsigned int) current_time - (unsigned int) last_clean_time)
				>= (unsigned int) clean_interval) {
			if (CkptClassAds) {
				Log("Cleaning ClassAd log...");
				CkptClassAds->TruncLog();
				Log("Done cleaning ClassAd log...");
			}
			last_clean_time = current_time;
		}
		if( current_time - last_check_parent_time >= check_parent_interval
			&& check_parent_interval > 0 )
		{
			if( kill(ppid,0) < 0 ) {
				if( errno == ESRCH ) {
					MyString msg;
					msg.formatstr("Parent process %d has gone away", ppid);
					NoMore(msg.Value());
				}
			}
			last_check_parent_time = current_time;
		}

		// and clean up any stale stuff, but only as fast as required in case
		// of a busy server.
		if (remove_stale_ckptfile_interval > 0 &&
			current_time-last_remove_stale_time>=remove_stale_ckptfile_interval)
		{
			RemoveStaleCheckpointFiles(ckpt_server_dir);
			last_remove_stale_time = time(NULL);
		}
    }
	free( canon_name );
	close(service_req_sd);
	close(store_req_sd);
	close(restore_req_sd);
	close(replicate_req_sd);
}


void Server::HandleRequest(int req_sd,
						   request_type req)
{
	condor_sockaddr    shadow_sa;
//	struct sockaddr_in shadow_sa;
//	int                shadow_sa_len;
	int                new_req_sd;
	service_req_pkt    service_req;
	store_req_pkt      store_req;
	restore_req_pkt    restore_req;
	char               log_msg[256];
	FDContext			fdc;
	int					ret;
	
	if ((new_req_sd=I_accept(req_sd, shadow_sa)) ==
		ACCEPT_ERROR) {
		dprintf(D_ALWAYS, "I_accept failed.\n");
		exit(ACCEPT_ERROR);
    }
	req_ID++;

	/* Set up our connection object */
	fdc.fd = new_req_sd;
	fdc.type = FDC_UNKNOWN;
	fdc.who = shadow_sa.to_sin().sin_addr;
	fdc.req_ID = req_ID;

	dprintf(D_ALWAYS, "----------------------------------------------------\n");
	switch (req) {
        case SERVICE_REQ:
		    sprintf(log_msg, "%s%s", "Receiving SERVICE request from ", 
					shadow_sa.to_ip_string().Value());
			break;
		case STORE_REQ:
			sprintf(log_msg, "%s%s", "Receiving STORE request from ", 
					shadow_sa.to_ip_string().Value());
			break;
		case RESTORE_REQ:
			sprintf(log_msg, "%s%s", "Receiving RESTORE request from ", 
					shadow_sa.to_ip_string().Value());
			break;
		case REPLICATE_REQ:
			sprintf(log_msg, "%s%s", "Receiving REPLICATE request from ", 
					shadow_sa.to_ip_string().Value());
			break;
		default:
			dprintf(D_ALWAYS, "ERROR: invalid request type encountered (%d)\n", 
					(int) req);
			exit(BAD_REQUEST_TYPE);
		}
	Log(req_ID, log_msg);
	sprintf(log_msg, "%s%d%s", "Using descriptor ", new_req_sd, 
			" to handle request");
	Log(log_msg);

	switch (req) {
        case SERVICE_REQ:
			ret = recv_service_req_pkt(&service_req, &fdc);
			if (ret != PC_OK) {
				Log(req_ID, "Unable to process SERVICE request! "
					"Closing connection.");
				close(fdc.fd);
				return;
			}

			ProcessServiceReq(req_ID, &fdc, shadow_sa.to_sin().sin_addr, service_req);

			return;

			break;

		case STORE_REQ:
			ret = recv_store_req_pkt(&store_req, &fdc);
			if (ret != PC_OK) {
				Log(req_ID, "Unable to process STORE request! "
					"Closing connection.");
				close(fdc.fd);
				return;
			}

			/* Reject the store request under certain conditions */
			if (((num_store_xfers + num_restore_xfers) == max_xfers) ||
				num_store_xfers == max_store_xfers)
			{
				store_reply_pkt store_reply;

				store_reply.server_name.s_addr = 0;
				store_reply.port = 0;
				store_reply.req_status = INSUFFICIENT_BANDWIDTH;

				send_store_reply_pkt(&store_reply, &fdc);

				Log(req_ID, "Request to store a file has been DENIED");
				Log("Configured maximum number of active transfers exceeded");

				close(fdc.fd);
				return;
			}

			ProcessStoreReq(req_ID, &fdc, shadow_sa.to_sin().sin_addr, store_req);

			return;
			break;

		case RESTORE_REQ:
			ret = recv_restore_req_pkt(&restore_req, &fdc);
			if (ret != PC_OK) {
				Log(req_ID, "Unable to process RESTORE request! "
					"Closing connection.");
				close(fdc.fd);
				return;
			}

			/* Reject the restore request under certain conditions */
			if (((num_store_xfers + num_restore_xfers) == max_xfers) ||
				 (num_restore_xfers == max_restore_xfers))
			{
				restore_reply_pkt restore_reply;

				restore_reply.server_name.s_addr = 0;
				restore_reply.port = 0;
				restore_reply.file_size = 0;
				restore_reply.req_status = INSUFFICIENT_BANDWIDTH;

				send_restore_reply_pkt(&restore_reply, &fdc);

				Log(req_ID, "Request to restore a file has been DENIED");
				Log("Configured maximum number of active transfers exceeded");

				close(fdc.fd);
				return;
			}

			ProcessRestoreReq(req_ID, &fdc, shadow_sa.to_sin().sin_addr, restore_req);

			return;
			break;

		case REPLICATE_REQ:
			Log(fdc.req_ID, "Ignoring REPLICATE_REQ. Feature not implemented.");

			close(fdc.fd);
			return;
			break;
	}
}

void Server::ProcessServiceReq(int             req_id,
							   FDContext       *fdc,
							   struct in_addr  shadow_IP,
							   service_req_pkt service_req)
{  
	service_reply_pkt  service_reply;
	char               log_msg[256];
	condor_sockaddr    server_sa;
	int                data_conn_sd = -1;
	struct stat        chkpt_file_status;
	char               pathname[MAX_PATHNAME_LENGTH];
	int                num_files = 0;
	int                child_pid;
	int                ret_code;

	/* By the time we get to this place, we've already read the first packet
		off of the wire, and know if we are talking to a 32bit or 64 bit
		client. */
	ASSERT(fdc->type == FDC_32 || fdc->type == FDC_64);

	/* give a base initialization to the reply structure which is adjusted
		as needed in the control flow. */
	service_reply.server_addr.s_addr = 0;
	service_reply.port = 0;
	service_reply.num_files = 0;
	service_reply.req_status = BAD_SERVICE_TYPE;
	memset(	service_reply.capacity_free_ACD, 0, MAX_ASCII_CODED_DECIMAL_LENGTH);

	
	if (service_req.ticket != AUTHENTICATION_TCKT) {
		service_reply.server_addr.s_addr = 0;
		service_reply.port = 0;
		service_reply.req_status = BAD_AUTHENTICATION_TICKET;

		send_service_reply_pkt(&service_reply, fdc);

		sprintf(log_msg, "Request for service from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		sprintf(log_msg, "Invalid authentication ticket [%lu] used", 
			(unsigned long) service_req.ticket);
		Log(log_msg);

		close(fdc->fd);
		return;
    }

	switch (service_req.service) {
        case CKPT_SERVER_SERVICE_STATUS:
		    sprintf(log_msg, "Service: CKPT_SERVER_SERVICE_STATUS");
			break;
		case SERVICE_RENAME:
			sprintf(log_msg, "Service: SERVICE_RENAME");
			break;
		case SERVICE_DELETE:
			sprintf(log_msg, "Service: SERVICE_DELETE");
			break;
		case SERVICE_EXIST:
			sprintf(log_msg, "Service: SERVICE_EXIST");
			break;
		default:
			sprintf(log_msg, "Service: Unknown service requested from %s. "
				"Closing connection.", inet_ntoa(shadow_IP));
			Log(log_msg);
			close(fdc->fd);
			return;
			break;
		}
	Log(log_msg);  
	sprintf(log_msg, "Owner:   %s", service_req.owner_name);
	Log(log_msg);  
	sprintf(log_msg, "File:    %s", service_req.file_name);
	Log(log_msg);  

	/* Make sure the various pieces we will be using from the client 
		don't escape the checkpointing directory by rejecting
		it if it is '.' '..' or contains a path separator. */

	if (ValidateNoPathComponents(service_req.owner_name) == FALSE) {
		service_reply.server_addr.s_addr = 0;
		service_reply.port = 0;
		service_reply.req_status = BAD_REQ_PKT;

		send_service_reply_pkt(&service_reply, fdc);

		sprintf(log_msg, "Owner field contans illegal path components!");
		Log(log_msg);
		sprintf(log_msg, "Service request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	if (ValidateNoPathComponents(service_req.file_name) == FALSE) {
		service_reply.server_addr.s_addr = 0;
		service_reply.port = 0;
		service_reply.req_status = BAD_REQ_PKT;

		send_service_reply_pkt(&service_reply, fdc);

		sprintf(log_msg, "Filename field contans illegal path components");
		Log(log_msg);
		sprintf(log_msg, "Service request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	if (ValidateNoPathComponents(inet_ntoa(shadow_IP)) == FALSE) {
		service_reply.server_addr.s_addr = 0;
		service_reply.port = 0;
		service_reply.req_status = BAD_REQ_PKT;

		send_service_reply_pkt(&service_reply, fdc);

		sprintf(log_msg, "ShadowIpAddr field contans illegal path components");
		Log(log_msg);
		sprintf(log_msg, "Service request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	switch (service_req.service) {
        case CKPT_SERVER_SERVICE_STATUS:

			/* XXX This code path appears never to be taken */

		    num_files = imds.GetNumFiles();
			service_reply.num_files = num_files;

			if (num_files > 0) {
				data_conn_sd = I_socket();
				if (data_conn_sd == INSUFFICIENT_RESOURCES) {
					service_reply.server_addr.s_addr = 0;
					service_reply.port = 0;
					service_reply.req_status = abs(INSUFFICIENT_RESOURCES);

					send_service_reply_pkt(&service_reply, fdc);

					sprintf(log_msg, "Service request from %s DENIED:", 
							inet_ntoa(shadow_IP));
					Log(log_msg);
					Log("Insufficient buffers/ports to handle request");

					close(fdc->fd);
					return;

				} else if (data_conn_sd == CKPT_SERVER_SOCKET_ERROR) {
					service_reply.server_addr.s_addr = 0;
					service_reply.port = 0;
					service_reply.req_status = abs(CKPT_SERVER_SOCKET_ERROR);

					send_service_reply_pkt(&service_reply, fdc);

					sprintf(log_msg, "Service request from %s DENIED:", 
							inet_ntoa(shadow_IP));
					Log(log_msg);
					Log("Cannot obtain a new socket from server");
					close(fdc->fd);
					return;
				}
				server_sa.clear();
				server_sa.set_ipv4();
				server_sa.set_addr_any();

                // let OS choose address
				//server_sa.sin_addr = server_addr;
				//server_sa.sin_port = htons(0);

				if ((ret_code=I_bind(data_conn_sd,server_sa,FALSE)) != CKPT_OK)
				{
					dprintf( D_ALWAYS,
							 "ERROR: I_bind() returned an error (#%d)\n", 
							 ret_code );
					exit(ret_code);
				}
				if (I_listen(data_conn_sd, 1) != CKPT_OK) {
					dprintf(D_ALWAYS, "ERROR: I_listen() failed to listen\n");
					exit(LISTEN_ERROR);
				}


		  		// From the I_bind() call, the port & addr should already be in
				// network-byte order. However, I need it in host order
				// so the protocol layer does the right thing when I send
				// the reply.
				service_reply.server_addr.s_addr = ntohl(server_addr.s_addr);
				service_reply.port = server_sa.get_port();
			} else {
				service_reply.server_addr.s_addr = 0;
				service_reply.port = 0;
			}

			service_reply.req_status = CKPT_OK;

			strcpy(service_reply.capacity_free_ACD, "0");

	  		sprintf(log_msg, "STATUS service address: %s:%d", 
	  			inet_ntoa(server_addr), service_reply.port);
	  		Log(log_msg);
			break;

		case SERVICE_RENAME:
			service_reply.server_addr.s_addr = 0;
			service_reply.port = 0;
			service_reply.num_files = 0;

			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0) || 
				(strlen(service_req.new_file_name) == 0)) {
				service_reply.req_status = BAD_REQ_PKT;
			} else {
				if (transfers.GetKey(shadow_IP, service_req.owner_name, 
								 service_req.file_name) == service_req.key) {
					transfers.SetOverride(shadow_IP, 
										  service_req.owner_name, 
										  service_req.file_name);
				}
				if (transfers.GetKey(shadow_IP, service_req.owner_name, 
							 service_req.new_file_name) == service_req.key) {
					transfers.SetOverride(shadow_IP, 
										  service_req.owner_name, 
										  service_req.new_file_name);
				}
				service_reply.req_status = 
						imds.RenameFile(shadow_IP, 
										  service_req.owner_name, 
										  service_req.file_name,
										  service_req.new_file_name);
				if (replication_level) {
					ScheduleReplication(shadow_IP,
										service_req.owner_name,
										service_req.new_file_name,
										replication_level);
				}
			}
			break;

		case SERVICE_COMMIT_REPLICATION:
			service_reply.server_addr.s_addr = 0;
			service_reply.port = 0;
			service_reply.num_files = 0;
			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0) || 
				(strlen(service_req.new_file_name) == 0)) {
				service_reply.req_status = BAD_REQ_PKT;
			} else {
				if (transfers.GetKey(service_req.shadow_IP,
									 service_req.owner_name, 
								 service_req.file_name) == service_req.key) {
					transfers.SetOverride(service_req.shadow_IP, 
										  service_req.owner_name, 
										  service_req.file_name);
				}
				if (transfers.GetKey(service_req.shadow_IP,
									 service_req.owner_name, 
							 service_req.new_file_name) == service_req.key) {
					transfers.SetOverride(service_req.shadow_IP, 
										  service_req.owner_name, 
										  service_req.new_file_name);
				}
				service_reply.req_status = 
						imds.RenameFile(service_req.shadow_IP, 
										  service_req.owner_name, 
										  service_req.file_name,
										  service_req.new_file_name);
			}
			break;

		case SERVICE_ABORT_REPLICATION:
			shadow_IP = service_req.shadow_IP;
			// no break == fall through to SERVICE_DELETE

		case SERVICE_DELETE:
			service_reply.server_addr.s_addr = 0;
			service_reply.port = 0;
			service_reply.num_files = 0;
			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0))
				service_reply.req_status = BAD_REQ_PKT;
			else {
				service_reply.req_status = 
						imds.RemoveFile(shadow_IP, 
										  service_req.owner_name, 
										  service_req.file_name);
				MyString key;
				key.formatstr("%s/%s/%s", inet_ntoa(shadow_IP), 
						service_req.owner_name, service_req.file_name);
				if (CkptClassAds) {
					CkptClassAds->DestroyClassAd(key.Value());
				}
			}
			break;

		case SERVICE_EXIST:
			service_reply.server_addr.s_addr = 0;
			service_reply.port = 0;
			service_reply.num_files = 0;
			if ((strlen(service_req.owner_name) == 0) || 
				(strlen(service_req.file_name) == 0))
				service_reply.req_status = BAD_REQ_PKT;
			else {
				sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, 
						inet_ntoa(shadow_IP), service_req.owner_name, 
						service_req.file_name);

				sprintf(log_msg, "Checking existance of file: %s",
					pathname);
				Log(log_msg);

				if (stat(pathname, &chkpt_file_status) == 0) {
						service_reply.req_status = 0;
				} else
					service_reply.req_status = DOES_NOT_EXIST;
			}
			break;
			
		default:
			service_reply.server_addr.s_addr = 0;
			service_reply.port = 0;
			service_reply.num_files = 0;
			service_reply.req_status = BAD_SERVICE_TYPE;
			memset(	service_reply.capacity_free_ACD, 
					0,
					MAX_ASCII_CODED_DECIMAL_LENGTH);
		}

	if (send_service_reply_pkt(&service_reply, fdc) == NET_WRITE_FAIL) {
		close(fdc->fd);
		sprintf(log_msg, "Service request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Cannot sent IP/port to shadow (socket closed)");
    } else {
		close(fdc->fd);
		if (service_req.service == CKPT_SERVER_SERVICE_STATUS) {
			/* XXX This code path appears never to be taken */
			if (num_files == 0) {
				Log("Request for server status GRANTED:");
				Log("No files on checkpoint server");
			} else {
				child_pid = fork();
				if (child_pid < 0) {
					if(data_conn_sd != -1) { close(data_conn_sd); }
					Log("Unable to honor status service request:");
					Log("Cannot fork child processes");	  
				} else if (child_pid != 0)  {
					// Parent (Master) process
					transfers.Insert(child_pid, req_id, shadow_IP,
									 "Server Status",
									 "Condor", 0, service_req.key, 0,
									 FILE_STATUS);
					Log("Request for server status GRANTED");
				} else {
					// Child process
					close(store_req_sd);
					close(restore_req_sd);
					close(service_req_sd);
					/* XXX This call sends a raw file_state_info structure
						down the wire to the other side. Since this code
						doesn't seem to be ever called, I'm not making it
						32/64 auto detecting and backwards compatible.
						Since I already know the service packet connection
						bit witdh that requested this reply, I could easily
						pass the FDContext here instead which will tell me
						what to send to the client. */
					if(data_conn_sd != -1) { SendStatus(data_conn_sd); }
					exit(CHILDTERM_SUCCESS);
				}
			}
		} else if (service_reply.req_status == CKPT_OK)
			Log("Service request successfully completed");
		else {
			Log("Service request cannot be completed:");
			sprintf(log_msg, "Attempt returns error #%d", 
					service_reply.req_status);
			Log(log_msg);
		}
	}
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

		condor_sockaddr tmp_addr(&peer_addr_list[(first_peer+i)%num_peers]);
		sprintf(log_msg, "Scheduling Replication: Prio=%d, Serv=%s, File=%s",
				i, tmp_addr.to_sinful().Value(),
				filename);
		Log(log_msg);
		replication_schedule.InsertReplicationEvent(e);
	}
}

void Server::Replicate()
{
	int 				child_pid, bytes_recvd=0, bytes_read;
	size_t				bytes_sent=0;
	ssize_t				bytes_left, bytes_written;
	char        		log_msg[256], buf[10240];
	char				pathname[MAX_PATHNAME_LENGTH];
	int 				server_sd, ret_code, fd;
	condor_sockaddr     server_sa;
	struct stat 		chkpt_file_status;
	time_t				file_timestamp;
	replicate_req_pkt 	req;
	replicate_reply_pkt	reply;

	Log("Checking replication schedule...");
	ReplicationEvent *e = replication_schedule.GetNextReplicationEvent();
	if (e) {
		struct sockaddr_in tmp_sockaddr = e->ServerAddr();
		server_sa = condor_sockaddr(&tmp_sockaddr);
		sprintf(log_msg, "Replicating: Prio=%d, Serv=%s, File=%s",
				e->Prio(), server_sa.to_sinful().Value(), e->File());
		Log(log_msg);
		sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX,
				inet_ntoa(e->ShadowIP()), e->Owner(), e->File());
		if (stat(pathname, &chkpt_file_status) < 0) {
			dprintf(D_ALWAYS, "ERROR: File '%s' can not be accessed.\n", 
					pathname);
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
				dprintf(D_ALWAYS, "ERROR: I_socket failed.\n");
				exit(server_sd);
			}
			/* TRUE means this is an outgoing connection */
			if( ! _condor_local_bind(TRUE, server_sd) ) {
				close( server_sd );
				dprintf(D_ALWAYS, "ERROR: unable to bind new socket to local interface\n");
				exit(1);
			}
			ret_code = net_write(server_sd, (char *)&req, sizeof(req));
			if (ret_code != sizeof(req)) {
				dprintf(D_ALWAYS, 
						"ERROR: write failed, wrote %d bytes instead of %d bytes\n", 
						ret_code, (int)sizeof(req));
				exit(CHILDTERM_CANNOT_WRITE);
			}
			if (condor_connect(server_sd, server_sa) < 0) {
				dprintf(D_ALWAYS, "ERROR: connect failed.\n");
				exit(CONNECT_ERROR);
			}
			if (net_write(server_sd, (char *) &req,
						  sizeof(req)) != sizeof(req)) {
				dprintf(D_ALWAYS, "ERROR: Write failed\n");
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
				dprintf(D_ALWAYS, "ERROR: I_socket failed (line %d)\n",
						__LINE__);
				exit(server_sd);
			}
			/* TRUE means this is an outgoing connection */
			if( ! _condor_local_bind(TRUE, server_sd) ) {
				close( server_sd );
				dprintf(D_ALWAYS, 
						"ERROR: unable to bind new socket to local interface\n");
				exit(1);
			}


			server_sa = condor_sockaddr(reply.server_name, ntohs(reply.port));
//			memset((char*) &server_sa, 0, sizeof(server_sa));
//			server_sa.sin_family = AF_INET;
//			memcpy((char *) &server_sa.sin_addr.s_addr,
//				   (char *) &reply.server_name, sizeof(reply.server_name));

			// this is quite strange. reply.port is network-ordered?
			// why it does copy directly?
//			server_sa.sin_port = reply.port;
			if (condor_connect(server_sd, server_sa) < 0) {
				dprintf(D_ALWAYS, "ERROR: Connect failed (line %d)\n",
						__LINE__);
				exit(CONNECT_ERROR);
			}
			if ((fd = safe_open_wrapper_follow(pathname, O_RDONLY)) < 0) {
				dprintf(D_ALWAYS, "ERROR: Can't open file '%s'\n", pathname);
				exit(CHILDTERM_CANNOT_OPEN_CHKPT_FILE);
			}
			bytes_read = 1;
			while (bytes_sent != req.file_size && bytes_read > 0) {
				errno = 0;
				bytes_read = read(fd, &buf, sizeof(buf));
				bytes_left = bytes_read;
				while (bytes_left > 0) {
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
				dprintf(D_ALWAYS, "ERROR: I_socket failed (line %d)\n", 
						__LINE__);
				exit(server_sd);
			}
			/* TRUE means this is an outgoing connection */
			if( ! _condor_local_bind(TRUE, server_sd) ) {
				close( server_sd );
				dprintf(D_ALWAYS, "ERROR: unable to bind new socket to local interface\n");
				exit(1);
			}
			server_sa.set_port(CKPT_SVR_SERVICE_REQ_PORT);
			if (condor_connect(server_sd, server_sa) < 0) {
				dprintf(D_ALWAYS, "ERROR: Connect failed (line %d)\n",
						__LINE__);
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
			if (ret_code != sizeof(s_req)) {
				dprintf(D_ALWAYS, "ERROR: Write failed (%d bytes instead of %d)\n",
						ret_code, (int)sizeof(s_req));
				exit(CHILDTERM_CANNOT_WRITE);
			}
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
  condor_sockaddr    chkpt_addr;
  int                xfer_sd;

  if ((xfer_sd=I_accept(data_conn_sd, chkpt_addr)) ==
      ACCEPT_ERROR)
    {
	  dprintf(D_ALWAYS, "ERROR: I_accept failed.\n");
      exit(CHILDTERM_ACCEPT_ERROR);
    }
  close(data_conn_sd);
  if (socket_bufsize) {
		  // Changing buffer size may fail
	  setsockopt(xfer_sd, SOL_SOCKET, SO_SNDBUF, (char*) &socket_bufsize, 
				 sizeof(socket_bufsize));
  }
  imds.TransferFileInfo(xfer_sd);
}

/* check to make sure something being used as a filename that we will
	be reading or writing isn't trying to do anything funny. This
	means the filename can't be "." ".." or have a path separator
	in it. */
int ValidateNoPathComponents(char *path)
{
	if (path == NULL) {
		return FALSE;
	}

	if (path[0] == '\0' ||
		strcmp(path, ".") == MATCH || 
		strcmp(path, "..") == MATCH ||
		strstr(path, "/") != NULL)
	{
		return FALSE;
	}

	return TRUE;
}

void Server::ProcessStoreReq(int            req_id,
							FDContext *fdc,
							struct in_addr shadow_IP,
							store_req_pkt  store_req)
{
	store_reply_pkt    store_reply;
	int                data_conn_sd;
	condor_sockaddr    server_sa;
	int                child_pid;
	char               pathname[MAX_PATHNAME_LENGTH];
	char               log_msg[256];
	int                err_code;

	ASSERT(fdc->type == FDC_32 || fdc->type == FDC_64);

	if (store_req.ticket != AUTHENTICATION_TCKT) {
		store_reply.server_name.s_addr = 0;
		store_reply.port = 0;
		store_reply.req_status = BAD_AUTHENTICATION_TICKET;

		send_store_reply_pkt(&store_reply, fdc);

		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Invalid authentication ticket used");

		close(fdc->fd);
		return;
    }

	if ((strlen(store_req.filename) == 0) || (strlen(store_req.owner) == 0)) {
		store_reply.server_name.s_addr = 0;
		store_reply.port = 0;
		store_reply.req_status = BAD_REQ_PKT;

		send_store_reply_pkt(&store_reply, fdc);

		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Incomplete request packet");

		close(fdc->fd);
		return;
    }

	sprintf(log_msg, "Owner:     %s", store_req.owner);
	Log(log_msg);
	sprintf(log_msg, "File name: %s", store_req.filename);
	Log(log_msg);
	sprintf(log_msg, "File size: %lu", (unsigned long int)store_req.file_size);
	Log(log_msg);

	/* Make sure the various pieces we will be using from the client 
		don't escape the checkpointing directory by rejecting
		it if it is '.' '..' or contains a path separator. */
	
	if (ValidateNoPathComponents(store_req.owner) == FALSE) {
		store_reply.server_name.s_addr = 0;
		store_reply.port = 0;
		store_reply.req_status = BAD_REQ_PKT;

		send_store_reply_pkt(&store_reply, fdc);

		sprintf(log_msg, "Owner field contans illegal path components!");
		Log(log_msg);
		sprintf(log_msg, "STORE request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	if (ValidateNoPathComponents(store_req.filename) == FALSE) {
		store_reply.server_name.s_addr = 0;
		store_reply.port = 0;
		store_reply.req_status = BAD_REQ_PKT;

		send_store_reply_pkt(&store_reply, fdc);

		sprintf(log_msg, "Filename field contans illegal path components!");
		Log(log_msg);
		sprintf(log_msg, "STORE request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	if (ValidateNoPathComponents(inet_ntoa(shadow_IP)) == FALSE) {
		store_reply.server_name.s_addr = 0;
		store_reply.port = 0;
		store_reply.req_status = BAD_REQ_PKT;

		send_store_reply_pkt(&store_reply, fdc);

		sprintf(log_msg, "ShadowIpAddr field contans illegal path components!");
		Log(log_msg);
		sprintf(log_msg, "STORE request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	data_conn_sd = I_socket();
	if (data_conn_sd == INSUFFICIENT_RESOURCES) {
		store_reply.server_name.s_addr = 0;
		store_reply.port = 0;
		store_reply.req_status = abs(INSUFFICIENT_RESOURCES);

		send_store_reply_pkt(&store_reply, fdc);

		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Insufficient buffers/ports to handle request");

		close(fdc->fd);
		return;

	} else if (data_conn_sd == CKPT_SERVER_SOCKET_ERROR) {
		store_reply.server_name.s_addr = 0;
		store_reply.port = 0;
		store_reply.req_status = abs(CKPT_SERVER_SOCKET_ERROR);

		send_store_reply_pkt(&store_reply, fdc);

		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Cannot obtain a new socket from server");

		close(fdc->fd);
		return;
	}

	server_sa.clear();
	server_sa.set_ipv4();
	server_sa.set_addr_any();
    // Let OS choose address
	//server_sa.sin_port = htons(0);
	//server_sa.sin_addr = server_addr;
	if ((err_code=I_bind(data_conn_sd, server_sa,FALSE)) != CKPT_OK) {
		sprintf(log_msg, "ERROR: I_bind() returns an error (#%d)", 
				err_code);
		Log(0, log_msg);
		exit(BIND_ERROR);
	}

	if (I_listen(data_conn_sd, 1) != CKPT_OK) {
		sprintf(log_msg, "ERROR: I_listen() fails to listen");
		Log(log_msg);
		exit(LISTEN_ERROR);
	}

	imds.AddFile(shadow_IP, store_req.owner, store_req.filename, 
				 store_req.file_size, NOT_PRESENT);


	// From the I_bind() call, the port & addr should already be in
	// network-byte order, so we undo it since it gets redone in the writing of
	// the packet.
	store_reply.server_name.s_addr = ntohl(server_addr.s_addr);
	store_reply.port = server_sa.get_port();

	store_reply.req_status = CKPT_OK;
	sprintf(log_msg, "STORE service address: %s:%d", 
		inet_ntoa(server_addr), store_reply.port);
	Log(log_msg);

	if (send_store_reply_pkt(&store_reply, fdc) == NET_WRITE_FAIL) {
		close(fdc->fd);
		sprintf(log_msg, "Store request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Cannot send IP/port to shadow (socket closed)");
		imds.RemoveFile(shadow_IP, store_req.owner, store_req.filename);
		close(data_conn_sd);
	} else {
		close(fdc->fd);

		child_pid = fork();

		if (child_pid < 0) {
			close(data_conn_sd);
			Log("Unable to honor store request:");
			Log("Cannot fork child processes");
		} else if (child_pid != 0) {
			// Parent (Master) process
				   close(data_conn_sd);
			num_store_xfers++;
			transfers.Insert(child_pid, req_id, shadow_IP, 
							 store_req.filename, store_req.owner,
							 store_req.file_size, store_req.key,
							 store_req.priority, RECV);
			Log("Request to store checkpoint file GRANTED");
			int len = strlen(store_req.filename);
			if (strcmp(store_req.filename+len-4, ".tmp") == MATCH) {
				store_req.filename[len-4] = '\0';
			}
			MyString keybuf;
			keybuf.formatstr( "%s/%s/%s", inet_ntoa(shadow_IP), store_req.owner,
					store_req.filename);
			char const *key = keybuf.Value();
			ClassAd *ad;
			if (CkptClassAds) {
				if (!CkptClassAds->LookupClassAd(key, ad)) {
					MyString buf;
					CkptClassAds->NewClassAd(key, CKPT_FILE_ADTYPE, "0");
					buf.formatstr( "\"%s\"", store_req.owner);
					CkptClassAds->SetAttribute(key, ATTR_OWNER, buf.Value());
					buf.formatstr( "\"%s\"", inet_ntoa(shadow_IP));
					CkptClassAds->SetAttribute(key, ATTR_SHADOW_IP_ADDR, buf.Value());
					buf.formatstr( "\"%s\"", store_req.filename);
					CkptClassAds->SetAttribute(key, ATTR_FILE_NAME, buf.Value());
				}
				char size[40];
				sprintf(size, "%d", (int) store_req.file_size);
				CkptClassAds->SetAttribute(key, ATTR_FILE_SIZE, size);
			}
		} else {
			// Child process
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
	}
}

void Server::ReceiveCheckpointFile(int         data_conn_sd,
								   const char* pathname,
								   int         file_size)
{
	struct sockaddr_in chkpt_addr;
	int                chkpt_addr_len;
	int                xfer_sd;
	int				   bytes_recvd=0;
	int				   file_fd;
	int				   peer_info_fd;
	char			   peer_info_filename[100];
	bool			   incomplete_file = false;
	char               log_msg[256];
	
	sprintf(log_msg, "Receiving checkpoint to file: %s", pathname);
	Log(log_msg);
	
#if 1
	file_fd = safe_open_wrapper_follow(pathname, O_WRONLY|O_CREAT|O_TRUNC,0664);
#else
	file_fd = safe_open_wrapper_follow("/dev/null", O_WRONLY, 0);
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
		dprintf(D_ALWAYS, "ERROR: I_accept() failed (line %d)\n",
				__LINE__);
		exit(CHILDTERM_ACCEPT_ERROR);
    }
	close(data_conn_sd);
	if (socket_bufsize) {
			// Changing buffer size may fail
		setsockopt(xfer_sd, SOL_SOCKET, SO_RCVBUF, (char*) &socket_bufsize, 
				   sizeof(socket_bufsize));
	}
	bytes_recvd = stream_file_xfer(xfer_sd, file_fd, file_size);
	// note that if file size == -1, we don't know if we got the complete 
	// file, so we must rely on the client to commit via SERVICE_RENAME
	if (bytes_recvd < file_size) incomplete_file = true;
	int n = htonl(bytes_recvd);
	net_write(xfer_sd, (char*) &n, sizeof(n));
	close(xfer_sd);
	fsync(file_fd);
	close(file_fd);

	// Write peer address and bytes received to a temporary file which
	// is read by the parent.  The peer address is very important for
	// logging purposes.
	sprintf(peer_info_filename, "/tmp/condor_ckpt_server.%d", getpid());
	peer_info_fd = safe_open_wrapper_follow(peer_info_filename, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (peer_info_fd >= 0) {
		write_or_die(peer_info_fd, (char *)&(chkpt_addr.sin_addr),
			  sizeof(struct in_addr));
		write_or_die(peer_info_fd, (char *)&bytes_recvd, sizeof(bytes_recvd));
		close(peer_info_fd);
	}

	if (incomplete_file) {
		dprintf(D_ALWAYS, "ERROR: Incomplete file.\n");
		exit(CHILDTERM_ERROR_INCOMPLETE_FILE);
	}
}


void Server::ProcessRestoreReq(int             req_id,
							   FDContext      *fdc,
							   struct in_addr  shadow_IP,
							   restore_req_pkt restore_req)
{
	struct stat        chkpt_file_status;
	condor_sockaddr    server_sa;
	restore_reply_pkt  restore_reply;
	int                data_conn_sd;
	int                child_pid;
	char               pathname[MAX_PATHNAME_LENGTH];
	int                preexist;
	char               log_msg[256];
	int                err_code;

	ASSERT(fdc->type == FDC_32 || fdc->type == FDC_64);
	
	if (restore_req.ticket != AUTHENTICATION_TCKT) {
		restore_reply.server_name.s_addr = 0;
		restore_reply.port = 0;
		restore_reply.file_size = 0;
		restore_reply.req_status = BAD_AUTHENTICATION_TICKET;

		send_restore_reply_pkt(&restore_reply, fdc);

		sprintf(log_msg, "Restore request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Invalid authentication ticket used");

		close(fdc->fd);
		return;
    }

	if ((strlen(restore_req.filename) == 0) || 
		(strlen(restore_req.owner) == 0)) {
		restore_reply.server_name.s_addr = 0;
		restore_reply.port = 0;
		restore_reply.file_size = 0;
		restore_reply.req_status = BAD_REQ_PKT;

		send_restore_reply_pkt(&restore_reply, fdc);

		sprintf(log_msg, "Restore request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Incomplete request packet");

		close(fdc->fd);
		return;
    }

	sprintf(log_msg, "Owner:     %s", restore_req.owner);
	Log(log_msg);
	sprintf(log_msg, "File name: %s", restore_req.filename);
	Log(log_msg);

	/* Make sure the various pieces we will be using from the client 
		don't escape the checkpointing directory by rejecting
		it if it is '.' '..' or contains a path separator. */

	if (ValidateNoPathComponents(restore_req.owner) == FALSE) {
		restore_reply.server_name.s_addr = 0;
		restore_reply.port = 0;
		restore_reply.req_status = BAD_REQ_PKT;

		send_restore_reply_pkt(&restore_reply, fdc);

		sprintf(log_msg, "Owner field contans illegal path components!");
		Log(log_msg);
		sprintf(log_msg, "RESTORE request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	if (ValidateNoPathComponents(restore_req.filename) == FALSE) {
		restore_reply.server_name.s_addr = 0;
		restore_reply.port = 0;
		restore_reply.req_status = BAD_REQ_PKT;

		send_restore_reply_pkt(&restore_reply, fdc);

		sprintf(log_msg, "Filename field contans illegal path components!");
		Log(log_msg);
		sprintf(log_msg, "RESTORE request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	if (ValidateNoPathComponents(inet_ntoa(shadow_IP)) == FALSE) {
		restore_reply.server_name.s_addr = 0;
		restore_reply.port = 0;
		restore_reply.req_status = BAD_REQ_PKT;

		send_restore_reply_pkt(&restore_reply, fdc);

		sprintf(log_msg, "ShadowIpAddr field contans illegal path components!");
		Log(log_msg);
		sprintf(log_msg, "RESTORE request DENIED!");
		Log(log_msg);

		close(fdc->fd);
		return;
	}

	sprintf(pathname, "%s%s/%s/%s", LOCAL_DRIVE_PREFIX, inet_ntoa(shadow_IP),
			restore_req.owner, restore_req.filename);
	if ((preexist=stat(pathname, &chkpt_file_status)) != 0) {
		restore_reply.server_name.s_addr = 0;
		restore_reply.port = 0;
		restore_reply.file_size = 0;
		restore_reply.req_status = DESIRED_FILE_NOT_FOUND;

		send_restore_reply_pkt(&restore_reply, fdc);

		sprintf(log_msg, "Restore request from %s DENIED:", 
				inet_ntoa(shadow_IP));
		Log(log_msg);
		Log("Requested file does not exist on server");

		close(fdc->fd);
		return;
    } else if (preexist == 0) {
		imds.AddFile(shadow_IP, restore_req.owner, restore_req.filename, 
					 chkpt_file_status.st_size, ON_SERVER);
	}
      data_conn_sd = I_socket();
      if (data_conn_sd == INSUFFICIENT_RESOURCES) {
		  restore_reply.server_name.s_addr = 0;
		  restore_reply.port = 0;
		  restore_reply.file_size = 0;
		  restore_reply.req_status = abs(INSUFFICIENT_RESOURCES);

		  send_restore_reply_pkt(&restore_reply, fdc);

		  sprintf(log_msg, "Restore request from %s DENIED:", 
				  inet_ntoa(shadow_IP));
		  Log(log_msg);
		  Log("Insufficient buffers/ports to handle request");

		  close(fdc->fd);
		  return;

	  } else if (data_conn_sd == CKPT_SERVER_SOCKET_ERROR) {

		  restore_reply.server_name.s_addr = 0;
		  restore_reply.port = 0;
		  restore_reply.file_size = 0;
		  restore_reply.req_status = abs(CKPT_SERVER_SOCKET_ERROR);

		  send_restore_reply_pkt(&restore_reply, fdc);

		  sprintf(log_msg, "Restore request from %s DENIED:", 
				  inet_ntoa(shadow_IP));
		  Log(log_msg);
		  Log("Cannot botain a new socket from server");

		  close(fdc->fd);
		  return;
	  }
      server_sa.clear();
      server_sa.set_ipv4();
      server_sa.set_addr_any();
      //server_sa.sin_port = 0;
      //server_sa.sin_addr = server_addr;
      if ((err_code=I_bind(data_conn_sd, server_sa,FALSE)) != CKPT_OK) {
		  sprintf(log_msg, "ERROR: I_bind() returns an error (#%d)", err_code);
		  Log(0, log_msg);
		  exit(BIND_ERROR);
	  }
      if (I_listen(data_conn_sd, 1) != CKPT_OK) {
		  sprintf(log_msg, "ERROR: I_listen() fails to listen");
		  Log(0, log_msg);
		  exit(LISTEN_ERROR);
	  }
	  // From the I_bind() call, the port & addr should already be in
	  // network-byte order, so we undo it since it gets redone in the
	  // writing of the packet.
	  restore_reply.server_name.s_addr = ntohl(server_addr.s_addr);
      restore_reply.port = server_sa.get_port();

      restore_reply.file_size = chkpt_file_status.st_size;
      restore_reply.req_status = CKPT_OK;
	  sprintf(log_msg, "RESTORE service address: %s:%d", 
	  	inet_ntoa(server_addr), restore_reply.port);
	  Log(log_msg);

	  if (send_restore_reply_pkt(&restore_reply, fdc) == NET_WRITE_FAIL) {

		  close(fdc->fd);
		  sprintf(log_msg, "Restore request from %s DENIED:", 
				  inet_ntoa(shadow_IP));
		  Log(log_msg);
		  Log("Cannot send IP/port to shadow (socket closed)");
		  close(data_conn_sd);
	  } else {
		  close(fdc->fd);
		  child_pid = fork();
		  if (child_pid < 0) {
			  close(data_conn_sd);
			  Log("Unable to honor restore request!");
			  Log("Cannot fork child processes");
		  } else if (child_pid != 0) {            // Parent (Master) process
			  close(data_conn_sd);
			  num_restore_xfers++;
			  transfers.Insert(child_pid, req_id, shadow_IP, 
							   restore_req.filename, restore_req.owner,
							   chkpt_file_status.st_size, restore_req.key,
							   restore_req.priority, XMIT);
			  Log("Request to restore checkpoint file GRANTED");
		  } else {
			  // Child process
			  close(store_req_sd);
			  close(restore_req_sd);
			  close(service_req_sd);
			  TransmitCheckpointFile(data_conn_sd, pathname,
									 chkpt_file_status.st_size);
			  exit(CHILDTERM_SUCCESS);
		  }
	  }
}


void Server::TransmitCheckpointFile(int         data_conn_sd,
									const char* pathname,
									int         file_size)
{
	struct sockaddr_in chkpt_addr;
	int                chkpt_addr_len;
	int                xfer_sd;
	int                bytes_sent=0;
	int				   file_fd;
	int				   peer_info_fd;
	char			   peer_info_filename[100];
	char               log_msg[256];
	
	sprintf(log_msg, "Transmitting checkpoint file: %s", pathname);
	Log(log_msg);

	file_fd = safe_open_wrapper_follow(pathname, O_RDONLY);
	if (file_fd < 0) {
		dprintf(D_ALWAYS, "ERROR: Can't open file '%s'\n", pathname);
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
		dprintf(D_ALWAYS, "I_accept() failed. (line %d)\n", __LINE__);
		exit(CHILDTERM_ACCEPT_ERROR);
    }
	close(data_conn_sd);

	if (socket_bufsize) {
			// Changing buffer size may fail
		setsockopt(xfer_sd, SOL_SOCKET, SO_SNDBUF, (char*) &socket_bufsize, 
			   sizeof(socket_bufsize));
	}

	bytes_sent = stream_file_xfer(file_fd, xfer_sd, file_size);

	close(xfer_sd);
	close(file_fd);

	// Write peer address to a temporary file which is read by the
	// parent.  The peer address is very important for logging purposes.
	sprintf(peer_info_filename, "/tmp/condor_ckpt_server.%d", getpid());
	peer_info_fd = safe_open_wrapper_follow(peer_info_filename, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (peer_info_fd >= 0) {
		write_or_die(peer_info_fd, (char *)&(chkpt_addr.sin_addr),
			  sizeof(struct in_addr));
		write_or_die(peer_info_fd, (char *)&bytes_sent, sizeof(bytes_sent));
		close(peer_info_fd);
	}

	if (bytes_sent != file_size) {
		dprintf(D_ALWAYS, "ERROR: Bad file size (sent %d, expected %d)\n",
				bytes_sent, file_size);
		exit(CHILDTERM_BAD_FILE_SIZE);
	}
}


void Server::ChildComplete()
{
	struct in_addr peer_addr;
	int			  peer_info_fd, xfer_size = 0;
	char		  peer_info_filename[100];
	int           child_pid;
	int           exit_status;
	int           exit_code;
	int           xfer_type;
	transferinfo* ptr;
	char          pathname[MAX_PATHNAME_LENGTH];
	char          log_msg[256];
	int           temp;
	bool	      success_flag = false;
	bool          at_least_one_child = false;

	memset((char *) &peer_addr, 0, sizeof(peer_addr));
	
	while ((child_pid=waitpid(-1, &exit_status, WNOHANG)) > 0) {
		at_least_one_child = true;
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
			dprintf(D_ALWAYS, "ERROR: Can not resolve child pid (#%d)\n",
					child_pid);
			exit(BAD_TRANSFER_LIST);
		}
		if ((ptr=transfers.Find(child_pid)) == NULL) {
			dprintf(D_ALWAYS, "ERROR: Can not resolve child pid (#%d)\n",
					child_pid);
			exit(BAD_TRANSFER_LIST);
		}
		switch (xfer_type) {
		    case RECV:
			    num_store_xfers--;
				if (exit_code == CHILDTERM_SUCCESS) {
					success_flag = true;
					Log(ptr->req_id, "File successfully received");
				} else if ((exit_code == CHILDTERM_KILLED) && 
						 (ptr->override == OVERRIDE)) {
					success_flag = true;
					Log(ptr->req_id, "File successfully received; kill signal");
					Log("overridden");
				} else {
					success_flag = false;
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
				if (exit_code == CHILDTERM_SUCCESS) {
					success_flag = true;
					Log(ptr->req_id, "File successfully transmitted");
				} else if ((exit_code == CHILDTERM_KILLED) && 
						 (ptr->override == OVERRIDE)) {
					success_flag = true;
					Log(ptr->req_id,
						"File successfully transmitted; kill signal");
					Log("overridden");
				} else if (exit_code == CHILDTERM_SIGNALLED) {
					success_flag = false;
					sprintf(log_msg, 
							"File transfer terminated due to signal #%d",
							WTERMSIG(exit_status));
					Log(ptr->req_id, log_msg);
					if (WTERMSIG(exit_status) == SIGPIPE)
						Log("Socket on receiving end closed");
					else if (WTERMSIG(exit_status) == SIGKILL)
						Log("User killed the peer process");
				} else if (exit_code == CHILDTERM_KILLED) {
					success_flag = false;
					Log(ptr->req_id, 
						"File transmission terminated by reclamation");
					Log("process");
				} else {
					success_flag = false;
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
				dprintf(D_ALWAYS, "ERROR: illegal transfer type used; terminating\n");
				exit(BAD_RETURN_CODE);	  
			}
		// Try to read the info file for this child to get the peer address.
		sprintf(peer_info_filename, "/tmp/condor_ckpt_server.%d", child_pid);
		peer_info_fd = safe_open_wrapper_follow(peer_info_filename, O_RDONLY);
		if (peer_info_fd >= 0) {
			read_or_die(peer_info_fd, (char *)&peer_addr, sizeof(struct in_addr));
			read_or_die(peer_info_fd, (char *)&xfer_size, sizeof(xfer_size));
			close(peer_info_fd);
			unlink(peer_info_filename);
		}
		transfers.Delete(child_pid, success_flag, peer_addr, xfer_size);
    }
	
	// I should have processed at least one child. If not, then how did the
	// SIGCHLD signal hanlder get called but I have nothing to reap?
	if (child_pid < 0 && at_least_one_child == false) {
		dprintf(D_ALWAYS, "ERROR from waitpid(): %d (%s)\n", errno,
					strerror(errno));
	}
}

void Server::RemoveStaleCheckpointFiles(const char *directory)
{
	time_t now;
	MyString str;

	now = time(NULL);

	if (directory == NULL) {
		// nothing to do
		return;
	}

	// Here we check to see if we need to clean stale ckpts out.
	if ((remove_stale_ckptfile_interval == 0) ||
		(now < next_time_to_remove_stale_ckpt_files))
	{
		// nothing to do....
		return;
	}

	dprintf(D_ALWAYS, "----------------------------------------------------\n");
	Log("Begin removing stale checkpoint files.");

	StatInfo st(directory);
	Directory dir(&st, PRIV_CONDOR);

	// do the dirty work.
	RemoveStaleCheckpointFilesRecurse(directory, &dir,
		now - stale_ckptfile_age_cutoff, st.GetAccessTime());

	Log("Done removing stale checkpoint files.");

	// set up the next time that I need to remove stale checkpoints 
	// do this _after_ removing them so I don't count the removal
	// time towards the interval to the next time I do it.
	next_time_to_remove_stale_ckpt_files =
		time(NULL) + remove_stale_ckptfile_interval;

	str.formatstr("Next stale checkpoint file check in %lu seconds.",
		(unsigned long)remove_stale_ckptfile_interval);

	Log(str.Value());
}

void Server::RemoveStaleCheckpointFilesRecurse(const char *path, 
	Directory *dir, time_t cutoff_time, time_t a_time)
{
	MyString str;
	const char *file = NULL;
	const char *base = NULL;

	char real_path[PATH_MAX];
	char real_ckpt_server_dir[PATH_MAX];

	if (path == NULL) {
		// in case of this, do nothing and return.
		return;
	}

	if (realpath(path, real_path) == 0) {
		str.formatstr("Server::RemoveStaleCheckpointFilesRecurse(): Could "
			"not resolve %s into a real path: %d(%s). Ignoring.\n",
			path, errno, strerror(errno));
		return;
	}

	if (realpath(ckpt_server_dir, real_ckpt_server_dir) == 0) {
		str.formatstr("Server::RemoveStaleCheckpointFilesRecurse(): Could "
			"not resolve %s into a real path: %d(%s). Strange..ignoring "
			"remove request for file under this directory.\n",
			path, errno, strerror(errno));
		return;
	}

	// We're going to do a small safety measure here and if the start of the
	// path isn't the ckpt_server_dir, we immediately stop since we could have
	// escaped that directory somehow and could be deleting who knows what
	// as root. Not Cool.
	if (
		// path is shorter than ckpt_server_dir
		(strlen(real_path) < strlen(real_ckpt_server_dir)) || 

		// OR if path isn't a subdirectory of ckpt_server_dir
		strncmp(real_path, real_ckpt_server_dir,
			strlen(real_ckpt_server_dir)) != MATCH ||

		// OR if path is tricksy
		((strlen(real_path) > strlen(real_ckpt_server_dir)) && 
			path[strlen(real_ckpt_server_dir)] != '/'))
	{
		str.formatstr(
			"WARNING: "
			"Server::RemoveStaleCheckpointFilesRecurse(): "
			"path name %s, whose real path is %s, appears to be outside "
			"of the ckpt_server_dir of %s. "
			"Ignoring it.", path, real_path, real_ckpt_server_dir);
		Log(str.Value());

		return;
	}

	// if a directory, map myself over it 
	if (IsDirectory(path)) {
		Directory files(path, PRIV_CONDOR);

		// Iterate over the directory
		while(files.Next()) {

			file = files.GetFullPath();

			/* skip these files, especially so we don't recurse into
				our parent directory.
			*/
			if (strcmp(file, ".") == MATCH ||
				strcmp(file, "..") == MATCH) {
				continue;
			}

			/* Here we pass a "back reference" to the directory object here
				so we can delete the file using the mechanics of the
				Directory object. A little clunky interface wise, 
				but easy to implement. If somehow a directory is found
				that leads outside of the ckpt_server_dir, the above code
				will catch it.
			*/
			RemoveStaleCheckpointFilesRecurse(file, &files, cutoff_time,
				files.GetAccessTime());
		}

		// all done with the directory.
		return;
	}

	// otherwise is a file so check to see if we need to remove the file

	// skip if any file, anywhere, named TransferLog is found
	base = condor_basename(path);
	if (strcmp(base, "TransferLog") == MATCH) {
		// do nothing, don't even log it since it isn't worth it.
		return;
	}

	/* everything else should be checked, though */

	if (a_time <= cutoff_time) {
		// Another safety rule is that I'll only remove files that
		// have "cluster" as a prefix. This should describe
		// the entire set of files I need to worry about--checkpoints
		// and temporary checkpoints that didn't get renamed for whatever
		// reason.
		base = condor_basename(path);
		// if for whatever reason path is a symlink, we'd end up just removing
		// the link itself, which is ok, especially if it points to something
		// outside of ckpt_server_dir. If it is inside it, and named right,
		// then it'll get cleaned up properly in this or a later pass.
		if (strncmp(base, "cluster", strlen("cluster")) == MATCH)
		{
			dprintf(D_ALWAYS, "Removing stale file: %s\n", path);
			// reach back into a previous stack frame 
			if (dir->Remove_Full_Path(path) == false) {
				dprintf(D_ALWAYS, "Failed to remove stale file %s "
					"(unknown why)\n", path);
			}
		}
	}
}

void Server::NoMore(char const *reason)
{
  more = 0;
  dprintf(D_ALWAYS, "%s: shutting down checkpoint server\n", 
  	reason==NULL?"(null)":reason);
}


void Server::ServerDump()
{
  imds.DumpInfo();
  transfers.Dump();
}


void Server::Log(int         request,
				 const char* event)
{
	dprintf(D_ALWAYS, "[reqid: %d] %s\n", request, event);
}


void Server::Log(const char* event)
{
	dprintf(D_ALWAYS, "    %s\n", event);
}


void InstallSigHandlers()
{
	struct sigaction sig_info;
	
	sig_info.sa_handler = (SIG_HANDLER) SigChildHandler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGCHLD, &sig_info, NULL) < 0) {
		dprintf(D_ALWAYS, "ERROR: cannot install SIGCHLD signal handler\n");
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigUser1Handler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGUSR1, &sig_info, NULL) < 0) {
		dprintf(D_ALWAYS, "ERROR: cannot install SIGUSR1 signal handler\n");
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigTermHandler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGTERM, &sig_info, NULL) < 0) {
		dprintf(D_ALWAYS, "ERROR: cannot install SIGTERM signal handler\n");
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigQuitHandler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGQUIT, &sig_info, NULL) < 0) {
		dprintf(D_ALWAYS, "ERROR: cannot install SIGQUIT signal handler\n");
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigHupHandler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGHUP, &sig_info, NULL) < 0) {
		dprintf(D_ALWAYS, "ERROR: cannot install SIGHUP signal handler\n");
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SIG_IGN;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGPIPE, &sig_info, NULL) < 0) {
		dprintf(D_ALWAYS, "ERROR: cannot install SIGPIPE signal handler (SIG_IGN)\n");
		exit(SIGNAL_HANDLER_ERROR);
    }
	sig_info.sa_handler = (SIG_HANDLER) SigAlarmHandler;
	sigemptyset(&sig_info.sa_mask);
	sig_info.sa_flags = 0;
	if (sigaction(SIGALRM, &sig_info, NULL) < 0) {
		dprintf(D_ALWAYS, "ERROR: cannot install SIGALRM signal handler\n");
		exit(SIGNAL_HANDLER_ERROR);
    }

	BlockSignals();
}


void SigAlarmHandler(int)
{
  int saveErrno = errno;
  rt_alarm.Expired();
  errno = saveErrno;
}


void SigChildHandler(int)
{
  int saveErrno = errno;
  BlockSignals();
  server.ChildComplete();
  UnblockSignals();
  errno = saveErrno;
}


void SigUser1Handler(int)
{
  int saveErrno = errno;
  BlockSignals();
  cout << "SIGUSR1 trapped; this handler is incomplete" << endl << endl;
  server.ServerDump();
  UnblockSignals();
  errno = saveErrno;
}


void SigTermHandler(int)
{
  int saveErrno = errno;
  BlockSignals();
  cout << "SIGTERM trapped; Shutting down" << endl << endl;
  server.NoMore("SIGTERM trapped");
  UnblockSignals();
  errno = saveErrno;
}

void SigQuitHandler(int)
{
  int saveErrno = errno;
  BlockSignals();
  cout << "SIGQUIT trapped; Shutting down" << endl << endl;
  server.NoMore("SIGQUIT trapped");
  UnblockSignals();
  errno = saveErrno;
}


void SigHupHandler(int)
{
  int saveErrno = errno;
  BlockSignals();
  dprintf(D_ALWAYS, "SIGHUP trapped; Re-initializing\n");
  server.Init();
  UnblockSignals();
  errno = saveErrno;
}


void BlockSignals()
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot obtain current signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
  if ((sigaddset(&mask, SIGCHLD) + sigaddset(&mask, SIGUSR1) +
       sigaddset(&mask, SIGTERM) + sigaddset(&mask, SIGPIPE)) != 0)
    {
	  dprintf(D_ALWAYS, "ERROR: cannot add signals to signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot set new signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
}


void UnblockSignals()
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot obtain current signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
  if ((sigdelset(&mask, SIGCHLD) + sigdelset(&mask, SIGUSR1) +
       sigdelset(&mask, SIGTERM) + sigdelset(&mask, SIGPIPE)) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot remove signals from signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot set new signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
}


int main( int /*argc*/, char **argv )
{
	/* For daemonCore, etc. */
	set_mySubSystem( "CKPT_SERVER", SUBSYSTEM_TYPE_DAEMON );

	myName = argv[0];
	server.Init();
	server.Execute();
	return 0;                            // Should never execute
}
