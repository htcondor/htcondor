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
*   File:    server_network.c                                                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This module contains prepackaged routines which are commonly used by the  *
*   Checkpoint Server.  Most of these routines use other "network" routines   *
*   such as socket(), bind(), gethostbyname(), etc.  However, these routines  *
*   automatically trap errors, and package specific pieces of information to  *
*   pass back to the calling routines.                                        *
*                                                                             *
*   All of the routines in the server_network module are written in C as the  *
*   shadow processes also use some of these routines.  The network routines   *
*   are external function calls as this module may be compiled with a C++     *
*   compiler.                                                                 *
*                                                                             *
******************************************************************************/


/* Header Files */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "condor_netdb.h"

#if !defined(WIN32)
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "constants2.h"
#include "condor_debug.h"

/* P R O T O T Y P E S */
char *param(void);
char *GetIPName(struct in_addr machine_IP);
int I_bind(int socket_desc, struct sockaddr_in* addr, int is_well_known);
char* gethostnamebyaddr(struct in_addr* addr);
int I_socket(void);
int I_listen(int socket_desc, int queue_len);
int I_accept(int socket_desc, struct sockaddr_in* addr, int* addr_len);
int net_write(int socket_desc, char* buffer, int size);
int _condor_local_bind(int, int);

char* GetIPName(struct in_addr machine_IP)
{
  char* temp_name;

  temp_name = inet_ntoa(machine_IP);
  if (temp_name == NULL)
    return "<Unresolved IP>";
  else
    return temp_name;
}


/******************************************************************************
*                                                                             *
*   Function: I_bind(int                 socket_desc,                         *
*                    struct sockaddr_in* addr,                                *
*                    int                 exit_status)                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function prepackages the bind() call, trapping its errors.           *
*   Moreover, after binding a socket to an IP/port, it returns the new port   *
*   via the Internet socket address pointer (struct sockaddr_in*), a feature  *
*   accomplished by calling the getsockname() function.                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int                 socket_desc - the socket descriptor of the       *
*                                          socket to be bound                 *
*        struct sockaddr_in* addr        - a pointer to an Internet socket    *
*                                          struct which contains the IP/port, *
*                                          as well as the type of socket, to  *
*                                          be bound                           *
*        int                 exit_status - upon error, this value will be     *
*                                          used when exiting the program.     *
*                                          If set to zero, default error      *
*                                          codes will be used                 *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


int I_bind(int                 socket_desc,
	    struct sockaddr_in* addr, int is_well_known)
{
  int temp;
  int					on = 1;
  struct linger		linger = {0, 0}; 
  int bind_return_value = 0;	
  int bind_port = 0;
  priv_state old_priv = PRIV_UNKNOWN;

  temp = sizeof(struct sockaddr_in);
  setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
  setsockopt(socket_desc, SOL_SOCKET, SO_LINGER,
			 (char*)&linger, sizeof(linger));

  // remember we did this transformation before calling this function...
  // so undo it here.
  bind_port = ntohs(addr->sin_port);
  if (bind_port < 1024) {
    // use root priv for the call to bind to allow privileged ports
    old_priv = set_root_priv();
  }

	if (is_well_known == TRUE) {
		bind_return_value = bind(socket_desc, (struct sockaddr*)addr, temp)==0 ?
 						TRUE : FALSE;
	} else {
		/* There is a good chance that this binding will be given to a
			user job for the purposes of an outgoing connection. So
			mark it as such. */
		bind_return_value = _condor_local_bind(TRUE, socket_desc);
	}

  if (bind_port < 1024) {
    set_priv (old_priv);
  }

  if (bind_return_value == FALSE)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: unable to bind socket (pid=%d)\n", 
	      (int) getpid());
	  fprintf(stderr, "\tUnknown errno. Sorry.\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      return BIND_ERROR;
    }
  if (getsockname(socket_desc, (struct sockaddr*)addr, &temp) < 0)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: getsockname() failed (pid=%d)\n", 
	      (int) getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      return BAD_SOCKET_DESC_ERROR;
    }
  if (temp != sizeof(struct sockaddr_in))
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: socket address (sd=%d, pid=%d) ", socket_desc,
	      (int) getpid());
      fprintf(stderr, "is not for TCP/IP\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      return NOT_TCPIP;
    }
  return CKPT_OK;
}




/******************************************************************************
*                                                                             *
*   Function: gethostnamebyaddr(struct in_addr* addr)                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function, by using the gethostbyaddr() function, returns a pointer   *
*   to the name (i.e., string) of a machine.  The machine is specified by     *
*   the parameter which is an IP address.                                     *
*                                                                             *
*   Note: this function may fail if the name server cannot find the name of   *
*         a machine.  In this case, this function will terminate the program. *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        struct in_addr* addr - a pointer to an IP address                    *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        char* - a pointer to a static area where a machine's (string) name   *
*                is held                                                      *
*                                                                             *
******************************************************************************/


char* gethostnamebyaddr(struct in_addr* addr)
{
  struct hostent* h;

  h = condor_gethostbyaddr((char*)addr, sizeof(struct in_addr), AF_INET);
  if (h == NULL)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot get host information (pid=%d)\n", 
	      (int) getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      return NULL;
    }
  return(h->h_name);
}


/******************************************************************************
*                                                                             *
*   Function: I_socket(int exit_status)                                       *
*                                                                              *
*******************************************************************************
*                                                                             *
*   This function basically creates a socket using the socket() call.         *
*   However, this function will always create an Internet socket, and will    *
*   trap an error from the socket() call.                                     *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int exit_status - the exit code to be used in case the socket() call *
*                          returns an error                                   *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - a socket descriptor                                            *
*                                                                             *
******************************************************************************/


int I_socket()
{
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	int sd;
	
	sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sd < 0) {
		if ((errno == EMFILE) || (errno == ENOBUFS)) {
			return INSUFFICIENT_RESOURCES;
		} else {
			fprintf(stderr, "\nERROR:\n");
			fprintf(stderr, "ERROR:\n");
			fprintf(stderr, "ERROR: cannot open the server request socket ");
			fprintf(stderr, "(%d)\n", (int) getpid());
			fprintf(stderr, "ERROR:\n");
			fprintf(stderr, "ERROR:\n\n");
			return CKPT_SERVER_SOCKET_ERROR;
		}
	}
	return sd;
#else
	return 0;
#endif
}




/******************************************************************************
*                                                                             *
*   Function: I_listen(int socket_desc,                                       *
*                      int queue_len)                                         *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function uses the listen() function to indicate that a socket is     *
*   willing to receive connection requests.  The number of connection         *
*   requests to be queued when handling an earlier request is limited by      *
*   most systems to 5.                                                        *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int socket_desc - the socket descriptor of the socket willing to     *
*                          receive connections                                *
*        int queue_len   - the maximum number of connection requests to be    *
*                          queued when handling an earlier request            *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


int I_listen(int socket_desc,
	     int queue_len)
{
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
  if ((queue_len > 5) || (queue_len < 0))
    queue_len = 5;
  if (listen(socket_desc, queue_len) < 0)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot listen from socket (sd=%d, pid=%d)\n",
	      socket_desc, (int) getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      return LISTEN_ERROR;
    }
#endif
  return CKPT_OK;
}




/******************************************************************************
*                                                                             *
*   Function: I_accept(int                 socket_desc,                       *
*                      struct sockaddr_in* addr,                              *
*                      int*                addr_len)                          *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function uses the accept() call to accept a connection request.      *
*   This function works the same way as accept(), using the same parameters   *
*   and returning the same data, but is resistant to signals.  A signal can   *
*   cause the accept() to stop blocking, so the accept() must be restarted    *
*   if this occurs.                                                           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int                 socket_desc - the socket descriptor of the       *
*                                          socket willing to receive          *
*                                          connections                        *
*        struct sockaddr_in* addr        - a pointer to an Internet socket    *
*                                          struct which contains the IP/port, *
*                                          as well as the type of socket, to  *
*                                          be bound                           *
*        int*                addr_len    - a pointer to the length of the     *
*                                          socket description struct.  This   *
*                                          is necessary because there are     *
*                                          different types of sockets, and    *
*                                          each description has a different   *
*                                          length                             *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - a new socket descriptor which is connected to the requestor.   *
*              This will be different from the socket descriptor which is     *
*              used to accept connections                                     *
*                                                                             *
******************************************************************************/


int I_accept(int                 socket_desc, 
			 struct sockaddr_in* addr, 
			 int*                addr_len)
{
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	int temp;
	int on = 1;
	
	while ((temp=accept(socket_desc, (struct sockaddr*) addr, addr_len)) < 0) {
		if (errno != EINTR) {
			fprintf(stderr, "\nERROR:\n");
			fprintf(stderr, "ERROR:\n");
			fprintf(stderr, "ERROR: cannot accept from socket ");
			fprintf(stderr, "(sd=%d, pid=%d)\n", socket_desc, (int) getpid());
			fprintf(stderr, "ERROR:\n");
			fprintf(stderr, "ERROR:\n\n");
			return ACCEPT_ERROR;
		}
    }
	setsockopt( temp, SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on) );
	return(temp);
#else
	return 0;
#endif
}




/******************************************************************************
*                                                                             *
*   Function: net_write(int   socket_desc,                                    *
*                       char* buffer,                                         *
*                       int   size)                                           *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function is used to ensure that data is written correctly.  In most  *
*   systems, a write() call is blocking when writing to a socket.             *
*   Furthermore, a write() may not actually write the number of bytes         *
*   specified due to buffering in the network.  Thus, a loop is set up to     *
*   write the actual number of bytes specified.                               *
*                                                                             *
*   Note: most implementations of UNIX automatically block the write() until  *
*         all bytes are written.                                              *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        int   socket_desc - the socket descriptor of the socket to write to  *
*        char* buffer      - a pointer to the data to be written              *
*        int   size        - the number of bytes to be written                *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        int - the total number of bytes written                              *
*                                                                             *
******************************************************************************/


int net_write(int   socket_desc, 
			  char* buffer, 
			  int   size)
{
#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
	int bytes_written;
	int bytes_remaining;
	int total=0;
	
	bytes_remaining = size;
	while (bytes_remaining > 0) {
		bytes_written = write(socket_desc, buffer, bytes_remaining);
		if (((bytes_written == 0) && (errno != EINTR)) || (bytes_written < 0))
			return(bytes_written);
		bytes_remaining -= bytes_written;
		total += bytes_written;
		buffer += bytes_written;
    }
	if (total != size)
		return -1;
	return total;
#else
	return 0;
#endif
}
