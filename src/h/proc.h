/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_PROC_INCLUDED
#define CONDOR_PROC_INCLUDED

#include "condor_universe.h"
#include "condor_header_features.h"

#define NEW_PROC 1

typedef struct {
	int		cluster;
	int		proc;
} PROC_ID;

typedef struct {
	int		writer;	/* index into cmd array */
	int		reader;	/* index into cmd array */
	char	*name;	/* for named pipes, NULL otherwise */
} P_DESC;

#if !defined(WIN32) // hopefully we won't need PROC structures in V6 so don't bother porting to NT
/*
** Version 3
*/
typedef struct {
	int				version_num;		/* version of this structure */
	PROC_ID			id;					/* job id */
	int				universe;			/* STANDARD, PIPE, LINDA, PVM, etc */
	int				checkpoint;			/* Whether checkpointing is wanted */
	int				remote_syscalls;	/* Whether to provide remote syscalls */
	char			*owner;				/* login of person submitting job */
	int				q_date;				/* UNIX time job was submitted */
	int				completion_date;	/* UNIX time job was completed */
	int				status;				/* Running, unexpanded, completed, .. */
	int				prio;				/* Job priority */
	int				notification;		/* Notification options */
	int				image_size;			/* Size of the virtual image in K */
	char			*env_v1or2;			/* environment */

		/* Number of commands and per/command items.  Each of these items
		** is declared as a pointer, but should be allocated as an array
		** at run time.
		*/
	int				n_cmds;				/* Number of executable files */
	char			**cmd;				/* Names of executable files */
	char			**args_v1or2;		/* command line args */
	char			**in;				/* file for stdin */
	char			**out;				/* file for stdout */
	char			**err;				/* file for stderr */
	struct rusage	*remote_usage;		/* accumulated usage on remote hosts */
	int				*exit_status;		/* final exit status */

		/* Number of pipes and their descriptions.  Each of these items
		** is declared as a pointer, but should be allocated as an array
		** at run time.
		*/
	int				n_pipes;			/* Number of pipes */
	P_DESC			*pipe;				/* Descriptions of pipes */

	/* These specify the min and max number of machines needed for
	** a PVM job
	*/
	int				min_needed;
	int				max_needed;

	char			*rootdir;			/* Root directory for chroot() */
	char			*iwd;				/* Initial working directory   */
	char			*requirements;		/* job requirements */
	char			*preferences;		/* job preferences */
	struct rusage	local_usage;		/* accumulated usage by shadows */
	char			pad[66];			/* make at least as big as V2 proc */
#if defined(NEW_PROC)
} PROC;
#else
} FUTURE_PROC;
#endif

/*
** Version 2
*/
typedef struct {
	int				version_num;		/* version of this structure */
	PROC_ID			id;					/* job id */
	char			*owner;				/* login of person submitting job */
	int				q_date;				/* UNIX time job was submitted */
	int				completion_date;	/* UNIX time job was completed */
	int				status;				/* Running, unexpanded, completed, .. */
	int				prio;				/* Job priority */
	int				notification;		/* Notification options */
	int				image_size;			/* Size of the virtual image in K */
	char			*cmd;				/* a.out file */
	char			*args_v1or2;		/* command line args */
	char			*env_v1or2;			/* environment */
	char			*in;				/* file for stdin */
	char			*out;				/* file for stdout */
	char			*err;				/* file for stderr */
	char			*rootdir;			/* Root directory for chroot() */
	char			*iwd;				/* Initial working directory   */
	char			*requirements;		/* job requirements */
	char			*preferences;		/* job preferences */
	struct rusage	local_usage;		/* accumulated usage by shadows */
	struct rusage	remote_usage;		/* accumulated usage on remote hosts */
#if defined(NEW_PROC)
} V2_PROC;
#else
} PROC;
#endif

#if defined(NEW_PROC)
#define PROC_VERSION	3
typedef PROC V3_PROC;
typedef union {
	V2_PROC v2_proc;
	V3_PROC v3_proc;
} GENERIC_PROC;
#else
#define PROC_VERSION	2
#endif

typedef struct {
	PROC_ID			id;		
	char			*owner;
	int				q_date;
	int				status;
	int				prio;
	int				notification;		/* Notification options */
	char			*cmd;
	char			*args_v1or2;
	char			*env_v1or2;
	char			*in;
	char			*out;
	char			*err;
	char			*rootdir;			/* Root directory for chroot() */
	char			*iwd;				/* Initial working directory   */
	char			*requirements;
	char			*preferences;
	struct rusage	local_usage;
	struct rusage	remote_usage;
} OLD_PROC;

#endif

/*
**	Possible notification options
*/
#define NOTIFY_NEVER		0
#define NOTIFY_ALWAYS		1
#define	NOTIFY_COMPLETE		2
#define NOTIFY_ERROR		3

typedef struct {
	int		n_ids;
	int		array_len;
	int		next_id;
	int		id[1];	/* dummy, space for actual array to be malloc'd as needed */
} CLUSTER_LIST;

#define READER	1
#define WRITER	2
#define	UNLOCK	8

/*
** Warning!  Keep these consistent with the strings defined in the
** JobStatusNames array defined in condor_util_lib/proc.c
*/
#define UNEXPANDED	0
#define IDLE		1
#define RUNNING		2
#define REMOVED		3
#define COMPLETED	4
#define	HELD		5
#define SUBMISSION_ERR	6
#define JOB_STATUS_MAX  7 /* A placeholder, not a valid status. */

// Put C funtion definitions here
BEGIN_C_DECLS

PROC_ID getProcByString( const char* str );
const char* getJobStatusString( int status );
int getJobStatusNum( const char* name );

END_C_DECLS

// Put C++ definitions here
#if defined(__cplusplus)
bool operator==( const PROC_ID a, const PROC_ID b);
#endif

#define ICKPT -1

#endif /* CONDOR_PROC_INCLUDED */
