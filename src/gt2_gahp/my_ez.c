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

/* This is a modified version of globus_gass_server_ez.c from the Globus
 * Toolkit 4.2.1. It allows up to 20 new connections to be authenticated
 * in parallel rather than just 1.
 */

/*
 * Copyright 1999-2006 University of Chicago
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/******************************************************************************
globus_gass_server_ez.c
 
Description:
    Simple File Server Library Implementation using GASS Server API
 
CVS Information:
 
    $Source: /home/globdev/CVS/globus-packages/gass/server_ez/source/globus_gass_server_ez.c,v $
    $Date: 2006/01/19 05:54:46 $
    $Revision: 1.37 $
    $Author: mlink $
******************************************************************************/

/******************************************************************************
                             Include header files
******************************************************************************/
#include "globus_common.h"
#include "globus_gass_server_ez.h"
#include "globus_gass_transfer.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#ifndef TARGET_ARCH_WIN32
#include <unistd.h>
#endif
#include <ctype.h>

/******************************************************************************
                               Misc stuff DHS
******************************************************************************/

static
globus_version_t local_version = 
{
    2,
    5,
    1137650088,
    1
};

extern globus_gass_transfer_listener_t gassServerListeners[];
static int number_listening;

#include "globus_gram_client.h"
#include "globus_gss_assist.h"
#include "internal.h"

/******************************************************************************
                               Type definitions
******************************************************************************/
/* Data type associated with each server_ez listener */
typedef struct globus_l_gass_server_ez_s
{
    globus_gass_transfer_listener_t listener;
    globus_gass_server_ez_client_shutdown_t callback;
    unsigned long options;
    globus_gass_transfer_requestattr_t * reqattr;
} globus_l_gass_server_ez_t;

/* Data type associated with each put request handled by the server_ez
 * library
 */
typedef struct globus_gass_server_ez_request_s
{
    int fd;
    globus_byte_t *line_buffer;
    unsigned long line_buffer_used;
    unsigned long line_buffer_length;
    globus_bool_t linebuffer;
    unsigned short port;
} globus_gass_server_ez_request_t;

/******************************************************************************
                          Module specific variables
******************************************************************************/

globus_hashtable_t globus_l_gass_server_ez_listeners;

globus_mutex_t globus_l_gass_server_ez_mutex;

/******************************************************************************
                          Module definition
******************************************************************************/
static int
globus_l_gass_server_ez_activate(void);

static int
globus_l_gass_server_ez_deactivate(void);

globus_module_descriptor_t globus_i_gass_server_ez_module =
{
    "globus_gass_server_ez",
    globus_l_gass_server_ez_activate,
    globus_l_gass_server_ez_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};


/******************************************************************************
                          Module specific prototypes
******************************************************************************/
/* callbacks called by globus_gass_server library when a request arrives */
static void globus_l_gass_server_ez_put_callback(void *arg,
				    globus_gass_transfer_request_t request,
    				    globus_byte_t *     bytes,
    				    globus_size_t       len,
				    globus_bool_t       last_data);

static void globus_l_gass_server_ez_get_callback(
 				    void *arg,
				    globus_gass_transfer_request_t request,
				    globus_byte_t *     bytes,
				    globus_size_t       len,
				    globus_bool_t       last_data);

/* callbacks to handle completed send or receive of part of the request's data */

static void globus_gass_server_ez_put_memory_done(void * arg,
						  globus_gass_transfer_request_t request,
						  globus_byte_t buffer[],
						  globus_size_t buffer_length,
						  int receive_length);
static void
globus_l_gass_server_ez_listen_callback(
                                void * user_arg,
                                globus_gass_transfer_listener_t listener);

static void
globus_l_gass_server_ez_close_callback(
                                void * user_arg,
                                globus_gass_transfer_listener_t listener);

static void
globus_l_gass_server_ez_register_accept_callback(
                                        void * user_arg,
                                        globus_gass_transfer_request_t request
                                        );

/* utility routines */
static int globus_l_gass_server_ez_tilde_expand(unsigned long options,
						char *inpath,
						char **outpath);

static int
globus_l_gass_server_ez_write(int fd,
                    globus_byte_t *buffer,
                    size_t length);

void try_to_listen(void);

#define globus_l_gass_server_ez_enter() globus_mutex_lock(&globus_l_gass_server_ez_mutex)
#define globus_l_gass_server_ez_exit()	globus_mutex_unlock(&globus_l_gass_server_ez_mutex)



/******************************************************************************
Function: globus_gass_server_ez_init()

Description: 

Parameters: 

Returns: 
******************************************************************************/
int
globus_gass_server_ez_init(globus_gass_transfer_listener_t * listener,
			   globus_gass_transfer_listenerattr_t * attr,
			   char * scheme,
			   globus_gass_transfer_requestattr_t * reqattr,
			   unsigned long options,
			   globus_gass_server_ez_client_shutdown_t callback)
{
    int rc;
    globus_l_gass_server_ez_t *server;
    globus_bool_t free_scheme=GLOBUS_FALSE;


    if(scheme==GLOBUS_NULL)
    {
	scheme=globus_malloc(6);  /* https/0 is the default */
	if(scheme == GLOBUS_NULL)
        {
            rc = GLOBUS_GASS_TRANSFER_ERROR_MALLOC_FAILED;
            goto error_exit;
        }
        free_scheme=GLOBUS_TRUE;
	globus_libc_lock();
        sprintf(scheme, "https");
        globus_libc_unlock();
    }

    if(reqattr==GLOBUS_NULL)
    {
	reqattr=(globus_gass_transfer_requestattr_t *)globus_malloc(sizeof(globus_gass_transfer_requestattr_t));

        globus_gass_transfer_requestattr_init(reqattr,
    					      scheme);
        globus_gass_transfer_secure_requestattr_set_authorization(reqattr,
							   GLOBUS_GASS_TRANSFER_AUTHORIZE_SELF,
							   scheme);
    }
    rc=globus_gass_transfer_create_listener(listener,
					    attr,
					    scheme);


    if(rc!=GLOBUS_SUCCESS)
    {
	goto error_exit;
    }

    server=(globus_l_gass_server_ez_t *)globus_malloc(
					sizeof (globus_l_gass_server_ez_t));
    if(server==GLOBUS_NULL)
    {
        rc = GLOBUS_GASS_TRANSFER_ERROR_MALLOC_FAILED;
	goto error_exit;
    }

    server->options=options;
    server->listener=*listener;
    server->reqattr=reqattr;
    server->callback=callback;

    globus_hashtable_insert(&globus_l_gass_server_ez_listeners,
			    (void *)*listener,
			    server);

    rc=globus_gass_transfer_register_listen(*listener,
				globus_l_gass_server_ez_listen_callback,
					(void *)reqattr);

    number_listening=1;
/* insert error handling here*/

    error_exit:

    if (free_scheme) globus_free(scheme);

    return rc;
} /* globus_gass_server_ez_init() */

/******************************************************************************
Function: globus_gass_server_ez_shutdown()

Description: 

Parameters: 

Returns: 
******************************************************************************/
int
globus_gass_server_ez_shutdown(globus_gass_transfer_listener_t listener)
{
    int rc;
    void * user_arg = GLOBUS_NULL;


    rc=globus_gass_transfer_close_listener(listener,
                           		globus_l_gass_server_ez_close_callback,
				 	user_arg); 

    return rc;
} /* globus_gass_server_ez_shutdown() */


/******************************************************************************
Function: globus_l_gass_server_ez_put_memory_done()

Description: 

Parameters: 

Returns: 
******************************************************************************/
static void
globus_gass_server_ez_put_memory_done(void * arg,
                                       globus_gass_transfer_request_t request,
                                       globus_byte_t buffer[],
                                       globus_size_t receive_length,
				       globus_bool_t last_data)
{
    globus_gass_server_ez_request_t *r=GLOBUS_NULL;
    globus_size_t max_length;
    unsigned long lastnl, x;
    int status;
    const int buffer_length=1024;
    
    /* This callback handles line-buffered put requests. 
     */    

    r = (globus_gass_server_ez_request_t *)arg;

    status=globus_gass_transfer_request_get_status(request);
    lastnl = 0UL;


    /* find last \n in the buffer, since we are line-buffering */
    max_length=receive_length;
    for(x = max_length; x > 0UL; x--)
    {
	if(buffer[x-1] == '\n')
	{
	    lastnl = x;
	    break;
	}
    }
    
    if(status == GLOBUS_GASS_TRANSFER_REQUEST_PENDING && !last_data)
    {
	/* data arrived, and more will be available, so write up until
	 * the last \n we've received and save the rest
	 */
	if(r->line_buffer != GLOBUS_NULL &&
	   lastnl != 0UL &&
	    r->line_buffer_used != 0UL)
	{
	    globus_l_gass_server_ez_write(r->fd,
				r->line_buffer,
				r->line_buffer_used);
	    r->line_buffer_used = 0UL;
	}
	
	if(lastnl != 0UL)
	{
	    globus_l_gass_server_ez_write(r->fd,
				buffer,
				lastnl);
	}
	else
	{
	    lastnl = 0;
	}
	if(r->line_buffer_used + receive_length - lastnl >
	   r->line_buffer_length)
	{
	    r->line_buffer = (globus_byte_t *)
		realloc(r->line_buffer,
			r->line_buffer_used + receive_length - lastnl);
	    r->line_buffer_length = r->line_buffer_used + receive_length - lastnl;
	    memcpy(r->line_buffer + r->line_buffer_used,
		   buffer + lastnl,
		   receive_length - lastnl);
	    r->line_buffer_used += receive_length - lastnl;
	}
	else
	{
	    memcpy(r->line_buffer + r->line_buffer_used,
		   buffer + lastnl,
		   receive_length - lastnl);
	    r->line_buffer_used += receive_length - lastnl;
	}
	
	globus_gass_transfer_receive_bytes(request,
				  	   buffer,
					   buffer_length,
					   1UL,
					   globus_gass_server_ez_put_memory_done,
					   arg);
					
    }
    else
    {
	
	if(r->line_buffer != GLOBUS_NULL &&
	   r->line_buffer_used != 0UL)
	{
	    globus_l_gass_server_ez_write(r->fd,
				r->line_buffer,
				r->line_buffer_used);
	}
	if(receive_length != 0UL)
	{
	    globus_l_gass_server_ez_write(r->fd,
				buffer,
				receive_length);
	}

	    
	if(r->fd!=fileno(stdout) && r->fd!=fileno(stderr))
	{
  	    globus_libc_close(r->fd);
	}

	if(buffer != GLOBUS_NULL)
	{
	    globus_free(buffer);
	}  

	globus_gass_transfer_request_destroy(request);
		

	if(r->linebuffer)
	{
	    globus_free(r->line_buffer);
	} 
	globus_free(r);
    }
} /* globus_l_gass_server_ez_put_memory_done() */

static void
globus_l_gass_server_ez_close_callback(
				void * user_arg,
				globus_gass_transfer_listener_t listener)
{
    /* should be cleaning up things related to the listener here
     * get rid of server struct stuff (hashtable) etc.
    */ 
}

static void
globus_l_gass_server_ez_listen_callback(
				void * user_arg,
				globus_gass_transfer_listener_t listener)
{
    int rc;
    globus_gass_transfer_request_t request;

	number_listening--;

    rc=globus_gass_transfer_register_accept(&request,
				 (globus_gass_transfer_requestattr_t *)
				 user_arg,
				 listener,
				 globus_l_gass_server_ez_register_accept_callback,
				 (void *)listener);

    try_to_listen();
#if 0
    if(rc != GLOBUS_SUCCESS)
    {
	/* to listen for additional requests*/
	globus_gass_transfer_register_listen(
	    listener,
	    globus_l_gass_server_ez_listen_callback,
	    user_arg);
    }
#endif
}


static void
globus_l_gass_server_ez_register_accept_callback(
					void * listener,
					globus_gass_transfer_request_t request 
					)
{
    int rc;
    char * path=GLOBUS_NULL;
    char * url;
    globus_url_t parsed_url;
    globus_l_gass_server_ez_t * s;
    globus_gass_server_ez_request_t *r;
    struct stat	statstruct;
    globus_byte_t * buf;
    int amt;
    int flags=0;

    /* lookup our options */
    s=(globus_l_gass_server_ez_t *)globus_hashtable_lookup(
                                &globus_l_gass_server_ez_listeners,
                                listener);

    /* Check for valid URL */
    url=globus_gass_transfer_request_get_url(request);
    rc = globus_url_parse(url, &parsed_url);
    if(rc != GLOBUS_SUCCESS ||
       parsed_url.url_path == GLOBUS_NULL || strlen(parsed_url.url_path) == 0U)
    {
        globus_gass_transfer_deny(request, 404, "File Not Found");
        globus_gass_transfer_request_destroy(request);
        if (rc == GLOBUS_SUCCESS)
            globus_url_destroy(&parsed_url);
	goto reregister_nourl;
    }

    if(globus_gass_transfer_request_get_type(request) ==
       GLOBUS_GASS_TRANSFER_REQUEST_TYPE_APPEND)
    {
        flags = O_CREAT | O_WRONLY | O_APPEND;
    }
    else if(globus_gass_transfer_request_get_type(request) ==
            GLOBUS_GASS_TRANSFER_REQUEST_TYPE_PUT)
    {
        flags = O_CREAT | O_WRONLY | O_TRUNC;
    }
    switch(globus_gass_transfer_request_get_type(request))
        {
          case GLOBUS_GASS_TRANSFER_REQUEST_TYPE_APPEND:
          case GLOBUS_GASS_TRANSFER_REQUEST_TYPE_PUT:

	    /* Check to see if this is a request we are allowed to handle */

            if(((s->options & GLOBUS_GASS_SERVER_EZ_WRITE_ENABLE) == 0UL) &&
              ((s->options & GLOBUS_GASS_SERVER_EZ_STDOUT_ENABLE) == 0UL) &&
              ((s->options & GLOBUS_GASS_SERVER_EZ_STDERR_ENABLE) == 0UL) &&
              ((s->options & GLOBUS_GASS_SERVER_EZ_CLIENT_SHUTDOWN_ENABLE) ==
									 0UL))
    	    {
		goto deny;
            }
	
	    /* Expand ~ and ~user prefix if enaabled in options */
    	    rc = globus_l_gass_server_ez_tilde_expand(s->options,
                                              parsed_url.url_path,
                                              &path);
    	    /* Check for "special" file names, and if we will handle them */
    	    if(strcmp(path, "/dev/stdout") == 0 &&
              (s->options & GLOBUS_GASS_SERVER_EZ_STDOUT_ENABLE))
            {
        	rc = fileno(stdout);
		goto authorize;
    	    }
    	    else if(strcmp(path, "/dev/stdout") == 0)
    	    {
		goto deny;
    	    }
    	    else if(strcmp(path, "/dev/stderr") == 0 &&
                   (s->options & GLOBUS_GASS_SERVER_EZ_STDERR_ENABLE))
    	    {
        	rc = fileno(stderr);
		goto authorize;
    	    }
    	    else if(strcmp(path, "/dev/stderr") == 0)
    	    {
		goto deny;
    	    }
    	    else if(strcmp(path, "/dev/globus_gass_client_shutdown") == 0)
    	    {
        	if(s->options & GLOBUS_GASS_SERVER_EZ_CLIENT_SHUTDOWN_ENABLE &&
           	   s->callback != GLOBUS_NULL)
        	{
            	    s->callback();
        	}

		goto deny;
    	    }
#ifdef TARGET_ARCH_WIN32
			// The call to open() in Windows defaults to text mode, so
			// we to override it.
			flags |= O_BINARY;
#endif
            rc = globus_libc_open(path, flags, 0600);

            if(rc < 0)
            {
                goto deny;
            }
	
	    authorize:
            globus_gass_transfer_authorize(request, 0);
	    if(s->options & GLOBUS_GASS_SERVER_EZ_LINE_BUFFER)
	    {
	        r=(globus_gass_server_ez_request_t *)globus_malloc(
				sizeof(globus_gass_server_ez_request_t));
		r->fd=rc;
		r->line_buffer=globus_malloc(80);
                r->line_buffer_used= 0UL;
        	r->line_buffer_length = 80UL;
        	r->linebuffer = GLOBUS_TRUE;

		globus_gass_transfer_receive_bytes(request,
						globus_malloc(1024),
						1024,
						1,
						globus_gass_server_ez_put_memory_done,
						r);
	    }
	    else
	    {
                globus_gass_transfer_receive_bytes(request,
                                               globus_malloc(1024),
                                               1024,
                                               1,
                                               globus_l_gass_server_ez_put_callback,
                                               (void *) rc);
	    }
            break;

          case GLOBUS_GASS_TRANSFER_REQUEST_TYPE_GET:
            flags = O_RDONLY;

			/* Expand ~ and ~user prefix if enaabled in options */
            rc = globus_l_gass_server_ez_tilde_expand(s->options,
                                              parsed_url.url_path,
                                              &path);

   	    if((s->options & GLOBUS_GASS_SERVER_EZ_READ_ENABLE) == 0UL)
    	    {
		goto deny;
    	    }
	   
	    if(stat(path, &statstruct)==0)
	    {
#ifdef TARGET_ARCH_WIN32
				// The call to open() in Windows defaults to text mode, 
				// so we to override it.
				flags |= O_BINARY;
#endif
                rc = globus_libc_open(path, flags, 0600);
		fstat(rc, &statstruct);
	    }
	    else
	    {
		globus_gass_transfer_deny(request, 404, "File Not Found");
		globus_gass_transfer_request_destroy(request);
		goto reregister;
	    }

            buf = globus_malloc(1024);
            amt = read(rc, buf, 1024);
            if(amt == -1)
            {
                globus_free(buf);
                goto deny;
            }
            globus_gass_transfer_authorize(request,
                                           statstruct.st_size);

            globus_gass_transfer_send_bytes(request,
                                            buf,
                                            amt,
                                            GLOBUS_FALSE,
                                            globus_l_gass_server_ez_get_callback,
                                            (void *) rc);
	  break;
	default:
	deny:
	  globus_gass_transfer_deny(request, 400, "Bad Request");
	  globus_gass_transfer_request_destroy(request);

	}

  reregister:
    globus_url_destroy(&parsed_url);

  reregister_nourl:
    try_to_listen();
/*
    globus_gass_transfer_register_listen(
				(globus_gass_transfer_listener_t) listener,
				globus_l_gass_server_ez_listen_callback,
				s->reqattr);
*/

    if (path != GLOBUS_NULL) globus_free(path);

} /*globus_l_gass_server_ez_register_accept_callback*/


/******************************************************************************
Function: globus_l_gass_server_ez_get_callback()

Description: 

Parameters: 

Returns: 
******************************************************************************/
static void 
globus_l_gass_server_ez_get_callback(
    void *arg,
    globus_gass_transfer_request_t request,
    globus_byte_t *     bytes,
    globus_size_t       len,
    globus_bool_t       last_data)
{
    int fd;
    globus_size_t amt;

    fd=(int) arg;
    if(!last_data)
    {
	amt = globus_libc_read(fd, bytes, len);
        if(amt == 0)
        {
            globus_gass_transfer_send_bytes(request,
                                            bytes,
                                            0,
                                            GLOBUS_TRUE,
                                            globus_l_gass_server_ez_get_callback,
                                            arg);
        }
        else
        {
            globus_gass_transfer_send_bytes(request,
                                            bytes,
                                            amt,
                                            GLOBUS_FALSE,
                                            globus_l_gass_server_ez_get_callback,
                                            arg);
        }
        return;
    }

    if((fd!=fileno(stdout))&&(fd!=fileno(stderr)))
    {
        close(fd);
    }
    globus_free(bytes);
    globus_gass_transfer_request_destroy(request);

} /* globus_l_gass_server_ez_get_callback() */

/******************************************************************************
Function: globus_l_gass_server_ez_put_callback()

Description: 

Parameters: 

Returns: 
******************************************************************************/
static void
globus_l_gass_server_ez_put_callback(
				    void *arg,
				    globus_gass_transfer_request_t request,
				    globus_byte_t *     bytes,
				    globus_size_t       len,
				    globus_bool_t       last_data)
{
    int fd;

    fd = (int) arg;

    globus_libc_write(fd, bytes, len);
    if(!last_data)
    {
        globus_gass_transfer_receive_bytes(request,
                                           bytes,
                                           1024,
                                           1,
                                           globus_l_gass_server_ez_put_callback,
                                           arg);
    return ;
    }

    if((fd!=fileno(stdout)) && (fd!=fileno(stderr)))
    {
        close(fd);
    }
    globus_free(bytes);
    globus_gass_transfer_request_destroy(request);
    return ;

} /* globus_l_gass_server_ez_put_callback() */

/******************************************************************************
Function: globus_l_gass_server_ez_tilde_expand()

Description: 

Parameters: 

Returns: 
******************************************************************************/
static int
globus_l_gass_server_ez_tilde_expand(unsigned long options,
			     char *inpath,
			     char **outpath)
{
#ifndef TARGET_ARCH_WIN32   
    /*
     * If this is a relative path, the strip off the leading /./
     */
    if(strlen(inpath) >= 2U)
    {
	if (inpath[1] == '.' && inpath[2] == '/')
	{
	    inpath += 3;
	    goto notilde;
	}
    }

    /* here call the new function globus_tilde_expand()*/
    return globus_tilde_expand(options,
				   GLOBUS_TRUE, /* url form /~[user][/etc]*/
				   inpath,
				   outpath);
#endif /* TARGET_ARCH_WIN32*/

notilde:
    *outpath = globus_malloc(strlen(inpath)+1);
    strcpy(*outpath, inpath);
    return GLOBUS_SUCCESS;
} /* globus_l_gass_server_ez_tilde_expand() */

/******************************************************************************
Function: globus_l_gass_server_ez_write()

Description:

Parameters:

Returns:
******************************************************************************/
int
globus_l_gass_server_ez_write(int fd,
                    globus_byte_t *buffer,
                    size_t length)
{
#ifndef TARGET_ARCH_WIN32
    ssize_t rc;
#else
	int rc;
#endif
    size_t written;

    written = 0;

    while(written < length)
    {
        rc = globus_libc_write(fd,
                               buffer + written,
                               length - written);
        if(rc < 0)
        {
            switch(errno)
            {
            case EAGAIN:
            case EINTR:
                break;
            default:
                return (int) rc;
            }
        }
        else
        {
            written += rc;
        }
    }

    return (int) written;
} /* globus_l_gass_server_ez_write() */


/******************************************************************************
Function: globus_l_gass_server_ez_activate()

Description: 

Parameters: 

Returns: 
******************************************************************************/
static int
globus_l_gass_server_ez_activate(void)
{
    int rc;
   
    rc = globus_module_activate(GLOBUS_COMMON_MODULE); 
    if(rc != GLOBUS_SUCCESS)
    {
        return rc;
    }
	
    rc = globus_module_activate(GLOBUS_GASS_TRANSFER_MODULE);
    if(rc != GLOBUS_SUCCESS)
    {
	return rc;
    }

    globus_mutex_init(&globus_l_gass_server_ez_mutex,
		      GLOBUS_NULL);

    globus_hashtable_init(&globus_l_gass_server_ez_listeners,
                          16,
                          globus_hashtable_int_hash,
                          globus_hashtable_int_keyeq);

    return GLOBUS_SUCCESS;
} /* globus_l_gass_server_ez_activate() */

/******************************************************************************
Function: globus_l_gass_server_ez_deactivate()

Description: 

Parameters: 

Returns: 
******************************************************************************/
static int
globus_l_gass_server_ez_deactivate(void)
{
    int rc;

    
    globus_mutex_destroy(&globus_l_gass_server_ez_mutex);

    rc = globus_module_deactivate(GLOBUS_GASS_TRANSFER_MODULE);
    globus_hashtable_destroy(&globus_l_gass_server_ez_listeners);

    rc|=globus_module_deactivate(GLOBUS_COMMON_MODULE);

    return rc;
} /* globus_l_gass_server_ez_deactivate() */



/* ================================= */

int
my_globus_gass_server_ez_init(globus_gass_transfer_listener_t * listener,
			   globus_gass_transfer_listenerattr_t * attr,
			   char * scheme,
			   globus_gass_transfer_requestattr_t * reqattr,
			   unsigned long options,
			   globus_gass_server_ez_client_shutdown_t callback)
{
    int rc;
    globus_l_gass_server_ez_t *server;
    globus_bool_t free_scheme=GLOBUS_FALSE;


    if(scheme==GLOBUS_NULL)
    {
	scheme=globus_malloc(6);  /* https/0 is the default */
	if(scheme == GLOBUS_NULL)
        {
            rc = GLOBUS_GASS_TRANSFER_ERROR_MALLOC_FAILED;
            goto error_exit;
        }
        free_scheme=GLOBUS_TRUE;
	globus_libc_lock();
        sprintf(scheme, "https");
        globus_libc_unlock();
    }

    if(reqattr==GLOBUS_NULL)
    {
	reqattr=(globus_gass_transfer_requestattr_t *)globus_malloc(sizeof(globus_gass_transfer_requestattr_t));

        globus_gass_transfer_requestattr_init(reqattr,
    					      scheme);
        globus_gass_transfer_secure_requestattr_set_authorization(reqattr,
							   GLOBUS_GASS_TRANSFER_AUTHORIZE_SELF,
							   scheme);
    }
    rc=globus_gass_transfer_create_listener(listener,
					    attr,
					    scheme);


    if(rc!=GLOBUS_SUCCESS)
    {
	goto error_exit;
    }

    server=(globus_l_gass_server_ez_t *)globus_malloc(
					sizeof (globus_l_gass_server_ez_t));
    if(server==GLOBUS_NULL)
    {
	rc = GLOBUS_GASS_TRANSFER_ERROR_MALLOC_FAILED;;
	goto error_exit;
    }

    server->options=options;
    server->listener=*listener;
    server->reqattr=reqattr;
    server->callback=callback;

    globus_hashtable_insert(&globus_l_gass_server_ez_listeners,
			    (void *)*listener,
			    server);

/* insert error handling here*/

    error_exit:

    if (free_scheme) globus_free(scheme);

    return rc;
} /* my_globus_gass_server_ez_init() */

void try_to_listen(void)
{
    int i,n,result;
    globus_l_gass_server_ez_t *s;

    if (number_listening>0) return;

    n=-1;
    for(i=0;i<20;i++) {
      globus_gass_transfer_listener_struct_t *l = globus_handle_table_lookup(&globus_i_gass_transfer_listener_handles, gassServerListeners[i]);
      if (l->status == GLOBUS_GASS_TRANSFER_LISTENER_STARTING) {
        n=i;
        break;
      }
    }

    if (n==-1) return;

    s = (globus_l_gass_server_ez_t *)globus_hashtable_lookup(
                                                     &globus_l_gass_server_ez_listeners,(void *)gassServerListeners[n]);

    result = globus_gass_transfer_register_listen(
                                gassServerListeners[n],
                                globus_l_gass_server_ez_listen_callback,
                                s->reqattr);

    if (result == GLOBUS_SUCCESS) {
      /*printf("registered index %d\n",i);*/
      number_listening++;
    }

}
