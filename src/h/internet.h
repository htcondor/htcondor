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
/*
** These are functions for generating internet addresses
** and internet names
**
**             Author : Dhrubajyoti Borthakur
**               28 July, 1994
*/

#ifndef INTERNET_H
#define INTERNET_H

#include "proc.h"
#include "expr.h"

#if !defined(SOCKET) && !defined(WIN32)
#define SOCKET int
#endif 

/* maximum length of a machine name */
#define  MAXHOSTLEN     1024	

typedef struct job_id
{
	struct in_addr	inet_addr;
	PROC_ID			id;
} JOB_ID;

#if defined(__cplusplus)
extern "C" {
#endif

/* 
**  returns the internet address of this host 
**  returns 1 on success and -1 on failure 
*/
int get_inet_address(struct in_addr *buffer);

/* 
** returns the full internet name of this host 
** returns 1 on success and -1 on failure
*/
int get_machine_name(char *buffer);

/*
** extract the internet-wide unique job_id from the job-context
*/
int evaluate_job_id(CONTEXT *context, JOB_ID *job_id);

/*
** appends the internet wide unique job_id to the job-context
*/
int append_job_id(CONTEXT *context, int cluster, int proc);

/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  TCP */
int string_to_sin(char *addr, struct sockaddr_in *sin);

char *sin_to_string(struct sockaddr_in *sin);

char *sock_to_string(SOCKET sockd);

/* Return the real hostname of a machine given a sin; return NULL if it cannot
 * be found or error.  Also return aliases. */
char *sin_to_hostname(const struct sockaddr_in *sin, char ***aliases);

void
display_from( struct sockaddr_in *from );

/* Returns 1 if h1 and h2 are both hostnames which refer to the same
   host, 0 if they don't, and -1 on error. */
int same_host(const char *h1, const char *h2);

#if defined(__cplusplus)
}
#endif

#endif INTERNET_H
