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
#include "typedefs2.h"
#include <unistd.h>




#endif
