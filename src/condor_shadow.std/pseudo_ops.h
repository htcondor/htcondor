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


#include "startup.h"
#include "condor_header_features.h"
 
BEGIN_C_DECLS

int pseudo_register_ckpt_platform( const char *platform, int len );
int pseudo_shell( char *command, int len );
int pseudo_get_ckpt_speed();
int pseudo_register_arch( const char *arch );
int pseudo_register_opsys( const char *opsys );
int pseudo_register_ckpt_server( const char *host );
int pseudo_register_syscall_version( const char *version );
int pseudo_register_begin_execution( void );
int pseudo_get_ckpt_mode( int sig );
int pseudo_getppid();
int pseudo_getpid();
int pseudo_getlogin(char *&loginbuf);
int pseudo_getuid();
int pseudo_geteuid();
int pseudo_getgid();
int pseudo_getegid();
int pseudo_extern_name( const char *path, char *buf, int bufsize );
#if !defined(PVM_RECEIVE)
int pseudo_reallyexit( int *status, struct rusage *use_p );
#endif
int pseudo_free_fs_blocks( const char *path );
int pseudo_image_size( int size );
int pseudo_send_rusage( struct rusage *use_p );
int pseudo_report_error( char *msg );
int pseudo_report_file_info( char *kind, char *name, int open_count, int read_count, int write_count, int seek_count, int read_bytes, int write_bytes );
int pseudo_report_file_info_new( char *name, long long open_count, long long read_count, long long write_count, long long seek_count, long long read_bytes, long long write_bytes );
int pseudo_getwd( char *&path );
int pseudo_send_a_file( const char *path, mode_t mode );
int pseudo_get_file( const char *name );
#if !defined(PVM_RECEIVE)
//int pseudo_work_request( PROC *p, char *&a_out, char *&targ, char *&orig, int *kill_sig );
#endif
int pseudo_rename(char *from, char *to);
int pseudo_get_file_stream(
		const char *file, size_t *len, unsigned int *ip_addr, u_short *port );
int pseudo_put_file_stream(
		const char *file, size_t len, unsigned int *ip_addr, u_short *port );
int pseudo_startup_info_request( STARTUP_INFO *s );
int pseudo_get_std_file_info( int which, char *&logical_name );
int pseudo_std_file_info( int which, char *&name, int *pipe_fd );
int pseudo_get_file_info_new( const char *logical_name, char *&actual_url );
int pseudo_get_file_info( const char *logical_name, char *&actual_url );
int pseudo_file_info( const char *name, int *pipe_fd, char *&extern_path );
int pseudo_get_buffer_info( int *bytes, int *block_size, int *prefetch_bytes );
int pseudo_get_iwd( char *&path );
int pseudo_get_ckpt_name( char *&path );
int pseudo_get_a_out_name( char *&path );
int pseudo_chdir( const char *path );
int pseudo_register_fs_domain( const char *fs_domain );
int pseudo_register_uid_domain( const char *uid_domain );
int pseudo_get_universe( int *my_universe );
#if !defined(PVM_RECEIVE)
int pseudo_get_username( char *&uname );
#endif
int pseudo_get_IOServerAddr(const int *reqtype, const char *filename,
							char *host, int *port );
#if !defined(PVM_RECEIVE)
int pseudo_pvm_info(char *pvmd_reply, int starter_tid);
int pseudo_pvm_task_info(int pid, int task_tid);
int pseudo_suspended(int suspended);
int pseudo_subproc_status(int subproc, int *statp, struct rusage *rusagep);
#endif /* PVM_RECEIVE */
int pseudo_lseekread(int fd, off_t offset, int whence, void *buf, size_t len);
int pseudo_lseekwrite(int fd, off_t offset, int whence, const void *buf, size_t len);

int CONDOR_NotSupported();
int SYSCALL(...);

int external_name( const char*, char*, int);
void get_local_rusage( struct rusage *bsd_rusage );
int tcp_accept_timeout( int, struct sockaddr*, int*, int );
void display_startup_info( const STARTUP_INFO *, int );

int pseudo_getrusage(int who, struct rusage *use_p );
int pseudo_sync();
int pseudo_statfs( const char *path, struct statfs *buf );
int pseudo_get_job_attr( const char *name, char *&expr );
int pseudo_set_job_attr( const char *name, const char *expr );
int pseudo_constrain( const char *expr );

END_C_DECLS

