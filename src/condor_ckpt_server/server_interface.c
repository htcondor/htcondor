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
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "server_interface.h"
#include "constants2.h"
#include "network2.h"
#include "internet.h"
#include <string.h>

static char* getserveraddr();


void StripPrefix(const char* pathname,
				 char        filename[MAX_CONDOR_FILENAME_LENGTH])
{
	int start;
	
	start = strlen(pathname) - 1;
	while ((start >= 0) && (pathname[start] != '/'))
		start--;
	filename[MAX_CONDOR_FILENAME_LENGTH-1] = '\0';
	start++;
	strncpy(filename, (pathname+start), MAX_CONDOR_FILENAME_LENGTH - 1);
}


int ConnectToServer(request_type type)
{
	int                conn_req_sd;
	struct sockaddr_in server_sa;
	char			   *server_IP;
	int				   on = 1;
	
	server_IP = getserveraddr();
	if (server_IP == 0) {
		return -1;
	}

	conn_req_sd = I_socket();
	if (conn_req_sd == INSUFFICIENT_RESOURCES) {
		dprintf( D_ALWAYS, "ERROR: insufficient resources for a new socket\n" );
		return INSUFFICIENT_RESOURCES;
    }
	else if (conn_req_sd == CKPT_SERVER_SOCKET_ERROR) {
		dprintf( D_ALWAYS, "ERROR: unable to create a new socket\n" );
		return CKPT_SERVER_SOCKET_ERROR;
    }

	/* TRUE means this is an outgoing connection */
	if( ! _condor_local_bind(TRUE, conn_req_sd) ) {
		close( conn_req_sd );
		dprintf( D_ALWAYS, "ERROR: unable to bind new socket to local interface\n");
		return CKPT_SERVER_SOCKET_ERROR;
	}

	memset((char*) &server_sa, 0, (int) sizeof(server_sa));
	server_sa.sin_family = AF_INET;
	memcpy((char*) &server_sa.sin_addr.s_addr, (char*) server_IP, 
		   sizeof(struct in_addr));
	switch (type) {
        case SERVICE_REQ:
		    server_sa.sin_port = htons(CKPT_SVR_SERVICE_REQ_PORT);
			break;
		case STORE_REQ:
			server_sa.sin_port = htons(CKPT_SVR_STORE_REQ_PORT);
			break;
		case RESTORE_REQ:
			server_sa.sin_port = htons(CKPT_SVR_RESTORE_REQ_PORT);
			break;
		case REPLICATE_REQ:
			dprintf(D_ALWAYS, "ERROR: REPLICATE_REQ not implemented.");
			close(conn_req_sd);
			return CKPT_SERVER_SOCKET_ERROR;
			break;
		default:
			dprintf(D_ALWAYS, "ERROR: ConnectToServer(): Unknown type!\n");
			close(conn_req_sd);
			return CKPT_SERVER_SOCKET_ERROR;
		}
	if (connect(conn_req_sd, (struct sockaddr*) &server_sa, 
				sizeof(server_sa)) < 0) {
		close(conn_req_sd);
		return CONNECT_ERROR;
	}
	setsockopt(conn_req_sd,SOL_SOCKET,SO_KEEPALIVE,(char*)&on,sizeof(on));
	return conn_req_sd;
}


int IsLocal(const char* path)
{
	struct stat file_stat;
	
	errno = 0;
	if (stat(path, &file_stat) == 0)
		return LOCAL;
	else
		return REMOTE;
}


int FileExists(const char *filename, const char *owner, const char *schedd)
{
	int rval;

	if (IsLocal(filename) == LOCAL) {
		return TRUE;
	}

	rval = FileOnServer(owner, schedd, filename);
	if (rval == 0) {
		return TRUE;
	} else if (rval == DOES_NOT_EXIST) {
		return FALSE;
	} else {
		return -1;
	}
}


int RequestStore(const char*     owner,
				 const char*	 schedd,
				 const char*     filename,
				 size_t          len,
				 struct in_addr* server_IP,
				 u_short*        port)
{
	int             server_sd;
	store_req_pkt   req;
	store_reply_pkt reply;
	u_lint          key;
	int             bytes_read;
	int             bytes_recvd=0;
	int             ret_code;
	
	server_sd = ConnectToServer(STORE_REQ);
	if (server_sd < 0)
		return server_sd;
	key = getpid();
	memset((void *)&req, 0, sizeof(req));
	req.file_size = htonl(len);
	req.ticket = htonl(AUTHENTICATION_TCKT);
	req.priority = htonl(0);
	req.time_consumed = htonl(0);
	req.key = htonl(key);
	strncpy(req.owner, owner, MAX_NAME_LENGTH-1);
	if (schedd) {
		int space_left = MAX_NAME_LENGTH-strlen(req.owner)-1;
		if (space_left) {
			strcat(req.owner, "@");
			space_left--;
			strncat(req.owner, schedd, space_left);
		}
	}
	StripPrefix(filename, req.filename);
	ret_code = net_write(server_sd, (char*) &req, sizeof(req));
	if (ret_code != sizeof(req)) {
		close(server_sd);
		return CHILDTERM_CANNOT_WRITE;
	}
	while (bytes_recvd != sizeof(reply)) {
		errno = 0;
		bytes_read = read(server_sd, ((char*) &reply)+bytes_recvd, 
						  sizeof(reply)-bytes_recvd);

		/* assert(bytes_read >= 0); */
		if ( !(bytes_read >= 0) ) {
			close(server_sd);
			return -1;
		}
			
		if (bytes_read == 0) {
			/* assert(errno == EINTR); */
			if ( !(errno == EINTR) ) {
				close(server_sd);
				return -1;
			}
		} else {
			bytes_recvd += bytes_read;
		}
    }
	close(server_sd);
	server_IP->s_addr = reply.server_name.s_addr;
	*port = reply.port;
	return ntohs(reply.req_status);
}


int RequestRestore(const char*     owner,
				   const char*	   schedd,
				   const char*     filename,
				   size_t*         len,
				   struct in_addr* server_IP,
				   u_short*        port)
{
	int               server_sd;
	restore_req_pkt   req;
	restore_reply_pkt reply;
	u_lint            key;
	int               bytes_read;
	int               bytes_recvd=0;
	
	server_sd = ConnectToServer(RESTORE_REQ);
	if (server_sd < 0)
		return server_sd;
	key = getpid();
	memset((void *)&req, 0, sizeof(req));
	req.ticket = htonl(AUTHENTICATION_TCKT);
	req.priority = htonl(0);
	req.key = htonl(key);
	strncpy(req.owner, owner, MAX_NAME_LENGTH-1);
	if (schedd) {
		int space_left = MAX_NAME_LENGTH-strlen(req.owner)-1;
		if (space_left) {
			strcat(req.owner, "@");
			space_left--;
			strncat(req.owner, schedd, space_left);
		}
	}
	StripPrefix(filename, req.filename);
	/* assert(net_write(server_sd, (char*) &req, sizeof(req)) == sizeof(req)); */
	if ( !(net_write(server_sd, (char*) &req, sizeof(req)) == sizeof(req)) ) {
		close(server_sd);
		return -1;
	}
	while (bytes_recvd != sizeof(reply))
    {
		errno = 0;
		bytes_read = read(server_sd, ((char*) &reply)+bytes_recvd, 
						  sizeof(reply)-bytes_recvd);
		/* assert(bytes_read >= 0); */
		if ( !( bytes_read >= 0 ) ) {
			close(server_sd);
			return -1;
		}
		if (bytes_read == 0) {
			/* assert(errno == EINTR); */
			if ( !( errno == EINTR ) ) {
				close(server_sd);
				return -1;
			}
		} else {
			bytes_recvd += bytes_read;
		}
    }
	close(server_sd);
	server_IP->s_addr = reply.server_name.s_addr;
	*port = reply.port;
	*len = ntohl(reply.file_size);
	return ntohs(reply.req_status);
}


int RequestService(const char*     owner,
				   const char*     schedd,
				   const char*     filename,
				   const char*     new_filename,
				   service_type    type,
				   struct in_addr* server_IP,
				   u_short*        port,
				   u_lint*         num_files,
				   char*           cap_free)
{
	int               server_sd;
	service_req_pkt   req;
	service_reply_pkt reply;
	u_lint            key;
	int               bytes_read;
	int               bytes_recvd=0;
	
	server_sd = ConnectToServer(SERVICE_REQ);
	if (server_sd < 0)
		return server_sd;
	key = getpid();
	memset((void *)&req, 0, sizeof(req));
	req.ticket = htonl(AUTHENTICATION_TCKT);
	req.key = htonl(key);
	req.service = htons((short)type);
	if (owner != NULL) {
		strncpy(req.owner_name, owner, MAX_NAME_LENGTH-1);
		if (schedd) {
			int space_left = MAX_NAME_LENGTH-strlen(req.owner_name)-1;
			if (space_left) {
				strcat(req.owner_name, "@");
				space_left--;
				strncat(req.owner_name, schedd, space_left);
			}
		}
	}
	if (filename != NULL)
		StripPrefix(filename, req.file_name);
	if (new_filename != NULL)
		StripPrefix(new_filename, req.new_file_name);
	/* assert(net_write(server_sd, (char*) &req, sizeof(req)) == sizeof(req)); */
	if ( !( net_write(server_sd, (char*) &req, sizeof(req)) == sizeof(req) )) {
		close(server_sd);
		return -1;
	}
	while (bytes_recvd != sizeof(reply)) {
		errno = 0;
		bytes_read = read(server_sd, ((char*) &reply)+bytes_recvd, 
						  sizeof(reply)-bytes_recvd);
		/* assert(bytes_read >= 0); */
		if ( !( bytes_read >= 0 ) ) {
			close(server_sd);
			return -1;
		}
		if (bytes_read == 0) {
			/* assert(errno == EINTR); */
			if ( !( errno == EINTR ) ) {
				close(server_sd);
				return -1;
			}
		} else {
			bytes_recvd += bytes_read;
		}
    }
	close(server_sd);
	if (server_IP != NULL)
		server_IP->s_addr = reply.server_addr.s_addr;
	if (port != NULL)
		*port = reply.port;
	if (num_files != NULL)
		*num_files = ntohl(reply.num_files);
	if (cap_free != NULL)
		strncpy(cap_free, reply.capacity_free_ACD,
				MAX_ASCII_CODED_DECIMAL_LENGTH);
	return ntohs(reply.req_status);
}


int FileOnServer(const char* owner,
				 const char* schedd,
				 const char* filename)
{
	return (RequestService(owner, schedd, filename, NULL, SERVICE_EXIST,
						   NULL, NULL, NULL, NULL));
}


int RemoveRemoteFile(const char* owner,
					 const char* schedd,
					 const char* filename)
{
	return (RequestService(owner, schedd, filename, NULL, SERVICE_DELETE, NULL,
						   NULL, NULL, NULL));
}


int RemoveLocalOrRemoteFile(const char* owner,
							const char* schedd,
							const char* filename)
{
	unlink(filename);
	return (RequestService(owner, schedd, filename, NULL, SERVICE_DELETE, NULL,
						   NULL, NULL, NULL));
}


int RenameRemoteFile(const char* owner,
					 const char* schedd,
					 const char* filename,
					 const char* new_filename)
{
	return (RequestService(owner, schedd, filename, new_filename,
						   SERVICE_RENAME, NULL, NULL, NULL, NULL));
}

static char *server_host = NULL;

int SetCkptServerHost(const char *host)
{
	if (server_host) free(server_host);
	if (host) {
		server_host = strdup(host);
	} else {
		server_host = NULL;
	}
	return 0;
}

/******************************************************************************
*                                                                             *
*   Function: getserveraddr(void)                                             *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function returns a pointer to the IP address of the Checkpoint       *
*   Server.  The pointer is a character pointer (char*) since the             *
*   gethostbyname() function is used and returns a char** pointer (see        *
*   gethostaddr() above).                                                     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        <none>                                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        char* - a pointer to a static area where the Checkpoint Server's IP  *
*                address is held                                              *
*                                                                             *
******************************************************************************/

static char* getserveraddr()
{
	struct hostent* h = NULL;

	if (server_host == NULL) return NULL;

	h = gethostbyname(server_host);
	if (h == NULL) {
		dprintf(D_ALWAYS,
				"Can't get address for checkpoint server host %s: %s\n",
				(server_host) ? server_host : "(NULL)", strerror(errno));
		return NULL;
	}

	return(h->h_addr);
}

static int ckpt_server_number;

int
set_ckpt_server_number(int new_server)
{
	int		rval;

	rval = ckpt_server_number;
	ckpt_server_number = new_server;
	return rval;
}


int
get_ckpt_server_count()
{
	int		i;
	char	ckpt_server_config[30], *tmp;

	for (i = 0; ; i++) {
		sprintf(ckpt_server_config, "CKPT_SERVER_HOST_%d", i);
		if( (tmp = param(ckpt_server_config)) == 0) {
			break;
		} else {
			free( tmp );
		}
	}

	if (i == 0) {
		if( (tmp = param("CKPT_SERVER_HOST")) == 0) {
			i = -1;
		} else {
			free( tmp );
		}
	}
	return i;
}
