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

/**
This file describes the simple C interface to file table
operations that the startup and checkpoint code needs
access to.
<p>
All these functiosn are prefixed with _condor because
they will be linked with user jobs.
*/

#if defined(__cplusplus)
extern "C" {
#endif

/** Display known info about the file table */
void _condor_file_table_dump();

/** Aggravate bugs by forcing virtual fds to be different from real fds. */
void _condor_file_table_aggravate( int on_off );

/** Set up the file table if necessary.  Calling this function multiple times is harmless.  All system calls that access FileTab should call this function first. */
void _condor_file_table_init();

/** Checkpoint the state of the file table */
void _condor_file_table_checkpoint();

/** Restore the file state after a checkpoint. */
void _condor_file_table_resume();

/** Get the real fd associated with this virtual fd. */
int _condor_get_unmapped_fd( int fd );

/** Return true if this virtual fd refers to a local file. */
int _condor_is_fd_local( int user_fd );

/** Return true if this file should be accessed locally */
int _condor_is_file_name_local( const char *name, char *local_name );

/** Just before the program is about to exit, perform any necessary cleanup such as buffer flushing, data reporting, etc. */
void _condor_file_table_cleanup();

#if defined(__cplusplus)
}
#endif

#endif /* _FILE_TABLE_INTERF_H */
