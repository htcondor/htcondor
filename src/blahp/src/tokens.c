// $Id: tokens.c,v 1.5 2010/03/17 13:19:34 mezzadri Exp $
/*
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


/**
 * @file tokens.cpp
 * @brief The implementation for token transmission and reception.
 * This file implements a couple of methods providing functionality
 * to send and receive tokens.
 * @author Salvatore Monforte salvatore.monforte@ct.infn.it
 * @author comments by Marco Pappalardo marco.pappalardo@ct.infn.it and Salvatore Monforte
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <globus_gss_assist.h>
#include "tokens.h"

const int _TIMEOUT_ = 300;

/**
 * Send a gss token.
 * This method send gss tokens using GSI socket objects.
 * @param arg pointer to the descriptor of the socket.
 * @param token pointer to the token buffer to be sent.
 * @param token_length token buffer length
 * @returns the number of bytes sent, or -1 on failure.
 */
int send_token(void *arg, void *token, size_t token_length)
{
    size_t			num_written = 0;
    ssize_t			n_written;
    int 			fd = *( (int *) arg );
    unsigned char		token_length_buffer[4];

    /* encode the token length in network byte order: 4 byte, big endian */
    token_length_buffer[0] = (unsigned char) ((token_length >> 24) & 0xff);
    token_length_buffer[1] = (unsigned char) ((token_length >> 16) & 0xff);
    token_length_buffer[2] = (unsigned char) ((token_length >>  8) & 0xff);
    token_length_buffer[3] = (unsigned char) ((token_length      ) & 0xff);

    /* send the token length */
    while(num_written < 4)
    {
      n_written = send(fd, token_length_buffer + num_written, 4 - num_written,0);
      
      if(n_written < 0)
	{
	  if(errno == EINTR)
	    continue;
	  else
	    return -1;
	}
      else
	num_written += n_written;
    }
    
    /* send the token */

    num_written = 0;
    while(num_written < token_length)
    {
       n_written = send(fd, ((u_char *)token) + num_written, token_length - num_written,0);
       
       if(n_written < 0)
	 {
	   if(errno == EINTR)
		continue;
	   else
	     return -1;
	 }
       else
	 num_written += n_written;
    }
    
    return 0;
}

/**
 * Receive a gss token.
 * This method receives gss tokens using GSI socket objects.
 * @param arg pointer to the descriptor of the socket.
 * @param token pointer to the token buffer to fill with received token.
 * @param token_length token buffer length
 * @returns 0 on success, or -1 on failure.
 */
int get_token(void *arg, void **token, size_t *token_length)
{
    size_t			num_read = 0;
    ssize_t			n_read;
    int 			fd = *( (int *) arg );
    unsigned char		token_length_buffer[4];

    while(num_read < 4)
    {
      
      n_read = recv(fd, token_length_buffer + num_read, 4 - num_read,0);
	
      if(n_read < 0)
	{
	    if(errno == EINTR)
		continue;
	    else
		return -1;
	}
        else if (n_read == 0)
                return GLOBUS_GSS_ASSIST_TOKEN_EOF;
	else
	    num_read += n_read;
    }
    num_read = 0;
    /* decode the token length from network byte order: 4 byte, big endian */

    *token_length  = (((size_t) token_length_buffer[0]) << 24) & 0xffff;
    *token_length |= (((size_t) token_length_buffer[1]) << 16) & 0xffff;
    *token_length |= (((size_t) token_length_buffer[2]) <<  8) & 0xffff;
    *token_length |= (((size_t) token_length_buffer[3])      ) & 0xffff;

    if(*token_length > 1<<24)
    {
	/* token too large */
	return -1;
    }

    /* allocate space for the token */

    *((void **)token) = (void *) malloc(*token_length);

    if(*token == NULL)
    {
	return -1;
    }

    /* receive the token */

    num_read = 0;
    while(num_read < *token_length)
    {
      n_read = recv(fd, ((u_char *) (*token)) + num_read,(*token_length) - num_read,0);
	
	if(n_read < 0)
	{
	    if(errno == EINTR)
		continue;
	    else
		return -1;
	}
	else
	    if(n_read == 0) return -1; 
	    num_read += n_read;
    }

    return 0;
}
