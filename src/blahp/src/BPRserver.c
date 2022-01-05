/*
#  File:     BPRserver.c
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
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/wait.h>
#include <globus_gss_assist.h>
#include <globus_gsi_proxy.h>
#include <globus_gsi_system_config.h>
#include "tokens.h"
#include "BPRcomm.h"

#define DEFAULT_POLL_INTERVAL 60
#define DEFAULT_MIN_PROXY_LIFETIME 180
#define PROXYTMP "tmp_proxy_XXXXXX"

int
main(int argc, char **argv)
{
	int fd_socket, read_socket;
	int listen_port;
	struct timeval tv;
	struct sockaddr_storage discarded_addr;
	int addr_size = sizeof(discarded_addr);
	char *client_name = NULL;
	fd_set readfs, masterfs;
	int retcod;
	int exit_program = 0;
	pid_t monitored_pid;
	char *jobId;
	char *message;
	int proxy_file;
	int poll_interval = 0;
	int min_proxy_lifetime = 0;
	int delegation = 0;
	int error;
	char buffer[BPR_OPLEN];
        char ainfo_port_string[16];
        struct addrinfo ai_req, *ai_ans, *cur_ans;
	int address_found;

	gss_cred_id_t	credential_handle = GSS_C_NO_CREDENTIAL;
	gss_ctx_id_t	context_handle = GSS_C_NO_CONTEXT;
	OM_uint32       major_status = 0, minor_status = 0;

	char *proxy_file_name;
	char *proxy_tmp_name = NULL;
        char *proxy_dir;
        char *proxy_file_name_copy;
	       
	if (globus_gsi_sysconfig_get_proxy_filename_unix(&proxy_file_name, GLOBUS_PROXY_FILE_INPUT) != 0)
	{
		fprintf(stderr, "Cannot find proxy filename\n");
		exit(1);
	}
	fprintf(stderr, "Proxy file: %s\n", proxy_file_name);
	
	/* Get the arguments
	 * ----------------- */
	if (argc < 2)
	{
		fprintf(stderr, "At least process pid is required\n");
		exit(1);
	}
	monitored_pid = atoi(argv[1]);
	if (argc > 2) poll_interval = atoi(argv[2]);
	if (!poll_interval) poll_interval = DEFAULT_POLL_INTERVAL; /* 0 IS NOT ACCEPTABLE */
	if (argc > 3) min_proxy_lifetime = atoi(argv[3]);
	if (!min_proxy_lifetime) min_proxy_lifetime = DEFAULT_MIN_PROXY_LIFETIME;
	if (argc > 4) jobId = argv[4];
	else          exit(3);


	/* Cycle through the port range */
	address_found = 0;
	for (listen_port = PORT_RANGE_FROM; (address_found == 0) && (listen_port <= PORT_RANGE_TO); listen_port++)
	{
                ai_req.ai_flags = AI_PASSIVE;
                ai_req.ai_family = PF_UNSPEC;
                ai_req.ai_socktype = SOCK_STREAM;
                ai_req.ai_protocol = 0; /* Any stream protocol is OK */

                sprintf(ainfo_port_string,"%5d",listen_port);

                if (getaddrinfo(NULL, ainfo_port_string, &ai_req, &ai_ans) != 0) {
                        printf("%s: cannot get address of passive SOCK_STREAM socket.\n", argv[0]);
                        exit(4);
                }
                for (cur_ans = ai_ans; cur_ans != NULL; cur_ans = cur_ans->ai_next)
		{

		 	/* Open the socket */
		 	/* --------------- */
			if ((fd_socket = socket(cur_ans->ai_family,
						cur_ans->ai_socktype,
						cur_ans->ai_protocol)) == -1)
			{
				continue;
			}
			if (bind(fd_socket,cur_ans->ai_addr, cur_ans->ai_addrlen) == 0) 
			{
				address_found = 1;
				break;
			}
			close(fd_socket);
		}
		freeaddrinfo(ai_ans);
	}

	if (address_found == 0)
	{
		fprintf(stderr, "Cannot bind to any port in the given range (%d-%d)\n", PORT_RANGE_FROM, PORT_RANGE_TO);
		exit(1);
	}
	else fprintf(stderr, "Bind on port %d\n", listen_port);

	if (listen(fd_socket,1) == -1)
	{
		fprintf(stderr, "Cannot listen from socket: %s\n", strerror(errno));
		exit(1);
	}


	/* Main loop
	 * --------- */
	while (!exit_program)
	{
		FD_ZERO(&masterfs);
		FD_SET(fd_socket, &masterfs);

		tv.tv_sec = poll_interval;
		tv.tv_usec = 0;
	
		readfs = masterfs;
		if ((retcod = select(FD_SETSIZE, &readfs, (fd_set *)NULL, (fd_set *)NULL, &tv)) == -1)
		{
			perror("Select error");
			close(fd_socket);
			exit(1);
		}
		if (retcod)
		{
		        if (FD_ISSET(fd_socket, &readfs))
		        {
				if ((read_socket = accept(fd_socket, (struct sockaddr *)&discarded_addr, &addr_size)) == -1)
				{
					fprintf(stderr,"\nCannot accept connection: %s\n", strerror(errno));
					exit(1);
				}

				/* Don't forget to terminate the string... */
				send(read_socket, jobId, strlen(jobId)+1, 0);

				
				FD_ZERO(&masterfs);
				FD_SET(read_socket, &masterfs);

				fprintf(stderr, "Negotiating protocol...\n");
				tv.tv_sec = 15;
				tv.tv_usec = 0;
				readfs = masterfs;
				if (retcod = select(FD_SETSIZE, &readfs, (fd_set *)NULL, (fd_set *)NULL, &tv) <= 0)
				{
					fprintf(stderr, "Nothing received.\n");
					fprintf(stderr, "Listening for further connections...\n");
					close(read_socket);
					continue;
				}

				/* Negotiate the receiving method (copy vs. delegation) */
				delegation = -1;
				if (recv(read_socket, buffer, sizeof(buffer), MSG_NOSIGNAL) > 0)
				{
					if (strncmp(buffer, BPR_OP_WRONGJOB, BPR_OPLEN - 1) == 0)
					{
						fprintf(stderr, "Client was not looking for us. Continuing...\n");
						close(read_socket);
						continue;
					}
					if (strncmp(buffer, BPR_OP_WILLDELEGATE, BPR_OPLEN - 1) == 0)
					{
						delegation = 1;
					}
					else if (strncmp(buffer, BPR_OP_WILLSEND, BPR_OPLEN - 1) == 0)
					{
						delegation = 0;
					}
				}
				buffer[BPR_OPLEN - 1] = '\000'; /* To be safe */

				if (delegation == -1)
				{
					send(read_socket, BPR_OP_ERROR, BPR_OPLEN, 0);
					fprintf(stderr, "Unknown protocol in negotiation phase.\n");
					fprintf(stderr, "Listening for further connections...\n");
					close(read_socket);
					continue;
				}
				else
				{
					if (delegation)
						fprintf(stderr, "%s: Will receive proxy via delegation. ",buffer);
					else
						fprintf(stderr, "%s: Will receive proxy via file transfer. ",buffer);
				}
				send(read_socket, BPR_OP_ACKNOWLEDGE, BPR_OPLEN, 0);

				FD_ZERO(&masterfs);
				FD_SET(read_socket, &masterfs);

				fprintf(stderr, "Waiting for the proxy...\n");
				tv.tv_sec = 15;
				tv.tv_usec = 0;
				readfs = masterfs;
				if (select(FD_SETSIZE, &readfs, (fd_set *)NULL, (fd_set *)NULL, &tv) <= 0)
				{
					fprintf(stderr, "Nothing received.\n");
					fprintf(stderr, "Listening for further connections...\n");
					close(read_socket);
					continue;
				}

				fprintf(stderr, "Acquiring credentials...\n");
				if ((credential_handle = acquire_cred(GSS_C_ACCEPT)) == GSS_C_NO_CREDENTIAL)
				{
					fprintf(stderr, "Unable to acquire credentials, exiting...\n");
					exit(1);
				}
	
				fprintf(stderr, "Accepting security context...\n");
				if ((context_handle = accept_context(credential_handle, &client_name, read_socket)) == GSS_C_NO_CONTEXT)
				{
					fprintf(stderr, "Cannot accept security context.\n");
					fprintf(stderr, "Listening for further connections...\n");
					gss_release_cred(&major_status, &credential_handle);
					close(read_socket);
				}
				else
				{
					fprintf(stderr, "Security context accepted\n");
					fprintf(stderr, "Client name: %s\n", client_name);
					if (delegation)
					{
						fprintf(stderr, "Receiving new delegated proxy...\n");
						error = receive_delegated_proxy(&message, context_handle, read_socket);
					}
					else
					{
						fprintf(stderr, "Receiving new proxy...\n");
						error = receive_proxy(&message, context_handle, read_socket);
					}
					if (!error)
					{
						proxy_file_name_copy = strdup(proxy_file_name);
						proxy_dir = dirname(proxy_file_name_copy); /* Use a copy as dirname can change the argument */
						if ((proxy_tmp_name = (char *)malloc(strlen(proxy_dir) + strlen(PROXYTMP) + 2)) == NULL)
						{
							fprintf(stderr, "Cannot allocate memory for tempfile name!\n");
							exit(1);
						}
						sprintf(proxy_tmp_name, "%s/%s", proxy_dir, PROXYTMP);
						proxy_file = mkstemp(proxy_tmp_name);
						if (proxy_file == -1)
						{
							perror("Cannot create temp file");
							exit(1);
						}
						write(proxy_file, message, strlen(message));
						close(proxy_file);
	
						fprintf(stderr, "Renaming %s to %s\n", proxy_tmp_name, proxy_file_name);
						if (rename(proxy_tmp_name, proxy_file_name))
						{
							perror("Error while renaming");
							unlink(proxy_tmp_name);
							exit(1);
						}
						gss_delete_sec_context(&minor_status, &context_handle, GSS_C_NO_BUFFER);
						if (message) free (message);
						free(proxy_file_name_copy);
						free(proxy_tmp_name);
						fprintf(stderr, "New proxy saved.\n");
					}
					gss_release_cred(&major_status, &credential_handle);
					close(read_socket);
					fprintf(stderr, "Listening for further connections...\n");
				}
			}
		}
		else /* listen timeout expired */
		{
			fprintf(stderr, "Time to see if process %d is alive... ", monitored_pid);
			if ( kill(monitored_pid, 0) )
			{
				fprintf(stderr, "No, it is terminated.\n");
				exit_program = 1;
			}
			else
			{
				fprintf(stderr, "Yes, it is alive.\n");
				fprintf(stderr, "Verifying credential lifetime...\n");
				if ((credential_handle = acquire_cred(GSS_C_ACCEPT)) == GSS_C_NO_CREDENTIAL)
				{
					fprintf(stderr, "Unable to acquire credentials, exiting...\n");
					exit(1);
				}
				fprintf(stderr, "%d seconds left.\n", get_cred_lifetime(credential_handle));
				if (get_cred_lifetime(credential_handle) < min_proxy_lifetime)
				{
					fprintf(stderr, "Killing the job with SIGTERM...\n");
					if (kill(monitored_pid, SIGTERM))
					{
						perror("Unable to kill the job:");
						fprintf(stderr, "Sleeping 30 seconds and retrying with SIGKILL...\n");
						sleep(30);
						kill(monitored_pid, SIGKILL);
						fprintf(stderr, "SIGKILL sent, exiting...\n");
					}
					exit_program = 1;
				}
				gss_release_cred(&major_status, &credential_handle);
			}
		}
	} /* while main loop */
}

