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

extern "C" {
	void log_checkpoint (struct rusage *, struct rusage *);
	void log_image_size (int);
	int access_via_afs (const char *);
	int access_via_nfs (const char *);
	int use_local_access (const char *);
	int use_special_access (const char *);
	void HoldJob( const char* buf );
}

extern int JobStatus;
extern int ImageSize;
extern struct rusage JobRusage;
extern struct rusage AccumRusage;
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
extern bool ManageBandwidth;
extern char *ExecutingHost;

int		LastCkptSig = 0;

char	CurrentWorkingDir[ _POSIX_PATH_MAX ];

extern "C" {

int connect_file_stream( int connect_sock );
ssize_t stream_file_xfer( int src_fd, int dst_fd, size_t n_bytes );
size_t file_size( int fd );
int create_tcp_port( u_short *port, int *fd );
void get_host_addr( unsigned int *ip_addr );
void display_ip_addr( unsigned int addr );
int has_ckpt_file();
void update_job_status( struct rusage *localp, struct rusage *remotep );
void update_job_rusage( struct rusage *localp, struct rusage *remotep );
void RequestCkptBandwidth(unsigned int ip_addr);

static char Executing_Filesystem_Domain[ MAX_STRING ];
static char Executing_UID_Domain[ MAX_STRING ];
char *Executing_Arch=NULL, *Executing_OpSys=NULL;

extern char My_Filesystem_Domain[];
extern char My_UID_Domain[];
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
	int		rval;
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
	if (size > 0) {
		int executable_size = 0;
		dprintf( D_SYSCALLS,
				 "\tGot Image Size report of %d kilobytes\n", size );
		JobAd->LookupInteger(ATTR_EXECUTABLE_SIZE, executable_size);
		dprintf( D_SYSCALLS, "\tAdding executable size of %d kilobytes\n",
				 executable_size );
		ImageSize = size + executable_size;
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
	 * condor_syscall_lib calling send_rusage.  So, we do what we did before
	 * (which is add to AccumRusage, used by the PVM code??), _AND_ now we
	 * also update the CPU usages in the job queue. -Todd Tannenbaum 10/95. */

	/* Change (11/30/98): we now receive the rusage before the job has
	 * committed the checkpoint, so we store it in uncommitted_rusage
	 * until the checkpoint is committed or until the job exits.  -Jim B */
	
	memcpy( &AccumRusage, use_p, sizeof(struct rusage) );
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
	job_report_store_file_info( kind, name, open_count, read_count, write_count, seek_count, read_bytes, write_bytes );
	return 0;
}

int
pseudo_getwd( char *path )
{
	strncpy(path,CurrentWorkingDir,_POSIX_PATH_MAX);
	return 1;
}

int
pseudo_send_file( const char *path, mode_t mode )
{
	char	buf[ CONDOR_IO_BUF_SIZE ];
	int rval = 0;
	int	bytes_to_go;
	int	file_len;
	int	checksum;
	int	fd;
	int	len, nbytes;
	mode_t omask;

	dprintf(D_SYSCALLS, "\tpseudo_send_file(%s,0%o)\n", path, mode );

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


/*
   rename() becomes a psuedo system call because the file may be a checkpoint
   stored on the checkpoint server.  Will try the local call first (in case
   it is just a rename being call ed by a user job), but if that fails, we'll
   see if the checkpoint server can do it for us.

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
			commit_rusage();
		}

		if (rval == 0 || rval == -1 && errno != ENOENT) {
			return rval;
		}

	}
	
	if (CkptFile) {
		if (RenameRemoteFile(p->owner, from, to) < 0)
			return -1;
		// if we just wrote a checkpoint to a new checkpoint server,
		// we should remove the checkpoint we left on the previous server.
		if (LastCkptServer &&
			same_host(LastCkptServer, CkptServerHost) == FALSE) {
			SetCkptServerHost(LastCkptServer);
			RemoveRemoteFile(p->owner, to);
			SetCkptServerHost(CkptServerHost);
		}
		if (LastCkptServer) free(LastCkptServer);
		if (CkptServerHost) LastCkptServer = strdup(CkptServerHost);
		LastCkptTime = time(0);
		NumCkpts++;
		commit_rusage();
	}

	return 0;
}

void
set_last_ckpt_server()
{
	if (!LastCkptServer) {
		LastCkptServer = (char *)malloc(_POSIX_PATH_MAX);
		if (JobAd->LookupString(ATTR_LAST_CKPT_SERVER,
								LastCkptServer) == 0) {
			free(LastCkptServer);
			LastCkptServer = NULL;
		}
		if (LastCkptServer) {
			SetCkptServerHost(LastCkptServer);
		} else {
			SetCkptServerHost(CkptServerHost);
		}
	}
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
	int		bytes_read;
	pid_t	child_pid;
#if defined(ALPHA)
	unsigned int	status;		/* 32 bit unsigned */
#else
	unsigned long	status;		/* 32 bit unsigned */
#endif
	int		rval;
	PROC *p = (PROC *)Proc;
	int		retry_wait;
	bool	CkptFile = is_ckpt_file(file);
	bool	ICkptFile = is_ickpt_file(file);
	priv_state	priv;

	dprintf( D_ALWAYS, "\tEntering pseudo_get_file_stream\n" );
	dprintf( D_ALWAYS, "\tfile = \"%s\"\n", file );

	*len = 0;
		/* open the file */
	if (CkptFile && UseCkptServer) {
		set_last_ckpt_server();
		retry_wait = 5;
		do {
			rval = RequestRestore(p->owner,file,len,
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
	} else {
		rval = -1;
	}

	// need Condor privileges to access checkpoint files
	if (CkptFile || ICkptFile) {
		priv = set_condor_priv();
	}

	if (CkptFile) NumRestarts++;

	if( ((file_fd=open(file,O_RDONLY)) < 0) || 
	   (rval != 1 && rval != 73 && rval != -1 && rval != -121)) {
		/* If it isn't here locally, maybe it is on the checkpoint server */
		/* And, maybe the checkpoint server still is working on the old one
		   so, if the return val is 1 (which indicates a locked file), we 
		   try again */
		if (file_fd >= 0) {
			close(file_fd);
		}
		if (CkptFile || ICkptFile) set_priv(priv); // restore user privileges
		retry_wait = 5;
		while (rval == 1) {
			rval = RequestRestore(p->owner,file,len,(struct in_addr*)ip_addr,port);
			if (rval == 1) {
				dprintf(D_ALWAYS, "ckpt server says file is locked, trying"
						" again in %d seconds\n", retry_wait);
				sleep(retry_wait);
				retry_wait *= 2;
				if (retry_wait > MaxRetryWait) {
					EXCEPT("ckpt server restore failed");
				}
			}
		} 

		*ip_addr = ntohl( *ip_addr );
		display_ip_addr( *ip_addr );
		*port = ntohs( *port );
		dprintf( D_ALWAYS, "RestoreRequest returned %d using port %d\n", 
				rval, *port );
		if ( rval ) {
			/* Oops, couldn't find it on the checkpoint server either */
			errno = ENOENT;
			return -1;
		} else {
			BytesSent += *len;
			return 0;
		}
	} else {

		*len = file_size( file_fd );
		dprintf( D_FULLDEBUG, "\tlen = %d\n", *len );
		BytesSent += *len;

		get_host_addr( ip_addr );
		display_ip_addr( *ip_addr );

		create_tcp_port( port, &connect_sock );
		dprintf( D_FULLDEBUG, "\tPort = %d\n", *port );
		
		switch( child_pid = fork() ) {
		    case -1:	/* error */
			    dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
				if (CkptFile || ICkptFile) set_priv(priv);
				return -1;
			case 0:	/* the child */
				data_sock = connect_file_stream( connect_sock );
				if( data_sock < 0 ) {
					exit( 1 );
				}
				dprintf( D_FULLDEBUG, "\tShould Send %d bytes of data\n", 
						*len );
				bytes_sent = stream_file_xfer( file_fd, data_sock, *len );
				if (bytes_sent != *len) {
					dprintf(D_ALWAYS,
							"Failed to transfer %d bytes (only sent %d)\n",
							*len, bytes_sent);
					exit(1);
				}

#if 0	/* For compressed checkpoints, we need to close the socket
		   so the client knows we are done sending (i.e., so the client's
		   read will return), so we no longer require this confirmation. */
				/*
				 Here we should get a confirmation that our peer was able to
				 read the same number of bytes we sent, but the starter doesn't
				 send it, so if our peer just closes his end, we let that go.
				 */
				bytes_read = read( data_sock, &status, sizeof(status) );
				if( bytes_read == 0 ) {
					exit( 0 );
				}
				
				assert( bytes_read == sizeof(status) );
				status = ntohl( status );
				assert( status == *len );
				dprintf( D_ALWAYS,
						"\tSTREAM FILE SENT OK (%d bytes)\n", status );
#endif
				exit( 0 );
			default:	/* the parent */
				close( file_fd );
				close( connect_sock );
				if (CkptFile || ICkptFile) set_priv(priv);
				return 0;
			}
	}

		/* Can never get here */
	return -1;
}

/*
  Provide a process which will accept the given file as a
  stream.  The ip_addr and port number passed back are in host
  byte order.
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
	dprintf( D_ALWAYS, "\tlen = %d\n", len );
	dprintf( D_ALWAYS, "\towner = %s\n", p->owner );

	get_host_addr( ip_addr );
	display_ip_addr( *ip_addr );

		/* open the file */
	if (CkptFile && UseCkptServer) {
		SetCkptServerHost(CkptServerHost);
		do {
			rval = RequestStore(p->owner, file, len,
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
		BytesRecvd += len;
		if (CkptFile && ManageBandwidth) {
			// tell negotiator that the job is writing a checkpoint
			RequestCkptBandwidth(*ip_addr);
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

	get_host_addr( ip_addr );
	display_ip_addr( *ip_addr );

	create_tcp_port( port, &connect_sock );
	dprintf( D_FULLDEBUG, "\tPort = %d\n", *port );
	BytesRecvd += len;
	if (CkptFile && ManageBandwidth) {
		// tell negotiator that the job is writing a checkpoint
		RequestCkptBandwidth(*ip_addr);
	}
	
	switch( child_pid = fork() ) {
	  case -1:	/* error */
		dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
		if (CkptFile || ICkptFile) set_priv(priv);	// restore user privileges
		return -1;
	  case 0:	/* the child */
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
		if (CkptFile || ICkptFile) set_priv(priv);	// restore user privileges
		return 0;
	}

		/* Can never get here */
	return -1;
}

void
RequestCkptBandwidth(unsigned int ip_addr)
{
	ClassAd request;
	char buf[100], source[100], owner[100], user[100], *str;
	int executable_size = 0, nice_user = 0;

	ip_addr = htonl(ip_addr);

	if (LastCkptSig == SIGUSR2) {
		sprintf(buf, "%s = \"PeriodicCheckpoint\"", ATTR_TRANSFER_TYPE);
	} else {
		sprintf(buf, "%s = \"VacateCheckpoint\"", ATTR_TRANSFER_TYPE);
	}
	request.Insert(buf);
	JobAd->LookupInteger(ATTR_NICE_USER, nice_user);
	JobAd->LookupString(ATTR_OWNER, owner);
	sprintf(user, "%s%s@%s", (nice_user) ? "nice-user." : "", owner,
			My_UID_Domain);
	sprintf(buf, "%s = \"%s\"", ATTR_USER, user);
	request.Insert(buf);
	sprintf(buf, "%s = 1", ATTR_FORCE);
	request.Insert(buf);
	strcpy(source, ExecutingHost+1);
	str = strchr(source, ':');
	*str = '\0';
	sprintf(buf, "%s = \"%s\"", ATTR_SOURCE, source);
	request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_REMOTE_HOST, ExecutingHost);
	request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_DESTINATION,
			inet_ntoa(*(struct in_addr *)&ip_addr));
	request.Insert(buf);
	JobAd->LookupInteger(ATTR_EXECUTABLE_SIZE, executable_size);
	sprintf(buf, "%s = %f", ATTR_REQUESTED_CAPACITY,
			(float)(ImageSize-executable_size)*1024.0);
	request.Insert(buf);
	Daemon Negotiator(DT_NEGOTIATOR);
	SafeSock sock;
	sock.timeout(10);
	if (!sock.connect(Negotiator.addr())) {
		dprintf(D_ALWAYS, "Couldn't connect to negotiator!\n");
		return;
	}
	sock.put(REQUEST_NETWORK);
	sock.put(1);
	request.put(sock);
	sock.end_of_message();
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
	static float send_estimate=0.0, recv_estimate=0.0;
	ClassAd send_request, recv_request;
	char buf[100], endpoint[100], owner[100], user[100], *str;
	int nice_user = 0;

	if (!syscall_sock) return;

	float bytes_sent = syscall_sock->get_bytes_sent() - prev_bytes_sent;
	float bytes_recvd = syscall_sock->get_bytes_recvd() - prev_bytes_recvd;

	prev_bytes_sent = syscall_sock->get_bytes_sent();
	prev_bytes_recvd = syscall_sock->get_bytes_recvd();

	float prev_send_estimate = send_estimate;
	float prev_recv_estimate = recv_estimate;

	send_estimate = (bytes_sent + send_estimate) / 2;
	recv_estimate = (bytes_recvd + recv_estimate) / 2;

	// don't bother allocating anything under 1KB
	if (send_estimate < 1024.0 && recv_estimate < 1024.0) return;

	Daemon Negotiator(DT_NEGOTIATOR);
	SafeSock sock;
	sock.timeout(10);
	if (!sock.connect(Negotiator.addr())) {
		dprintf(D_ALWAYS, "Couldn't connect to negotiator!\n");
		return;
	}
	sock.put(REQUEST_NETWORK);
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
	sprintf(buf, "%s = %f", ATTR_BYTES_SENT, bytes_sent);
	send_request.Insert(buf);
	sprintf(buf, "%s = %f", ATTR_BYTES_RECVD, bytes_recvd);
	send_request.Insert(buf);
	sprintf(buf, "%s = %f", ATTR_PREV_SEND_ESTIMATE, prev_send_estimate);
	send_request.Insert(buf);
	sprintf(buf, "%s = %f", ATTR_PREV_RECV_ESTIMATE, prev_recv_estimate);
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
	sprintf(buf, "%s = %f", ATTR_REQUESTED_CAPACITY, recv_estimate);
	send_request.Insert(buf);
	send_request.put(sock);
	sprintf(buf, "%s = \"%s\"", ATTR_DESTINATION, endpoint);
	recv_request.Insert(buf);
	sprintf(buf, "%s = \"%s\"", ATTR_SOURCE, inet_ntoa(*my_sin_addr()));
	recv_request.Insert(buf);
	sprintf(buf, "%s = %f", ATTR_REQUESTED_CAPACITY, send_estimate);
	recv_request.Insert(buf);
	recv_request.put(sock);
	sock.end_of_message();
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
	dprintf( D_FULLDEBUG, "\tGot data connection at fd %d\n", data_sock );

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
  other process.  Using "port" and "fd" as output parameters, give back
  the port number to which the other process should connect, and the
  file descriptor with which this process can refer to the socket.  Return
  0 if everything works OK, and -1 otherwise.  N.B. The port number is
  returned in host byte order.
*/
int
create_tcp_port( u_short *port, int *fd )
{
	struct sockaddr_in	sin;
	int		addr_len = sizeof sin;

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
	if( getsockname(*fd,(struct sockaddr *)&sin, &addr_len) < 0 ) {
		EXCEPT("getsockname");
	}
	*port = ntohs( sin.sin_port );

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
		dprintf( D_ALWAYS, "\t%d.%d.%d.%d\n",
			HI(B_NET(addr)),
			LO(B_NET(addr)),
			HI(B_HOST(addr)),
			LO(B_HOST(addr))
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
	struct stat	st_buf;
	char	*env_string;
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

	JobAd->LookupInteger(ATTR_KILL_SIG, s->soft_kill_sig);

	s->cmd = Strdup( p->cmd[0] );
	s->args = Strdup( p->args[0] );
	s->env = Strdup( p->env );
	s->iwd = Strdup( p->iwd );

	s->is_restart = has_ckpt_file();

	if (JobAd->LookupBool(ATTR_WANT_CHECKPOINT, s->ckpt_wanted) == 0) {
		s->ckpt_wanted = TRUE;
	}

	CkptWanted = s->ckpt_wanted;

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
This call translates a logical path name specified by a user job
into an actual url which describes how and where to fetch
the file from.

Example:
	The user opens a file named:
		data
	The shadow adds on the current directory to get:
		/usr/data
	The shadow then determines the access method for the full path:
		remote:/usr/data
	The syscall lib receives this url and then opens the file appropriately.
*/

int
pseudo_get_file_info( const char *logical_name, char *actual_url )
{
	char	remap_list[ATTRLIST_MAX_EXPRESSION];
	char	full_path[_POSIX_PATH_MAX];
	char	*method;

	dprintf( D_SYSCALLS, "\tlogical_name = \"%s\"\n", logical_name );

	/* First check to see if the logical name matches a
	   filename that is remapped by the job ad. */

	JobAd->LookupString(ATTR_FILE_REMAPS,remap_list);
        if(filename_remap_find(remap_list,(char*)logical_name,actual_url)) {
		dprintf(D_SYSCALLS,"\tremapped to: %s\n",actual_url);
		return 1;
        }

	dprintf( D_SYSCALLS, "\tnot remapped.\n");

	/* No special remap was specified by the user, so decide what
	   method is to be used, based on the complete path. */

	complete_path(logical_name,full_path);

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
	} else if( use_local_access(full_path) ) {
		method = "local";
	} else if( access_via_afs(full_path) ) {
		method = "local";
	} else if(access_via_nfs(full_path) ) {
		method = "local";
	} else {
		method = "remote";
	}

	sprintf(actual_url,"%s:%s",method,full_path);

	dprintf(D_SYSCALLS,"\tactual_url: %s\n",actual_url);

	return 0;
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

	sscanf(url,"%[^:]:%s",method,physical_name);

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
	PROC	*p = (PROC *)Proc;

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
	PROC	*p = (PROC *)Proc;
	char exec_buf[_POSIX_PATH_MAX], *exec_name=exec_buf;
	char final_buf[_POSIX_PATH_MAX], *final=final_buf;
	char *tptr;

	exec_buf[0] = '\0';
	final_buf[0] = '\0';
	path[0] = '\0';

	if ( JobAd ) {
			JobAd->LookupString(ATTR_JOB_CMD,exec_name);
			if ( (tptr=substr(exec_name,"$$")) ) {
				JobAd->LookupString(ATTR_JOB_CMDEXT,final);
				if ( final[0] ) {
					strcpy(tptr,final);
					strcpy(path,exec_name);
				}
			}
	}


	if ( path[0] == '\0' ) {
		strcpy( path, ICkptName );
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
  Take note of the executing machine's filesystem domain
*/
int
pseudo_register_fs_domain( const char *fs_domain )
{
	strcpy( Executing_Filesystem_Domain, fs_domain );
	dprintf( D_SYSCALLS, "\tFS_Domain = \"%s\"\n", fs_domain );
}

/*
  Take note of the executing machine's UID domain
*/
int
pseudo_register_uid_domain( const char *uid_domain )
{
	strcpy( Executing_UID_Domain, uid_domain );
	dprintf( D_SYSCALLS, "\tUID_Domain = \"%s\"\n", uid_domain );
}

int
use_local_access( const char *file )
{
	return ((strcmp(file, "/dev/null") == MATCH) ||
			(strcmp(file, "/dev/zero") == MATCH));
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

int
has_ckpt_file()
{
	int		rval, retry_wait = 5;
	PROC *p = (PROC *)Proc;
	priv_state	priv;
	long	accum_usage;

	if (p->universe != STANDARD) return 0;
	accum_usage = p->remote_usage[0].ru_utime.tv_sec +
		p->remote_usage[0].ru_stime.tv_sec;
	priv = set_condor_priv();
	do {
		set_last_ckpt_server();
		rval = FileExists(RCkptName, p->owner);
		if (rval == -1 && UseCkptServer && accum_usage > MaxDiscardedRunTime) {
			dprintf(D_ALWAYS, "failed to contact ckpt server, trying again"
					" in %d seconds\n", retry_wait);
			sleep(retry_wait);
			retry_wait *= 2;
			if (retry_wait > MaxRetryWait) {
				EXCEPT("failed to contact ckpt server");
			}
		}
	} while (rval == -1 && UseCkptServer && accum_usage > MaxDiscardedRunTime);
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
   Specify checkpoint server host.
*/
int
pseudo_register_ckpt_server(const char *host)
{
	if (StarterChoosesCkptServer) {
		if (CkptServerHost) free(CkptServerHost);
		CkptServerHost = strdup(host);
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

} /* extern "C" */
