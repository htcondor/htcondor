/*
#  File:     BPRclient.c
#
#
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#
#
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <globus_gss_assist.h>
#include <globus_gsi_credential.h>
#include <globus_gsi_proxy.h>
#include <globus_gsi_cert_utils.h>
#include "tokens.h"
#include "BPRcomm.h"

char* chop(char* long_id);
char* pre_chop(char* long_id);

int
main(int argc, char **argv)
{
	int fd_socket;
	int server_port;
	struct timeval tv;
	int retcod, status;
	int proxy_file;
	char *proxy_filename;
	char *jobId;
	char *workernode;
	struct stat proxy_stat;
	char buffer[8192];
	char *globus_location;
	int delegation = 0;
	char *the_proxy;
	const char *deleg_prefix="deleg";
	int connection_poll_timeout=3000;
	char *connection_poll_timeout_env;
	struct pollfd connpoll;
	char ainfo_port_string[16];
	struct addrinfo ai_req, *ai_ans, *cur_ans;
	int service_found;

	OM_uint32	major_status;
	OM_uint32	minor_status;
	int		token_status = 0;
	gss_cred_id_t	credential_handle = GSS_C_NO_CREDENTIAL;
	OM_uint32	ret_flags = 0;
	gss_ctx_id_t	context_handle = GSS_C_NO_CONTEXT;

	if (argc < 2)
	{
		printf("%s: missing proxy filename", argv[0]);
		exit(1);
	}
	proxy_filename = argv[1];

	if (argc < 3)
	{
		printf("%s: missing job name", argv[0]);
		exit(1);
	}
	jobId = argv[2];

	if (argc < 4)
	{
		printf("%s: missing worker node", argv[0]);
		exit(1);
	}
	workernode = argv[3];

	if (argc > 4 && 
	    strncasecmp(argv[4],deleg_prefix,strlen(deleg_prefix)) == 0)
	{
		delegation = 1;
	}

	connection_poll_timeout_env = getenv("BPRCLIENT_CONNECTION_POLL_TIMEOUT");
	if (connection_poll_timeout_env != NULL)
		connection_poll_timeout = atoi(connection_poll_timeout_env);

	setenv("X509_USER_PROXY", proxy_filename, 1);

	/* Acquire GSS credential */
	if ((credential_handle = acquire_cred(GSS_C_INITIATE)) == GSS_C_NO_CREDENTIAL)
	{
		printf("%s: unable to acquire credentials", argv[0]);
		exit(2);
	}


	/* Search for the job on the worker node */
	service_found = 0;
	for (server_port = PORT_RANGE_FROM; (service_found==0) && (server_port <= PORT_RANGE_TO); server_port++)
	{
		ai_req.ai_flags = 0;
		ai_req.ai_family = PF_UNSPEC;
		ai_req.ai_socktype = SOCK_STREAM;
		ai_req.ai_protocol = 0; /* Any stream protocol is OK */

		sprintf(ainfo_port_string,"%5d",server_port);

		if (getaddrinfo(workernode, ainfo_port_string, &ai_req, &ai_ans) != 0) {
			printf("%s: unknown host %s", argv[0], workernode);
			exit(4);
		}
		/* Try all found protocols and addresses */
		for (cur_ans = ai_ans; cur_ans != NULL; cur_ans = cur_ans->ai_next)
		{
			/* Create the socket everytime (cannot be reused once closed) */
			if ((fd_socket = socket(cur_ans->ai_family, 
						cur_ans->ai_socktype,
						cur_ans->ai_protocol)) == -1)
			{
				continue;
			}

			if (connect(fd_socket, cur_ans->ai_addr, cur_ans->ai_addrlen) == -1)
			{
				close(fd_socket);
				continue;
			}
	
			connpoll.fd = fd_socket;
			connpoll.events = (POLLIN|POLLPRI);
			connpoll.revents = 0;
			if (poll(&connpoll, 1, connection_poll_timeout) <= 0)
			{
				close(fd_socket);
				continue;
			}
	
			recv(fd_socket, buffer, sizeof(buffer), 0);
	
			/* 
			   Additional part of the requested and received jobids are removed before strcmp:
			   it is assumed that the ramaining part is unique for every batch system.    
			   old version : if (strcmp(buffer, jobId) != 0)
			*/
			if (strcmp(pre_chop(chop(buffer)), pre_chop(chop(jobId))) != 0)
			{
				send(fd_socket, BPR_OP_WRONGJOB, BPR_OPLEN, 0);
				close(fd_socket);
				continue;
			}
			else
			{
				service_found = 1;
				break;
			}
		}
		freeaddrinfo(ai_ans);
	}

	if (service_found == 0)
	{
		printf("%s: job %s not found on node %s", argv[0], jobId, workernode);
		exit(1);
	}
	
	/* Negotiate the sending method (copy vs. delegation) */
	send(fd_socket, delegation ? BPR_OP_WILLDELEGATE : BPR_OP_WILLSEND, BPR_OPLEN, 0);
	recv(fd_socket, buffer, sizeof(buffer), 0);
	if (strcmp(buffer, BPR_OP_ACKNOWLEDGE) != 0)
	{
		printf("%s: unable to negotiate the sending method", argv[0]);
		exit(1);
	}

	if ((context_handle = initiate_context(credential_handle, "GSI-NO-TARGET", fd_socket)) == GSS_C_NO_CONTEXT)
	{
		printf("%s: cannot initiate security context", argv[0]);
		exit(1);
	}

	if (verify_context(context_handle))
	{
		printf("%s: server certificate mismatch", argv[0]);
		close(fd_socket);
		exit(1);
	}

	if (stat(proxy_filename, &proxy_stat) == -1)
	{
		printf("%s: unable to stat proxy file %s", argv[0], proxy_filename);
		close(fd_socket);
		exit(1);
	}

	if (delegation)
	{
		if (delegate_proxy(proxy_filename, credential_handle, context_handle, fd_socket) != BPR_DELEGATE_PROXY_OK)
		{
			printf("%s: unable to delegate proxy file %s", argv[0], proxy_filename);
			close(fd_socket);
			exit(1);
		}
	}
	else /* send the proxy over a GSI encrypted channel */
	{
		if ((proxy_file = open(proxy_filename, O_RDONLY)) == -1)
		{
			printf("%s: unable to open proxy file %s", argv[0], proxy_filename);
			close(fd_socket);
			exit(1);
		}
		
		if ((the_proxy = (char *)malloc(proxy_stat.st_size + 1)) == NULL)
		{
			printf("%s: unable to allocate read buffer (%d bytes)", argv[0], proxy_stat.st_size + 1);
			close(fd_socket);
			exit(1);
		}

		if (read(proxy_file, the_proxy, proxy_stat.st_size) != proxy_stat.st_size)
		{
			printf("%s: unable to read proxy file %s", argv[0], proxy_filename);
			close(proxy_file);
			free(the_proxy);
			close(fd_socket);
			exit(1);
		}
		close(proxy_file);
		the_proxy[proxy_stat.st_size] = 0;

		if (send_proxy(the_proxy, context_handle, fd_socket) != BPR_SEND_PROXY_OK)
		{
			printf("%s: unable to send proxy file %s", argv[0], proxy_filename);
			free(the_proxy);
			close(fd_socket);
			exit(1);
		}
		free(the_proxy);
	}

	close(fd_socket);
	exit(0);
}

/* Elimination of server from jobid */
char*
chop(char* long_id)
{
        char* tmpchop=0;
        char* startstring=long_id;
        int chop_count =0;
        while((*(long_id))!= '.')
        {
                if((*(long_id)) == 0)
                return strdup(startstring);
                else
                {
                        chop_count++;
                        long_id++;
                }
        }
        tmpchop=(char*)malloc(chop_count+1);
        memcpy(tmpchop,startstring,chop_count);
        tmpchop[chop_count]=0;
        return
                tmpchop;
}

/* Elimination of gridtype and logfile from jobid */
char*
pre_chop(char* long_id)
{
        char* startstring=long_id;
        int len=strlen(long_id);
        int count=len-1;
        while(long_id[count]!= '/')
        {
                if(count == 0)
                return startstring;
                else count--;
        }
        return &long_id[++count];
}

