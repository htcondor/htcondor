/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#ifndef _SCHED_H_
#define _SCHED_H_

#if defined(HPUX9)
#include "fake_flock.h"
#endif

typedef struct {
	int		p_disk_avail;
	char	*p_host;
} PERM;

typedef struct {
	int		s_users;
	int		s_wanting;
	int		s_running;
	int		s_state;
	int		s_disk_avail;
	int		s_disk_wanted;
} STATUS;

typedef struct {
	int		port1;
	int		port2;
} PORTS;

#ifdef JUNK
typedef struct {
	int		c_cmd;
} COMMAND;

typedef struct
{
	time_t	when;
	int		state;
} STATE_CHANGE;

typedef struct {
	int		user_time;
	int		ru_time;
	int		accept_time;
	int		n_changes;
	STATE_CHANGE	*changes;
} STATISTICS;

typedef struct {
	int		add_flags;
	int		del_flags;
} FLAGS;
#endif /* JUNK */

typedef struct {
	int		pid;
	int		job_id;
	int		umbilical;
} SHADOW_REC;

#if !defined(WIN32)	// no time_t in WIN32

typedef struct {
	int		clusterId;
	int		procId;
	char*	host;
	char*  	owner;
	int		q_date;
	time_t	finishTime;
	time_t	cpuTime;
} COMPLETED_JOB_INFO;

typedef struct {
	char*       capability;
	char*       server;
} FIN_MATCHES;

typedef struct {
	int		clusterID;
	int		procID;
	time_t	startTime;
	char*	host;
} RUNNING_JOB_INFO;

#endif // if !defined(WIN32)

typedef struct {        /* record sent by startd to shadow */
	int		version_num;/* always negative */
	PORTS	ports;
	int     ip_addr;    /* internet addressof executing machine */
	char*   server_name;/* name of executing machine */
} StartdRec;
	/* Startd version numbers : always negative  */
#define VERSION_FOR_FLOCK   -1

#include "condor_commands.h" /* for command id constants */

#include "condor_network.h"	/* for port numbers */

#define XDR_BLOCKSIZ (1024*4)
#define DEFAULT_MEMORY 3

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MATCH
#define MATCH 0		/* "Equality" return for strcmp() */
#endif

#ifndef ACCEPTED
#define ACCEPTED 1
#endif

#ifndef REJECTED
#define REJECTED 0
#endif

#define OK		TRUE
#define NOT_OK	FALSE

#define LOCK_JOB_QUEUE(q,m) GuardQueue(q,m,__FILE__,__LINE__)
#define CLOSE_JOB_QUEUE(q) GuardQueue(q,LOCK_UN,__FILE__,__LINE__)

/* new stuff for capability management */
#define SIZE_OF_CAPABILITY_STRING 11
#define SIZE_OF_FINAL_STRING 40					/* ipaddr:port + capability */

#define	EXITSTATUS_OK		0					/* exit status of the agent */
#define EXITSTATUS_NOTOK	1
#define EXITSTATUS_FAIL		2

#define	MATCH_ACTIVE		1					/* match record status */
#define	NOTMATCHED			0

#endif
