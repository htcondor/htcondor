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

#ifndef NOT_OK 
#define NOT_OK	0
#endif
#ifndef OK
#define OK		1
#endif

#define LOCK_JOB_QUEUE(q,m) GuardQueue(q,m,__FILE__,__LINE__)
#define CLOSE_JOB_QUEUE(q) GuardQueue(q,LOCK_UN,__FILE__,__LINE__)

/* new stuff for capability management */
#define SIZE_OF_CAPABILITY_STRING 40
#define SIZE_OF_FINAL_STRING 70					/* ipaddr:port + capability */

#define	EXITSTATUS_OK		0					/* exit status of the agent */
#define EXITSTATUS_NOTOK	1
#define EXITSTATUS_FAIL		2

#define	MATCH_ACTIVE		1					/* match record status */
#define	NOTMATCHED			0

#endif
