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
#include "_condor_fix_types.h"
#include "expr.h"
#include <netinet/in.h>

/* maximum length of a machine name */
#define  MAXHOSTLEN     1024	

typedef struct job_id
{
	struct in_addr	inet_addr;
	PROC_ID			id;
} JOB_ID;

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

void
display_from( struct sockaddr_in *from );

#endif INTERNET_H
