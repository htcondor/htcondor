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

#ifndef CONDOR_PROC_INCLUDED
#define CONDOR_PROC_INCLUDED


typedef struct {
	int		cluster;
	int		proc;
} PROC_ID;

typedef struct {
	int		writer;	/* index into cmd array */
	int		reader;	/* index into cmd array */
	char	*name;	/* for named pipes, NULL otherwise */
} P_DESC;

	/* Condor "universes" */
#define STANDARD	1	/* Original - single process jobs, 1 per machine */
#define PIPE		2	/* Pipes - all processes on single machine */
#define LINDA		3	/* Parallel applications via Linda */
#define PVM			4	/* Parallel applications via Parallel Virtual Machine */
#define VANILLA		5	/* Non- condor linked jobs (PVMD) */
#define PVMD		6	/* Explicit, PVM daemon process */


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
	char			*env;				/* environment */

		/* Number of commands and per/command items.  Each of these items
		** is declared as a pointer, but should be allocated as an array
		** at run time.
		*/
	int				n_cmds;				/* Number of executable files */
	char			**cmd;				/* Names of executable files */
	char			**args;				/* command line args */
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
	char			pad[50];			/* make at least as big as V2 proc */
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
	char			*args;				/* command line args */
	char			*env;				/* environment */
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
	char			*args;
	char			*env;
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
** Warning!  Keep these consistent with the strings defined below
** in "JobStatusNames".
*/
#define UNEXPANDED	0
#define IDLE		1
#define RUNNING		2
#define REMOVED		3
#define COMPLETED	4
#define SUBMISSION_ERR	5

#ifdef INCLUDE_STATUS_NAME_ARRAY
char    *JobStatusNames[] = {
    "UNEXPANDED",
    "IDLE",
    "RUNNING",
    "REMOVED",
    "COMPLETED",
    "SUBMISSION_ERR",
};
#else
extern char *JobStatusNames[];
#endif


#define ICKPT -1

#endif /* CONDOR_PROC_INCLUDED */
