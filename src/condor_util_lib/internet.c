/*
** These are functions for generating internet addresses 
** and internet names
**
**             Author : Dhrubajyoti Borthakur
**               28 July, 1994
*/

#include "condor_common.h"

#include <sys/time.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "except.h"
#include "debug.h"
#include "proc.h"
#include "internet.h"
#include "expr.h"

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

#if 0 /* don't use CONTEXTs in V6 */
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
#endif


/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  TCP */
/* (Also allow strings of the form "<hostname>:pppp>")  */
int
string_to_sin(char *addr, struct sockaddr_in *sin)
{
	int             i;
	char    *cur_byte;
	char    *end_string;
	int 	temp=0;
	char*	addrCpy = (char*)malloc(strlen(addr)+1);
	char*	string = addrCpy;
	char*   colon = 0;
	struct  hostent *hostptr;

	strcpy(addrCpy, addr);
	string++;					/* skip the leading '<' */

	/* allow strings of the form "<hostname:pppp>" */
	colon = strchr(string, ':');
	*colon = '\0';
	if ((hostptr=gethostbyname(string)) != NULL && hostptr->h_addrtype==AF_INET)
	{
			sin->sin_addr = *(struct in_addr *)(hostptr->h_addr_list[0]);
			string = colon + 1;
	}
	else
	{	
		/* parse the string in the traditional <xxx.yyy.zzz.aaa> form ... */	
		*colon = ':';
		cur_byte = (char *) &(sin->sin_addr);
		for(end_string = string; end_string != 0; ) {
			end_string = strchr(string, '.');
			if (end_string == 0) {
				end_string = strchr(string, ':');
				if (end_string) colon = end_string;
			}
			if (end_string) {
				*end_string = '\0';
				*cur_byte = atoi(string);
				cur_byte++;
				string = end_string + 1;
				*end_string = '.';
			}
		}
	}
	
	string[strlen(string) - 1] = '\0'; /* Chop off the trailing '>' */
	sin->sin_port = htons(atoi(string));
	sin->sin_family = AF_INET;
	string[temp-1] = '>';
	string[temp] = '\0';
	*colon = ':';
	free(addrCpy);

	return 1;
}


char *
sin_to_string(struct sockaddr_in *sin)
{
	int             i;
	static  char    buf[24];
	char    tmp_buf[10];
	char    *cur_byte;
	unsigned char   this_byte;

	buf[0] = '\0';
	if (!sin) return buf;
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

void
display_from( from )
struct sockaddr_in  *from;
{
    struct hostent  *hp, *gethostbyaddr();

	if( !from ) {
		dprintf( D_ALWAYS, "from NULL source\n" );
		return;
	}

    if( (hp=gethostbyaddr((char *)&from->sin_addr,
                sizeof(struct in_addr), from->sin_family)) == NULL ) {
        dprintf( D_ALWAYS, "from (%s), port %d\n",
            inet_ntoa(from->sin_addr), ntohs(from->sin_port) );
    } else {
        dprintf( D_ALWAYS, "from %s, port %d\n",
                                        hp->h_name, ntohs(from->sin_port) );
    }
}

char *
calc_subnet_name(char* host)
{
	struct hostent	*hostp;
	char			hostname[MAXHOSTNAMELEN];
	char			subnetname[MAXHOSTNAMELEN];
	char			*subnet_ptr;
	char			*host_addr_string;
	int				subnet_length;
	struct in_addr	in;

	if(!host)
	{
		if( gethostname(hostname, sizeof(hostname)) == -1 ) {
			dprintf( D_ALWAYS, "Gethostname failed");
			return strdup("");
		}
	}
	else
	{
		strcpy(hostname, host);
	}
	
	if( (hostp = gethostbyname(hostname)) == NULL ) {
		dprintf( D_ALWAYS, "Gethostbyname failed");
		return strdup("");
	}
	memcpy((char *) &in,(char *)hostp->h_addr, hostp->h_length);
	host_addr_string = inet_ntoa( in );
	if( host_addr_string ) {
		subnet_ptr = (char *) strrchr(host_addr_string, '.');
		if(subnet_ptr == NULL) {
			return strdup("");
		}
		subnet_length = subnet_ptr - host_addr_string;
		strncpy(subnetname, host_addr_string, subnet_length);
		subnetname[subnet_length] = '\0';
		return (strdup(subnetname));
	}
	return strdup("");
}
