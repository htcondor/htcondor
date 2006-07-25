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
