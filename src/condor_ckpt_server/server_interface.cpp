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
#include "condor_config.h"
#include "condor_debug.h"
#include "server_interface.h"
#include "constants2.h"
#include "network2.h"
#include "internet.h"
#include "condor_netdb.h"
#include "condor_sockaddr.h"
#include "ipv6_hostname.h"
#include "subsystem_info.h"
#include "MyString.h"
#include <string.h>
#include <map>
#include "condor_network.h"

using namespace std;

static condor_sockaddr getserveraddr(void);
int get_ckpt_server_count(void);

/* from condor_util_lib/do_connect.c */
//extern "C" {
//int tcp_connect_timeout(int sockfd, struct sockaddr *sinful, int len,
//						int timeout);
//}


/* NOTE: There is no particularly good reason why this codebase is C linkage.
	It just saved me some time one day to keep it that way.
*/


extern "C" void StripPrefix(const char* pathname,
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


extern "C" int ConnectToServer(request_type type)
{
	int                conn_req_sd;
	condor_sockaddr    server_sa;
	condor_sockaddr    server_IP;
	int				   on = 1;
	MyString			str;
	static				map<MyString, time_t> penalty_box;
	map<MyString, time_t>::iterator it;
	time_t				embargo_time;
	time_t				ckpt_server_retry;
	int					ckpt_server_timeout;
	int					ret;
	time_t				now;

	now = time(NULL);

	ckpt_server_timeout = 
		param_integer("CKPT_SERVER_CLIENT_TIMEOUT", 20, 0);

	ckpt_server_retry = 
		param_integer("CKPT_SERVER_CLIENT_TIMEOUT_RETRY", 1200, 0);

	server_IP = getserveraddr();

	if (server_IP == condor_sockaddr::null) {
		return -1;
	}

	/* Get a reasonable name for it. This next line is bad and wrong. */
	str = server_IP.to_ip_string();

	/////////////////////////////////////////////////
	// Check cache to see if this checkpoint server had timed out recently
	/////////////////////////////////////////////////

	if (ckpt_server_timeout != 0) {
		it = penalty_box.find(str);

		if (it != penalty_box.end()) {
			/* This server is embargoed, let's see if we honor it or if it
				has passed the embargoed time.
			*/
			embargo_time = (*it).second;

			if (now < embargo_time) {
				dprintf(D_ALWAYS, "Skipping connection to previously timed out "
					"ckpt server: %s.\n", str.Value());
				return CKPT_SERVER_TIMEOUT;
			}

			dprintf(D_ALWAYS, "Previously timed out ckpt server %s given "
							"reprieve. Trying it again.\n",
							str.Value());

			penalty_box.erase(it);
		}
	} else {
		/* if we want to block while talking to the ckpt server, then clean
			out the hash table since we won't be needing it. If there was
			something in there when the admin changed the timeout value to
			0, then we'll assume the admin knows what he is doing and have
			it take effect immediately. */
		penalty_box.clear();
	}

	/////////////////////////////////////////////////
	// Figure out request type and connect to the ckpt server
	/////////////////////////////////////////////////

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
		dprintf( D_ALWAYS, "ERROR: unable to bind new socket to local "
			"interface\n");
		return CKPT_SERVER_SOCKET_ERROR;
	}

	server_sa = server_IP;
	switch (type) {
        case SERVICE_REQ:
		    server_sa.set_port(CKPT_SVR_SERVICE_REQ_PORT);
			break;
		case STORE_REQ:
			server_sa.set_port(CKPT_SVR_STORE_REQ_PORT);
			break;
		case RESTORE_REQ:
			server_sa.set_port(CKPT_SVR_RESTORE_REQ_PORT);
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

	ret = tcp_connect_timeout(conn_req_sd, server_sa,
							ckpt_server_timeout);
	
	if (ret < 0) {

		/* We don't need the fd anymore. */
		close(conn_req_sd);

		switch(ret) {
			case -2:
				/* Timeout happened while connecting, so put it into the
					hash table with a time into the future that will have 
					to pass before it the connect can be attempted again.
				*/
				
				/* In the clase of timeout == 0, this code path will
					never happen
				*/

				dprintf(D_ALWAYS, "Skipping connect to checkpoint server "
						"%s for %d seconds due to connection timeout.\n",
						str.Value(), (int)ckpt_server_retry);

				embargo_time = now + ckpt_server_retry;

				penalty_box.insert(
					pair<MyString, time_t>(str, embargo_time) );

				return CKPT_SERVER_TIMEOUT;
				break;

			case -1:
				/* Error */
				return CONNECT_ERROR;
				break;

			default:
				EXCEPT("ConnectToServer(): Programmer error with "
					"tcp_connect_timeout!\n");
				break;
		}
	}

	setsockopt(conn_req_sd,SOL_SOCKET,SO_KEEPALIVE,(char*)&on,sizeof(on));
	return conn_req_sd;
}

extern "C" int IsLocal(const char* path)
{
	struct stat file_stat;
	
	errno = 0;
	if (stat(path, &file_stat) == 0)
		return LOCAL;
	else
		return REMOTE;
}


extern "C" int FileExists(const char *filename, const char *owner, const char *schedd)
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

// build a string containing "name@domain" in a fixed size buffer
// truncating the result if necessary, but always returning a zero-terminated
// output buffer.
// return:  true if there was enough room, false if truncation was needed.
//
extern "C" bool BuildOwnerName(
			char * pszOwner,
			size_t cchOwner,
			const char * pszName,
			const char * pszDomain)	 // may be null
{
	if (cchOwner <= 0) 
		return false;

	char * psz = pszOwner;
	char * pszEnd = pszOwner + cchOwner -1; // ptr to last character in the buffer

	// copy the name part
	while (psz <= pszEnd) {
		if (!(*psz = *pszName))
			break;
		++psz, ++pszName;
	}

	// if no domain, we are done, return true if the name fit in the buffer
	if ( ! pszDomain) {
		*pszEnd = 0; // put a null in the last char of the buffer just in case.
		return  (psz <= pszEnd && *psz == 0);
	}

	// if there is room append @domain
	if (psz < pszEnd) {
		*psz++ = '@';
		while (psz <= pszEnd) {
			if (!(*psz = *pszDomain))
				return true; // we copied everything.
			++psz, ++pszDomain;
		}
	}

	// if we get to here, the buffer wasn't big enough
	// so put a null at the very last character of the buffer
	// and return false.
	*pszEnd = 0;
	return false;
}

extern "C" int RequestStore(const char*     owner,
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
#if 1
	BuildOwnerName(req.owner, COUNTOF(req.owner), owner, schedd);
#else
	strncpy(req.owner, owner, MAX_NAME_LENGTH-1);
	if (schedd) {
		int space_left = MAX_NAME_LENGTH-strlen(req.owner)-1;
		if (space_left) {
			strcat(req.owner, "@");
			space_left--;
			strncat(req.owner, schedd, space_left);
		}
	}
#endif
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


extern "C" int RequestRestore(const char*     owner,
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
	/* priority must stay zero for rsteq_is_32bit() and rsteq_is_64bit() 
		to do the right thing for packet type determination. */
	req.priority = htonl(0);
	req.key = htonl(key);
#if 1
	BuildOwnerName(req.owner, COUNTOF(req.owner), owner, schedd); 
#else
	strncpy(req.owner, owner, MAX_NAME_LENGTH-1);
	if (schedd) {
		int space_left = MAX_NAME_LENGTH-strlen(req.owner)-1;
		if (space_left) {
			strcat(req.owner, "@");
			space_left--;
			strncat(req.owner, schedd, space_left);
		}
	}
#endif
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


extern "C" int RequestService(const char*     owner,
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
#if 1
		BuildOwnerName(req.owner_name, COUNTOF(req.owner_name), owner, schedd); 
#else
		strncpy(req.owner_name, owner, MAX_NAME_LENGTH-1);
		if (schedd) {
			int space_left = MAX_NAME_LENGTH-strlen(req.owner_name)-1;
			if (space_left) {
				strcat(req.owner_name, "@");
				space_left--;
				strncat(req.owner_name, schedd, space_left);
			}
		}
#endif
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


extern "C" int FileOnServer(const char* owner,
				 const char* schedd,
				 const char* filename)
{
	return (RequestService(owner, schedd, filename, NULL, SERVICE_EXIST,
						   NULL, NULL, NULL, NULL));
}


extern "C" int RemoveRemoteFile(const char* owner,
					 const char* schedd,
					 const char* filename)
{
	return (RequestService(owner, schedd, filename, NULL, SERVICE_DELETE, NULL,
						   NULL, NULL, NULL));
}


static char *server_host = NULL;

extern "C" int RemoveLocalOrRemoteFile(const char* owner,
							const char* schedd,
							const char* filename)
{
	MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
	unlink(filename);
	if (server_host == NULL) { // NULL means we have no checkpoint server
		return 0;
	}
	return (RequestService(owner, schedd, filename, NULL, SERVICE_DELETE, NULL,
						   NULL, NULL, NULL));
}


extern "C" int RenameRemoteFile(const char* owner,
					 const char* schedd,
					 const char* filename,
					 const char* new_filename)
{
	return (RequestService(owner, schedd, filename, new_filename,
						   SERVICE_RENAME, NULL, NULL, NULL, NULL));
}


extern "C" int SetCkptServerHost(const char *host)
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

static condor_sockaddr getserveraddr()
{
	std::vector<condor_sockaddr> addrs;
	addrs = resolve_hostname(server_host);
	if (addrs.empty()) {
		dprintf(D_ALWAYS,
				"Can't get address for checkpoint server host %s: %s\n",
				(server_host) ? server_host : "(NULL)", strerror(errno));
		return condor_sockaddr::null;
	}
	return addrs.front();
}

static int ckpt_server_number;

extern "C" int
set_ckpt_server_number(int new_server)
{
	int		rval;

	rval = ckpt_server_number;
	ckpt_server_number = new_server;
	return rval;
}


extern "C" int
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
