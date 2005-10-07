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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "../condor_syscall_lib/syscall_param_sizes.h"
#include "my_hostname.h"
#include "pseudo_ops.h"
#include "condor_ckpt_name.h"
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


extern "C" {
	void log_checkpoint (struct rusage *, struct rusage *);
	void log_image_size (int);
	void log_execute (char *);
	int access_via_afs (const char *);
	int access_via_nfs (const char *);
	int use_local_access (const char *);
	int use_special_access (const char *);
	int use_buffer( char *method, char *path );
	int use_compress( char *method, char *path );
	int use_fetch( char *method, char *path );
	int use_append( char *method, char *path );
	void HoldJob( const char* buf );
	void reaper();
	#if defined(DUX4)
		int statfs(const char *, struct statfs*);
	#elif defined(DUX5)
		int _F64_statfs(char *, struct statfs*, ...);
	#endif
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

int		LastCkptSig = 0;

/* Until the cwd is actually set, use "dot" */
char CurrentWorkingDir[ _POSIX_PATH_MAX ]=".";

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

#ifdef WANT_NETMAN
bool ManageBandwidth = false;   // notify negotiator about network usage?
int	 NetworkHorizon = 0;		// how often do we request network bandwidth?
static Daemon Negotiator(DT_NEGOTIATOR);
void RequestCkptBandwidth();
#endif

/* performing a blocking call to system() */
int
pseudo_shell( char *command, int len )
{
	int rval;
	char *tmp;
	int terrno;

	dprintf( D_SYSCALLS, "\tcommand = \"%s\"\n", command );

	/* This feature of Condor is sort of a security hole, um, maybe
		a big security hole, so by default, if this is not
		defined by the condor admin, do NOT run the command. */

	tmp = param("SHADOW_ALLOW_UNSAFE_REMOTE_EXEC");
	if (tmp == NULL || (tmp[0] != 'T' && tmp[0] != 't')) {
		dprintf(D_SYSCALLS, 
			"\tThe CONDOR_shell remote system call is currently disabled.\n");
		dprintf(D_SYSCALLS, 
			"\tTo enable it, please use SHADOW_ALLOW_UNSAFE_REMOTE_EXEC.\n");
		rval = -1;
		errno = ENOSYS;
		return rval;
	}

	rval = -1;
	errno = ENOSYS;

	if (tmp[0] == 'T' || tmp[0] == 't') {
		rval = system(command);
	}
	
	terrno = errno;
	free(tmp);
	errno = terrno;

	return rval;
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
pseudo_getlogin(char *loginbuf)
{
	char *temp;

	temp = ((PROC *)Proc)->owner; 

	if ( (temp != NULL) && (strlen(temp) < 30) ) {
		strcpy(loginbuf,temp);
		return 0;
	} else {
		return -1;
	}
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
	} else {
		// no need to update job queue here on a vacate checkpoint, since
		// we will do it in Wrapup()
		update_job_rusage( &local_rusage, &uncommitted_rusage );
	}
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
pseudo_getrusage(int who, struct rusage *use_p )
{

	// No need to look at the "who" param; the special switch for getrusage
	// already checks the "who" param and verifies it is RUSAGE_SELF

	// Pass back the rusage committed so far via checkpoint
	memcpy(use_p, &Proc->remote_usage[0], sizeof(struct rusage) );

	return 0;
}

int
pseudo_report_error( char *msg )
{
	dprintf(D_ALWAYS,"error: %s\n",msg);
	return job_report_store_error( msg );
}

int
pseudo_report_file_info( char *kind, char *name, int open_count, int read_count, int write_count, int seek_count, int read_bytes, int write_bytes )
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
pseudo_getwd( char *path )
{
	strncpy(path,CurrentWorkingDir,_POSIX_PATH_MAX);
	return 1;
}

int
pseudo_send_a_file( const char *path, mode_t mode )
{
	char	buf[ CONDOR_IO_BUF_SIZE ];
	int rval = 0;
	int	bytes_to_go;
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
	if( (fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,mode)) < 0 ) {
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
	int	file_size, bytes_to_go;
	int	fd;

		/* Open the remote file and return status from the open */
	errno = 0;
	fd = open( name, O_RDONLY, 0 );
	dprintf(D_SYSCALLS, "\topen(%s,O_RDONLY,0) = %d, errno = %d\n",
														name, fd, errno );
	if( fd < 0 ) {
		return -1;
	}

		/* Send the status from open() so client knows we are going ahead */
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
pseudo_work_request( PROC *p, char *a_out, char *targ, char *orig, int *kill_sig )
{
	struct stat	st_buf;
	int soft_kill = SIGUSR1;

		/* Copy Proc struct into space provided */
	memcpy( p, (PROC *)Proc, sizeof(PROC) );

	if( stat(ICkptName,&st_buf) == 0 ) {
		strcpy( a_out, ICkptName );
	} else {
		EXCEPT( "Can't find a.out file" );
	}

		/* Copy current checkpoint name into space provided */
	if( stat(CkptName,&st_buf) == 0 ) {
		strcpy( orig, CkptName );
	} else if( stat(ICkptName,&st_buf) == 0 ) {
		strcpy( orig, ICkptName );
	} else {
		EXCEPT( "Can't find checkpoint file" );
	}

		/* Copy next checkpoint name into space provided */
	strcpy( targ, RCkptName );

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
	if (strcmp(path, test_path) == 0) return true;
	strcat(test_path, ".tmp");
	if (strcmp(path, test_path) == 0) return true;
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
	if (strcmp(path, test_path) == 0) return true;
	return false;
}


#if WANT_NETMAN
enum file_stream_transfer_type {
	INITIAL_CHECKPOINT, CHECKPOINT_RESTART,
	PERIODIC_CHECKPOINT, VACATE_CHECKPOINT, COREFILE_TRANSFER
};
static 
struct _file_stream_info {
	_file_stream_info() : active(false) {}
	bool active;
	bool remote;
	pid_t pid;
	file_stream_transfer_type type;
	char src[16], dst[16];
	int bytes_so_far;
	int total_bytes;
	time_t last_update;
} file_stream_info;
#endif

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
#if WANT_NETMAN
				file_stream_info.bytes_so_far = file_stream_info.total_bytes;
				RequestCkptBandwidth();
				file_stream_info.active = false;
#endif
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
#if WANT_NETMAN
		file_stream_info.bytes_so_far = file_stream_info.total_bytes;
		RequestCkptBandwidth();
		file_stream_info.active = false;
#endif
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
	int		bytes_sent;
	pid_t	child_pid;
	int		rval;
	PROC *p = (PROC *)Proc;
	int		retry_wait;
	bool	CkptFile = is_ckpt_file(file);
	bool	ICkptFile = is_ickpt_file(file);
	priv_state	priv;

	dprintf( D_ALWAYS, "\tEntering pseudo_get_file_stream\n" );
	dprintf( D_ALWAYS, "\tfile = \"%s\"\n", file );
#if WANT_NETMAN
	if (file_stream_info.active) {
#if !defined(PVM_RECEIVE)
		if (!file_stream_info.remote) {
			reaper();
		}
#endif
		if (file_stream_info.active) {
			dprintf(D_ALWAYS, "\tWarning: previous file_stream transfer "
					"still active!  Cleaning up...\n");
			file_stream_info.bytes_so_far = file_stream_info.total_bytes;
			RequestCkptBandwidth();
			file_stream_info.active = false;
		}
	}
#endif

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
			if (rval) { // network error, try again
				dprintf(D_ALWAYS, "ckpt server restore failed, trying again"
						" in %d seconds\n", retry_wait);
				sleep(retry_wait);
				retry_wait *= 2;
				if (retry_wait > MaxRetryWait) {
					EXCEPT("ckpt server restore failed");
				}
			}
		} while (rval);

#if WANT_NETMAN
		file_stream_info.active = true;
		file_stream_info.remote = true;
		file_stream_info.pid = getpid();
		file_stream_info.type = CHECKPOINT_RESTART;
		struct in_addr sin;
		memcpy(&sin, ip_addr, sizeof(struct in_addr));
		strcpy(file_stream_info.src, inet_ntoa(sin));
		strncpy(file_stream_info.dst, ExecutingHost+1, 16);
		char *str = strchr(file_stream_info.dst, ':');
		*str = '\0';
		file_stream_info.bytes_so_far = 0;
		file_stream_info.total_bytes = *len;
		RequestCkptBandwidth();
#endif

		*ip_addr = ntohl( *ip_addr );
		display_ip_addr( *ip_addr );
		*port = ntohs( *port );
		dprintf( D_ALWAYS, "RestoreRequest returned %d using port %d\n", 
				rval, *port );
		BytesSent += *len;
		NumRestarts++;
		return 0;
	}

		// need Condor privileges to access checkpoint files
	if (CkptFile || ICkptFile) {
		priv = set_condor_priv();
	}

	file_fd=open(file,O_RDONLY);

	if (CkptFile || ICkptFile) set_priv(priv); // restore user privileges

	if (file_fd < 0) return -1;

	*len = file_size( file_fd );
	dprintf( D_FULLDEBUG, "\tlen = %lu\n", (unsigned long)*len );
	BytesSent += *len;

    //get_host_addr( ip_addr );
	//display_ip_addr( *ip_addr );

    create_tcp_port( ip_addr, port, &connect_sock );
    dprintf( D_NETWORK, "\taddr = %s\n", ipport_to_string(htonl(*ip_addr), htons(*port)));
		
#if WANT_NETMAN
	if (CkptFile || ICkptFile) {
		file_stream_info.active = true;
		file_stream_info.remote = false;
		file_stream_info.type = CkptFile ? CHECKPOINT_RESTART :
			INITIAL_CHECKPOINT;
		strcpy(file_stream_info.src, inet_ntoa(*my_sin_addr()));
		strncpy(file_stream_info.dst, ExecutingHost+1, 16);
		char *str = strchr(file_stream_info.dst, ':');
		*str = '\0';
		file_stream_info.bytes_so_far = 0;
		file_stream_info.total_bytes = *len;
	}
#endif

	switch( child_pid = fork() ) {
	case -1:	/* error */
		dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
		if (CkptFile || ICkptFile) set_priv(priv);
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
					"Failed to transfer %lu bytes (only sent %d)\n",
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
#if WANT_NETMAN
			file_stream_info.pid = child_pid;
			RequestCkptBandwidth();
#endif
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
	priv_state	priv;
	mode_t	omask;

	dprintf( D_ALWAYS, "\tEntering pseudo_put_file_stream\n" );
	dprintf( D_ALWAYS, "\tfile = \"%s\"\n", file );
	dprintf( D_ALWAYS, "\tlen = %u\n", len );
	dprintf( D_ALWAYS, "\towner = %s\n", p->owner );
#if WANT_NETMAN
	if (file_stream_info.active) {
#if !defined(PVM_RECEIVE)
		if (!file_stream_info.remote) {
			reaper();
		}
#endif
		if (file_stream_info.active) {
			dprintf(D_ALWAYS, "\tWarning: previous file_stream transfer "
					"still active!  Cleaning up...\n");
			file_stream_info.bytes_so_far = file_stream_info.total_bytes;
			RequestCkptBandwidth();
			file_stream_info.active = false;
		}
	}
#endif

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
			if (rval) {	/* request denied or network error, try again */
				dprintf(D_ALWAYS, "store request to ckpt server failed, "
						"trying again in %d seconds\n", retry_wait);
				sleep(retry_wait);
				retry_wait *= 2;
				if (retry_wait > MaxRetryWait) {
					EXCEPT("ckpt server store failed");
				}
			}
		} while (rval);

#if WANT_NETMAN
		file_stream_info.active = true;
		file_stream_info.remote = true;
		file_stream_info.pid = getpid();
		file_stream_info.type = (LastCkptSig == SIGUSR2) ?
			PERIODIC_CHECKPOINT : VACATE_CHECKPOINT;
		struct in_addr sin;
		memcpy(&sin, ip_addr, sizeof(struct in_addr));
		strcpy(file_stream_info.dst, inet_ntoa(sin));
		strncpy(file_stream_info.src, ExecutingHost+1, 16);
		char *str = strchr(file_stream_info.src, ':');
		*str = '\0';
		file_stream_info.bytes_so_far = 0;
		file_stream_info.total_bytes = len;
#endif

		display_ip_addr( *ip_addr );
		*ip_addr = ntohl(*ip_addr);
		*port = ntohs( *port );
		dprintf(D_ALWAYS,  "Returned addr\n");
		display_ip_addr( *ip_addr );
		dprintf(D_ALWAYS, "Returned port %d\n", *port);
	} else {
		rval = -1;
	}
	if (!rval) {
		/* Checkpoint server says ok */
		if (len > 0) {
				// need to handle the len == -1 case someday
			BytesRecvd += len;
#if WANT_NETMAN		
			if (CkptFile && ManageBandwidth) {
					// tell negotiator that the job is writing a checkpoint
				RequestCkptBandwidth();
			}
#endif
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

	if( (file_fd=open(file,O_WRONLY|O_CREAT|O_TRUNC,0664)) < 0 ) {
		if (CkptFile || ICkptFile) set_priv(priv);	// restore user privileges
		(void)umask(omask);
		return -1;
	}

	(void)umask(omask);

	//get_host_addr( ip_addr );
	//display_ip_addr( *ip_addr );

	create_tcp_port(ip_addr, port, &connect_sock);
    dprintf(D_NETWORK, "\taddr = %s\n", ipport_to_string(htonl(*ip_addr), htons(*port)));
	
#ifdef WANT_NETMAN
	file_stream_info.active = true;
	file_stream_info.remote = false;
	file_stream_info.type = (CkptFile || ICkptFile) ?
		((LastCkptSig == SIGUSR2) ?	PERIODIC_CHECKPOINT : VACATE_CHECKPOINT) :
		COREFILE_TRANSFER;
	strcpy(file_stream_info.dst, inet_ntoa(*my_sin_addr()));
	strncpy(file_stream_info.src, ExecutingHost+1, 16);
	char *str = strchr(file_stream_info.src, ':');
	*str = '\0';
	file_stream_info.bytes_so_far = 0;
	file_stream_info.total_bytes = len;
#endif

	switch( child_pid = fork() ) {
	  case -1:	/* error */
		dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
		if (CkptFile || ICkptFile) set_priv(priv);	// restore user privileges
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
#ifdef WANT_NETMAN
			file_stream_info.pid = child_pid;
#endif
			if (CkptFile) NumRestarts++;
		}
		if (len > 0) {
				// need to handle the len == -1 case someday
			BytesRecvd += len;
#ifdef WANT_NETMAN
			if (CkptFile && ManageBandwidth) {
					// tell negotiator that the job is writing a checkpoint
				RequestCkptBandwidth();
			}
#endif
		}
		return 0;
	}

		/* Can never get here */
	return -1;
}

#ifdef WANT_NETMAN
/* This function is called whenever a child process exits.   */
void
file_stream_child_exit(pid_t pid)
{
	if (file_stream_info.active && !file_stream_info.remote &&
		file_stream_info.pid == pid) {
		switch(file_stream_info.type) {
		case INITIAL_CHECKPOINT:
			file_stream_info.bytes_so_far = file_stream_info.total_bytes;
			RequestCkptBandwidth();
			file_stream_info.active = false;
			break;
		case COREFILE_TRANSFER:
			file_stream_info.active = false;
			break;
		case CHECKPOINT_RESTART:
				// handled in pseudo_get_ckpt_mode()
		case PERIODIC_CHECKPOINT:
		case VACATE_CHECKPOINT:
				// handled in rename() (yuk!)
			break;
		default:
			dprintf(D_ALWAYS, "Bad file_stream_transfer_type %d in "
					"file_stream_child_exit!\n", file_stream_info.type);
			return;
		}
	}
}

extern "C" {
void
file_stream_progress_report(int bytes_moved)
{
	if (!file_stream_info.active) {
		static bool warned_already = false;
		if (!warned_already) {
			dprintf(D_ALWAYS, "Warning: file_stream_progress_report() called "
					"with no active file_stream_transfer.\n");
		}
		warned_already = true;
		return;
	}
	if (file_stream_info.remote) {
		static bool warned_already = false;
		if (!warned_already) {
			dprintf(D_ALWAYS, "Warning: file_stream_progress_report() called "
					"with active REMOTE file_stream_transfer.\n");
		}
		warned_already = true;
		return;
	}
	file_stream_info.bytes_so_far = bytes_moved;
	time_t current_time = time(0);
	if (current_time >= file_stream_info.last_update + (NetworkHorizon/5)) {
		RequestCkptBandwidth();
	}
}
}

void
abort_file_stream_transfer()
{
	if (file_stream_info.active) {
		if (!file_stream_info.remote) {
			priv_state priv = set_condor_priv();
			kill(file_stream_info.pid, SIGKILL);
			set_priv(priv);
		}
			// invalidate our network bandwidth allocation
		file_stream_info.bytes_so_far = file_stream_info.total_bytes;
		RequestCkptBandwidth();
		file_stream_info.active = false;
	}
}

/* 
   We allocate bandwidth for checkpoint and executable transfers here
   (i.e., anything transferred using the file_stream protocol).  We
   include the startd vm id so the matchmaker can uniquely identify
   checkpoint transfers to/from SMP machines.  The matchmaker may have
   already allocated the bandwidth for this transfer.  In that case,
   our request serves as an update on the amount of data we're
   expecting to transfer.  We can update our request at any time
   during the transfer.  We must send a final update when the transfer
   completes, so the matchmaker can free the allocation.  The
   allocation will timeout if our update is lost.
*/
void
RequestCkptBandwidth()
{
	ClassAd request;
	char buf[100], owner[100], user[100];
	int nice_user = 0, vm_id = 1;

	if (!ManageBandwidth) return;

	if (!file_stream_info.active) {
		dprintf(D_ALWAYS, "Warning: RequestCkptBandwidth() called with no "
				"active file_stream_transfer.\n");
		return;
	}
	if (file_stream_info.total_bytes < 0) return; // TODO (someday)
	
	switch(file_stream_info.type) {
	case INITIAL_CHECKPOINT:
		sprintf(buf, "%s = \"InitialCheckpoint\"", ATTR_TRANSFER_TYPE);
		dprintf(D_ALWAYS, "Transferring InitialCkpt from %s to %s: "
				"%d bytes to go.\n", file_stream_info.src,
				file_stream_info.dst,
				file_stream_info.total_bytes-file_stream_info.bytes_so_far);
		break;
	case CHECKPOINT_RESTART:
		sprintf(buf, "%s = \"CheckpointRestart\"", ATTR_TRANSFER_TYPE);
		dprintf(D_ALWAYS, "Transferring RestartCkpt from %s to %s: "
				"%d bytes to go.\n", file_stream_info.src,
				file_stream_info.dst,
				file_stream_info.total_bytes-file_stream_info.bytes_so_far);
		break;
	case PERIODIC_CHECKPOINT:
		sprintf(buf, "%s = \"PeriodicCheckpoint\"", ATTR_TRANSFER_TYPE);
		dprintf(D_ALWAYS, "Transferring PeriodicCkpt from %s to %s: "
				"%d bytes to go.\n", file_stream_info.src,
				file_stream_info.dst,
				file_stream_info.total_bytes-file_stream_info.bytes_so_far);
		break;
	case VACATE_CHECKPOINT:
		sprintf(buf, "%s = \"VacateCheckpoint\"", ATTR_TRANSFER_TYPE);
		dprintf(D_ALWAYS, "Transferring VacateCkpt from %s to %s: "
				"%d bytes to go.\n", file_stream_info.src,
				file_stream_info.dst,
				file_stream_info.total_bytes-file_stream_info.bytes_so_far);
		break;
	case COREFILE_TRANSFER:
		return;					// ignore for now
	default:
		dprintf(D_ALWAYS, "Bad file_stream_transfer_type %d in "
				"RequestCkptBandwidth!\n", file_stream_info.type);
		return;
	}
	request.Insert(buf);
	JobAd->LookupInteger(ATTR_NICE_USER, nice_user);
	JobAd->LookupString(ATTR_OWNER, owner);
	sprintf(user, "%s%s@%s", (nice_user) ? "nice-user." : "", owner,
			My_UID_Domain);
	sprintf(buf, "%s = \"%s\"", ATTR_USER, user);
	request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_CLUSTER_ID, Proc->id.cluster);
	request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_PROC_ID, Proc->id.proc);
	request.Insert(buf);
 	JobAd->LookupInteger(ATTR_REMOTE_VIRTUAL_MACHINE_ID, vm_id);
	dprintf(D_ALWAYS, "vm_id = %d\n", vm_id);
 	sprintf(buf, "%s = %d", ATTR_VIRTUAL_MACHINE_ID, vm_id);
 	request.Insert(buf);
    // Using ip:pid as an identity of the process is not complete because of private
    // addresses are reusable. TODO (someday)
	sprintf(buf, "%s = \"%u.%d\"", ATTR_TRANSFER_KEY, my_ip_addr(),
			file_stream_info.pid);
 	request.Insert(buf);
	sprintf(buf, "%s = 1", ATTR_FORCE);
	request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_SOURCE, file_stream_info.src);
	request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_REMOTE_HOST, ExecutingHost);
	request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_DESTINATION, file_stream_info.dst);
	request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_REQUESTED_CAPACITY,
			file_stream_info.total_bytes-file_stream_info.bytes_so_far);
	request.Insert(buf);

    SafeSock sock;
	sock.timeout(10);
	if (!sock.connect(Negotiator.addr())) {
		dprintf(D_ALWAYS, "Couldn't connect to negotiator (%s)!\n",
				Negotiator.addr());
		return;
	}

	Negotiator.startCommand (REQUEST_NETWORK, &sock);

	sock.put(1);
	request.put(sock);
	sock.end_of_message();


	buf[0] = '\0';
	if (JobAd->LookupString(ATTR_REMOTE_POOL, buf)) {
		Daemon FlockNegotiator(DT_NEGOTIATOR, buf, buf);
        if (!sock.connect(FlockNegotiator.addr())) {
			dprintf(D_ALWAYS, "Couldn't connect to flock negotiator (%s)!\n",
					FlockNegotiator.addr());
			return;
		}

		FlockNegotiator.startCommand(REQUEST_NETWORK, &sock);

		sock.put(1);
		request.put(sock);
		sock.end_of_message();
	}
	file_stream_info.last_update = time(0);
}

/* 
   We allocate bandwidth for RSC here.  Since we don't know how much
   RSC I/O we will generate in the future, we must make some
   prediction.  We use an exponential average of our past network
   usage for RIO to predict future RIO needs.  We can track our
   prediction error as we go.
*/

void
RequestRSCBandwidth()
{
	static float prev_bytes_sent=0.0, prev_bytes_recvd=0.0;
	static int send_estimate=0, recv_estimate=0;
	ClassAd send_request, recv_request;
	char buf[100], endpoint[100], owner[100], user[100], *str;
	int nice_user = 0;

	if (!syscall_sock || !ManageBandwidth) return;

	int bytes_sent = (int)(syscall_sock->get_bytes_sent()-prev_bytes_sent);
	int bytes_recvd = (int)(syscall_sock->get_bytes_recvd()-prev_bytes_recvd);

	prev_bytes_sent = syscall_sock->get_bytes_sent();
	prev_bytes_recvd = syscall_sock->get_bytes_recvd();

	int prev_send_estimate = send_estimate;
	int prev_recv_estimate = recv_estimate;

	send_estimate = (bytes_sent + send_estimate) / 2;
	recv_estimate = (bytes_recvd + recv_estimate) / 2;

	// don't bother allocating anything under 1KB
	if (send_estimate < 1024.0 && recv_estimate < 1024.0) return;

    SafeSock sock;
	sock.timeout(10);
	if (!sock.connect(Negotiator.addr())) {
		dprintf(D_ALWAYS, "Couldn't connect to negotiator!\n");
		return;
	}

	Negotiator.startCommand (REQUEST_NETWORK, &sock);
	sock.put(2);

	// these attributes are only required in the first network req. ClassAd
	sprintf(buf, "%s = \"RemoteSyscalls\"", ATTR_TRANSFER_TYPE);
	send_request.Insert(buf);
	JobAd->LookupInteger(ATTR_NICE_USER, nice_user);
	JobAd->LookupString(ATTR_OWNER, owner);
	sprintf(user, "%s%s@%s", (nice_user) ? "nice-user." : "", owner,
			My_UID_Domain);
	sprintf(buf, "%s = \"%s\"", ATTR_USER, user);
	send_request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_BYTES_SENT, bytes_sent);
	send_request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_BYTES_RECVD, bytes_recvd);
	send_request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_PREV_SEND_ESTIMATE, prev_send_estimate);
	send_request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_PREV_RECV_ESTIMATE, prev_recv_estimate);
	send_request.Insert(buf);
	strcpy(endpoint, ExecutingHost+1);
	str = strchr(endpoint, ':');
	*str = '\0';
	sprintf(buf, "%s = \"%s\"", ATTR_REMOTE_HOST, ExecutingHost);
	send_request.Insert(buf);

	// these attributes must be defined for both network req. ClassAds
	sprintf(buf, "%s = 1", ATTR_FORCE);
	send_request.Insert(buf);
	recv_request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_SOURCE, endpoint);
	send_request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_DESTINATION, inet_ntoa(*my_sin_addr()));
	send_request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_REQUESTED_CAPACITY, recv_estimate);
	send_request.Insert(buf);
	send_request.put(sock);
	sprintf(buf, "%s = \"%s\"", ATTR_DESTINATION, endpoint);
	recv_request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_SOURCE, inet_ntoa(*my_sin_addr()));
	recv_request.Insert(buf);
	sprintf(buf, "%s = %d", ATTR_REQUESTED_CAPACITY, send_estimate);
	recv_request.Insert(buf);
	recv_request.put(sock);
	sock.end_of_message();


	buf[0] = '\0';
	if (JobAd->LookupString(ATTR_REMOTE_POOL, buf)) {
		Daemon FlockNegotiator(DT_NEGOTIATOR, buf, buf);

		if (!sock.connect(FlockNegotiator.addr())) {
			dprintf(D_ALWAYS, "Couldn't connect to flock negotiator (%s)!\n",
					FlockNegotiator.addr());
			return;
		}
        FlockNegotiator.startCommand(REQUEST_NETWORK, &sock);

		sock.put(2);
		send_request.put(sock);
		recv_request.put(sock);
		sock.end_of_message();
	}
}
#endif

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
	if( (answer=lseek(fd,0,2)) < 0 ) {
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
        EXCEPT( "socket" );
    }
    dprintf( D_FULLDEBUG, "\tconnect_sock = %d\n", *fd );

        /* bind it to an address */

    if( ! _condor_local_bind(*fd) ) {
        EXCEPT( "bind" );
    }

        /* determine which local port number we were assigned to */
    struct sockaddr_in *tmp = getSockAddr(*fd);
    if (tmp == NULL) {
        EXCEPT("getSockAddr failed");
    }
    *ip = ntohl(tmp->sin_addr.s_addr);
    *port = ntohs(tmp->sin_port);

        /* Get ready to accept a connection */
    if( listen(*fd,1) < 0 ) {
        EXCEPT( "listen" );
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
	*ip_addr = my_ip_addr();
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
	int		net_part;
	int		host_part;

	if( IN_CLASSB(addr) ) {
		net_part = B_NET(addr);
		host_part = B_HOST(addr);
		dprintf( D_ALWAYS, "\t%lu.%lu.%lu.%lu\n",
			(unsigned long) HI(B_NET(addr)),
			(unsigned long) LO(B_NET(addr)),
			(unsigned long) HI(B_HOST(addr)),
			(unsigned long) LO(B_HOST(addr))
		);
	} else {
		dprintf( D_ALWAYS, "\t Weird 0x%x\n", addr );
	}
}

/*
  Gather up the stuff needed to satisfy a request for work from the
  condor_starter.  This is the new version which passes a STARUP_INFO
  structure rather than a PROC structure.
*/
char * find_env( const char * name, const char * env );
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
	s->args = Strdup( p->args[0] );
	s->env = Strdup( p->env );
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
pseudo_get_std_file_info( int which, char *logical_name )
{
	PROC	*p = (PROC *)Proc;

	switch( which ) {
		case 0:
			strcpy( logical_name, p->in[0] );
			break;
		case 1:
			strcpy( logical_name, p->out[0] );
			break;
		case 2:
			strcpy( logical_name, p->err[0] );
			break;
		default:
			logical_name[0] = '\0';
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
pseudo_std_file_info( int which, char *logical_name, int *pipe_fd )
{
	*pipe_fd = -1;
	return pseudo_get_std_file_info( which, logical_name );
}


/*
If short_path is an absolute path, copy it to full path.
Otherwise, tack the current directory on to the front
of short_path, and copy it to full_path.
*/
 
static void complete_path( const char *short_path, char *full_path )
{
	if(short_path[0]=='/') {
		strcpy(full_path,short_path);
	} else {
		strcpy(full_path,CurrentWorkingDir);
		strcat(full_path,"/");
		strcat(full_path,short_path);
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

int do_get_file_info( char *logical_name, char *url, int allow_complex );
void append_buffer_info( char *url, char *method, char *path );

int
pseudo_get_file_info_new( const char *logical_name, char *actual_url )
{
	return do_get_file_info( (char*)logical_name, actual_url, 1 );
}

int
pseudo_get_file_info( const char *logical_name, char *actual_url )
{
	return do_get_file_info( (char*)logical_name, actual_url, 0 );
}

int
do_get_file_info( char *logical_name, char *actual_url, int allow_complex )
{
	char	remap_list[ATTRLIST_MAX_EXPRESSION];
	char	split_dir[_POSIX_PATH_MAX];
	char	split_file[_POSIX_PATH_MAX];
	char	full_path[_POSIX_PATH_MAX];
	char	remap[_POSIX_PATH_MAX];
	char	*method;
	int		want_remote_io;
	static int		warned_once = FALSE;

	dprintf( D_SYSCALLS, "\tlogical_name = \"%s\"\n", logical_name );

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

	filename_split( (char*) logical_name, split_dir, split_file );
	complete_path( logical_name, full_path );

	/* Any name comparisons must check the logical name, the simple name, and the full path */

	if(JobAd->LookupString(ATTR_FILE_REMAPS,remap_list) &&
	  (filename_remap_find( remap_list, (char*) logical_name, remap ) ||
	   filename_remap_find( remap_list, split_file, remap ) ||
	   filename_remap_find( remap_list, full_path, remap ))) {

		dprintf(D_SYSCALLS,"\tremapped to: %s\n",remap);

		/* If the remap is a full URL, return right away */
		/* Otherwise, continue processing */

		if(strchr(remap,':')) {
			dprintf(D_SYSCALLS,"\tremap is complete url\n");
			strcpy(actual_url,remap);
			if (want_remote_io == FALSE) {
				return 1;
			}
			return 0;
		} else {
			dprintf(D_SYSCALLS,"\tremap is simple file\n");
			complete_path( remap, full_path );
		}
	} else {
		dprintf(D_SYSCALLS,"\tnot remapped\n");
	}

	dprintf( D_SYSCALLS,"\tfull_path = \"%s\"\n", full_path );

	/* Now, we have a full pathname. */
	/* Figure out what url modifiers to slap on it. */

#ifdef HPUX
	/* I have no idea why this is happening, but I have seen it happen many
	 * times on the HPUX version, so here is a quick hack -Todd 5/19/95 */
	if ( strcmp(full_path,"/usr/lib/nls////strerror.cat") == 0 )
		strcpy(full_path,"/usr/lib/nls/C/strerror.cat\0");
#endif

	if(is_ckpt_file(full_path) || is_ickpt_file(full_path)) {
		method = "remote";
	} else if( use_special_access(full_path) ) {
		method = "special";
	} else if ( want_remote_io == FALSE ) {
		method = "local";
	} else if( use_local_access(full_path) ) {
		method = "local";
	} else if( access_via_afs(full_path) ) {
		method = "local";
	} else if( access_via_nfs(full_path) ) {
		method = "local";
	} else {
		method = "remote";
	}

	actual_url[0] = 0;

	if( allow_complex ) {
		if( use_fetch(method,full_path) ) {
			strcat(actual_url,"fetch:");
		}
		if( use_compress(method,full_path) ) {
			strcat(actual_url,"compress:");
		}

		append_buffer_info(actual_url,method,full_path);

		if( use_append(method,full_path) ) {
			strcat(actual_url,"append:");
		}
	}

	strcat(actual_url,method);
	strcat(actual_url,":");
	strcat(actual_url,full_path);

	dprintf(D_SYSCALLS,"\tactual_url: %s\n",actual_url);

	/* This return value will make the caller in the user job never call
		the shadow ever again to see what to do about a job. Instead the
		FileTable will assume ALL files are local. */
	if (want_remote_io == FALSE) {
		return 1;
	}

	return 0;
}

void append_buffer_info( char *url, char *method, char *path )
{
	char buffer_list[ATTRLIST_MAX_EXPRESSION];
	char buffer_string[ATTRLIST_MAX_EXPRESSION];
	char dir[_POSIX_PATH_MAX];
	char file[_POSIX_PATH_MAX];
	int s,bs,ps;
	int result;

	filename_split(path,dir,file);

	/* Get the default buffer setting */
	pseudo_get_buffer_info( &s, &bs, &ps );

	/* Now check for individual file overrides */
	/* These lines have the same syntax as a remap list */

	if(JobAd->LookupString(ATTR_BUFFER_FILES,buffer_list)) {
		if( filename_remap_find(buffer_list,path,buffer_string) ||
		    filename_remap_find(buffer_list,file,buffer_string) ) {

			/* If the file is merely mentioned, turn on the default buffer */
			strcat(url,"buffer:");

			/* If there is also a size setting, use that */
			result = sscanf(buffer_string,"(%d,%d)",&s,&bs);
			if( result==2 ) strcat(url,buffer_string);

			return;
		}
	}

	/* Turn on buffering if the value is set and is not special or local */
	/* In this case, use the simple syntax 'buffer:' so as not to confuse old libs */

	if( s>0 && bs>0 && strcmp(method,"local") && strcmp(method,"special")  ) {
		strcat(url,"buffer:");
	}
}

/* Return true if this JobAd attribute contains this path */

static int attr_list_has_file( const char *attr, const char *path )
{
	char dir[_POSIX_PATH_MAX];
	char file[_POSIX_PATH_MAX];
	char str[ATTRLIST_MAX_EXPRESSION];
	StringList *list=0;

	filename_split( path, dir, file );

	str[0] = 0;
	JobAd->LookupString(attr,str);
	list = new StringList(str);

	if( list->contains_withwildcard(path) || list->contains_withwildcard(file) ) {
		delete list;
		return 1;
	} else {
		delete list;
		return 0;
	}
}


int use_append( char *method, char *path )
{
	return attr_list_has_file( ATTR_APPEND_FILES, path );
}

int use_compress( char *method, char *path )
{
	return attr_list_has_file( ATTR_COMPRESS_FILES, path );
}

int use_fetch( char *method, char *path )
{
	return attr_list_has_file( ATTR_FETCH_FILES, path );
}

/*
This is for compatibility with the old starter.
It will go away in the near future.
*/

int pseudo_file_info( const char *logical_name, int *fd, char *physical_name )
{
	char	url[_POSIX_PATH_MAX];
	char	method[_POSIX_PATH_MAX];
	int	result;

	result = pseudo_get_file_info( logical_name, url );
	if(result<0) return result;

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
pseudo_get_iwd( char *path )
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

	strcpy( path, p->iwd );
	dprintf( D_SYSCALLS, "\tanswer = \"%s\"\n", p->iwd );
	return 0;
}


/*
  Return name of checkpoint file for this process
*/
int
pseudo_get_ckpt_name( char *path )
{
	strcpy( path, RCkptName );
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
#ifdef WANT_NETMAN
			// The job has completed its checkpoint restart now, so we
			// can release the network bandwidth allocation (by
			// updating our bandwidth request).
		if (file_stream_info.active &&
			file_stream_info.type == CHECKPOINT_RESTART) {
			file_stream_info.bytes_so_far = file_stream_info.total_bytes;
			RequestCkptBandwidth();
			file_stream_info.active = false;
		} else {
			dprintf( D_ALWAYS, "Warning: received get_ckpt_mode(0) with no "
					 "info about active checkpoint restart transfer!\n" );
		}
#endif
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
pseudo_get_a_out_name( char *path )
{
	path[0] = '\0';
	int result;

	// See if we can find an executable in the spool dir.
	// Switch to Condor uid first, since ickpt files are 
	// stored in the SPOOL directory as user Condor.
	priv_state old_priv = set_condor_priv();
	result = access(ICkptName,R_OK);
	set_priv(old_priv);

	if ( result >= 0 ) {
			// we can access an executable in the spool dir
		strcpy( path, ICkptName );
	} else {
			// nothing in the spool dir; the executable 
			// probably has $$(opsys) in it... so use what
			// the jobad tells us.
		ASSERT(JobAd);
		JobAd->LookupString(ATTR_JOB_CMD,path);
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
	dprintf( D_SYSCALLS, "\tOrig CurrentWorkingDir = \"%s\"\n", CurrentWorkingDir );

	rval = chdir( path );

	if( rval < 0 ) {
		dprintf( D_SYSCALLS, "Failed\n" );
		return rval;
	}

	if( path[0] == '/' ) {
		strcpy( CurrentWorkingDir, path );
	} else {
		strcat( CurrentWorkingDir, "/" );
		strcat( CurrentWorkingDir, path );
	}
	dprintf( D_SYSCALLS, "\tNew CurrentWorkingDir = \"%s\"\n", CurrentWorkingDir );
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
access_via_afs( const char *file )
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
access_via_nfs( const char *file )
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
	if (JobAd && JobAd->LookupString(ATTR_VERSION, job_version)) {
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
			dprintf(D_ALWAYS, "failed to contact ckpt server, trying again"
					" in %d seconds\n", retry_wait);
			sleep(retry_wait);
			retry_wait *= 2;
			if (retry_wait > MaxRetryWait) {
				EXCEPT("failed to contact ckpt server");
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
pseudo_get_username( char *uname )
{
	PROC	*p = (PROC *)Proc;

	strcpy( uname, p->owner );
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
		if( (fd=open(name,O_WRONLY|O_CREAT,0666)) < 0 ) {
			EXCEPT( name );
		}
		close( fd );

		fp = fopen( name, "a" );
		if( fp == NULL ) {
			EXCEPT( "Can't open \"%s\" for appending\n", name );
		}
	}

	fprintf( fp, msg );
	(void)umask( omask );
}

int pseudo_get_IOServerAddr(const int *reqtype, const char *filename,
							char *host, int *port )
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
pseudo_pvm_info(char *pvmd_reply, int starter_tid)
{
	return 0;
}


int
pseudo_pvm_task_info(int pid, int task_tid)
{
	return 0;
}


int
pseudo_suspended(int suspended)
{
	return 0;
}


int
pseudo_subproc_status(int subproc, int *statp, struct rusage *rusagep)
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

int
pseudo_lseekread(int fd, off_t offset, int whence, void *buf, size_t len)
{
        int rval;

        if ( lseek( fd, offset, whence ) < 0 ) {
                return (off_t) -1;
        }

        rval = read( fd, buf, len );
        return rval;    
}

int
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
		char *use_ckpt_server = param( "USE_CKPT_SERVER" );
		if (!use_ckpt_server ||
			(use_ckpt_server[0] != 'F' && use_ckpt_server[0] != 'f')) {
			if (CkptServerHost) free(CkptServerHost);
			CkptServerHost = strdup(host);
			UseCkptServer = TRUE;
		}
		if (use_ckpt_server) free(use_ckpt_server);
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
pseudo_register_syscall_version( const char *version )
{
	CondorVersionInfo versInfo;
	if( versInfo.is_compatible(version) ) {
		dprintf( D_FULLDEBUG, "User job is compatible with this shadow version\n" );
		return 0;
	} 
		// Not compatible, we're screwed.
	dprintf( D_ALWAYS, "ERROR: User job is NOT compatible with this shadow version\n" );
	char buf[4096];
	char line[256];

	buf[0] = '\0';
	line[0] = '\0';
	strcat( buf, "Since the time that you ran condor_compile to link your job with\n" );
	strcat( buf, "the Condor libraries, your local Condor administrator has\n" );
	strcat( buf, "installed a different version of the condor_shadow program.\n" );
	strcat( buf, "The version of the Condor libraries you linked in with your job,\n" );
	strcat( buf, version );
	strcat( buf, "\nis not compatible with the currently installed condor_shadow,\n" );
	strcat( buf, CondorVersion() );
	strcat( buf, "\n\nYou must do one of the following:\n\n" );
	sprintf( line, "1) Remove your job (\"condor_rm %d.%d\"), re-link it with a\n",
			 Proc->id.cluster, Proc->id.proc );
	strcat( buf, line );
	strcat( buf, "compatible version of the Condor libraries (rerun " );
	strcat( buf, "\"condor_compile\")\nand re-submit it (rerun \"condor_submit\").\n" );
	strcat( buf, "\nor\n\n" );
	strcat( buf, "2) Have your Condor administrator install a different version\n" );
	strcat( buf, "of the condor_shadow program on your submit machine.\n" );
	strcat( buf, "In this case, once the compatible shadow is in place, you\n" );
	sprintf( line, "can release your job with \"condor_release %d.%d\".\n",
			 Proc->id.cluster, Proc->id.proc );
	strcat( buf, line );
	
	HoldJob( buf );  // This sends the email and exits with JOB_SHOULD_HOLD. 
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

#if defined(Solaris) || defined(IRIX)
	ret = statfs( str, buf, 0, 0);
#elif defined DUX5
	ret = _F64_statfs( str, buf );
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

int pseudo_get_job_attr( const char *name, char *expr )
{
	errno = ENOSYS;
	return -1;
}

int pseudo_set_job_attr( const char *name, const char *expr )
{
	errno = ENOSYS;
	return -1;
}

int pseudo_constrain( const char *expr )
{
	errno = ENOSYS;
	return -1;
}

} /* extern "C" */
