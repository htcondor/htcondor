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

#ifdef C_COMPILE
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include "server_network.h"
#endif

#include "server_constants.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>




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
*                                          used when exiting the program      *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        void                                                                 *
*                                                                             *
******************************************************************************/


void I_bind(int                 socket_desc,
	    struct sockaddr_in* addr,
	    int                 exit_status)
{
  int temp;

  temp = sizeof(struct sockaddr_in);
  if (bind(socket_desc, (struct sockaddr*)addr, temp) < 0)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: unable to bind socket (pid=%d)\n", getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      if (exit_status)
	exit(exit_status);
      else
	exit(BIND_ERROR);
    }
  if (getsockname(socket_desc, (struct sockaddr*)addr, &temp) < 0)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: getsockname() failed (pid=%d)\n", getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      if (exit_status)
	exit(exit_status);
      else
	exit(BAD_SOCKET_DESC_ERROR);
    }
  if (temp != sizeof(struct sockaddr_in))
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: socket address (sd=%d, pid=%d) ", socket_desc,
	      getpid());
      fprintf(stderr, "is not for TCP/IP\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      if (exit_status)
	exit(exit_status);
      else
	exit(NOT_TCPIP);
    }
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

  h = gethostbyaddr((char*)addr, sizeof(struct in_addr), AF_INET);
  if (h == NULL)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot get host information (pid=%d)\n", 
	      getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      exit(GETHOSTBYADDR_ERROR);
    }
  return(h->h_name);
}




/******************************************************************************
*                                                                             *
*   Function: gethostaddr(void)                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   This function returns a pointer to the IP address of the machine          *
*   executing the function.  The pointer is a character pointer (char*) since *
*   the gethostbyname() function is used and returns a char** pointer.        *
*   Therefore, even though this is a character pointer, it points to an IP    *
*   address, and it is NOT guaranteed that the IP address falls on a fullword *
*   boundary (required for RISC machines).  Thus, the IP address should be    *
*   copied using memcpy() and the return char* pointer.                       *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Parameters:                                                               *
*        <none>                                                               *
*                                                                             *
*******************************************************************************
*                                                                             *
*   Return Type:                                                              *
*        char* - a pointer to a static area where a machine's IP address is   *
*                held                                                         *
*                                                                             *
******************************************************************************/


char* gethostaddr(void)
{
  struct hostent* h;
  char            hostname[256];

  if (gethostname(hostname, 256) < 0)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot get host name (pid=%d)\n", getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      exit(HOSTNAME_ERROR);
    }
  h = gethostbyname(hostname);
  if (h == NULL)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot get host information (pid=%d)\n", 
	      getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      exit(GETHOSTBYNAME_ERROR);
    }
  return(h->h_addr);
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


char* getserveraddr()
{
  struct hostent* h;

  h = gethostbyname(SERVER);
  if (h == NULL)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot get host information (pid=%d)\n", 
	      getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      exit(GETHOSTBYNAME_ERROR);
    }
  return(h->h_addr);
}




/******************************************************************************
*                                                                             *
*   Function: I_socket(int exit_status)                                       *
*                                                                             *
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


int I_socket(int exit_status)
{
  int sd;

  sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd < 0)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot open the server request socket ");
      fprintf(stderr, "(%d)\n", getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      if (exit_status)
	return(exit_status);
      else
	exit(CKPT_SERVER_SOCKET_ERROR);
    }
  return(sd);
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


void I_listen(int socket_desc,
	      int queue_len)
{
  if ((queue_len > 5) || (queue_len < 0))
    queue_len = 5;
  if (listen(socket_desc, queue_len) < 0)
    {
      fprintf(stderr, "\nERROR:\n");
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR: cannot listen from socket (sd=%d, pid=%d)\n",
	      socket_desc, getpid());
      fprintf(stderr, "ERROR:\n");
      fprintf(stderr, "ERROR:\n\n");
      exit(LISTEN_ERROR);
    }
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
  int temp;

  while ((temp=accept(socket_desc, (struct sockaddr*) addr, addr_len)) < 0)
    {
      if (errno != EINTR)
	{
	  fprintf(stderr, "\nERROR:\n");
	  fprintf(stderr, "ERROR:\n");
	  fprintf(stderr, "ERROR: cannot accept from socket ");
	  fprintf(stderr, "(sd=%d, pid=%d)\n", socket_desc, getpid());
	  fprintf(stderr, "ERROR:\n");
	  fprintf(stderr, "ERROR:\n\n");
	  exit(ACCEPT_ERROR);
	}
    }
  return(temp);
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


int net_write(int          socket_desc, 
	      const char*  buffer, 
	      unsigned int size)
{
  long int bytes_written;
  int bytes_remaining;
  int total=0;
  
  bytes_remaining = size;
  while (bytes_remaining > 0)
    {
      bytes_written = write(socket_desc, buffer, bytes_remaining);
      if (bytes_written <= 0)
	return(bytes_written);
      bytes_remaining -= bytes_written;
      total += bytes_written;
      buffer += bytes_written;
    }
  if (total != size)
    exit(CHILDTERM_BAD_FILE_SIZE);
  return total;
}










