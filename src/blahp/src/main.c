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
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef MTRACE_ON
#include <mcheck.h>
#endif
#include "blahpd.h"
#include "server.h"
#include "console.h"

extern int synchronous_termination;
/* #define MTRACE_ON */

int
main(int argc, char *argv[])
{
    int fd_socket, read_socket;
    fd_set readfs, masterfs;
    int retcod, status;
    int exit_program = 0;
    pid_t pid;
#ifdef MTRACE_ON
    char mtrace_log[2048];
#endif
   
#ifdef MTRACE_ON
    sprintf(mtrace_log, "mtrace_%d.log", getpid());
    setenv("MALLOC_TRACE", mtrace_log, 1);
    mtrace();
#endif

    if ((argc > 1) && (strncmp(argv[1],"-s",2) == 0))
        synchronous_termination = TRUE; 

    serveConnection(0, "(stdin)");

    exit(0);
}
