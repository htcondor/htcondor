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
#include "condor_classad.h"

#include <memory>

namespace htcondor {

struct CredData;

}

	int REMOTE_CONDOR_register_job_info(const ClassAd& ad);
	int REMOTE_CONDOR_register_mpi_master_info( ClassAd *ad );
	int REMOTE_CONDOR_register_starter_info(const ClassAd& ad );
	int REMOTE_CONDOR_get_job_info( ClassAd *ad );
	int REMOTE_CONDOR_get_user_info( ClassAd *ad );
	int REMOTE_CONDOR_job_exit( int status, int reason, ClassAd *ad );
	int REMOTE_CONDOR_job_termination(const ClassAd& ad);
	int REMOTE_CONDOR_begin_execution( void );
	int REMOTE_CONDOR_open( char const *path, open_flags_t flags, int mode);
	int REMOTE_CONDOR_close( int fd );
	int REMOTE_CONDOR_unlink( char *path );
	int REMOTE_CONDOR_rename( char *path, char *newpath );
	int REMOTE_CONDOR_read( int fd, void *data, size_t length );
	int REMOTE_CONDOR_write( int fd, void *data, size_t length );
	off_t REMOTE_CONDOR_lseek( int fd, off_t offset, int whence );
	int REMOTE_CONDOR_mkdir( char *path, int mode );
	int REMOTE_CONDOR_rmdir( char *path );
	int REMOTE_CONDOR_fsync( int fd );
	int REMOTE_CONDOR_get_file_info_new( char *path, char *&url );
	int REMOTE_CONDOR_ulog_printf( int hold_reason_code, int hold_reason_subcode, char const *str, ... ) CHECK_PRINTF_FORMAT(3,4);
	int REMOTE_CONDOR_ulog_error( int hold_reason_code, int hold_reason_subcode, char const *str );
	int REMOTE_CONDOR_ulog(const ClassAd& ad);
	int REMOTE_CONDOR_get_job_attr( char *name, char *&expr );
	int REMOTE_CONDOR_set_job_attr( char *name, char *expr );
	int REMOTE_CONDOR_constrain( char *expr );
	int REMOTE_CONDOR_pread( int fd, void *data, size_t length, size_t offset );
	int REMOTE_CONDOR_pwrite( int fd , void* buf ,size_t len, size_t offset );
	int REMOTE_CONDOR_sread(int fd , void* buf , size_t len, size_t offset,
		size_t stride_length, size_t stride_skip );
	int REMOTE_CONDOR_swrite( int fd , void* buf ,size_t len, size_t offset,
		size_t stride_length, size_t stride_skip );
	int REMOTE_CONDOR_rmall( char *path );
	int REMOTE_CONDOR_fstat( int fd, char *buffer );
	int REMOTE_CONDOR_fstatfs( int fd, char *buffer );
	int REMOTE_CONDOR_fchown( int fd, int uid, int gid );
	int REMOTE_CONDOR_fchmod( int fd, int mode );
	int REMOTE_CONDOR_ftruncate( int fd, int length );
	int REMOTE_CONDOR_getfile( char *path, char **buffer );
	int REMOTE_CONDOR_putfile( char *path, int mode, int length );
	int REMOTE_CONDOR_putfile_buffer( void *buffer, int length );
	int REMOTE_CONDOR_getlongdir( char *path, char *&buffer );
	int REMOTE_CONDOR_getdir( char *path, char *&buffer );
	int REMOTE_CONDOR_whoami( int length, void *buffer);
	int REMOTE_CONDOR_whoareyou( char *host, int length, void *buffer );
	int REMOTE_CONDOR_link( char *path, char *newpath );
	int REMOTE_CONDOR_symlink( char *path, char *newpath );
	int REMOTE_CONDOR_readlink( char *path, int length, char **buffer );
	int REMOTE_CONDOR_stat( char *path, char *buffer );
	int REMOTE_CONDOR_lstat( char *path, char *buffer );
	int REMOTE_CONDOR_statfs( char *path, char *buffer );
	int REMOTE_CONDOR_access( char *path, int mode );
	int REMOTE_CONDOR_chmod( char *path, int mode );
	int REMOTE_CONDOR_chown( char *path, int uid, int gid );
	int REMOTE_CONDOR_lchown( char *path, int uid, int gid );
	int REMOTE_CONDOR_truncate( char *path, int length );
	int REMOTE_CONDOR_utime( char *path, int actime, int modtime );
	int REMOTE_CONDOR_dprintf_stats(const char *message);
	int REMOTE_CONDOR_getcreds( const char *creds_receive_dir, std::unordered_map<std::string, std::unique_ptr<htcondor::CredData>> &);
	int REMOTE_CONDOR_get_delegated_proxy( const char* proxy_source_path, const char* proxy_dest_path, time_t proxy_expiration );

	int REMOTE_CONDOR_get_sec_session_info(
		char const *starter_reconnect_session_info,
		std::string &reconnect_session_id,
		std::string &reconnect_session_info,
		std::string &reconnect_session_key,
		char const *starter_filetrans_session_info,
		std::string &filetrans_session_id,
		std::string &filetrans_session_info,
		std::string &filetrans_session_key);

	int REMOTE_CONDOR_event_notification(const ClassAd& event);

#endif




