/*
#  File:     resbuffer.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   23 Mar 2004 - Original release
#   31 Mar 2008 - Switched from linked list to single string buffer
#                 Dropped support for persistent (file) buffer
#                 Async mode handled internally (no longer in server.c)
#
#  Description:
#   Mantain the result line buffer
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "blahpd.h"
#include "resbuffer.h"

/* These are global variables as
 * they have to be shared among all threads
 * */
static char* resbuffer_result_string = NULL; /* the result lines */
static int   resbuffer_num_lines = 0;        /* the number of the result lines */
static int   resbuffer_current_bufsize = 0;  /* the currently allocated buffer size */
static int   resbuffer_current_ressize = 0;  /* the current length of the results */
static int   resbuffer_async_mode = 0;       /* current async mode (0 = off) */ 
static int   resbuffer_async_notify = 0;     /* set to 1 if a new result has been enqueued */
                                             /* since the last buffer flush                */
static pthread_mutex_t resbuffer_lock = PTHREAD_MUTEX_INITIALIZER;
const char line_sep[] = BLAHPD_CRLF;


int
init_resbuffer(void)
/* Init the buffer
 * */
{
	pthread_mutex_init(&resbuffer_lock, NULL);
	return (0);
}


int
set_async_mode(int mode)
/* Set the async mode on or off
 * Return 1 if the required mode was already set
 * */
{
	if (resbuffer_async_mode == mode) return(1);

	/* Acquire lock */
	pthread_mutex_lock(&resbuffer_lock);

	resbuffer_async_mode = mode;
	resbuffer_async_notify = mode;

	/* Release lock */
	pthread_mutex_unlock(&resbuffer_lock);
	return(0);
}


int
push_result(const char *res)
/* Push a new result line into result buffer
 * allocate memory in chunks, in order to reduce the number of realloc
 * Return 1 if the "R" (async notification) has to be printed out, 0 otherwise
 * */
{
	int reslen;
	int required_bytes;
	int required_chunks;
	int async_notify = 0;

	reslen = strlen(res) + sizeof(line_sep) - 1;

	/* Acquire lock */
	pthread_mutex_lock(&resbuffer_lock);

	/* Add the new entry */
	if ((required_bytes = (reslen + resbuffer_current_ressize - resbuffer_current_bufsize)) > 0)
	{
		required_chunks = required_bytes / ALLOC_CHUNKS + 1;
		resbuffer_result_string = (char *)realloc(resbuffer_result_string, resbuffer_current_bufsize + ALLOC_CHUNKS * required_chunks);
		if (resbuffer_result_string == NULL)
		{
			fprintf(stderr, "Out of memory! Cannot allocate result buffer\n");
			exit(MALLOC_ERROR);
		}
		resbuffer_current_bufsize += (ALLOC_CHUNKS * required_chunks);
	}
	memcpy(resbuffer_result_string + resbuffer_current_ressize, line_sep, sizeof(line_sep) - 1);
	strcpy(resbuffer_result_string + resbuffer_current_ressize + sizeof(line_sep) - 1, res);
	
	resbuffer_current_ressize += reslen;

	resbuffer_num_lines++;

	/* Notify caller if it has to print the async 'R' notification */
	if (resbuffer_async_notify == ASYNC_MODE_ON && resbuffer_async_mode == ASYNC_MODE_ON)
	{
		async_notify = 1;
		resbuffer_async_notify = ASYNC_MODE_OFF;
	}

	/* Release lock */
	pthread_mutex_unlock(&resbuffer_lock);
	return(async_notify);
}


char *
get_lines(void)
/* Return the number n of result lines and the
 * whole buffer, in the form:
 *
 * S <n>
 * <line 1>
 * <line 2>
 * ...
 * <line n>
 *
 * Finally flush the buffer
 * */
{
	char *res_lines = NULL;
	char *reallocated;

	/* Acquire lock */
	pthread_mutex_lock(&resbuffer_lock);

	/* Allocate the result string */
	res_lines = (char *)malloc(resbuffer_current_ressize + 16);
	if (res_lines == NULL)
	{
		fprintf(stderr, "Out of memory! Cannot allocate result buffer\n");
		exit(MALLOC_ERROR);
	}

	sprintf(res_lines, "S %d", resbuffer_num_lines);
	if (resbuffer_result_string)
	{
		strcat(res_lines, resbuffer_result_string);
		free(resbuffer_result_string);
		resbuffer_result_string = NULL;
		resbuffer_num_lines = 0;
		resbuffer_current_ressize = 0;
		resbuffer_current_bufsize = 0;
		resbuffer_async_notify = resbuffer_async_mode;
	}

	/* Release lock */
	pthread_mutex_unlock(&resbuffer_lock);
	return(res_lines);
}

