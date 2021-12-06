/*
#  File:     main.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   20 Mar 2004: Original release
#
#  Description:
#   Open a sochet and listen for connection; fork new
#   processes to serve incoming connections.
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#ifdef MTRACE_ON
#include <mcheck.h>
#endif
#include "blahpd.h"
#include "server.h"
#include "console.h"

int
main(int argc, char *argv[])
{
    int fd_socket, read_socket;
    int listen_port;
    char client_ip[120];
    struct hostent *resolved_client;
    struct sockaddr_storage tentative_addr;
    struct sockaddr *cli_addr;
    int real_addr_size;
    fd_set readfs, masterfs;
    int retcod, status;
    int exit_program = 0;
    pid_t pid;
    char ainfo_port_string[16];
    struct addrinfo ai_req, *ai_ans, *cur_ans;
    int address_found;

#ifdef MTRACE_ON
    char mtrace_log[2048];
#endif

#ifdef MTRACE_ON
    sprintf(mtrace_log, "mtrace_%d.log", getpid());
    setenv("MALLOC_TRACE", mtrace_log, 1);
    mtrace();
#endif
    
    listen_port = 19999;
    if (argc > 2)
    {
       if (strncmp(argv[1],"-p", 2) == 0)
           listen_port = atoi(argv[2]);
    }

    openlog("blahpd", LOG_PID, LOG_DAEMON);
    syslog(LOG_DAEMON | LOG_INFO, "Starting blah server (%s)", RCSID_VERSION);
    
    printf("Starting BLAHP server...\n");
    printf("%s\n", RCSID_VERSION);

    ai_req.ai_flags = AI_PASSIVE;
    ai_req.ai_family = PF_UNSPEC;
    ai_req.ai_socktype = SOCK_STREAM;
    ai_req.ai_protocol = 0; /* Any stream protocol is OK */

    sprintf(ainfo_port_string,"%5d",listen_port);

    if (getaddrinfo(NULL, ainfo_port_string, &ai_req, &ai_ans) != 0)
    {
        fprintf(stderr, "Cannot get address of passive SOCK_STREAM socket.\n");
        exit(1);
    }

    address_found = 0;
    for (cur_ans = ai_ans; cur_ans != NULL; cur_ans = cur_ans->ai_next)
    {
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

    if (address_found == 0)
    {
        fprintf(stderr, "Cannot create or bind socket: %s\n", strerror(errno));
        exit(1);
    }

    if (listen(fd_socket,1) == -1)
    {
        fprintf(stderr, "Cannot listen from socket: %s\n", strerror(errno));
        exit(1);
    } else printf("Server up and listening on port %d...\n", listen_port);

    FD_ZERO(&masterfs);
    FD_SET(0, &masterfs);
    FD_SET(fd_socket, &masterfs);

    while (!exit_program)
    {
        printf("\nBLAHP Server > ");

        readfs = masterfs;

        fflush(stdout);
        if ((retcod = select(FD_SETSIZE, &readfs, (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0)) < 0)
        {
            perror("Select error");
            close(fd_socket);
            exit(1);
        } 

        if (FD_ISSET(0, &readfs))
        {
            exit_program = processConsoleCommand();
        }

        if (FD_ISSET(fd_socket, &readfs))
        {
            real_addr_size = sizeof(tentative_addr);
            cli_addr = (struct sockaddr *)(&tentative_addr);
            if ((read_socket = accept(fd_socket, cli_addr, &real_addr_size)) == -1)
            {
                fprintf(stderr,"\nCannot accept connection: %s\n", strerror(errno));
                exit(1);
            }
            if ((getnameinfo(cli_addr, real_addr_size,
                             client_ip, sizeof(client_ip), NULL, 0, 0)) < 0)
            {
                client_ip[sizeof(client_ip)-1]='\000';
                strncpy(client_ip, "-unknown-", sizeof(client_ip) - 1);
            }

            printf("\nIncoming connection from %s\n", client_ip);
            while (waitpid(-1, &status, WNOHANG) > 0);
            pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "\nCannot fork connection manager: %s\n", strerror(errno));
                close(read_socket);
            }
            else if (pid >0)
            {
                printf("\nNew connection managed by child process %d\n", pid);
                close(read_socket);
            }
            else
            {
                close(fd_socket);
		syslog(LOG_DAEMON | LOG_INFO, "fork to serve connection from %s", client_ip);
                serveConnection(read_socket, client_ip);
            }
        }
    }

    printf("Server shutting down...\n");

    shutdown(fd_socket, SHUT_RDWR);
    close(fd_socket);
    printf("Socket closed\n");

    /* Shutdown of child processes */

    printf("Goodbye!\n");
    exit(0);
}
