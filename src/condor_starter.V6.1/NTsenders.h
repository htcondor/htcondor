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


#ifndef NT_SENDERS_H
#define NT_SENDERS_H

#include "condor_common.h"

class ClassAd;

extern "C" {
	int REMOTE_CONDOR_register_job_info( ClassAd *ad );
	int REMOTE_CONDOR_register_machine_info( char *uiddomain, char *fsdomain, char const *address, char *fullHostname, int key );
	int REMOTE_CONDOR_register_mpi_master_info( ClassAd *ad );
	int REMOTE_CONDOR_register_starter_info( ClassAd *ad );
	int REMOTE_CONDOR_get_job_info( ClassAd *ad );
	int REMOTE_CONDOR_get_user_info( ClassAd *ad );
	int REMOTE_CONDOR_get_executable( char *destination );
	int REMOTE_CONDOR_job_exit( int status, int reason, ClassAd *ad );
	int REMOTE_CONDOR_begin_execution( void );
	int REMOTE_CONDOR_open( char const *path, open_flags_t flags, int mode );
	int REMOTE_CONDOR_close( int fd );
	int REMOTE_CONDOR_unlink( char *path );
	int REMOTE_CONDOR_rename( char *path, char *newpath );
	int REMOTE_CONDOR_read( int fd, void *data, size_t length );
	int REMOTE_CONDOR_write( int fd, void *data, size_t length );
	int REMOTE_CONDOR_lseek( int fd, off_t offset, int whence );
	int REMOTE_CONDOR_mkdir( char *path, int mode );
	int REMOTE_CONDOR_rmdir( char *path );
	int REMOTE_CONDOR_fsync( int fd );
	int REMOTE_CONDOR_get_file_info_new( char *path, char *&url );
	int REMOTE_CONDOR_ulog_printf( int hold_reason_code, int hold_reason_subcode, char const *str, ... ) CHECK_PRINTF_FORMAT(3,4);
	int REMOTE_CONDOR_ulog_error( int hold_reason_code, int hold_reason_subcode, char const *str );
	int REMOTE_CONDOR_ulog( ClassAd *ad );
	int REMOTE_CONDOR_get_job_attr( char *name, char *&expr );
	int REMOTE_CONDOR_set_job_attr( char *name, char *expr );
	int REMOTE_CONDOR_constrain( char *expr );

    int REMOTE_CONDOR_get_sec_session_info(
		char const *starter_reconnect_session_info,
		MyString &reconnect_session_id,
		MyString &reconnect_session_info,
		MyString &reconnect_session_key,
		char const *starter_filetrans_session_info,
		MyString &filetrans_session_id,
		MyString &filetrans_session_info,
		MyString &filetrans_session_key);

}

#endif




