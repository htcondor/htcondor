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

 

#ifndef _FILE_TABLE_INTERF_H
#define _FILE_TABLE_INTERF_H

/*
This file describes the simple C interface to file table
operations that the startup and checkpoint code needs
access to.
*/

#if defined(__cplusplus)
extern "C" {
#endif

/** Display known info about the file table */
void DumpOpenFds();

/** Set up the file table if necessary.  Calling this function
multiple times is harmless.  All system calls that access FileTab
should call this function first. */
void InitFileState();

/** Perform a periodic checkpoint of the file state. */
void CheckpointFileState();

/** Checkpoint the file state in preparation for a vacation. */
void SuspendFileState();

/** Restore the file state after a checkpoint. */
void ResumeFileState();

/** Map a virtual fd to the same real fd.  This function generally
is only called but the startup code to get a temporary working
stdin/stdout. */
int pre_open( int fd, int readable, int writeable, int is_remote );

/** This function still works, but it will eventually go away.
(It's not up to the switches to decide where to map a file to.
When mapping is in effect, talk to the open file table.) */
int MapFd( int user_fd );

/** This function still works, but it will eventually go away.
See MapFd above. */
int LocalAccess( int user_fd );


#if defined(__cplusplus)
}
#endif

#endif
