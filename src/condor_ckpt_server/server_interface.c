#include "condor_common.h"
#include "condor_debug.h"
#include "server_interface.h"
#if 0
#include "../server2/constants2.h"
#include "../server2/network2.h"
#else
#include "constants2.h"
#include "network2.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#if !defined(WIN32)
#include <unistd.h>
#endif
#include <stdio.h>

#if !defined(TRUE)
#define TRUE (1)
#endif

#if !defined(FALSE)
#define FALSE (0)
#endif

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
	
	conn_req_sd = I_socket();
	if (conn_req_sd == INSUFFICIENT_RESOURCES) {
		fprintf(stderr, "ERROR:\n");
		fprintf(stderr, "ERROR:\n");
		fprintf(stderr, "ERROR: insufficient resources for a new socket\n");
		fprintf(stderr, "ERROR:\n");
		fprintf(stderr, "ERROR:\n");
		return INSUFFICIENT_RESOURCES;
    }
	else if (conn_req_sd == CKPT_SERVER_SOCKET_ERROR) {
		fprintf(stderr, "ERROR:\n");
		fprintf(stderr, "ERROR:\n");
		fprintf(stderr, "ERROR: unable to create a new socket\n");
		fprintf(stderr, "ERROR:\n");
		fprintf(stderr, "ERROR:\n");
		return CKPT_SERVER_SOCKET_ERROR;
    }
	memset((char*) &server_sa, 0, (int) sizeof(server_sa));
	server_sa.sin_family = AF_INET;
	server_IP = getserveraddr();
	if (server_IP == 0) {
		close(conn_req_sd);
		return -1;
	}
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
		}
	if (connect(conn_req_sd, (struct sockaddr*) &server_sa, 
				sizeof(server_sa)) < 0)
		return CONNECT_ERROR;
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


int FileExists(const char *filename, const char *owner)
{
	if (IsLocal(filename) == LOCAL) {
		return TRUE;
	}
	
	if (FileOnServer(owner, filename) == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}


int RequestStore(const char*     owner,
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
	
	key = getpid();
	req.file_size = htonl(len);
	req.ticket = htonl(AUTHENTICATION_TCKT);
	req.priority = htonl(0);
	req.time_consumed = htonl(0);
	req.key = htonl(key);
	strncpy(req.owner, owner, MAX_NAME_LENGTH);
	StripPrefix(filename, req.filename);
	server_sd = ConnectToServer(STORE_REQ);
	if (server_sd < 0)
		return server_sd;
	ret_code = net_write(server_sd, (char*) &req, sizeof(req));
	if (ret_code != sizeof(req))
		return CHILDTERM_CANNOT_WRITE;
	while (bytes_recvd != sizeof(reply)) {
		errno = 0;
		bytes_read = read(server_sd, ((char*) &reply)+bytes_recvd, 
						  sizeof(reply)-bytes_recvd);
		assert(bytes_read >= 0);
		if (bytes_read == 0)
			assert(errno == EINTR);
		else
			bytes_recvd += bytes_read;
    }
	close(server_sd);
	server_IP->s_addr = reply.server_name.s_addr;
	*port = reply.port;
	return ntohs(reply.req_status);
}


int RequestRestore(const char*     owner,
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
	
	key = getpid();
	server_sd = ConnectToServer(RESTORE_REQ);
	if (server_sd < 0)
		return server_sd;
	req.ticket = htonl(AUTHENTICATION_TCKT);
	req.priority = htonl(0);
	req.key = htonl(key);
	strncpy(req.owner, owner, MAX_NAME_LENGTH);
	StripPrefix(filename, req.filename);
	assert(net_write(server_sd, (char*) &req, sizeof(req)) == sizeof(req));
	while (bytes_recvd != sizeof(reply))
    {
		errno = 0;
		bytes_read = read(server_sd, ((char*) &reply)+bytes_recvd, 
						  sizeof(reply)-bytes_recvd);
		assert(bytes_read >= 0);
		if (bytes_read == 0)
			assert(errno == EINTR);
		else
			bytes_recvd += bytes_read;
    }
	close(server_sd);
	server_IP->s_addr = reply.server_name.s_addr;
	*port = reply.port;
	*len = ntohl(reply.file_size);
	return ntohs(reply.req_status);
}


int RequestService(const char*     owner,
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
	
	key = getpid();
	server_sd = ConnectToServer(SERVICE_REQ);
	if (server_sd < 0)
		return server_sd;
	req.ticket = htonl(AUTHENTICATION_TCKT);
	req.key = htonl(key);
	req.service = htons((short)type);
	if (owner != NULL)
		strncpy(req.owner_name, owner, MAX_NAME_LENGTH);
	if (filename != NULL)
		StripPrefix(filename, req.file_name);
	if (new_filename != NULL)
		StripPrefix(new_filename, req.new_file_name);
	assert(net_write(server_sd, (char*) &req, sizeof(req)) == sizeof(req));
	while (bytes_recvd != sizeof(reply)) {
		errno = 0;
		bytes_read = read(server_sd, ((char*) &reply)+bytes_recvd, 
						  sizeof(reply)-bytes_recvd);
		assert(bytes_read >= 0);
		if (bytes_read == 0)
			assert(errno == EINTR);
		else
			bytes_recvd += bytes_read;
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
				 const char* filename)
{
	return (RequestService(owner, filename, NULL, SERVICE_EXIST,
						   NULL, NULL, NULL, NULL));
}


int RemoveRemoteFile(const char* owner,
					 const char* filename)
{
	return (RequestService(owner, filename, NULL, SERVICE_DELETE, NULL,
						   NULL, NULL, NULL));
}


int RemoveLocalOrRemoteFile(const char* owner,
							const char* filename)
{
	unlink(filename);
	return (RequestService(owner, filename, NULL, SERVICE_DELETE, NULL,
						   NULL, NULL, NULL));
}


int RenameRemoteFile(const char* owner,
					 const char* filename,
					 const char* new_filename)
{
	return (RequestService(owner, filename, new_filename, SERVICE_RENAME,
						   NULL, NULL, NULL, NULL));
}

extern int ckpt_server_number;

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
	char	ckpt_server_config[30];

	for (i = 0; ; i++) {
		sprintf(ckpt_server_config, "CKPT_SERVER_HOST_%d", i);
		if (param(ckpt_server_config) == 0) {
			break;
		}
	}

	if (i == 0) {
		if (param("CKPT_SERVER_HOST") == 0) {
			i = -1;
		}
	}
	return i;
}
