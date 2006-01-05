/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
	char	*args_v1or2;			/* Command arguments given by the user */
	char	*env_v1or2;	   			/* Environment variables */
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
