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

#if !defined(WIN32)
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "typedefs2.h"



#ifdef __cplusplus


/* External Functions */

#if 0

#ifdef OSF1
extern "C" { unsigned int ntohl(unsigned int); }
extern "C" { unsigned int htonl(unsigned int); }
extern "C" { unsigned short int ntohs(unsigned short int); }
extern "C" { unsigned short int htons(unsigned short int); }
#else
extern "C" { unsigned long int ntohl(unsigned long int); }
extern "C" { unsigned long int htonl(unsigned long int); }
extern "C" { unsigned short int ntohs(unsigned int); }
extern "C" { unsigned short int htons(unsigned int); }
#endif
extern "C" { void bzero(char*, int); }
extern "C" { pid_t getpid(void); }
/* extern "C" { int gethostname(const char*, int); } */
extern "C" { int close(int); }
extern "C" { pid_t fork(void); }
extern "C" { int select(int, fd_set*, fd_set*, fd_set*, struct timeval*); }
extern "C" { int socket(int, int, int); }
extern "C" { int bind(int, const struct sockaddr*, int); }
extern "C" { int accept(int, struct sockaddr*, int*); }
extern "C" { int listen(int, int); }
extern "C" { int getsockname(int, struct sockaddr*, int*); }
extern "C" { int getsockopt(int, int, int, void*, int*); }
extern "C" { int setsockopt(int, int, int, const void*, int); }

#endif



/* Function Prototypes*/

extern "C" { char* GetIPName(struct in_addr machine_IP); }
extern "C" { int I_bind(int socket_desc, 
			 struct sockaddr_in* addr, int is_well_known); }
extern "C" { char* gethostnamebyaddr(struct in_addr* addr); }
extern "C" { int I_socket(void); }
extern "C" { int I_listen(int socket_desc, int queue_len); }
extern "C" { int I_accept(int                 socket_desc, 
			  struct sockaddr_in* addr, 
			  int*                addr_len); }
extern "C" { int net_write(int socket_desc, 
			   char* buffer, 
			   int size); }


#else


/* Function Prototypes*/

char* GetIPName(struct in_addr machine_IP); 
int I_bind(int                 socket_desc, 
	   struct sockaddr_in* addr);
char* gethostnamebyaddr(struct in_addr* addr); 
int I_socket(void);
int I_listen(int socket_desc, 
	     int queue_len); 
int I_accept(int                 socket_desc, 
	     struct sockaddr_in* addr, 
	     int*                addr_len); 
int net_write(int   socket_desc, 
	      char* buffer, 
	      int   size); 


#endif



#endif

