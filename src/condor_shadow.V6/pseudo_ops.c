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
** Author:  Mike Litzkow and Jim Pruyne
**
*/ 

#define _POSIX_SOURCE

#if defined(Solaris)
#include "_condor_fix_types.h"
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_jobqueue.h"
#include "xdr_lib.h"
#include "condor_file_info.h"
#include "afs.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "../condor_syscall_lib/syscall_param_sizes.h"

static char *_FileName_ = __FILE__;

#include "user_log.h"
extern USER_LOG	*ULog;

extern int ClientUid;
extern int ClientGid;
extern int JobStatus;
extern int ImageSize;
extern struct rusage JobRusage;
extern struct rusage AccumRusage;
extern XDR *xdr_syscall;
extern PROC *Proc;
extern char CkptName[];
extern char ICkptName[];
extern char RCkptName[];

char	CurrentWorkingDir[ _POSIX_PATH_MAX ];

int connect_file_stream( int connect_sock );
ssize_t stream_file_xfer( int src_fd, int dst_fd, size_t n_bytes );
size_t file_size( int fd );
int create_tcp_port( u_short *port, int *fd );
void get_host_addr( unsigned int *ip_addr );
void display_ip_addr( unsigned int addr );
int has_ckpt_file();

static char Executing_AFS_Cell[ MAX_STRING ];
static char Executing_Filesystem_Domain[ MAX_STRING ];
static char Executing_UID_Domain[ MAX_STRING ];

extern char My_AFS_Cell[];
extern char My_Filesystem_Domain[];
extern char My_UID_Domain[];
extern int	UseAFS;
extern int  UseNFS;
extern int  UseCkptServer;

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
pseudo_getpid()
{

	return( Proc->id.proc );
}

int
pseudo_getuid()
{
	return ClientUid;
}

int
pseudo_geteuid()
{
	return ClientUid;
}

int
pseudo_getgid()
{
	return ClientGid;
}

int
pseudo_getegid()
{
	return ClientGid;
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
	return free_fs_blocks( path );
}

int
pseudo_image_size( int size )
{
	dprintf( D_SYSCALLS, "\tGot Image Size report of %d kilobytes\n", size );
	if( size > ImageSize ) {
		ImageSize = size;
		dprintf( D_SYSCALLS, "Set Image Size to %d kilobytes\n", size );
		PutUserLog( ULog, IMAGE_SIZE, size );
	}
	return 0;
}

int
pseudo_send_rusage( struct rusage *use_p )
{
	struct rusage local_rusage;

	/* A periodic checkpoint (checkpoint and keep running) now results in the 
	 * condor_syscall_lib calling send_rusage.  So, we do what we did before
	 * (which is add to AccumRusage, used by the PVM code??), _AND_ now we
	 * also update the CPU usages in the job queue. -Todd Tannenbaum 10/95. */
	
	memcpy( &AccumRusage, use_p, sizeof(struct rusage) );

	get_local_rusage( &local_rusage );
	update_job_status( &local_rusage, use_p );
	PutUserLog(ULog, CHECKPOINTED);		/* let the user know it happened */

	
	return 0;
}

int
pseudo_perm_error( const char *msg )
{
	PERM_ERR( msg );
	return 0;
}

int
pseudo_getwd( char *path )
{
	return getwd(path);
}

int
pseudo_send_file( const char *path, mode_t mode )
{
	char	buf[ XDR_BLOCKSIZ ];
	int rval = 0;
	int	bytes_to_go;
	int	file_len;
	int	checksum;
	int	fd;
	int	len, nbytes;

	dprintf(D_SYSCALLS, "\tpseudo_send_file(%s,0%o)\n", path, mode );

		/* Open the file for writing, send back status from the open */
	if( (fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,mode)) < 0 ) {
		return -1;
	}
	xdr_syscall->x_op = XDR_ENCODE;
	assert( xdr_int(xdr_syscall, &fd) );
	assert( xdrrec_endofrecord(xdr_syscall,TRUE) );

		/* Get the file length */
	xdr_syscall->x_op = XDR_DECODE;
	assert( xdrrec_skiprecord(xdr_syscall) );
	assert( xdr_int(xdr_syscall, &file_len) );
	dprintf(D_SYSCALLS, "\tFile size is %d\n", file_len );
		
		/* Transfer all the data */
	for( bytes_to_go = file_len; bytes_to_go; bytes_to_go -= len ) {
		len = bytes_to_go < sizeof(buf) ? bytes_to_go : sizeof(buf);
		nbytes = len;
		errno = 0;
		if( !xdr_opaque(xdr_syscall,buf,len) ) {
			dprintf( D_ALWAYS, "\txdr_opaque() failed, errno = %d\n",
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
	assert( xdr_int(xdr_syscall,&checksum) );
	dprintf(D_SYSCALLS, "\tchecksum is %d\n", checksum );
	assert( checksum == file_len );

	return rval;
}

int
pseudo_get_file( const char *name )
{
	char	buf[ XDR_BLOCKSIZ ];
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
	xdr_syscall->x_op = XDR_ENCODE;
	assert( xdr_int(xdr_syscall,&fd) );

		/* Send the size of the file */
	file_size = lseek( fd, 0, 2 );
	(void)lseek( fd, 0, 0 );
	dprintf(D_SYSCALLS, "\tFile size is %d bytes\n", file_size );
	if( file_size < 0 ) {
		return -1;
	}
	assert( xdr_int(xdr_syscall,&file_size) );

		/* Transfer the data */
	for( bytes_to_go = file_size; bytes_to_go; bytes_to_go -= len ) {
		len = bytes_to_go < sizeof(buf) ? bytes_to_go : sizeof(buf);
		if( read(fd,buf,len) < len ) {
			dprintf( D_ALWAYS, "Can't read from \"%s\"\n", name );
			read_status = -1;
		}
		assert( xdr_opaque(xdr_syscall,buf,len) );
	}
	(void)close( fd );

		/* Send the size of the file again as a check */
	assert( xdr_int(xdr_syscall,&file_size) );
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
   rename() becomes a psuedo system call because the file may be a checkpoint
   stored on the checkpoint server.  Will try the local call first (in case
   it is just a rename being call ed by a user job), but if that fails, we'll
   see if the checkpoint server can do it for us.
*/

int
pseudo_rename(char *from, char *to)
{
	int		rval;
	PROC *p = (PROC *)Proc;

	rval = rename(from, to);
	if (rval == 0 || rval == -1 && errno != ENOENT) {
		return rval;
	}
	
	RenameRemoteFile(p->owner, from, to);
	return 0;
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
	ssize_t		bytes_sent;
	ssize_t		bytes_read;
	pid_t		child_pid;
#if defined(ALPHA)
	unsigned int	status;		/* 32 bit unsigned */
#else
	unsigned long	status;		/* 32 bit unsigned */
#endif
	int		rval;
	PROC *p = (PROC *)Proc;
	int		retry_wait = 1;
	struct in_addr sin;
	extern int RequestRestore(const char *, const char *, unsigned long *,
							  struct in_addr *, unsigned short *);

	dprintf( D_ALWAYS, "\tEntering pseudo_get_file_stream\n" );
	dprintf( D_ALWAYS, "\tfile = \"%s\"\n", file );

	*len = 0;
		/* open the file */
	if (UseCkptServer)
		rval = RequestRestore(p->owner,file,(unsigned long *)len,(struct in_addr*)ip_addr,port);
	else
		rval = -1;

	if( ((file_fd=open(file,O_RDONLY)) < 0) || 
	   (rval != 1 && rval != 73 && rval != -1 && rval != -121)) {
		/* If it isn't here locally, maybe it is on the checkpoint server */
		/* And, maybe the checkpoint server still is working on the old one
		   so, if the return val is 1 (which indicates a locked file), we 
		   try again */
		if (file_fd >= 0) {
			close(file_fd);
		}
		while (rval == 1 && retry_wait < 20) {
			rval = RequestRestore(p->owner,file,(unsigned long *)len,&sin,port);
			if (rval == 1) {
				sleep(retry_wait);
				retry_wait *= 2;
			}
		} 

		*ip_addr = ntohl( sin.s_addr );
		display_ip_addr( *ip_addr );
		*port = ntohs( *port );
		dprintf( D_ALWAYS, "RestoreRequest returned %d using port %d\n", 
				rval, *port );
		if ( rval ) {
			/* Oops, couldn't find it on the checkpoint server either */
			errno = ENOENT;
			return -1;
		} else {
			return 0;
		}
	} else {

		*len = file_size( file_fd );
		dprintf( D_FULLDEBUG, "\tlen = %d\n", *len );

		get_host_addr( ip_addr );
		display_ip_addr( *ip_addr );

		create_tcp_port( port, &connect_sock );
		dprintf( D_FULLDEBUG, "\tPort = %d\n", *port );
		
		switch( child_pid = fork() ) {
		    case -1:	/* error */
			    dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
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
				
				exit( 0 );
			default:	/* the parent */
				close( file_fd );
				close( connect_sock );
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
	ssize_t		bytes_read;
	ssize_t		answer;
	pid_t	child_pid;
	int		rval;
	PROC *p = (PROC *)Proc;

	dprintf( D_ALWAYS, "\tEntering pseudo_put_file_stream\n" );
	dprintf( D_ALWAYS, "\tfile = \"%s\"\n", file );
	dprintf( D_ALWAYS, "\tlen = %d\n", len );
	dprintf( D_ALWAYS, "\towner = %s\n", p->owner );

	get_host_addr( ip_addr );
	display_ip_addr( *ip_addr );

		/* open the file */
	if ((strncmp(file, "core", 4) != MATCH) &&
		(strstr(file, "/core") == NULL) && UseCkptServer) {
		rval = RequestStore(p->owner, file, len, ip_addr, port);
		display_ip_addr( *ip_addr );
#if !defined(LINUX) /* FIX ME GREGER */
		*ip_addr = ntohl(*ip_addr);
#endif
		*port = ntohs( *port );
		dprintf(D_ALWAYS,  "Returned addr\n");
		display_ip_addr( *ip_addr );
		dprintf(D_ALWAYS, "Returned port %d\n", *port);
	} else {
		rval = -1;
	}
	if (!rval) {
		/* Checkpoint server says ok */
		return 0;
	}
		
	if( (file_fd=open(file,O_WRONLY|O_CREAT|O_TRUNC,0664)) < 0 ) {
		return -1;
	}

	get_host_addr( ip_addr );
	display_ip_addr( *ip_addr );

	create_tcp_port( port, &connect_sock );
	dprintf( D_FULLDEBUG, "\tPort = %d\n", *port );

	switch( child_pid = fork() ) {
	  case -1:	/* error */
		dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno );
		return -1;
	  case 0:	/* the child */
		data_sock = connect_file_stream( connect_sock );
		if( data_sock < 0 ) {
			exit( 1 );
		}
		bytes_read = stream_file_xfer( data_sock, file_fd, len );
		fsync( file_fd );

			/* Send status assuring peer that we got everything */
		answer = htonl( bytes_read );
		write( data_sock, &answer, sizeof(answer) );
		dprintf( D_ALWAYS,
			"\tSTREAM FILE RECEIVED OK (%d bytes)\n", bytes_read );

		exit( 0 );
	  default:	/* the parent */
	    close( file_fd );
		close( connect_sock );
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
		return -1;
	}

		/* determine the file's size */
	if( (answer=lseek(fd,0,2)) < 0 ) {
		return -1;
	}

		/* seek back to where we were in the file */
	if( lseek(fd,cur_pos,0) < 0 ) {
		return -1;
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
	memset( &sin, '\0', sizeof sin );
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	if( bind(*fd,(struct sockaddr *)&sin, sizeof sin) < 0 ) {
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
	char				this_host[MAX_STRING];
	struct hostent      *host_ptr;

		/* determine our own host name */
	if( gethostname(this_host,MAX_STRING) < 0 ) {
		EXCEPT( "gethostname" );
	}
	dprintf( D_FULLDEBUG, "\tthis_host = \"%s\"\n", this_host );

		/* Look up the address of the host */
	host_ptr = gethostbyname( this_host );
	assert( host_ptr );
	dprintf( D_FULLDEBUG, "\tFound host entry for \"%s\"\n", this_host );

	assert( host_ptr->h_length == sizeof(unsigned int) );
	memcpy( ip_addr, host_ptr->h_addr, (size_t)host_ptr->h_length );

	*ip_addr = ntohl( *ip_addr );
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
	char*	dup = (char*)malloc(1);

	dup[0] = '\0';
	if(str == NULL)
	{
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
	s->uid = ClientUid;
	s->gid = ClientGid;

		/* This is the shadow's virtual pid for the PVM task, and will
		   need to be supplied when we get around to using the structure
		   with PVM capable shadows.
		*/
	s->virt_pid = -1;

	s->soft_kill_sig = SIGUSR1;

	s->cmd = Strdup( p->cmd[0] );
	s->args = Strdup( p->args[0] );
	s->env = Strdup( p->env );
	s->iwd = Strdup( p->iwd );

	s->is_restart = has_ckpt_file();

		/* If user has supplied environment variable "CHECKPOINT = FALSE"
		   then we don't do checkpointing, otherwise we assume it's wanted */
	env_string = find_env( "CHECKPOINT", s->env );
	if( env_string && stricmp("false",env_string) == MATCH ) {
		s->ckpt_wanted = FALSE;
	} else {
		s->ckpt_wanted = TRUE;
	}

		/* If user has supplied environment variable "CONDOR_CORESIZE = nnn"
		   then we honor that limit, otherwise the core dump size is
		   unlimited.
		*/
	if( env_string = find_env("CONDOR_CORESIZE",s->env) ) {
		s->coredump_limit_exists = TRUE;
		s->coredump_limit = atoi(env_string);
	} else {
		s->coredump_limit_exists = FALSE;
		s->coredump_limit = -1;
	}

	display_startup_info( s, D_SYSCALLS );
	return 0;
}

/*
  Given the fd number of a standard file (fd 0, 1, or 2), we return
  whether the file is an ordinary file, or a pipe.  In the case of
  a pipe, we give the fd number at which the user process can find
  the pipe already open, and in the case of an ordinary file, we
  give the name of the file.

  Note: we aren't dealing with pipes yet, so for now the answer is always
  an ordinary file.
*/
int
pseudo_std_file_info( int which, char *name, int *pipe_fd )
{
	PROC	*p = (PROC *)Proc;

	switch( which ) {
		case 0:
			strcpy( name, p->in[0] );
			break;
		case 1:
			strcpy( name, p->out[0] );
			break;
		case 2:
			strcpy( name, p->err[0] );
			break;
		default:
			name[0] = '\0';
			break;
	}
	*pipe_fd = -1;

	dprintf( D_SYSCALLS, "\tfd = %d\n", which );
	dprintf( D_SYSCALLS, "\tIS_FILE, name: \"%s\", fd: %d\n",
		name, -1
	);

	return IS_FILE;
}

/*
  This is the call which answers the question "what should I do to
  open this file?", given its name.
  Parameters:
	name:
		the name specified by the user code
	pipe_fd:
		filled inthe the fd number if this file is a pipe, else -1
	extern_path:
		filled in with a name by which the executing machine
		can access the file directly.
  Return Value:
	IS_AFS:			accessible by afs as "extern_path"
	IS_NFS:			accessible by nfs as "extern_path"
	IS_RSC:			accessible by remote syscalls as "extern_path"
	IS_PRE_OPEN:	already open at fd number "pipe_fd"
	-1:				error
*/
int
pseudo_file_info( const char *name, int *pipe_fd, char *extern_path )
{
	int		answer;
	char	full_path[ _POSIX_PATH_MAX ];

	dprintf( D_SYSCALLS, "\tname = \"%s\"\n", name );

	*pipe_fd = -1;

	if( name[0] == '/' ) {
		strcpy( full_path, name );
	} else {
		strcpy( full_path, CurrentWorkingDir );
		strcat( full_path, "/" );
		strcat( full_path, name );
	}

	strcpy( extern_path, full_path );

#ifdef HPUX9
	/* I have no idea why this is happening, but I have seen it happen many
	 * times on the HPUX version, so here is a quick hack -Todd 5/19/95 */
	if ( strcmp(extern_path,"/usr/lib/nls////strerror.cat") == 0 )
		strcpy( extern_path,"/usr/lib/nls/C/strerror.cat\0");
#endif

	dprintf( D_SYSCALLS, "\tpipe_fd = %d\n", *pipe_fd);
	dprintf( D_SYSCALLS, "\tCurrentWorkingDir = \"%s\"\n", CurrentWorkingDir );
	dprintf( D_SYSCALLS, "\textern_path = \"%s\"\n", extern_path );

	if( access_via_afs(full_path) ) {
		answer = IS_AFS;
		dprintf( D_SYSCALLS, "\tanswer = IS_AFS\n" );
	} else if(access_via_nfs(full_path) ) {
		answer = IS_NFS;
		dprintf( D_SYSCALLS, "\tanswer = IS_NFS\n" );
	} else {
		answer = IS_RSC;
		dprintf( D_SYSCALLS, "\tanswer = IS_RSC\n" );
	}

	return answer;
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
  Return name of executable file for this process
*/
int
pseudo_get_a_out_name( char *path )
{
	PROC	*p = (PROC *)Proc;

	strcpy( path, ICkptName );
	dprintf( D_SYSCALLS, "\tanswer = \"%s\"\n", path );
	return 0;
}

/*
  Change directory as indicated.  We also keep track of current working
  directory so we can build full pathname from relative pathnames
  were we need them.
*/
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
  Take note of the executing machine's AFS cell.
*/
pseudo_register_afs_cell( const char *cell )
{
	strcpy( Executing_AFS_Cell, cell );
	dprintf( D_SYSCALLS, "\tCell = \"%s\"\n", cell );
}

/*
  Take note of the executing machine's filesystem domain
*/
pseudo_register_fs_domain( const char *fs_domain )
{
	strcpy( Executing_Filesystem_Domain, fs_domain );
	dprintf( D_SYSCALLS, "\tFS_Domain = \"%s\"\n", fs_domain );
}

/*
  Take note of the executing machine's UID domain
*/
pseudo_register_uid_domain( const char *uid_domain )
{
	strcpy( Executing_UID_Domain, uid_domain );
	dprintf( D_SYSCALLS, "\tUID_Domain = \"%s\"\n", uid_domain );
}

access_via_afs( const char *file )
{
	char *file_cell;

	dprintf( D_SYSCALLS, "\tentering access_via_afs()\n" );

	if( !UseAFS ) {
		dprintf( D_SYSCALLS, "\tnot configured to use AFS for file access\n" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning FALSE\n" );
		return FALSE;
	}

	if( !Executing_AFS_Cell[0] ) {
		dprintf( D_SYSCALLS, "\tdon't know cell of executing machine" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning FALSE\n" );
		return FALSE;
	}

	file_cell = get_file_cell( file );

	if( !file_cell ) {
		dprintf( D_SYSCALLS, "\t- not an AFS file\n" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning FALSE\n" );
		return FALSE;
	}

	dprintf( D_SYSCALLS,
		"\tfile_cell = \"%s\", executing_cell = \"%s\"\n",
		file_cell,
		Executing_AFS_Cell
	);
	if( strcmp(file_cell,Executing_AFS_Cell) == MATCH ) {
		dprintf( D_SYSCALLS, "\tAFS cells do match\n" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning TRUE\n" );
		return TRUE;
	} else {
		dprintf( D_SYSCALLS, "\tAFS cells don't match\n" );
		dprintf( D_SYSCALLS, "\taccess_via_afs() returning FALSE\n" );
		return FALSE;
	}
}

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
	struct stat	buf;
	int		rval;
	PROC	*p = (PROC *) Proc;

	return FileExists(RCkptName, p->owner);

#if 0
		/* see if we can stat the ckpt file */
	if( stat(RCkptName,&buf) == 0 ) {
		return TRUE;
	}

		/* reason ought to be that there wasn't one, not some other error */
	if( errno == ENOENT ) {
		rval = FileOnServer(p->owner, RCkptName);
		if (rval == 0) {
			return TRUE;
		} else {
			return FALSE;
		}
	}

	EXCEPT( "Can't stat checkpoint file \"%s\"", RCkptName );
#endif
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
}


#if !defined(PVM_RECEIVE)
int
pseudo_pvm_info(pvmd_reply, starter_tid)
char	*pvmd_reply;
int		starter_tid;
{
	return 0;
}


int
pseudo_pvm_task_info(pid, task_tid)
int pid;
int task_tid;
{
	return 0;
}


int
pseudo_suspended(suspended)
int suspended;
{
	return 0;
}


int
pseudo_subproc_status(int subproc, union wait *statp, struct rusage *rusagep)
{
	struct rusage local_rusage;

	get_local_rusage( &local_rusage );
	memcpy( &JobRusage, rusagep, sizeof(struct rusage) );
	update_job_status( &local_rusage, &JobRusage );
	LogRusage( ULog,  THIS_RUN, &local_rusage, &JobRusage );

	return 0;
}

#endif /* PVM_RECEIVE */
