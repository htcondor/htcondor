/*
** These are functions for generating internet addresses 
** and internet names
**
**             Author : Dhrubajyoti Borthakur
**               28 July, 1994
*/
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "except.h"
#include "debug.h"
#include "proc.h"
#include "internet.h"
#include "expr.h"
#include "condor_fix_string.h"

/* 
**  returns the internet address of this host 
**  returns 1 on success and -1 on failure 
*/
int 
get_inet_address(buffer)
struct in_addr*	buffer;
{
	char			host[MAXHOSTLEN];
	struct hostent	*hostptr;
	struct in_addr	*ptr;

	if ( gethostname(host, MAXHOSTLEN) == -1 )
	{
		dprintf(D_FULLDEBUG,"Error in gethostname\n");
		return -1;
	}
	if ( (hostptr=gethostbyname(host)) == NULL)
	{	
		dprintf(D_FULLDEBUG,"Error in gethostbyname\n");
		return -1;
	}
	switch( hostptr->h_addrtype)
	{
		case AF_INET:  *buffer = *(struct in_addr *)(hostptr->h_addr_list[0]);
					   return 1;
		default     :  dprintf(D_FULLDEBUG,"Unknown type\n");
					   return -1;
	}
}

/* 
** returns the full internet name of this host 
** returns 1 on success and -1 on failure
*/
int 
get_machine_name(buffer)
char*	buffer;
{
	char			host[MAXHOSTLEN];
	struct hostent	*hostptr;
	if ( gethostname(host, MAXHOSTLEN) == -1 )
	{
		dprintf(D_FULLDEBUG, "Error in gethostname\n");
		return -1;
	}
	if ( (hostptr=gethostbyname(host)) == NULL)
	{	
		dprintf(D_FULLDEBUG, "Error in gethostbyname\n");
		return -1;
	}
	strcpy(buffer, hostptr->h_name);
	return 1;
}

/*
** extract the internet-wide unique job_id from the job-context
*/
int
evaluate_job_id( context, job_id)
JOB_ID*		job_id;
CONTEXT*	context;
{
	char *tmp;

	if ( evaluate_string("JOB_ID", &tmp, context, NULL) < 0 )
	{
		dprintf(D_ALWAYS, "Can't evaluate JOB_ID\n");
		return -1;
	}
	if ( sscanf(tmp,"(%d, %d, %d)", &(job_id->inet_addr),&job_id->id.cluster,
												   &job_id->id.proc) != 3 )
			return -1;
	return 1;
}	

/*
** appends the internet wide unique job_id to the job-context
*/
int
append_job_id( context, cluster, proc)
CONTEXT*		context;
int				cluster;
int				proc;
{
	 char    line[2048];
	 struct in_addr   buffer;

	 memset(&buffer, '\0', sizeof(buffer));
	 if ( get_inet_address(&buffer) == -1 )
	 {
		  dprintf(D_ALWAYS, "Error in getting internet address\n");
		  return -1;
	 }
     sprintf(line, "JOB_ID = \"(%d, %d, %d)\"", buffer.s_addr, cluster, proc);
	 add_stmt(scan(line), context);
	 return 1;
}



/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  JCP */

int
string_to_sin(char *string, struct sockaddr_in *sin)
{
	int             i;
	char    *cur_byte;
	char    *end_string;

	string++;					/* skip the leading '<' */
	cur_byte = (char *) &(sin->sin_addr);
	for(end_string = string; end_string != 0; ) {
		end_string = strchr(string, '.');
		if (end_string == 0) {
			end_string = strchr(string, ':');
		}
		if (end_string) {
			*end_string = '\0';
			*cur_byte = atoi(string);
			cur_byte++;
			string = end_string + 1;
			*end_string = '.';
		}
	}
	
	string[strlen(string) - 1] = '\0'; /* Chop off the trailing '>' */
	sin->sin_port = htons(atoi(string));
}


char *
sin_to_string(struct sockaddr_in *sin)
{
	int             i;
	static  char    buf[24];
	char    tmp_buf[10];
	char    *cur_byte;
	unsigned char   this_byte;

	buf[0] = '<';
	buf[1] = '\0';
	cur_byte = (char *) &(sin->sin_addr);
	for (i = 0; i < sizeof(sin->sin_addr); i++) {
		this_byte = (unsigned char) *cur_byte;
		sprintf(tmp_buf, "%u.", this_byte);
		cur_byte++;
		strcat(buf, tmp_buf);
	}
	buf[strlen(buf) - 1] = ':';
	sprintf(tmp_buf, "%d>", ntohs(sin->sin_port));
	strcat(buf, tmp_buf);
	return buf;
}

