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


#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "../condor_syscall_lib/syscall_param_sizes.h"
#include "my_hostname.h"
#include "pseudo_ops.h"
#include "spooled_job_files.h"
#include "condor_ckpt_mode.h"
#include "../condor_ckpt_server/server_interface.h"
#include "internet.h"
#include "condor_config.h"
#include "filename_tools.h"
#include "daemon.h"
#include "job_report.h"
#include "metric_units.h"
#include "condor_file_info.h"
#include "util_lib_proto.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "string_list.h"
#include "condor_socket_types.h"
#include "classad_helpers.h"
#include "condor_holdcodes.h"
#include "basename.h"
#include "MyString.h"

#include "internet_obsolete.h"
#include "ipv6_hostname.h"

extern "C" {
	void log_checkpoint (struct rusage *, struct rusage *);
	void log_image_size (int);
	void log_execute (char *);
	int access_via_afs (const char *);
	int access_via_nfs (const char *);
	int use_local_access (const char *);
	int use_special_access (const char *);
	int use_compress( const char *method, char const *path );
	int use_fetch( const char *method, char const *path );
	int use_append( const char *method, char const *path );
	void HoldJob( const char* long_reason, const char* short_reason,
				  int reason_code, int reason_subcode );
	void reaper();
}

extern int JobStatus;
extern int MyPid;
extern int ImageSize;
extern struct rusage JobRusage;
extern ReliSock *syscall_sock;
extern PROC *Proc;
extern char CkptName[];
extern char ICkptName[];
extern char RCkptName[];
extern char *CkptServerHost;
extern char *LastCkptServer;
extern int  LastCkptTime;
extern int  NumCkpts;
extern int  NumRestarts;
extern int MaxDiscardedRunTime;
extern char *ExecutingHost;
extern char *scheddName;
extern char *LastCkptPlatform;

int		LastCkptSig = 0;

/* Until the cwd is actually set, use "dot" */
MyString CurrentWorkingDir(".");

extern "C" {

int connect_file_stream( int connect_sock );
ssize_t stream_file_xfer( int src_fd, int dst_fd, size_t n_bytes );
size_t file_size( int fd );
int create_tcp_port( unsigned int *ip, u_short *port, int *fd );
void get_host_addr( unsigned int *ip_addr );
void display_ip_addr( unsigned int addr );
int has_ckpt_file();
void update_job_status( struct rusage *localp, struct rusage *remotep );
void update_job_rusage( struct rusage *localp, struct rusage *remotep );
bool JobPreCkptServerScheddNameChange();

static char Executing_Filesystem_Domain[ MAX_STRING ];
static char Executing_UID_Domain[ MAX_STRING ];
char *Executing_Arch=NULL, *Executing_OpSys=NULL;

extern char* My_Filesystem_Domain;
extern char* My_UID_Domain;
extern int	UseAFS;
extern int  UseNFS;
extern int  UseCkptServer;
extern int  StarterChoosesCkptServer;
extern int  CompressPeriodicCkpt;
extern int  CompressVacateCkpt;
extern int	PeriodicSync;
extern int  SlowCkptSpeed;
extern char *Spool;
extern ClassAd *JobAd;

// count of network bytes send and received outside of CEDAR RSC socket
extern float BytesSent, BytesRecvd;

const int MaxRetryWait = 3600;
static bool CkptWanted = true;	// WantCheckpoint from Job ClassAd
static bool RestoreCkptWithNoScheddName = false; // compat with old naming

int
pseudo_register_ckpt_platform( const char *platform, int /*len*/ )
{
	/* Here we set the platform into a global variable which the
		shadow will only be placed into the jobad (much later)
		IFF the checkpoint was actually successful. */

	if (LastCkptPlatform != NULL) {
		free(LastCkptPlatform);
	}

	LastCkptPlatform = strdup(platform);

	return 0;
}


/* performing a blocking call to system() */
int
pseudo_shell( char *command, int /*len*/ )
{
	int rval;

	dprintf( D_SYSCALLS, "\tcommand = \"%s\"\n", command );

	rval = -1;
	errno = ENOSYS;

	return rval;

	/* This feature of Condor is sort of a security hole, um, maybe
		a big security hole, so let's just never do it.  As far as
		we know, nobody uses this.  */

}

/*
**	getppid normally returns the parents id, right?  Well in
**	the CONDOR case, this job may be started and checkpointed many
**	times.  The parent will change each time.  Instead we will
**	treat it as though the parent process died and this process
**	was inherited by init.
*/
int
pseudo_getppid()
{
	return 1;
}

/*
**	getpid should return the same number each time.  As indicated
**	above, this CONDOR job may be started and checkpointed several times
**	during its lifetime, we return the condor "process id".
*/
int
pseudo_getpid()
{

	return( Proc->id.proc );
}

/* always return the owner of the job as the loginname */
int
pseudo_getlogin(char *&loginbuf)
{
	char *temp;

	ASSERT( loginbuf == NULL );
	temp = ((PROC *)Proc)->owner; 

	if( temp == NULL ) {
		return -1;
	}
	loginbuf = strdup( temp );
	return 0;
}

int
pseudo_getuid()
{
	return get_user_uid();
}

int
pseudo_geteuid()
{
	return get_user_uid();
}

int
pseudo_getgid()
{
	return get_user_gid();
}

int
pseudo_getegid()
{
	return get_user_gid();
}

int pseudo_extern_name( const char *path, char *buf, int bufsize )
{
	return external_name( path, buf, bufsize );
}


#if !defined(PVM_RECEIVE)
int
pseudo_reallyexit( int *status, struct rusage *use_p )
{
	JobStatus = *status;
	memcpy( &JobRusage, use_p, sizeof(struct rusage) );
	return 0;
}
#endif

int
pseudo_free_fs_blocks( const char *path )
{
	return sysapi_disk_space( path );
}

int
pseudo_image_size( int size )
{
	int new_size;
	if (size > 0) {
		int executable_size = 0;
		dprintf( D_SYSCALLS,
				 "\tGot Image Size report of %d kilobytes\n", size );
		JobAd->LookupInteger(ATTR_EXECUTABLE_SIZE, executable_size);
		dprintf( D_SYSCALLS, "\tAdding executable size of %d kilobytes\n",
				 executable_size );
		new_size = size + executable_size;
		if( new_size == ImageSize ) {
			dprintf( D_SYSCALLS, "Got Image Size report with same size "
					 "(%d)\n", new_size );
			return 0;
		}
		ImageSize = new_size;
		dprintf( D_SYSCALLS, "Set Image Size to %d kilobytes\n", ImageSize );
		
		// log the event
		log_image_size (ImageSize);
	} else {
		dprintf( D_ALWAYS,
				 "\tGot Image Size report of %d kilobytes!  Ignoring.\n",
				 size );
	}
	return 0;
}

static struct rusage uncommitted_rusage;

void
commit_rusage()
{
	struct rusage local_rusage;

	get_local_rusage( &local_rusage );

	if (LastCkptSig == SIGUSR2) {
		// log the periodic checkpoint event
		log_checkpoint (&local_rusage, &uncommitted_rusage);
		// update job info to the job queue
		update_job_status( &local_rusage, &uncommitted_rusage );

		// update the checkpointing signature
	} else {
		// no need to update job queue here on a vacate checkpoint, since
		// we will do it in Wrapup()
		update_job_rusage( &local_rusage, &uncommitted_rusage );
	}

	// Also, we should update the job ad to record the
	// checkpointing signature of the execution machine now
	// that we know we wrote the checkpoint properly.
	JobAd->Assign(ATTR_LAST_CHECKPOINT_PLATFORM,LastCkptPlatform);
}

int
pseudo_send_rusage( struct rusage *use_p )
{
	/* A periodic checkpoint (checkpoint and keep running) now results in the 
	 * condor_syscall_lib calling send_rusage.  So, we now
	 * update the CPU usages in the job queue. -Todd Tannenbaum 10/95. */

	/* Change (11/30/98): we now receive the rusage before the job has
	 * committed the checkpoint, so we store it in uncommitted_rusage
	 * until the checkpoint is committed or until the job exits.  -Jim B */
	
	memcpy( &uncommitted_rusage, use_p, sizeof(struct rusage) );

	return 0;
}

int
pseudo_getrusage(int /*who*/, struct rusage *use_p )
{

	// No need to look at the "who" param; the special switch for getrusage
	// already checks the "who" param and verifies it is RUSAGE_SELF

	// Pass back the rusage committed so far via checkpoint
	memcpy(use_p, &Proc->remote_usage[0], sizeof(struct rusage) );

	return 0;
}

int
pseudo_report_error( const char *msg )
{
	dprintf(D_ALWAYS,"error: %s\n",msg);
	return job_report_store_error( msg );
}

int
pseudo_report_file_info( char * /*kind*/, char *name, int open_count, int read_count, int write_count, int seek_count, int read_bytes, int write_bytes )
{
	job_report_store_file_info( name, open_count, read_count, write_count, seek_count, read_bytes, write_bytes );
	return 0;
}

int
pseudo_report_file_info_new( char *name, long long open_count, long long read_count, long long write_count, long long seek_count, long long read_bytes, long long write_bytes )
{
	job_report_store_file_info( name, open_count, read_count, write_count, seek_count, read_bytes, write_bytes );
	return 0;
}

int
pseudo_getwd_special( char *&path )
{
	path = strdup( CurrentWorkingDir.Value() );
	return 1;
}

int
pseudo_send_a_file( const char *path, mode_t mode )
{
	char	buf[ CONDOR_IO_BUF_SIZE ];
	int rval = 0;
	size_t	bytes_to_go;
	int	file_len;
	int	checksum;
	int	fd;
	int	len, nbytes;
	mode_t omask;

	dprintf(D_SYSCALLS, "\tpseudo_send_a_file(%s,0%o)\n", path, mode );

		// Want to make sure our umask is reasonable, since the user
		// job might have called umask(), which goes remote and
		// effects us, the shadow.  -Derek Wright 6/11/98
	omask = umask( 022 );

		/* Open the file for writing, send back status from the open */
	if( (fd=safe_open_wrapper_follow(path,O_WRONLY|O_CREAT|O_TRUNC,mode)) < 0 ) {
		(void)umask(omask);
		return -1;
	}
	(void)umask(omask);

	syscall_sock->encode();
	assert( syscall_sock->code(fd) );
	assert( syscall_sock->end_of_message() );

		/* Get the file length */
	syscall_sock->decode();
	assert( syscall_sock->code(file_len) );
	dprintf(D_SYSCALLS, "\tFile size is %d\n", file_len );
		
		/* Transfer all the data */
	for( bytes_to_go = file_len; bytes_to_go; bytes_to_go -= len ) {
		len = bytes_to_go < sizeof(buf) ? bytes_to_go : sizeof(buf);
		nbytes = len;
		errno = 0;
		if( !syscall_sock->code_bytes(buf,len) ) {
			dprintf( D_ALWAYS, "\tcode_bytes() failed, errno = %d\n",
													errno );
			rval = -1;
			nbytes = 0;
		}
		if( write(fd,buf,nbytes) < nbytes ) {
			dprintf( D_ALWAYS, "\tCan't write to \"%s\", errno = %d\n",
													path, errno );
			rval = -1;
			nbytes = 0;
		}
	}
	(void)close( fd );

		/* Get a repeat of the file length as a check */
	assert( syscall_sock->code(checksum) );
	assert( syscall_sock->end_of_message() );
	dprintf(D_SYSCALLS, "\tchecksum is %d\n", checksum );
	assert( checksum == file_len );

	return rval;
}

int
pseudo_get_file( const char *name )
{
	char	buf[ CONDOR_IO_BUF_SIZE ];
	int	len;
	int	read_status = 0;
	int	file_size;
	size_t bytes_to_go;
	int	fd;

		/* Open the remote file and return status from the open */
	errno = 0;
	fd = safe_open_wrapper_follow( name, O_RDONLY, 0 );
	dprintf(D_SYSCALLS, "\topen(%s,O_RDONLY,0) = %d, errno = %d\n",
														name, fd, errno );
	if( fd < 0 ) {
		return -1;
	}

		/* Send the status from safe_open_wrapper_follow() so client knows we are going ahead */
	syscall_sock->encode();
	assert( syscall_sock->code(fd) );

		/* Send the size of the file */
	file_size = lseek( fd, 0, 2 );
	(void)lseek( fd, 0, 0 );
	dprintf(D_SYSCALLS, "\tFile size is %d bytes\n", file_size );
	if( file_size < 0 ) {
		return -1;
	}
	assert( syscall_sock->code(file_size) );

		/* Transfer the data */
	for( bytes_to_go = file_size; bytes_to_go; bytes_to_go -= len ) {
		len = bytes_to_go < sizeof(buf) ? bytes_to_go : sizeof(buf);
		if( read(fd,buf,len) < len ) {
			dprintf( D_ALWAYS, "Can't read from \"%s\"\n", name );
			read_status = -1;
		}
		assert( syscall_sock->code_bytes(buf,len) );
	}
	(void)close( fd );

		/* Send the size of the file again as a check */
	assert( syscall_sock->code(file_size) );
	assert( syscall_sock->end_of_message() );
	dprintf(D_SYSCALLS, "\tSent file size %d\n", file_size );

		/* Send final status */
	return read_status;
}

#include "startup.h"

/*
  Gather up the stuff needed to satisfy a request for work from the
  condor_starter.
*/

#if !defined(PVM_RECEIVE)
int
pseudo_work_request( PROC *p, char *&a_out, char *&targ, char *&orig, int *kill_sig )
{
	struct stat	st_buf;
	int soft_kill = SIGUSR1;

	ASSERT( a_out == NULL );
	ASSERT( targ == NULL );
	ASSERT( orig == NULL );

		/* Copy Proc struct into space provided */
	memcpy( p, (PROC *)Proc, sizeof(PROC) );

	if( stat(ICkptName,&st_buf) == 0 ) {
		a_out = strdup( ICkptName );
	} else {
		EXCEPT( "Can't find initial checkpoint file %s", ICkptName);
	}

		/* Copy current checkpoint name into space provided */
	if( stat(CkptName,&st_buf) == 0 ) {
		orig = strdup( CkptName );
	} else if( stat(ICkptName,&st_buf) == 0 ) {
		orig = strdup( ICkptName );
	} else {
		EXCEPT( "Can't find any checkpoint file: CkptFile='%s' "
			"or ICkptFile='%s'", CkptName, ICkptName);
	}

		/* Copy next checkpoint name into space provided */
	targ = strdup( RCkptName );

		/* Copy kill signal number into space provided */
	memcpy( kill_sig, &soft_kill, sizeof(soft_kill) );
	return 0;
}
#endif


/*
  Returns true if this path points to the checkpoint
  file for this job.  Used to determine if we should switch to 
  Condor privileges to access the file.
*/
bool
is_ckpt_file(const char path[])
{
	char *test_path;

	test_path = gen_ckpt_name( Spool, Proc->id.cluster, Proc->id.proc, 0 );
	if (strcmp(path, test_path) == 0) {
		free(test_path); test_path = NULL;
		return true;
	}
	strcat(test_path, ".tmp");
	if (strcmp(path, test_path) == 0) {
		free(test_path); test_path = NULL;
		return true;
	}

	free(test_path); test_path = NULL;
	return false;
}

/*
  Returns true if this path points to the executable file
  for this job.  Used to determine if we should switch to 
  Condor privileges to access the file.
*/
bool
is_ickpt_file(const char path[])
{
	char *test_path;

	test_path = gen_ckpt_name( Spool, Proc->id.cluster, ICKPT, 0 );
	if (strcmp(path, test_path) == 0) {
		free(test_path); test_path = NULL;
		return true;
	}

	free(test_path); test_path = NULL;
	return false;
}


/*
   rename() becomes a psuedo system call because the file may be a checkpoint
   stored on the checkpoint server.  If it is a checkpoint file and we're
   using the checkpoint server, then we send the rename command to the
   checkpoint server.  Also, if it is a checkpoint file, we commit the
   job's rusage here.

   Also, be sure we've got a reasonable umask() when we twiddle
   checkpoint files, since the user job might have called umask(),
   which would have gone remote, and would now be in effect here in
   the shadow.   -Derek Wright 6/11/98
*/
int
pseudo_rename(char *from, char *to)
{
	int		rval;
	PROC *p = (PROC *)Proc;
	bool	CkptFile = is_ckpt_file(from) && is_ckpt_file(to);
	priv_state	priv;
	mode_t	omask;

	if (!CkptFile || !UseCkptServer) {

		if (CkptFile) {
			priv = set_condor_priv();
			omask = umask( 022 );
		}

		rval = rename(from, to);

		if (CkptFile) {
			(void)umask( omask );
			set_priv(priv);
			if (rval == 0) {	// if the rename was successful
					// Commit our usage and ckpt location before removing
					// the checkpoint left on a checkpoint server.
				char *PrevCkptServer = LastCkptServer;
				LastCkptServer = NULL; // stored ckpt file on local disk
				LastCkptTime = time(0);
				NumCkpts++;
				commit_rusage();
					// We just successfully wrote a local checkpoint,
					// so we should remove the checkpoint we left on
					// a checkpoint server, if any.
				if (PrevCkptServer) {
					SetCkptServerHost(PrevCkptServer);
					RemoveRemoteFile(p->owner, scheddName, to);
					if (JobPreCkptServerScheddNameChange()) {
						RemoveRemoteFile(p->owner, NULL, to);
					}
					free(PrevCkptServer);
				}
			}
		}

		return rval;

	} else {

		if (RenameRemoteFile(p->owner, scheddName, from, to) < 0)
			return -1;
			// Commit our usage and ckpt location before removing
			// our previous checkpoint.
		char *PrevCkptServer = LastCkptServer;
		LastCkptServer = strdup(CkptServerHost);
		LastCkptTime = time(0);
		NumCkpts++;
		commit_rusage();
			// if we just wrote a checkpoint to a new checkpoint server,
			// we should remove any previous checkpoints we left around.
		if (!PrevCkptServer) {
				// previous checkpoint is on the local disk
			priv = set_condor_priv();
			omask = umask( 022 );
			unlink(from);
			unlink(to);
			(void)umask( omask );
			set_priv(priv);
		} else {
			if (same_host(PrevCkptServer, CkptServerHost) == FALSE) {
					// previous checkpoint is on a different ckpt server
				SetCkptServerHost(PrevCkptServer);
				RemoveRemoteFile(p->owner, scheddName, to);
				if (JobPreCkptServerScheddNameChange()) {
					RemoveRemoteFile(p->owner, NULL, to);
				}
				SetCkptServerHost(CkptServerHost);
			}
			free(PrevCkptServer);
		}
		if (LastCkptServer) free(LastCkptServer);
		if (CkptServerHost) LastCkptServer = strdup(CkptServerHost);
		LastCkptTime = time(0);
		NumCkpts++;
		commit_rusage();
		return 0;

	}

	return -1;					// should never get here
}

/*
  Provide a process which will serve up the requested file as a
  stream.  The ip_addr and port number passed back are in host
  byte order.
*/
int
pseudo_get_file_stream(
		const char *file, size_t *len, unsigned int *ip_addr, u_short *port )
{
	int		connect_sock;
	int		data_sock;
	int		file_fd;
	size_t	bytes_sent;
	pid_t	child_pid;
	int		rval;
	PROC *p = (PROC *)Proc;
	int		retry_wait;
	bool	CkptFile = is_ckpt_file(file);
	bool	ICkptFile = is_ickpt_file(file);
	priv_state	priv = PRIV_UNKNOWN;
	struct in_addr taddr;
	MyString buf;

	dprintf( D_ALWAYS, "\tEntering pseudo_get_file_stream\n" );
	dprintf( D_ALWAYS, "\tfile = \"%s\"\n", file );

	*len = 0;

	if (CkptFile && LastCkptServer) {
			// If LastCkptServer is not NULL, we stored our last checkpoint
			// file on the checkpoint server.
		SetCkptServerHost(LastCkptServer);
		retry_wait = 5;
		do {
			rval = RequestRestore(p->owner,
								  (RestoreCkptWithNoScheddName) ? NULL :
								  								  scheddName,
								  file,len,
								  (struct in_addr*)ip_addr,port);
			taddr.s_addr = *ip_addr;
			buf = inet_ntoa(taddr);

			if (rval) { // network error, try again
				dprintf(D_ALWAYS, "ckpt server restore to %s failed, "
						"trying again in %d seconds\n", 
						buf.Value(),
						retry_wait);
				sleep(retry_wait);
				retry_wait *= 2;
				if (retry_wait > MaxRetryWait) {
					EXCEPT("ckpt server restore to %s failed", buf.Value());
				}
			}
		} while (rval);

		*ip_addr = ntohl( *ip_addr );
		*port = ntohs( *port );
		dprintf( D_ALWAYS, "RestoreRequest returned address:\n");
		display_ip_addr( *ip_addr );
		dprintf( D_ALWAYS, "at port %d\n", *port );
		BytesSent += *len;
		NumRestarts++;
		return 0;
	}

		// need Condor privileges to access checkpoint files
	if (CkptFile || ICkptFile) {
		priv = set_condor_priv();
	}

	file_fd=safe_open_wrapper_follow(file,O_RDONLY);

	if (CkptFile || ICkptFile) set_priv(priv); // restore user privileges

	if (file_fd < 0) return -1;

	*len = file_size( file_fd );
	dprintf( D_FULLDEBUG, "\tlen = %lu\n", (unsigned long)*len );
	BytesSent += *len;

    //get_host_addr( ip_addr );
	//display_ip_addr( *ip_addr );

    create_tcp_port( ip_addr, port, &connect_sock );
    dprintf( D_NETWORK, "\taddr = %s\n", ipport_to_string(htonl(*ip_addr), htons(*port)));
		
	switch( child_pid = fork() ) {
	case -1:	/* error */
		dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
		if (CkptFile || ICkptFile) set_priv(priv);
		close(file_fd);
		return -1;
	case 0:	/* the child */
			// reset this so dprintf has the right pid in the header
		MyPid = getpid();
		data_sock = connect_file_stream( connect_sock );
		if( data_sock < 0 ) {
			exit( 1 );
		}
		dprintf( D_FULLDEBUG, "\tShould Send %lu bytes of data\n", 
				 (unsigned long)*len );
		bytes_sent = stream_file_xfer( file_fd, data_sock, *len );
		if (bytes_sent != *len) {
			dprintf(D_ALWAYS,
					"Failed to transfer %lu bytes (only sent %ld)\n",
					(unsigned long)*len, bytes_sent);
			exit(1);
		}

		exit( 0 );
	default:	/* the parent */
		close( file_fd );
		close( connect_sock );
		if (CkptFile || ICkptFile) {
			set_priv(priv);
			if (CkptFile) NumRestarts++;
		}
		return 0;
	}

		/* Can never get here */
	return -1;
}

/*
  Provide a process which will accept the given file as a
  stream.  The ip_addr and port number passed back are in host
  byte order.

  Note: len can be -1 here, if the sender doesn't know how much it's
  going to send (for example, in the case of compressed checkpoints).
  That causes problems for our accounting and network bandwidth
  allocation.  We really should get an estimate somehow of how much
  the sender is going to send and record how much it actually ends up
  sending.
*/
int
pseudo_put_file_stream(
		const char *file, size_t len, unsigned int *ip_addr, u_short *port )
{
	int		connect_sock;
	int		data_sock;
	int		file_fd;
	int		bytes_read;
	int		answer;
	pid_t	child_pid;
	int		rval;
	PROC *p = (PROC *)Proc;
	bool	CkptFile = is_ckpt_file(file);
	bool	ICkptFile = is_ickpt_file(file);
	int		retry_wait = 5;
	priv_state	priv = PRIV_UNKNOWN;
	mode_t	omask;
	struct in_addr taddr;
	MyString buf;

	dprintf( D_ALWAYS, "\tEntering pseudo_put_file_stream\n" );
	dprintf( D_ALWAYS, "\tfile = \"%s\"\n", file );
	dprintf( D_ALWAYS, "\tlen = %lu\n", len );
	dprintf( D_ALWAYS, "\towner = %s\n", p->owner );

    // ip_addr will be updated down below because I changed create_tcp_port so that
    // ip address the socket is bound to is determined by that function. Therefore,
    // we don't need to initialize ip_addr here
    //get_host_addr( ip_addr );
	//display_ip_addr( *ip_addr );

		/* open the file */
	if (CkptFile && UseCkptServer) {
		SetCkptServerHost(CkptServerHost);
		do {
            // ip_addr is set within RequestStore
			rval = RequestStore(p->owner, scheddName, file, len,
								(struct in_addr*)ip_addr, port);
			taddr.s_addr = *ip_addr;
			buf = inet_ntoa(taddr);

			if (rval) {	/* request denied or network error, try again */
				dprintf(D_ALWAYS, "store request to ckpt server %s failed, "
						"trying again in %d seconds\n", 
						buf.Value(),
						retry_wait);
				sleep(retry_wait);
				retry_wait *= 2;
				if (retry_wait > MaxRetryWait) {
					EXCEPT("ckpt server store to %s failed", buf.Value());
				}
			}
		} while (rval);

		*ip_addr = ntohl(*ip_addr);
		*port = ntohs( *port );
		dprintf(D_ALWAYS,  "StoreRequest returned addr:\n");
		display_ip_addr( *ip_addr );
		dprintf(D_ALWAYS, "at port: %d\n", *port);
	} else {
		rval = -1;
	}
	if (!rval) {
		/* Checkpoint server says ok */
		if (len > 0) {
				// need to handle the len == -1 case someday
			BytesRecvd += len;
		}
		return 0;
	}

	// need Condor privileges to access checkpoint files
	if (CkptFile || ICkptFile) {
		priv = set_condor_priv();
	}

		// Want to make sure our umask is reasonable, since the user
		// job might have called umask(), which goes remote and
		// effects us, the shadow.  -Derek Wright 6/11/98
	omask = umask( 022 );

	int open_attempts=0;
	for(open_attempts=0;open_attempts<100;open_attempts++) {
		file_fd=safe_open_wrapper_follow(file,O_WRONLY|O_CREAT|O_TRUNC,0664);

		if( file_fd < 0 && errno == ENOENT && (CkptFile || ICkptFile) ) {

				// Perhaps the shared spool directory hierarchy does
				// not yet exist for this job.  In case of a race in
				// which we create the directory and something else
				// removes it, we try multiple times.

			if( !SpooledJobFiles::createJobSpoolDirectory( JobAd, PRIV_CONDOR ) ) {
				dprintf(D_ALWAYS,"Failed to create parent spool directory for %s\n",file);
				break;
			}
			continue;
		}
		break;
	}

	if( file_fd < 0 && open_attempts > 0 ) {
		dprintf(D_ALWAYS,"Failed to open spool file %s after %d tries.\n",file,open_attempts);
	}

	if( file_fd < 0 ) {
		if (CkptFile || ICkptFile) set_priv(priv);	// restore user privileges
		(void)umask(omask);
		return -1;
	}

	(void)umask(omask);

	//get_host_addr( ip_addr );
	//display_ip_addr( *ip_addr );

	create_tcp_port(ip_addr, port, &connect_sock);
    dprintf(D_NETWORK, "\taddr = %s\n", ipport_to_string(htonl(*ip_addr), htons(*port)));
	
	switch( child_pid = fork() ) {
	  case -1:	/* error */
		dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
		if (CkptFile || ICkptFile) set_priv(priv);	// restore user privileges
		close(file_fd);
		return -1;
	  case 0:	/* the child */
			// reset this so dprintf has the right pid in the header
		MyPid = getpid();
		data_sock = connect_file_stream( connect_sock );
		if( data_sock < 0 ) {
			exit( 1 );
		}
		bytes_read = stream_file_xfer( data_sock, file_fd, len );
		if ( fsync( file_fd ) < 0 ) {
			// if fsync() fails, send failure back to peer 
			bytes_read = -1;
		}

			/* Send status assuring peer that we got everything */
		answer = htonl( bytes_read );
		write( data_sock, &answer, sizeof(answer) );
		dprintf( D_ALWAYS,
			"\tSTREAM FILE RECEIVED OK (%d bytes)\n", bytes_read );

		exit( 0 );
	  default:	/* the parent */
	    close( file_fd );
		close( connect_sock );
		if (CkptFile || ICkptFile) {
			set_priv(priv);	// restore user privileges
			if (CkptFile) NumRestarts++;
		}
		if (len > 0) {
				// need to handle the len == -1 case someday
			BytesRecvd += len;
		}
		return 0;
	}

		/* Can never get here */
	return -1;
}

/*
  Accept a TCP connection on the connect_sock.  Close the connect_sock,
  and return the new fd.
*/
int
connect_file_stream( int connect_sock )
{
	struct sockaddr from;
	int		from_len = sizeof(from); /* FIX : dhruba */
	int		data_sock;

	if( (data_sock=tcp_accept_timeout(connect_sock, &from, 
									  &from_len, 300)) < 0 ) {
		dprintf(D_ALWAYS,
				"connect_file_stream: accept failed (%d), errno = %d\n",
				data_sock, errno);
		return -1;
	}
	dprintf( D_NETWORK, "\tGot data connection at fd %d\n", data_sock );

	close( connect_sock );
	return data_sock;
}


/*
  Determine the size of the file referenced by the given descriptor.
*/
size_t
file_size( int fd )
{
	off_t	cur_pos;
	size_t	answer;

		/* find out where we are in the file */
	if( (cur_pos=lseek(fd,0,1)) < 0 ) {
		return 0;
	}

		/* determine the file's size */
	if( (answer=lseek(fd,0,2)) == (size_t)-1) {
		return 0;
	}

		/* seek back to where we were in the file */
	if( lseek(fd,cur_pos,0) < 0 ) {
		return 0;
	}

	return answer;
}

/*
  Create a tcp socket and begin listening for a connection by some
  other process.  Using "ip", "port", and "fd" as output parameters, give back
  the address to which the other process should connect, and the
  file descriptor with which this process can refer to the socket.  Return
  0 if everything works OK, and -1 otherwise.  N.B. The port number is
  returned in host byte order.
*/
int
create_tcp_port( unsigned int *ip, u_short *port, int *fd )
{
    struct sockaddr_in  sin;
    SOCKET_LENGTH_TYPE  addr_len;
   
    addr_len = sizeof(sin);

        /* create a tcp socket */
    if( (*fd=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
        EXCEPT( "create_tcp_port(): socket() failed: %d(%s)", 
			errno, strerror(errno) );
    }
    dprintf( D_FULLDEBUG, "\tconnect_sock = %d\n", *fd );

        /* bind it to an address */

        /* FALSE means this is an incoming connection */
    if( ! _condor_local_bind(FALSE, *fd) ) {
        EXCEPT( "create_tcp_port(): bind() failed: %d(%s)",
			errno, strerror(errno) );
    }

        /* determine which local port number we were assigned to */
    struct sockaddr_in *tmp = getSockAddr(*fd);
    if (tmp == NULL) {
        EXCEPT("create_tcp_port(): getSockAddr() failed");
    }
    *ip = ntohl(tmp->sin_addr.s_addr);
    *port = ntohs(tmp->sin_port);

        /* Get ready to accept a connection */
    if( listen(*fd,1) < 0 ) {
        EXCEPT( "create_tcp_port(): listen() failed: %d(%s)", 
			errno, strerror(errno) );
    }
    dprintf( D_FULLDEBUG, "\tListening...\n" );

    return 0;
}


/*
  Get the IP address of this host.  The address is in host byte order.
*/
void
get_host_addr( unsigned int *ip_addr )
{
	// [TODO:IPV6]
	condor_sockaddr myaddr = get_local_ipaddr();
	sockaddr_in saddr = myaddr.to_sin();
	*ip_addr = ntohl(saddr.sin_addr.s_addr);
	display_ip_addr( *ip_addr );
}

#define B_NET(x) (((long)(x)&IN_CLASSB_NET)>>IN_CLASSB_NSHIFT)
#define B_HOST(x) ((long)(x)&IN_CLASSB_HOST)
#define HI(x) (((long)(x)&0xff00)>>8)
#define LO(x) ((long)(x)&0xff)


/*
  Display an IP address.  The address is in host byte order.
*/
void
display_ip_addr( unsigned int addr )
{
	dprintf( D_ALWAYS, "\t%lu.%lu.%lu.%lu\n",
		(unsigned long) HI(B_NET(addr)),
		(unsigned long) LO(B_NET(addr)),
		(unsigned long) HI(B_HOST(addr)),
		(unsigned long) LO(B_HOST(addr))
	);
}

/*
  Gather up the stuff needed to satisfy a request for work from the
  condor_starter.  This is the new version which passes a STARUP_INFO
  structure rather than a PROC structure.
*/
char *strdup( const char *);
char *Strdup( const char *str)
{
	char*	dup;

	if(str == NULL)
	{
		dup = (char *)malloc(1);
		dup[0] = '\0';
		return dup;
	}
	return strdup(str);
}

int
pseudo_startup_info_request( STARTUP_INFO *s )
{
	PROC *p = (PROC *)Proc;

	s->version_num = STARTUP_VERSION;

	s->cluster = p->id.cluster;
	s->proc = p->id.proc;
	s->job_class = p->universe;
	s->uid = get_user_uid();
	s->gid = get_user_gid();

		/* This is the shadow's virtual pid for the PVM task, and will
		   need to be supplied when we get around to using the structure
		   with PVM capable shadows.
		*/
	s->virt_pid = -1;

	int soft_kill = findSoftKillSig( JobAd );
	if( soft_kill < 0 ) {
		soft_kill = SIGTERM;
	}
	s->soft_kill_sig = soft_kill;

	s->cmd = Strdup( p->cmd[0] );
	s->args_v1or2 = Strdup( p->args_v1or2[0] );
	s->env_v1or2 = Strdup( p->env_v1or2 );
	s->iwd = Strdup( p->iwd );

	if (JobAd->LookupBool(ATTR_WANT_CHECKPOINT, s->ckpt_wanted) == 0) {
		s->ckpt_wanted = TRUE;
	}

	CkptWanted = s->ckpt_wanted;

	s->is_restart = has_ckpt_file();

	if (JobAd->LookupInteger(ATTR_CORE_SIZE, s->coredump_limit) == 1) {
		s->coredump_limit_exists = TRUE;
	} else {
		s->coredump_limit_exists = FALSE;
	}

	display_startup_info( s, D_SYSCALLS );
	return 0;
}

/*
Given the fd number of a standard file (fd 0, 1, or 2), we return
a logical file name.  To transform a logical file name into a physical
url, call get_file_info.
*/

int
pseudo_get_std_file_info( int which, char *&logical_name )
{
	PROC	*p = (PROC *)Proc;

	ASSERT( logical_name == NULL );

	switch( which ) {
		case 0:
			logical_name = strdup( p->in[0] );
			break;
		case 1:
			logical_name = strdup( p->out[0] );
			break;
		case 2:
			logical_name = strdup( p->err[0] );
			break;
		default:
			logical_name = strdup("");
			break;
	}

	dprintf( D_SYSCALLS, "\tfd = %d\n", which );
	dprintf( D_SYSCALLS, "\tlogical name: \"%s\"\n",logical_name);

	return 0;
}

/*
This is for compatibility with the old starter.
It will go away in the near future.
*/

int
pseudo_std_file_info( int which, char *&logical_name, int *pipe_fd )
{
	*pipe_fd = -1;
	return pseudo_get_std_file_info( which, logical_name );
}


/*
If short_path is an absolute path, copy it to full path.
Otherwise, tack the current directory on to the front
of short_path, and copy it to full_path.
*/
 
static void complete_path( const char *short_path, MyString &full_path )
{
	if(short_path[0]=='/') {
		full_path = short_path;
	} else {
		full_path = CurrentWorkingDir;
		full_path += "/";
		full_path += short_path;
	}
}

/*
These calls translates a logical path name specified by a user job
into an actual url which describes how and where to fetch
the file from.

For example, joe/data might become buffer:remote:/usr/joe/data

The "new" version of the call allows the nesting of methods.
The "old" version of the call only returns the basic method,
such as "local:" or "remote:" and assumes the syscall lib will
add the necessary transformations.
*/

int do_get_file_info( char const *logical_name, char *&url, int allow_complex );
void append_buffer_info( MyString &url, const char *method, char const *path );

int
pseudo_get_file_info_new( const char *logical_name, char *&actual_url )
{
	return do_get_file_info( logical_name, actual_url, 1 );
}

int
pseudo_get_file_info( const char *logical_name, char *&actual_url )
{
	return do_get_file_info( logical_name, actual_url, 0 );
}

int
do_get_file_info( char const *logical_name, char *&actual_url, int allow_complex )
{
	MyString	remap_list;
	MyString	split_dir;
	MyString	split_file;
	MyString	full_path;
	MyString	remap;
	const char	*method;
	int		want_remote_io;
	MyString url_buf;
	static int		warned_once = FALSE;

	dprintf( D_SYSCALLS, "\tlogical_name = \"%s\"\n", logical_name );
	ASSERT( actual_url == NULL );

	/* if this attribute is not present, then default to TRUE */
	if ( ! JobAd->LookupInteger( ATTR_WANT_REMOTE_IO, want_remote_io ) ) {
		want_remote_io = TRUE;
	} 

	/* output some debugging info on the first call to this RPC */
	if (warned_once == FALSE) {

		warned_once = TRUE;

		if (want_remote_io == TRUE) {
			dprintf( D_SYSCALLS, 
				"\tshadow process always deciding location of url\n" );
		} else {
			dprintf( D_SYSCALLS, 
				"\tshadow process permanently gives up deciding location of "
				"url and informs job to default all urls to 'local:' "
				"(except [i]ckpt).\n");
		}
	}

	/* The incoming logical name might be a simple, relative, or complete path */
	/* We need to examine both the full path and the simple name. */

	filename_split( logical_name, split_dir, split_file );
	complete_path( logical_name, full_path );

	/* Any name comparisons must check the logical name, the simple name, and the full path */

	if(JobAd->LookupString(ATTR_FILE_REMAPS,remap_list) &&
	  (filename_remap_find( remap_list.Value(), logical_name, remap ) ||
	   filename_remap_find( remap_list.Value(), split_file.Value(), remap ) ||
	   filename_remap_find( remap_list.Value(), full_path.Value(), remap ))) {

		dprintf(D_SYSCALLS,"\tremapped to: %s\n",remap.Value());

		/* If the remap is a full URL, return right away */
		/* Otherwise, continue processing */

		if(strchr(remap.Value(),':')) {
			dprintf(D_SYSCALLS,"\tremap is complete url\n");
			actual_url = strdup(remap.Value());
			if (want_remote_io == FALSE) {
				return 1;
			}
			return 0;
		} else {
			dprintf(D_SYSCALLS,"\tremap is simple file\n");
			complete_path( remap.Value(), full_path );
		}
	} else {
		dprintf(D_SYSCALLS,"\tnot remapped\n");
	}

	dprintf( D_SYSCALLS,"\tfull_path = \"%s\"\n", full_path.Value() );

	/* Now, we have a full pathname. */
	/* Figure out what url modifiers to slap on it. */

#ifdef HPUX
	/* I have no idea why this is happening, but I have seen it happen many
	 * times on the HPUX version, so here is a quick hack -Todd 5/19/95 */
	if ( full_path == "/usr/lib/nls////strerror.cat" )
		full_path = "/usr/lib/nls/C/strerror.cat\0";
#endif

	if(is_ckpt_file(full_path.Value()) || is_ickpt_file(full_path.Value())) {
		method = "remote";
	} else if( use_special_access(full_path.Value()) ) {
		method = "special";
	} else if ( want_remote_io == FALSE ) {
		method = "local";
	} else if( use_local_access(full_path.Value()) ) {
		method = "local";
	} else if( access_via_afs(full_path.Value()) ) {
		method = "local";
	} else if( access_via_nfs(full_path.Value()) ) {
		method = "local";
	} else {
		method = "remote";
	}

	if( allow_complex ) {
		if( use_fetch(method,full_path.Value()) ) {
			url_buf += "fetch:";
		}
		if( use_compress(method,full_path.Value()) ) {
			url_buf += "compress:";
		}

		append_buffer_info(url_buf,method,full_path.Value());

		if( use_append(method,full_path.Value()) ) {
			url_buf += "append:";
		}
	}

	url_buf += method;
	url_buf += ":";
	url_buf += full_path;
	actual_url = strdup(url_buf.Value());

	dprintf(D_SYSCALLS,"\tactual_url: %s\n",actual_url);

	/* This return value will make the caller in the user job never call
		the shadow ever again to see what to do about a job. Instead the
		FileTable will assume ALL files are local. */
	if (want_remote_io == FALSE) {
		return 1;
	}

	return 0;
}

void append_buffer_info( MyString &url, const char *method, char const *path )
{
	MyString buffer_list;
	MyString buffer_string;
	MyString dir;
	MyString file;
	int s,bs,ps;
	int result;

	filename_split(path,dir,file);

	/* Get the default buffer setting */
	pseudo_get_buffer_info( &s, &bs, &ps );

	/* Now check for individual file overrides */
	/* These lines have the same syntax as a remap list */

	if(JobAd->LookupString(ATTR_BUFFER_FILES,buffer_list)) {
		if( filename_remap_find(buffer_list.Value(),path,buffer_string) ||
		    filename_remap_find(buffer_list.Value(),file.Value(),buffer_string) ) {

			/* If the file is merely mentioned, turn on the default buffer */
			url += "buffer:";

			/* If there is also a size setting, use that */
			result = sscanf(buffer_string.Value(),"(%d,%d)",&s,&bs);
			if( result==2 ) url += buffer_string;

			return;
		}
	}

	/* Turn on buffering if the value is set and is not special or local */
	/* In this case, use the simple syntax 'buffer:' so as not to confuse old libs */

	if( s>0 && bs>0 && strcmp(method,"local") && strcmp(method,"special")  ) {
		url += "buffer:";
	}
}

/* Return true if this JobAd attribute contains this path */

static int attr_list_has_file( const char *attr, const char *path )
{
	char const *file;
	MyString str;

	file = condor_basename(path);

	JobAd->LookupString(attr,str);
	StringList list(str.Value());

	if( list.contains_withwildcard(path) || list.contains_withwildcard(file) ) {
		return 1;
	} else {
		return 0;
	}
}


int use_append( const char * /*method*/, char const *path )
{
	return attr_list_has_file( ATTR_APPEND_FILES, path );
}

int use_compress( const char * /*method*/, char const *path )
{
	return attr_list_has_file( ATTR_COMPRESS_FILES, path );
}

int use_fetch( const char * /*method*/, char const *path )
{
	return attr_list_has_file( ATTR_FETCH_FILES, path );
}

/*
This is for compatibility with the old starter.
It will go away in the near future.
*/

int pseudo_file_info( const char *logical_name, int * /*fd*/, char *&physical_name )
{
	char	*url = NULL;
	char	*method = NULL;
	int	result;

	ASSERT( physical_name == NULL );

	result = pseudo_get_file_info( logical_name, url );
	if(result<0) {
		free( url );
		return result;
	}

	method = (char *)malloc(strlen(url)+1);
	physical_name = (char *)malloc(strlen(url)+1);
	ASSERT( method );
	ASSERT( physical_name );

	/* glibc21 (and presumeably glibc20) have a problem where the range
		specifier can't be 8bit. So, this only allows a 7bit clean filename. */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	sscanf(url,"%[^:]:%[\x1-\x7F]",method,physical_name);
	#else
	sscanf(url,"%[^:]:%[\x1-\xFF]",method,physical_name);
	#endif

	if(!strcmp(method,"local")) {
		result = IS_LOCAL;
	} else {
		result = IS_RSC;
	}

	free( url );
	free( method );
	return result;
}

/*
Return the buffer configuration.  If the classad contains nothing,
assume it is zero.
*/

int pseudo_get_buffer_info( int *bytes_out, int *block_size_out, int *prefetch_bytes_out )
{
	int bytes=0, block_size=0;

	JobAd->LookupInteger(ATTR_BUFFER_SIZE,bytes);
	JobAd->LookupInteger(ATTR_BUFFER_BLOCK_SIZE,block_size);

	if( bytes<0 ) bytes = 0;
	if( block_size<0 ) block_size = 0;
	if( bytes<block_size ) block_size = bytes;

	*bytes_out = bytes;
	*block_size_out = block_size;
	*prefetch_bytes_out = 0;

	dprintf(D_SYSCALLS,"\tbytes=%d block_size=%d\n",bytes, block_size );

	return 0;
}


/*
  Return process's initial working directory
*/
int
pseudo_get_iwd( char *&path )
{
		/*
		  Try to log an execute event.  We had made logging the
		  execute event it's own pseudo syscall, but that broke
		  compatibility with older versions of Condor.  So, even
		  though this is still kind of hacky, it should be right,
		  since all vanilla jobs call this, and all standard jobs call
		  pseudo_chdir(), before they start executing.  log_execute()
		  keeps track of itself and makes sure it only really logs the
		  event once in the lifetime of a shadow.
		*/
	log_execute( ExecutingHost );

	PROC	*p = (PROC *)Proc;

	ASSERT( path == NULL );
	path = strdup( p->iwd );
	dprintf( D_SYSCALLS, "\tanswer = \"%s\"\n", p->iwd );
	return 0;
}


/*
  Return name of checkpoint file for this process
*/
int
pseudo_get_ckpt_name( char *&path )
{
	ASSERT( path == NULL );
	path = strdup( RCkptName );
	dprintf( D_SYSCALLS, "\tanswer = \"%s\"\n", path );
	return 0;
}

/*
  Specify checkpoint mode parameters.  See
  condor_includes/condor_ckpt_mode.h definitions.
*/
int
pseudo_get_ckpt_mode( int sig )
{
	int mode = 0;
	if (!CkptWanted) {
		mode = CKPT_MODE_ABORT;
	} else if (sig == SIGTSTP) { // vacate checkpoint
		if (CompressVacateCkpt) mode |= CKPT_MODE_USE_COMPRESSION;
		if (SlowCkptSpeed) mode |= CKPT_MODE_SLOW;
	} else if (sig == SIGUSR2) { // periodic checkpoint 
		if (CompressPeriodicCkpt) mode |= CKPT_MODE_USE_COMPRESSION;
		if (PeriodicSync) mode |= CKPT_MODE_MSYNC;
	} else if (sig == 0) {		// restart
		if (PeriodicSync) mode |= CKPT_MODE_MSYNC;
	} else {
		dprintf( D_ALWAYS,
				 "pseudo_get_ckpt_mode called with unknown signal %d\n", sig );
		mode = -1;
	}
	LastCkptSig = sig;
	return mode;
}

/*
 Specify the speed of slow checkpointing in KB/s.
*/
int
pseudo_get_ckpt_speed()
{
	return SlowCkptSpeed;
}

/*
  Return name of executable file for this process
*/
int
pseudo_get_a_out_name( char *&path )
{
	int result;

	ASSERT( path == NULL );

	// See if we can find an executable in the spool dir.
	// Switch to Condor uid first, since ickpt files are 
	// stored in the SPOOL directory as user Condor.
	priv_state old_priv = set_condor_priv();
	result = access(ICkptName,R_OK);
	set_priv(old_priv);

	if ( result >= 0 ) {
			// we can access an executable in the spool dir
		path = strdup( ICkptName );
	} else {
			// nothing in the spool dir; the executable 
			// probably has $$(opsys) in it... so use what
			// the jobad tells us.
		ASSERT(JobAd);
		JobAd->LookupString(ATTR_JOB_CMD,&path);
	}

	dprintf( D_SYSCALLS, "\tanswer = \"%s\"\n", path );
	return 0;
}

/*
  Change directory as indicated.  We also keep track of current working
  directory so we can build full pathname from relative pathnames
  were we need them.
*/
int
pseudo_chdir( const char *path )
{
	int		rval;
	
		/*
		  Try to log an execute event.  We had made logging the
		  execute event it's own pseudo syscall, but that broke
		  compatibility with older versions of Condor.  So, even
		  though this is still kind of hacky, it should be right,
		  since all standard jobs call this, and all vanilla jobs call
		  pseudo_get_iwd(), before they start executing.
		  log_execute() keeps track of itself and makes sure it only
		  really logs the event once in the lifetime of a shadow.
		*/
	log_execute( ExecutingHost );

	dprintf( D_SYSCALLS, "\tpath = \"%s\"\n", path );
	dprintf( D_SYSCALLS, "\tOrig CurrentWorkingDir = \"%s\"\n", CurrentWorkingDir.Value() );

	rval = chdir( path );

	if( rval < 0 ) {
		dprintf( D_SYSCALLS, "Failed\n" );
		return rval;
	}

	if( path[0] == '/' ) {
		CurrentWorkingDir = path;
	} else {
		CurrentWorkingDir += "/";
		CurrentWorkingDir += path;
	}
	dprintf( D_SYSCALLS, "\tNew CurrentWorkingDir = \"%s\"\n", CurrentWorkingDir.Value() );
	dprintf( D_SYSCALLS, "OK\n" );
	return rval;
}

/*
  Take note of the executing machine's filesystem domain */
int
pseudo_register_fs_domain( const char *fs_domain )
{
	strcpy( Executing_Filesystem_Domain, fs_domain );
	dprintf( D_SYSCALLS, "\tFS_Domain = \"%s\"\n", fs_domain );
	return 0;
}

/*
  Take note of the executing machine's UID domain
*/
int
pseudo_register_uid_domain( const char *uid_domain )
{
	strcpy( Executing_UID_Domain, uid_domain );
	dprintf( D_SYSCALLS, "\tUID_Domain = \"%s\"\n", uid_domain );
	return 0;
}

/*
  Take note that the job is about to start executing and log that to
  the userlog
*/
int
pseudo_register_begin_execution( void )
{
	log_execute( ExecutingHost );
	return 0;
}

int
use_local_access( const char *file )
{
	return
		!strcmp(file,"/dev/null") ||
		!strcmp(file,"/dev/zero") ||
		attr_list_has_file( ATTR_LOCAL_FILES, file );
}

/* "special" access means open the file locally, and also protect it by preventing checkpointing while it is open.   This needs to be applied to sockets (not trapped here) and special files that take the place of sockets. */

int
use_special_access( const char *file )
{
	return
		!strcmp(file,"/dev/tcp") ||
		!strcmp(file,"/dev/udp") ||
		!strcmp(file,"/dev/icmp") ||
		!strcmp(file,"/dev/ip");	
}

int
access_via_afs( const char * /*file*/ )
{
	dprintf( D_SYSCALLS, "\tentering access_via_afs()\n" );

	if( !UseAFS ) {
		dprintf( D_SYSCALLS, "\tnot configured to use AFS for file access\n" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning FALSE\n" );
		return FALSE;
	}

	if( !Executing_Filesystem_Domain[0] ) {
		dprintf( D_SYSCALLS, "\tdon't know FS domain of executing machine\n" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning FALSE\n" );
		return FALSE;
	}

	dprintf( D_SYSCALLS,
		"\tMy_FS_Domain = \"%s\", Executing_FS_Domain = \"%s\"\n",
		My_Filesystem_Domain,
		Executing_Filesystem_Domain
	);

	if( strcmp(My_Filesystem_Domain,Executing_Filesystem_Domain) != MATCH ) {
		dprintf( D_SYSCALLS, "\tFilesystem domains don't match\n" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning FALSE\n" );
		return FALSE;
	}

	dprintf( D_SYSCALLS, "\taccess_via_afs() returning TRUE\n" );
	return TRUE;
}

int
access_via_nfs( const char * /*file*/ )
{
	dprintf( D_SYSCALLS, "\tentering access_via_nfs()\n" );

	if( !UseNFS ) {
		dprintf( D_SYSCALLS, "\tnot configured to use NFS for file access\n" );
		dprintf( D_SYSCALLS, "\taccess_via_nfs() returning FALSE\n" );
		return FALSE;
	}

	if( !Executing_UID_Domain[0] ) {
		dprintf( D_SYSCALLS, "\tdon't know UID domain of executing machine" );
		dprintf( D_SYSCALLS, "\taccess_via_NFS() returning FALSE\n" );
		return FALSE;
	}

	if( !Executing_Filesystem_Domain[0] ) {
		dprintf( D_SYSCALLS, "\tdon't know FS domain of executing machine\n" );
		dprintf( D_SYSCALLS, "\taccess_via_NFS() returning FALSE\n" );
		return FALSE;
	}

	dprintf( D_SYSCALLS,
		"\tMy_FS_Domain = \"%s\", Executing_FS_Domain = \"%s\"\n",
		My_Filesystem_Domain,
		Executing_Filesystem_Domain
	);

	dprintf( D_SYSCALLS,
		"\tMy_UID_Domain = \"%s\", Executing_UID_Domain = \"%s\"\n",
		My_UID_Domain,
		Executing_UID_Domain
	);

	if( strcmp(My_Filesystem_Domain,Executing_Filesystem_Domain) != MATCH ) {
		dprintf( D_SYSCALLS, "\tFilesystem domains don't match\n" );
		dprintf( D_SYSCALLS, "\taccess_via_NFS() returning FALSE\n" );
		return FALSE;
	}

	if( strcmp(My_UID_Domain,Executing_UID_Domain) != MATCH ) {
		dprintf( D_SYSCALLS, "\tUID domains don't match\n" );
		dprintf( D_SYSCALLS, "\taccess_via_NFS() returning FALSE\n" );
		return FALSE;
	}

	dprintf( D_SYSCALLS, "\taccess_via_NFS() returning TRUE\n" );
	return TRUE;
}

/*
**  Starting with version 6.2.0, we store checkpoints on the checkpoint
**  server using "owner@scheddName" instead of just "owner".  If the job
**  was submitted before this change, we need to check to see if its
**  checkpoint was stored using the old naming scheme.
*/
bool
JobPreCkptServerScheddNameChange()
{
	char job_version[150];
	job_version[0] = '\0';
	if (JobAd && JobAd->LookupString(ATTR_VERSION, job_version, sizeof(job_version))) {
		CondorVersionInfo ver(job_version, "JOB");
		if (ver.built_since_version(6,2,0) &&
			ver.built_since_date(11,16,2000)) {
			return false;
		}
	}
	return true;				// default to version compat. mode
}

int
has_ckpt_file()
{
	int		rval, retry_wait = 5;
	PROC *p = (PROC *)Proc;
	priv_state	priv;
	long	accum_usage;

	if (p->universe != CONDOR_UNIVERSE_STANDARD) return 0;
	if (!CkptWanted) return 0;
	accum_usage = p->remote_usage[0].ru_utime.tv_sec +
		p->remote_usage[0].ru_stime.tv_sec;
	priv = set_condor_priv();
	do {
		SetCkptServerHost(LastCkptServer);
		rval = FileExists(RCkptName, p->owner, scheddName);
		if (rval == 0 && JobPreCkptServerScheddNameChange()) {
			rval = FileExists(RCkptName, p->owner, NULL);
			if (rval == 1) {
				RestoreCkptWithNoScheddName = true;
			}
		}
		if(rval == -1 && LastCkptServer && accum_usage > MaxDiscardedRunTime) {
			dprintf(D_ALWAYS, "failed to contact last ckpt server at %s, "
					"trying again in %d seconds\n", LastCkptServer, retry_wait);
			sleep(retry_wait);
			retry_wait *= 2;
			if (retry_wait > MaxRetryWait) {
				EXCEPT("failed to contact last ckpt server %s", LastCkptServer);
			}
		}
	} while(rval == -1 && LastCkptServer && accum_usage > MaxDiscardedRunTime);
	if (rval == -1) { /* not on local disk & not using ckpt server */
		rval = FALSE;
	}
	set_priv(priv);
	return rval;
}

/*
  This pseudo function get_universe used to be called by condor_startd right 
  before exec-ing
  condor_starter.  The startd needs to check the universe so that
  knows if it needs to evaluate *_VANILLA parameters or the normal
  params.  Now, however, the universe is passwd to the startd by the
  shadow in the initial mini-job context.  We now have a rule: "no
  remote syscalls in the startd!"  why?  cuz we never ever want the
  startd to sit and block on something...  -Todd Tannenbaum, 8/95
*/
int
pseudo_get_universe( int *my_universe )
{
	PROC	*p = (PROC *)Proc;

	*my_universe = p->universe;

	return 0;
}


#if !defined(PVM_RECEIVE)
int
pseudo_get_username( char *&uname )
{
	PROC	*p = (PROC *)Proc;

	ASSERT( uname == NULL );
	uname = strdup( p->owner );
	return 0;
}
#endif


void
simp_log( const char *msg )
{
	static FILE	*fp;
	static char name[] = "/tmp/condor_log";
	int		fd;
	mode_t	omask;

		// Want to make sure our umask is reasonable, since the user
		// job might have called umask(), which goes remote and
		// effects us, the shadow.  -Derek Wright 6/11/98
	omask = umask( 022 );

	if( !fp ) {
		if( (fd=safe_open_wrapper_follow(name,O_WRONLY|O_CREAT,0666)) < 0 ) {
			EXCEPT( "Can't create/open \"%s\"", name );
		}
		close( fd );

		fp = safe_fopen_wrapper( name, "a" );
		if( fp == NULL ) {
			EXCEPT( "Can't open \"%s\" for appending", name );
		}
	}

	fprintf( fp, msg );
	(void)umask( omask );
}

int pseudo_get_IOServerAddr(const int * /*reqtype*/, const char * /*filename*/,
							char * /*host*/, int * /*port*/ )
{
        /* Should query the collector or look in the config file for server
           names.  Always return -1 until this can be fixed.  -Jim B. */

#if 0
  int i;
  char *servers[5] = 
  {
    "vega15", 
    "vega15", 
    "vega15", 
    "vega15", 
    "vega15"
  };

  int ports[5] = 
  {
    5078, 5083, 5087, 5042, 5047
  };

  
  dprintf(D_SYSCALLS,"in pseudo_get_IOserver .. req_type = %s\n", (*reqtype==OPE
N_REQ)?"open_req":"server_down");
  
  switch (*reqtype)
    {
    case OPEN_REQ:
      strcpy(host,servers[0]);
      *port = ports[0];
      dprintf(D_SYSCALLS,"server = %s, port = %d\n",host,*port); 
      return 0;
    case SERVER_DOWN:
      for(i=0;i<5;i++)
        {
          if ((strcmp(host, servers[i]) == 0) && (*port == ports[i])) 
            break;
        }
      if (i<4) 
        {
          strcpy(host,servers[i+1]);
          *port = ports[i+1];
          return 0;
        }
      return -1;
    }
#endif

  return -1;
}

#if !defined(PVM_RECEIVE)
int
pseudo_pvm_info(char * /*pvmd_reply*/, int /*starter_tid*/)
{
	return 0;
}


int
pseudo_pvm_task_info(int /*pid*/, int /*task_tid*/)
{
	return 0;
}


int
pseudo_suspended(int /*suspended*/)
{
	return 0;
}


int
pseudo_subproc_status(int /*subproc*/, int * /*statp*/, struct rusage *rusagep)
{
	struct rusage local_rusage;

	get_local_rusage( &local_rusage );
	memcpy( &JobRusage, rusagep, sizeof(struct rusage) );
	update_job_status( &local_rusage, &JobRusage );

	// Rusages are logged when a job is evicted, or if it terminates
    // I'm not sure if this logging step is required.  Skip for now.
    // Of course, need to convert to condor_event  --RR
    // LogRusage( ULog,  THIS_RUN, &local_rusage, &JobRusage );

	return 0;
}

#endif /* PVM_RECEIVE */

/* XXX Hrm, there is a type size mismatch between the off_t and the ssize_t
	from read's return type for this function. That is annoying. At
	least they are both of the right signedness.
*/
off_t
pseudo_lseekread(int fd, off_t offset, int whence, void *buf, size_t len)
{
        int rval;

        if ( lseek( fd, offset, whence ) < 0 ) {
                return (off_t) -1;
        }

        rval = read( fd, buf, len );
        return rval;    
}

/* XXX Hrm, there is a type size mismatch between the off_t and the ssize_t
	from write's return type for this function. That is annoying. At
	least they are both of the right signedness.
*/
off_t
pseudo_lseekwrite(int fd, off_t offset, int whence, const void *buf, size_t len)
{
	int rval;

	if ( lseek( fd, offset, whence ) < 0 ) {
		return (off_t) -1;
	}

	rval = write( fd, buf, len );
	return rval;
}


/* 
   For sync(), we just want to call sync, since on most systems,
   sync() just returns void, and we'll pass back a 0 anyway. 
   -Derek Wright 7/17/98
*/
int
pseudo_sync()
{
    sync();
	return 0;
}

/*
   Allow the starter to specify our checkpoint server.  We only use
   the starter's recommendation if the user didn't explicitly tell us
   not to by setting USE_CKPT_SERVER = False or
   STARTER_CHOOSES_CKPT_SERVER = False.
*/
int
pseudo_register_ckpt_server(const char *host)
{
	if (StarterChoosesCkptServer) {
		if (param_boolean_crufty("USE_CKPT_SERVER", true)) {
			if (CkptServerHost) free(CkptServerHost);
			CkptServerHost = strdup(host);
			UseCkptServer = TRUE;
		}
	}

	return 0;
}

int
pseudo_register_arch(const char *arch)
{
	if (Executing_Arch) free(Executing_Arch);
	Executing_Arch = strdup(arch);
	return 0;
}

int
pseudo_register_opsys(const char *opsys)
{
	if (Executing_OpSys) free(Executing_OpSys);
	Executing_OpSys = strdup(opsys);
	return 0;
}

int
pseudo_register_syscall_version( const char *job_version )
{
	CondorVersionInfo versInfo;
	const char *shadow_version = NULL;
	char *exec_path = NULL;
	MyString novers = "[No Condor Version Available]";
	MyString hold_msg;
	MyString buf;
	MyString line;

	// Obviously, I'm in the shadow process when this is called. :)
	shadow_version = CondorVersion();

	if( versInfo.is_compatible(job_version) ) {
		dprintf( D_FULLDEBUG, "User job version '%s' is compatible with this "
			"shadow version '%s'\n", 
			job_version?job_version:novers.Value(), 
			shadow_version );
		return 0;
	} 

	// Not compatible, we're screwed. Put the job on Hold.

	dprintf( D_ALWAYS, "ERROR: User job (which was compiled with condor "
		"version: %s) is NOT compatible with this shadow version which is "
		"at version %s\n",
		job_version?job_version:novers.Value(), shadow_version  );

	buf +=
		"Since the time that you ran condor_compile to link your job with\n"
		"the Condor libraries, your local Condor administrator has\n"
		"installed a different version of the condor_shadow program.\n"
		"The version of the Condor libraries you linked in with your job,\n";
	buf += job_version;
	buf +=
		"\nis not compatible with the currently installed condor_shadow,\n";
	buf += shadow_version;
	buf += "\n\nYou must do one of the following:\n\n";

	line.formatstr(
		"1) Remove your job (\"condor_rm %d.%d\"), re-link it with a\n",
		 Proc->id.cluster, Proc->id.proc );
	buf += line;

	buf +=
		"compatible version of the Condor libraries (rerun "
		"\"condor_compile\")\nand re-submit it (rerun \"condor_submit\").\n"
		"\nor\n\n"
		"2) Have your Condor administrator install a different version\n"
		"of the condor_shadow program on your submit machine.\n"
		"In this case, once the compatible shadow is in place, you\n";

	line.formatstr( "can release your job with \"condor_release %d.%d\".\n",
			 Proc->id.cluster, Proc->id.proc );
	buf += line;

	// Create a good hold reason so the user knows exactly what happened.
	hold_msg += 
		"This standard universe job and shadow version aren't compatible. "
		"The job was condor_compiled with Condor version '";
	hold_msg += job_version;
	hold_msg += "'. The condor_shadow version is '";
	hold_msg += shadow_version;
	hold_msg += 
	"'. The path to the condor_shadow which acted on behalf of this job is ";

	exec_path = getExecPath();
	if (exec_path != NULL) {
		hold_msg += "'";
		hold_msg += exec_path;
		hold_msg += "'.";
		free(exec_path);
	} else {
		hold_msg += "not available.";
	}

	// This sends the email and exits with JOB_SHOULD_HOLD. 
	HoldJob( buf.Value(), hold_msg.Value(), 
		CONDOR_HOLD_CODE_JobShadowMismatch, 0 );

	return 0;
}


int
pseudo_statfs( const char *path, struct statfs *buf )
{
	int ret;
	char *str;

	/* This idiocy is because you can't pass a const char * to a function
		that expects a char * and type casting away the const seems wrong to 
		 me. */
	str = strdup(path);

#if defined(Solaris)
	ret = statfs( str, buf, 0, 0);
#else
	ret = statfs( str, buf );
#endif

	free(str);
	return ret;
}

/*
These three calls are used in the new starter/shadow
by Chirp.  They certainly could be implemented in the old
shadow, but we have no need for them just yet.
*/

int pseudo_get_job_attr( const char * /*name*/, char *& /*expr*/ )
{
	errno = ENOSYS;
	return -1;
}

int pseudo_set_job_attr( const char * /*name*/, const char * /*expr*/ )
{
	errno = ENOSYS;
	return -1;
}

int pseudo_constrain( const char * /*expr*/ )
{
	errno = ENOSYS;
	return -1;
}

} /* extern "C" */
