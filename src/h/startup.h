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
#if !defined(_STARTUP_H)
#define _STARTUP_H

typedef struct {
	int		version_num;			/* version of this structure */
	int		cluster;				/* Condor Cluster # */
	int		proc;					/* Condor Proc # */
	int		job_class;				/* STANDARD, VANILLA, PVM, ... */
#if !defined(WIN32)	// don't know how to port these yet
	uid_t	uid;					/* Execute job under this UID */ 
	gid_t	gid;					/* Execute job under this gid */
	pid_t	virt_pid;				/* PVM virt pid of this process */
#endif
	int		soft_kill_sig;			/* Use this signal for a soft kill */
	char	*cmd;					/* Command name given by the user */
	char	*args;					/* Command arguments given by the user */
	char	*env;					/* Environment variables */
	char	*iwd;					/* Initial working directory */
	BOOLEAN	ckpt_wanted;			/* Whether user wants checkpointing */
	BOOLEAN	is_restart;				/* Whether this run is a restart */
	BOOLEAN	coredump_limit_exists;	/* Whether user wants to limit core size */
	int		coredump_limit;			/* Limit in bytes */
} STARTUP_INFO;

#define STARTUP_VERSION 1

/*
Should eliminate starter knowing the a.out name.  Should go to
shadow for instructions on how to fetch the executable - e.g. local file
with name <name> or get it from TCP port <x> on machine <y>.
*/

#endif /* _STARTUP_H */
