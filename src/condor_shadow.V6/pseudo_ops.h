/* 
** Copyright 1994 by Miron Livny, Mike Litzkow and Jim Pruyne
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
**
*/ 

extern "C" { 

int pseudo_getppid();
int pseudo_getpid();
int pseudo_getlogin(char *loginbuf);
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
int pseudo_perm_error( const char *msg );
int pseudo_getwd( char *path );
int pseudo_send_file( const char *path, mode_t mode );
int pseudo_get_file( const char *name );
#if !defined(PVM_RECEIVE)
int
pseudo_work_request( PROC *p, char *a_out, char *targ, char *orig, int *kill_sig );
#endif
int pseudo_rename(char *from, char *to);
int pseudo_get_file_stream(
		const char *file, size_t *len, unsigned int *ip_addr, u_short *port );
int pseudo_put_file_stream(
		const char *file, size_t len, unsigned int *ip_addr, u_short *port );
int pseudo_startup_info_request( STARTUP_INFO *s );
int pseudo_std_file_info( int which, char *name, int *pipe_fd );
int pseudo_file_info( const char *name, int *pipe_fd, char *extern_path );
int pseudo_get_iwd( char *path );
int pseudo_get_ckpt_name( char *path );
int pseudo_get_a_out_name( char *path );
int pseudo_chdir( const char *path );
int pseudo_register_afs_cell( const char *cell );
int pseudo_register_fs_domain( const char *fs_domain );
int pseudo_register_uid_domain( const char *uid_domain );
int pseudo_get_universe( int *my_universe );
#if !defined(PVM_RECEIVE)
int pseudo_get_username( char *uname );
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
int PERM_ERR(...);

} /* extern "C" */
