#include "server_network.h"
#include <stdlib.h>
#include <iostream.h>
#include <time.h>
#include <sys/time.h>
#include "server.h"
#include <errno.h> 
#include "server_constants.h"
#include "server_typedefs.h"
#include <fstream.h>
#include <string.h>
#include <stdio.h>
#include "server_xferstat.h"
#include <math.h>
#include <sys/wait.h>
#include <sys/stat.h>


// Global Variables

ofstream      log_file;
TransferState transfers;
FileStat      file_statistics;
Server        server;
int           num_recv_xfers;
int           num_xmit_xfers;


void Server::SetupReqPorts()
{
#ifdef DEBUG
  int temp, len;

  log_file << "Setting up request ports ...";
#endif
  recv_req_sd = I_socket(CKPT_SERVER_SOCKET_ERROR);
  server_addr.sin_port = htons(CKPT_SVR_STORE_REQ_PORT);
  I_bind(recv_req_sd, &server_addr, 0);
  I_listen(recv_req_sd, 5);
  xmit_req_sd = I_socket(CKPT_SERVER_SOCKET_ERROR);
  server_addr.sin_port = htons(CKPT_SVR_RESTORE_REQ_PORT);
  I_bind(xmit_req_sd, &server_addr, 0);
  I_listen(xmit_req_sd, 5);
  if (recv_req_sd > xmit_req_sd)
    max_req_fdp1 = recv_req_sd + 1;
  else
    max_req_fdp1 = xmit_req_sd + 1;
#ifdef DEBUG
  log_file << "request ports initialized" << endl;
  log_file << "Server IP:     " << inet_ntoa(server_addr.sin_addr) << endl;
  log_file << "Receive Port:  " << CKPT_SVR_STORE_REQ_PORT << endl;
  log_file << "Transmit Port: " << CKPT_SVR_RESTORE_REQ_PORT << endl;
  log_file << "These are REQUEST PORTS, not for data transfers!!" << endl;
  log_file << "Port queue sizes:" << endl;
  len = sizeof(temp);
  if (getsockopt(recv_req_sd, SOL_SOCKET, SO_RCVBUF, (char*)&temp, &len) < 0)
    perror("getsockopt() error:");
  else
    log_file << "\tReceive: " << temp << endl;
  len = sizeof(temp);
  if (getsockopt(recv_req_sd, SOL_SOCKET, SO_SNDBUF, (char*)&temp, &len) < 0)
    perror("getsockopt() error:");
  else
    log_file << "\tSend:    " << temp << endl;
  log_file << "Store socket descriptor:   " << recv_req_sd << endl;
  log_file << "Restore socket descriptor: " << xmit_req_sd << endl;
  log_file << "max_req_fdp1:              " << max_req_fdp1 << endl;
#endif
}


Server::Server()
{
  char* temp;

  temp = gethostaddr();
  bzero((char*)&server_addr, sizeof(struct sockaddr_in));
  memcpy((char*)&server_addr.sin_addr.s_addr, temp, sizeof(struct in_addr));
  server_addr.sin_family = AF_INET;
  recv_req_sd = -1;
  xmit_req_sd = -1;
  num_recv_xfers = 0;
  num_xmit_xfers = 0;
  max_transfers = 0;
  max_recv_xfers = 0;
  max_xmit_xfers = 0;
  max_req_fdp1 = -1;
  sigchild.sa_handler = (SIG_HANDLER) SigChildHandler;
  sigemptyset(&sigchild.sa_mask);
  sigchild.sa_flags = 0;
  if (sigaction(SIGCHLD, &sigchild, NULL) < 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot install signal handler" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(SIGNAL_HANDLER_ERROR);
    }
}


Server::~Server()
{
  close(recv_req_sd);
  close(xmit_req_sd);
}


void Server::Init(int max_xfers,
		  int max_recv,
		  int max_send)
{
  ifstream test;

  max_transfers = max_xfers;
  max_recv_xfers = max_recv;
  max_xmit_xfers = max_send;
  start_interval_time = time(NULL);
  test.open(LOG_FILE);
  if (!test.fail())
    {
      test.close();

      // If the rename fails, do not worry about it
      rename(LOG_FILE, OLD_LOG_FILE);
    }
  log_file.open(LOG_FILE);
  if (log_file.fail())
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot open log file \"" << LOG_FILE << "\"" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(LOG_FILE_OPEN_ERROR);
    }
  SetupReqPorts();
//  file_statistics.store();
}


void Server::StoreCheckpointFile(char* pathname,
				 int   size,
				 int   data_conn_sd)
{
  struct sockaddr_in chkpt_addr;
  int                chkpt_addr_len;
  int                bytes_read;
  char               buffer[DATA_BUFFER_SIZE];
  int                bytes_remaining;
  ofstream           chkpt_file;
  int                xfer_sd;
  int                bytes_recvd=0;

  chkpt_file.open(pathname);
  if (chkpt_file.fail())
    exit(CHILDTERM_CANNOT_CREATE_CHKPT_FILE);
  bytes_remaining = size;
  if (bytes_remaining < 0)
    exit(CHILDTERM_BAD_FILE_SIZE);
  chkpt_addr_len = sizeof(struct sockaddr_in);
  xfer_sd = I_accept(data_conn_sd, &chkpt_addr, &chkpt_addr_len);
  close(data_conn_sd);
  while (bytes_remaining > 0)
    {
      bytes_read = read(xfer_sd, buffer, DATA_BUFFER_SIZE);
      if (bytes_read <= 0)
	{
	  chkpt_file.close();
	  exit(CHILDTERM_EXEC_KILLED_SOCKET);
	}
      chkpt_file.write(buffer, bytes_read);
      if (chkpt_file.fail())
	{
	  chkpt_file.close();
	  exit(CHILDTERM_ERROR_ON_CHKPT_FILE_WRITE);
	}
#ifdef DEBUG
//cout << "Wrote " << bytes_read << " bytes" << endl;
#endif
      bytes_remaining -= bytes_read;
      bytes_recvd += bytes_read;
      if (bytes_remaining < 0)
	exit(CHILDTERM_WRONG_FILE_SIZE);
    }
  if (bytes_remaining != 0)
    exit(CHILDTERM_BAD_FILE_SIZE);
  bytes_recvd = htonl(bytes_recvd);

  // If executing maching already closed socket, then okay (i.e., do not need 
  //   to check return code from the write() call
  net_write(xfer_sd, (char*)&bytes_recvd, sizeof(bytes_recvd));
  close(xfer_sd);
  chkpt_file.close();
}


void Server::RestoreCheckpointFile(char* pathname,
				   int   size,
				   int   data_conn_sd)
{
  struct sockaddr_in chkpt_addr;
  int                chkpt_addr_len;
  int                bytes_to_read=DATA_BUFFER_SIZE;
  int                bytes_read;
  int                bytes_sent=0;
  ifstream           chkpt_file;
  char               buffer[DATA_BUFFER_SIZE];
  int                xfer_sd;
  int                bytes_recvd;

  chkpt_file.open(pathname);
  if (chkpt_file.fail())
    exit(CHILDTERM_CANNOT_OPEN_CHKPT_FILE);
  chkpt_addr_len = sizeof(struct sockaddr_in);
  xfer_sd = I_accept(data_conn_sd, &chkpt_addr, &chkpt_addr_len);
  close(data_conn_sd);
  while (bytes_sent != size)
    {
      chkpt_file.read(buffer, bytes_to_read);
      bytes_read=chkpt_file.gcount();
      if (chkpt_file.fail())
	{
	  chkpt_file.close();
	  exit(CHILDTERM_ERROR_ON_CHKPT_FILE_READ);
	}
      bytes_sent += bytes_read;
      if (bytes_sent > size)
	{
	  chkpt_file.close();
	  exit(CHILDTERM_BAD_FILE_SIZE);
	}
      if (net_write(xfer_sd, buffer, bytes_read) == -1)
	{
	  chkpt_file.close();
	  exit(CHILDTERM_EXEC_KILLED_SOCKET);
	}
      if (size-bytes_sent < DATA_BUFFER_SIZE)
	bytes_to_read = size-bytes_sent;
    }
  if (bytes_sent != size)
    exit(CHILDTERM_BAD_FILE_SIZE);

  if (read(xfer_sd, (char*)&bytes_recvd, sizeof(bytes_recvd)) == 
      sizeof(bytes_recvd))
    {
      bytes_recvd = ntohl(bytes_recvd);
      if (bytes_recvd != bytes_sent)
	exit(CHILDTERM_BAD_FILE_SIZE);
    }
  close(xfer_sd);
  chkpt_file.close();
}


void Server::ProcessStoreReq()
{
  int                len;
  int                ret_code;
  struct sockaddr_in shadow_addr;
  int                shadow_addr_len;
  char*              temp_name;
  char               shadow_name[100];
  fd_set             req_sd_set;
  int                req_sd;
  int                child_pid;
  int                version;
  char               pathname[MAX_PATHNAME_LENGTH];
  int                data_conn_sd;
  recv_req_pkt       recv_req_info;
  recv_reply_pkt     recv_reply_info;
  int                buf_size=DATA_BUFFER_SIZE;

  shadow_addr_len = sizeof(struct sockaddr_in);
  req_sd = I_accept(recv_req_sd, &shadow_addr, &shadow_addr_len);
  BlockSigChild();
  temp_name = gethostnamebyaddr(&shadow_addr.sin_addr);
  strcpy(shadow_name, temp_name);
  log_file << "Received request to store a checkpoint file from shadow \""
           << shadow_name << "\"" << endl;;
  UnblockSigChild();

  /* Wait for the request packet to come through.  To prevent a shadow from
       making the server wait indefinitely, the server simply waits for
       a set amount of time.  If the entire request packet has not been 
       received, then the request socket is killed.                          */
  req_pkt_timeout.tv_sec = 0;
  req_pkt_timeout.tv_usec = 500000;
  while ((ret_code=select(0, NULL, NULL, NULL, &req_pkt_timeout)) < 0)
    if (ret_code != 0)
      if (errno != EINTR)
	{
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: cannot use select for delay" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  exit(SELECT_ERROR);
	}
  if ((num_recv_xfers == max_recv_xfers) || (num_recv_xfers+num_xmit_xfers
					     == max_transfers))
    {
      recv_reply_info.server_name.s_addr = htonl(0);
      recv_reply_info.port = htons(0);
      recv_reply_info.req_status = htons(INSUFFICIENT_BANDWIDTH);
      
      // Does not matter if shadow is listening to reply packet
      net_write(req_sd, (char*)&recv_reply_info, sizeof(recv_reply_pkt));
      close(req_sd);
      BlockSigChild();
      log_file << "Request to store from shadow \"" << shadow_name 
	       << "\" rejected due to" << endl 
	       << "\tinsufficient network bandwidth" << endl;
      UnblockSigChild();
    }
  else
    {
      req_pkt_timeout.tv_sec = 0;
      req_pkt_timeout.tv_usec = 0;
      FD_ZERO(&req_sd_set);
      FD_SET(req_sd, &req_sd_set);
      while ((ret_code=select(req_sd+1, &req_sd_set, NULL, NULL, 
			      &req_pkt_timeout)) < 0)
	if (errno != EINTR)
	  {
	    cerr << endl << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR: cannot select from store request port" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    exit(SELECT_ERROR);
	  }
      switch (ret_code)
	{
	case 0: 
	  recv_reply_info.server_name.s_addr = htonl(0);
	  recv_reply_info.port = htons(0);
	  recv_reply_info.req_status = htons(NO_REQ_RECVD);
	  
	  // Does not matter if shadow is listening to reply packet
	  net_write(req_sd, (char*)&recv_reply_info, sizeof(recv_reply_pkt));
	  close(req_sd);
	  BlockSigChild();
	  log_file << "Request to store from shadow \"" << shadow_name 
	           << "\" rejected; no request packet" << endl;
	  UnblockSigChild();
	  break;
	case 1:
	  len = read(req_sd, (char*)&recv_req_info, sizeof(recv_req_info));
	  if (len != sizeof(recv_req_info))
	    {
	      recv_reply_info.server_name.s_addr = htonl(0);
	      recv_reply_info.port = htons(0);
	      recv_reply_info.req_status = htons(PARTIAL_REQ_RECVD);

	      // Does not matter if shadow is listening to reply packet
	      net_write(req_sd, (char*)&recv_reply_info, 
			sizeof(recv_reply_pkt));
	      close(req_sd);
	      BlockSigChild();
	      log_file << "Request to store from shadow \"" << shadow_name 
		       << "\" rejected; partial" << endl
		       << "\trequest packet received before timeout" << endl;
	      UnblockSigChild();
	    }
	  else
	    {
	      if (ntohl(recv_req_info.ticket) != AUTHENTICATION_TCKT)
		{
		  recv_reply_info.server_name.s_addr = htonl(0);
		  recv_reply_info.port = htons(0);
		  recv_reply_info.req_status = 
		                   htons(BAD_AUTHENTICATION_TICKET);
		  
		  // Does not matter if shadow is listening to reply packet
		  net_write(req_sd, (char*)&recv_reply_info, 
			    sizeof(recv_reply_pkt));
		  close(req_sd);
		  BlockSigChild();
		  log_file << "Request to store from shadow \"" << shadow_name 
		           << "\" rejected; bad" << endl
		           << "\tauthentication ticket" << endl;
		  UnblockSigChild();
		}
	      data_conn_sd = I_socket(XFER_BAD_SOCKET);
	      server_addr.sin_port = htons(0);
	      I_bind(data_conn_sd, &server_addr, XFER_CANNOT_BIND);
	      I_listen(data_conn_sd, 1);
	      if (setsockopt(data_conn_sd, SOL_SOCKET, SO_RCVBUF, 
			     (char*)&buf_size, sizeof(buf_size)) < 0)
		{
		  BlockSigChild();
		  log_file << "Unable to adjust the size of the receiving "
		           << "buffer" << endl;
		  UnblockSigChild();
		}
	      memcpy((char*)&recv_reply_info.server_name.s_addr, 
		     (char*)&server_addr.sin_addr.s_addr,
		     sizeof(struct in_addr));

	      // From the bind() call in I_bind(), the port should already
	      //   be in network byte order
	      recv_reply_info.port = server_addr.sin_port;
	      recv_reply_info.req_status = CKPT_OK;
	      if (net_write(req_sd, (char*)&recv_reply_info, 
			    sizeof(recv_reply_pkt)) < 0)
		{
		  close(req_sd);
		  BlockSigChild();
		  log_file << "Request to store from shadow \"" << shadow_name 
		           << "\" rejected; cannot" << endl
		           << "\tsend IP/port to shadow" << endl;
		  UnblockSigChild();
		  close(data_conn_sd);
		}
	      else
		{
		  close(req_sd);
		  StripFilename(recv_req_info.filename);
		  BlockSigChild();
		  child_pid = fork();
		  if (child_pid < 0)
		    {
		      cerr << endl << "ERROR:" << endl;
		      cerr << "ERROR:" << endl;
		      cerr << "ERROR: cannot fork server process"
			   << endl;
		      cerr << "ERROR:" << endl;
		      cerr << "ERROR:" << endl;
		      exit(FORK_ERROR);
		    }
		  else if (child_pid != 0)     // Parent process (master)
		    {
		      close(data_conn_sd);
		      num_recv_xfers++;
		      transfers.Insert(child_pid, file_statistics, 
				       &shadow_addr.sin_addr, 
				       recv_req_info.filename, 
				       recv_req_info.owner, 
				       ntohl(recv_req_info.file_size),
				       recv_req_info.priority,
				       RECV);
		      log_file << "Request to store from shadow \"" 
			       << shadow_name << "\" has been accepted (" 
			       << child_pid << ')' << endl;
#ifdef DEBUG
log_file << "\tRequest included: machine:  " << shadow_name << endl
         << "\t                  owner:    " << recv_req_info.owner << endl
         << "\t                  filename: " << recv_req_info.filename << endl;
#endif
		    }
		  else                        // Child process
		    {
		      log_file.close();
		      version = file_statistics.GetVersion(shadow_name,
						     recv_req_info.owner,
						     recv_req_info.filename);
		      sprintf(pathname, "/tmp/%s/%s/%s.%d",
			      shadow_name, recv_req_info.owner,
			      recv_req_info.filename, version+1);
		      StoreCheckpointFile(pathname, 
					  ntohl(recv_req_info.file_size),
					  data_conn_sd);
		      exit(CHILDTERM_SUCCESS);
		    }
		  UnblockSigChild();
		}
	    }
	  break;
	default:
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: select returns bad value (" << ret_code << ')'
	    << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  exit(SELECT_ERROR);
	}
    }
}


void Server::ProcessRestoreReq()
{
  int                len;
  int                ret_code;
  struct sockaddr_in shadow_addr;
  int                shadow_addr_len;
  char*              shadow_name;
  fd_set             req_sd_set;
  int                req_sd;
  int                child_pid;
  int                version;
  char               pathname[MAX_PATHNAME_LENGTH];
  int                data_conn_sd;
  xmit_req_pkt       xmit_req_info;
  xmit_reply_pkt     xmit_reply_info;
  int                buf_size=DATA_BUFFER_SIZE;
  struct stat        chkpt_file_status;

  shadow_addr_len = sizeof(struct sockaddr_in);
  req_sd = I_accept(xmit_req_sd, &shadow_addr, &shadow_addr_len);
  BlockSigChild();
  shadow_name = gethostnamebyaddr(&shadow_addr.sin_addr);
  log_file << "Received request to restore a checkpoint file from shadow \""
           << shadow_name << "\"" << endl;;
  UnblockSigChild();

  /* Wait for the request packet to come through.  To prevent a shadow from
       making the server wait indefinitely, the server simply waits for
       a set amount of time.  If the entire request packet has not been 
       received, then the request socket is killed.                          */
  req_pkt_timeout.tv_sec = 0;
  req_pkt_timeout.tv_usec = 500000;
  while ((ret_code=select(0, NULL, NULL, NULL, &req_pkt_timeout)) < 0)
    if (ret_code != 0)
      if (errno != EINTR)
	{
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: cannot use select for delay" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  exit(SELECT_ERROR);
	}
  if ((num_xmit_xfers == max_xmit_xfers) || (num_recv_xfers+num_xmit_xfers
					     == max_transfers))
    {
      xmit_reply_info.server_name.s_addr = htonl(0);
      xmit_reply_info.port = htons(0);
      xmit_reply_info.file_size = htonl(0);
      xmit_reply_info.req_status = htons(INSUFFICIENT_BANDWIDTH);
      
      // Does not matter if shadow is listening to reply packet
      net_write(req_sd, (char*)&xmit_reply_info, sizeof(xmit_reply_pkt));
      close(req_sd);
      BlockSigChild();
      log_file << "Request to store from shadow \"" << shadow_name 
	       << "\" rejected due to" << endl 
	       << "\tinsufficient network bandwidth" << endl;
      UnblockSigChild();
    }
  else
    {
      req_pkt_timeout.tv_sec = 0;
      req_pkt_timeout.tv_usec = 0;
      FD_ZERO(&req_sd_set);
      FD_SET(req_sd, &req_sd_set);
      while ((ret_code=select(req_sd+1, &req_sd_set, NULL, NULL, 
			      &req_pkt_timeout)) < 0)
	if (errno != EINTR)
	  {
	    cerr << endl << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR: cannot select from store request port" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    exit(SELECT_ERROR);
	  }
      switch (ret_code)
	{
	case 0: 
	  xmit_reply_info.server_name.s_addr = htonl(0);
	  xmit_reply_info.port = htons(0);
	  xmit_reply_info.file_size = htonl(0);
	  xmit_reply_info.req_status = htons(NO_REQ_RECVD);
	  
	  // Does not matter if shadow is listening to reply packet
	  net_write(req_sd, (char*)&xmit_reply_info, sizeof(xmit_reply_pkt));
	  close(req_sd);
	  BlockSigChild();
	  log_file << "Request to restore from shadow \"" << shadow_name 
	           << "\" rejected; no request packet" << endl;
	  UnblockSigChild();
	  break;
	case 1:
	  len = read(req_sd, (char*)&xmit_req_info, sizeof(xmit_req_info));
	  if (len != sizeof(xmit_req_info))
	    {
	      xmit_reply_info.server_name.s_addr = htonl(0);
	      xmit_reply_info.port = htons(0);
	      xmit_reply_info.file_size = htonl(0);
	      xmit_reply_info.req_status = htons(PARTIAL_REQ_RECVD);
	  
	      // Does not matter if shadow is listening to reply packet
	      net_write(req_sd, (char*)&xmit_reply_info, 
			sizeof(xmit_reply_pkt));
	      close(req_sd);
	      BlockSigChild();
	      log_file << "Request to restore from shadow \"" << shadow_name 
		       << "\" rejected; partial" << endl
		       << "\trequest packet received before timeout" << endl;
	      UnblockSigChild();
	    }
	  else
	    {
	      if (ntohl(xmit_req_info.ticket) != AUTHENTICATION_TCKT)
		{
		  xmit_reply_info.server_name.s_addr = htonl(0);
		  xmit_reply_info.port = htons(0);
		  xmit_reply_info.file_size = htonl(0);
		  xmit_reply_info.req_status = 
		                           htons(BAD_AUTHENTICATION_TICKET);
		  
		  // Does not matter if shadow is listening to reply packet
		  net_write(req_sd, (char*)&xmit_reply_info, 
			    sizeof(xmit_reply_pkt));
		  close(req_sd);
		  BlockSigChild();
		  log_file << "Request to restore from shadow \"" 
		           << shadow_name << "\" rejected; bad" << endl
		           << "\tauthentication ticket" << endl;
		  UnblockSigChild();
		}
	      StripFilename(xmit_req_info.filename);
	      BlockSigChild();
	      version = file_statistics.FillPathname(pathname, shadow_name, 
						     xmit_req_info.owner,
						     xmit_req_info.filename);
	      UnblockSigChild();
	      if (version <= 0)
		{
		  xmit_reply_info.server_name.s_addr = htonl(0);
		  xmit_reply_info.port = htons(0);
		  xmit_reply_info.file_size = htonl(0);
		  xmit_reply_info.req_status = htons(DESIRED_FILE_NOT_FOUND);
		  
		  // Does not matter if shadow is listening to reply packet
		  net_write(req_sd, (char*)&xmit_reply_info, 
			    sizeof(xmit_reply_pkt));
		  close(req_sd);
		  BlockSigChild();
		  log_file << "Request to restore from shadow \"" 
		           << shadow_name << "\" rejected;" << endl
			   << "\tnon-existent checkpoint file" << endl;
		  UnblockSigChild();
		}
	      else if (stat(pathname, &chkpt_file_status) != 0)
		{
		  xmit_reply_info.server_name.s_addr = htonl(0);
		  xmit_reply_info.port = htons(0);
		  xmit_reply_info.file_size = htonl(0);
		  xmit_reply_info.req_status = htons(DESIRED_FILE_NOT_FOUND);
		  
		  // Does not matter if shadow is listening to reply packet
		  net_write(req_sd, (char*)&xmit_reply_info, 
			    sizeof(xmit_reply_pkt));
		  close(req_sd);
		  BlockSigChild();
		  log_file << "Request to restore from shadow \"" 
		           << shadow_name << "\" rejected; unable" << endl
			   << "\tto access specified file" << endl;
		  UnblockSigChild();
		}
	      else
		{
		  data_conn_sd = I_socket(XFER_BAD_SOCKET);
		  server_addr.sin_port = htons(0);
		  I_bind(data_conn_sd, &server_addr, XFER_CANNOT_BIND);
		  I_listen(data_conn_sd, 1);
		  if (setsockopt(data_conn_sd, SOL_SOCKET, SO_SNDBUF, 
				 (char*)&buf_size, sizeof(buf_size)) < 0)
		    {
		      BlockSigChild();
		      log_file << "Unable to adjust the size of the sending "
			       << "buffer" << endl;
		      UnblockSigChild();
		    }
		  memcpy((char*)&xmit_reply_info.server_name.s_addr, 
			 (char*)&server_addr.sin_addr.s_addr,
			 sizeof(struct in_addr));
		  
		  // From the bind() call in I_bind(), the port should already
		  //   be in network byte order
		  xmit_reply_info.port = server_addr.sin_port;
		  xmit_reply_info.req_status = htons(CKPT_OK);
		  xmit_reply_info.file_size = htonl(chkpt_file_status.st_size);
		  if (net_write(req_sd, (char*)&xmit_reply_info, 
				sizeof(xmit_reply_pkt)) < 0)
		    {
		      close(req_sd);
		      BlockSigChild();
		      log_file << "Request to restore from shadow \"" 
		               << shadow_name << "\" rejected; cannot" << endl
		               << "\tsend IP/port & file size to shadow" 
			       << endl;
		      UnblockSigChild();
		      close(data_conn_sd);
		    }
		  else
		    {
		      close(req_sd);
		      child_pid = fork();
		      if (child_pid < 0)
			{
			  cerr << endl << "ERROR:" << endl;
			  cerr << "ERROR:" << endl;
			  cerr << "ERROR: cannot fork server process"
			       << endl;
			  cerr << "ERROR:" << endl;
			  cerr << "ERROR:" << endl;
			  exit(FORK_ERROR);
			}
		      else if (child_pid != 0)     // Parent process (master)
			{
			  close(data_conn_sd);
			  BlockSigChild();
			  num_xmit_xfers++;
			  transfers.Insert(child_pid, file_statistics, 
					   &shadow_addr.sin_addr, 
					   xmit_req_info.filename, 
					   xmit_req_info.owner, 
					   chkpt_file_status.st_size,
					   xmit_req_info.priority,
					   XMIT);
			  log_file << "Request to restore from shadow \"" 
			           << shadow_name << "\" has been accepted"
				   <<  '(' << child_pid << ')' << endl;
#ifdef DEBUG
log_file << "\tRequest included: machine:  " << shadow_name << endl
         << "\t                  owner:    " << xmit_req_info.owner << endl
         << "\t                  filename: " << xmit_req_info.filename << endl;
#endif
			  UnblockSigChild();
			}
		      else                        // Child process
			{
			  log_file.close();
			  BlockSigChild();
			  RestoreCheckpointFile(pathname, 
						chkpt_file_status.st_size, 
						data_conn_sd);
			  UnblockSigChild();
			  exit(CHILDTERM_SUCCESS);
			}
		    }
		}
	    }
	  break;
	default:
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: select returns bad value (" << ret_code << ')'
	    << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  exit(SELECT_ERROR);
	}
    }
}


void Server::StripFilename(char* pathname)
{
  int s_index, len, count;

  len = strlen(pathname);
  s_index = len-1;
  while ((s_index >= 0) && (pathname[s_index] != '/'))
    s_index--;
  if (s_index >= 0)
    {
      s_index++;
      count = 0;
      while (pathname[s_index] != '\0')
	pathname[count++] = pathname[s_index++];
      pathname[count] = '\0';
    }
}


void Server::Execute()
{
  int    num_ready;
  time_t current_time;

  while (1)
    {
      FD_ZERO(&fd_req_sds);
      FD_SET(recv_req_sd, &fd_req_sds);
      FD_SET(xmit_req_sd, &fd_req_sds);  
      while ((num_ready=select(max_req_fdp1, &fd_req_sds, NULL, NULL, NULL)) 
	     > 0)
	{
	  if (FD_ISSET(recv_req_sd, &fd_req_sds))
	    ProcessStoreReq();
	  if (FD_ISSET(xmit_req_sd, &fd_req_sds))
	    ProcessRestoreReq();
	  FD_ZERO(&fd_req_sds);
	  FD_SET(recv_req_sd, &fd_req_sds);
	  FD_SET(xmit_req_sd, &fd_req_sds);  
	}
      if (num_ready < 0)
	if (errno != EINTR)
	  {
	    cerr << endl << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR: cannot select from request ports" << endl;
	    cerr << "ERROR:" << endl;
	    cerr << "ERROR:" << endl;
	    exit(SELECT_ERROR);
	  }
      current_time = time(NULL);
      if (current_time-start_interval_time >= RECLAIM_INTERVAL)
	{
	  BlockSigChild();
	  transfers.Reclaim(current_time);
	  UnblockSigChild();
	  start_interval_time = time(NULL);
	}
    }
}


void BlockSigChild(void)
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot get current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigaddset(&mask, SIGCHLD) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot add SIG_CHLD to current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot set signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(SIGNAL_MASK_ERROR);
    }
}


void UnblockSigChild(void)
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot get current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigdelset(&mask, SIGCHLD) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot add SIG_CHLD to current signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      cerr << endl << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: cannot set signal mask" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      exit(SIGNAL_MASK_ERROR);
    }  
}


void SigChildHandler(int)
{
  int   exit_status;
  int   exit_code;
  int   ds_code;
  int   fs_code;
  int   xfer_type;
  pid_t child_pid;

  BlockSigChild();
  while ((child_pid=waitpid(-1, &exit_status, WNOHANG)) > 0)
    {
#ifdef DEBUG
      transfers.Dump();
#endif
      if (WIFEXITED(exit_status))
	exit_code = WEXITSTATUS(exit_status);
      else
	exit_code = CHILDTERM_KILLED;
      xfer_type = transfers.GetXferType(child_pid);
      if (xfer_type == BAD_CHILD_PID)
	{	
	  log_file << "ERROR: cannot resolve child pid (bad data structure)"
	           << endl;
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: cannot resolve child pid (#" << child_pid << ')'
	       << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  exit(BAD_TRANSFER_LIST);
	}
      else if ((xfer_type != RECV) && (xfer_type != XMIT))
	{
	  cerr << endl << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR: unrecognized return code from GetXferType()" << endl;
	  cerr << "ERROR:" << endl;
	  cerr << "ERROR:" << endl;
	  exit(BAD_RETURN_CODE);
	}
      else
	{
	  if (exit_code == CHILDTERM_SUCCESS)
	    log_file << "Child #" << child_pid 
	             << " has successfully completed:" << endl;
	  else if (exit_code == CHILDTERM_KILLED)
	    log_file << "Child #" << child_pid << " was killed by the "
	             << "reclaiming routine:" << endl;
	  else
	    log_file << "Child #" << child_pid 
	             << " has terminated abnormally ("
	             << exit_code << "):" << endl;
	  ds_code = transfers.Delete(child_pid, exit_code, file_statistics, 
				     fs_code);
	  if (xfer_type == RECV)
	    {
	      num_recv_xfers--;
	      switch (ds_code)
		{
		case CKPT_OK:
		  if (exit_code != CHILDTERM_SUCCESS)
		    log_file << "\tsuccessfully removed traces of received "
		             << "file"  << endl;
		  else
		    log_file << "\tsuccessful reception of file" << endl;
		  break;
		case BAD_CHILD_PID:
		  log_file << "ERROR: cannot resolve child pid (bad data "
		           << "structure)" << endl;
		  cerr << endl << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR: cannot resolve child pid (#" << child_pid 
		       << ')' << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  exit(BAD_TRANSFER_LIST);
		case CANNOT_DELETE_FILE:
		  log_file << "unable to delete partial file of a bad transfer"
		           << endl;
		  break;
		default:
		  log_file << "\tunrecognized return code from Delete() ("
		           << ds_code << ')' << endl;
		  cerr << endl << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR: unrecognized return code from Delete() ("
		       << ds_code << ')' << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  exit(BAD_RETURN_CODE);
		}
	    }
	  else
	    {
	      num_xmit_xfers--;
	      switch (ds_code)
		{
		case CKPT_OK:
		  if (exit_code != CHILDTERM_SUCCESS)
		    log_file << "\tfile transmission aborted" << endl;
		  else
		    log_file << "\tsuccessful transmission of file" << endl;
		  break;
		case BAD_CHILD_PID:
		  log_file << "ERROR: cannot resolve child pid (bad data "
		           << "structure)" << endl;
		  cerr << endl << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR: cannot resolve child pid (#" << child_pid 
		       << ')' << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  exit(BAD_TRANSFER_LIST);
		default:
		  log_file << "\tunrecognized return code from Delete() ("
		           << ds_code << ')' << endl;
		  cerr << endl << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR: unrecognized return code from Delete() ("
		       << ds_code << ')' << endl;
		  cerr << "ERROR:" << endl;
		  cerr << "ERROR:" << endl;
		  exit(BAD_RETURN_CODE);
		}
	    }
	  if (fs_code != CKPT_OK)
	    {
	      log_file << "\t-> In addition, while updating, errors were "
	               << "found:" << endl;
	      switch (fs_code)
		{
		case CANNOT_DELETE_FILE:
		  log_file << "\t\tunable to delete an old checkpoint file" 
		           << endl;
		  break;
		case CANNOT_DELETE_DIRECTORY:
		  log_file << "\t\tunable to delete a directory" << endl;
		  break;
		default:
		  log_file << "\t\tunexpected return code (" << fs_code 
		           << "); signal handler terminating program" << endl;
		  exit(BAD_RETURN_CODE);
		}
	    }
	}
    }
  UnblockSigChild();
}


int main(int   argc,
	 char* argv[])
{
  int    max_xfers=1;
  int    max_recv_xfers=1;
  int    max_xmit_xfers=1;

  if ((argc != 2) && (argc != 4))
    {
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: incorrect number of parameters" << endl;
      cerr << "\tUsage: " << argv[0] << " " 
	<< "<MAX_TRANSFERS> {<MAX_RECEIVE> <MAX_TRANSMIT>}" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      return(BAD_PARAMETERS);
    }
  max_xfers = atoi(argv[1]);
  if (argc == 2)
    {
      max_recv_xfers = max_xfers;
      max_xmit_xfers = max_xfers;
    }
  else
    {
      max_recv_xfers = atoi(argv[2]);
      max_xmit_xfers = atoi(argv[3]);
    }
  if (max_xfers <= 0)
    {
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: the maximum number of (concurrent) transfers must be"
	   << " greater than 0" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      return(-1);
    }
  if (max_recv_xfers > max_xfers)
    {
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: the maximum number receiving transfers is greater than "
	   << "the total maximum" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      return(-1);
    }
  if (max_xmit_xfers > max_xfers)
    {
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR: the maximum number transmitting transfers is greater "
	   << "than the total maximum" << endl;
      cerr << "ERROR:" << endl;
      cerr << "ERROR:" << endl;
      return(-1);
    }
  server.Init(max_xfers, max_recv_xfers, max_xmit_xfers);
  server.Execute();
  return(0);
}











