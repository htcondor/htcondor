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

#ifndef NT_SENDERS_H
#define NT_SENDERS_H

#include "condor_common.h"

class ClassAd;

extern "C" {
	int REMOTE_CONDOR_register_machine_info( char *uiddomain, char *fsdomain, char *address, char *fullHostname, int key );
	int REMOTE_CONDOR_register_mpi_master_info( ClassAd *ad );
	int REMOTE_CONDOR_register_starter_info( ClassAd *ad );
	int REMOTE_CONDOR_get_job_info( ClassAd *ad );
	int REMOTE_CONDOR_get_user_info( ClassAd *ad );
	int REMOTE_CONDOR_get_executable( char *destination );
	int REMOTE_CONDOR_job_exit( int status, int reason, ClassAd *ad );
	int REMOTE_CONDOR_begin_execution( void );
	int REMOTE_CONDOR_open( char *path, open_flags_t flags, int mode );
	int REMOTE_CONDOR_close( int fd );
	int REMOTE_CONDOR_unlink( char *path );
	int REMOTE_CONDOR_rename( char *path, char *newpath );
	int REMOTE_CONDOR_read( int fd, void *data, size_t length );
	int REMOTE_CONDOR_write( int fd, void *data, size_t length );
	int REMOTE_CONDOR_lseek( int fd, off_t offset, int whence );
	int REMOTE_CONDOR_mkdir( char *path, int mode );
	int REMOTE_CONDOR_rmdir( char *path );
	int REMOTE_CONDOR_fsync( int fd );
	int REMOTE_CONDOR_get_file_info_new( char *path, char *url );
	int REMOTE_CONDOR_ulog_printf( char const *str, ... );
	int REMOTE_CONDOR_ulog_error( char const *str );
	int REMOTE_CONDOR_ulog( ClassAd *ad );
	int REMOTE_CONDOR_get_job_attr( char *name, char *expr );
	int REMOTE_CONDOR_set_job_attr( char *name, char *expr );
	int REMOTE_CONDOR_constrain( char *expr );
}

#endif




