/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/******************************************************************************
*                                                                             *
*   Author:  Hsu-lin Tsao                                                     *
*   Project: Condor Checkpoint Server                                         *
*   Date:    May 1, 1995                                                      *
*   Version: 0.5 Beta                                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Module:  server_network                                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   File:    server_network.h                                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains prototypes for prepackaged routines which are        *
*   commonly used by the Checkpoint Server.  Most of these routines use other *
*   "network" routines such as socket(), bind(), gethostbyname(), accept(),   *
*   listen(), etc.  However, most of these routines automatically trap        *
*   errors, and package specific pieces of information to pass back to the    *
*   calling routines.                                                         *
*                                                                             *
*   All of the routines in the server_network module are written in C as the  *
*   shadow processes also use some of these routines.                         *
*                                                                             *
******************************************************************************/


#ifndef SERVER_NETWORK_H
#define SERVER_NETWORK_H


/* Header Files */

#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>




/* External Functions */

extern "C" { int socket(int, int, int); }
extern "C" { int bind(int, const struct sockaddr*, int); }
extern "C" { int accept(int, struct sockaddr*, int*); }
extern "C" { int listen(int, int); }
extern "C" { int getsockname(int, struct sockaddr*, int*); }
extern "C" { int getpid(void); }
extern "C" { int gethostname(char*, int); }
extern "C" { struct hostent* gethostbyname(char*); }
extern "C" { struct hostent* gethostbyaddr(char*, int, int); }




/* Function Prototypes*/

void I_bind(int socket_desc, struct sockaddr_in* addr, int exit_status);
char* gethostaddr(void);
char* gethostnamebyaddr(struct in_addr* addr);
char* getserveraddr(void);
int I_socket(int exit_status);
void I_listen(int socket_desc, int queue_len);
int I_accept(int socket_desc, struct sockaddr_in* addr, int* addr_len);
int net_write(int socket_desc, const char* buffer, unsigned int size);


#endif
