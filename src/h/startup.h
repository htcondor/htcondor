/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
