/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
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
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#if !defined(HPUX9)
#include "condor_fix_timeval.h"
#endif
#include <sys/resource.h>
#include "fcntl.h"
#include <string.h>
#include "condor_syscalls.h"
#include "condor_sys.h"
#include "image.h"
#include "file_table_interf.h"
#include "url_condor.h"
#include "condor_debug.h"
static char *_FileName_ = __FILE__;

#ifndef SIG_DFL
#include <signal.h>
#endif

extern int errno;
extern int _condor_in_file_stream;

const int KILO = 1024;

extern "C" void report_image_size( int );

#ifdef SAVE_SIGSTATE
extern "C" void condor_save_sigstates();
extern "C" void condor_restore_sigstates();
#endif

#if defined(OSF1)
	extern "C" unsigned int htonl( unsigned int );
	extern "C" unsigned int ntohl( unsigned int );
#elif defined(AIX32)
#	include <net/nh.h>
#elif defined(HPUX9)
#	include <netinet/in.h>
#else
	extern "C" unsigned long htonl( unsigned long );
	extern "C" unsigned long ntohl( unsigned long );
#endif

void terminate_with_sig( int sig );
void Suicide();
static void find_stack_location( RAW_ADDR &start, RAW_ADDR &end );
static int SP_in_data_area();
static void calc_stack_to_save();
extern "C" void _install_signal_handler( int sig, SIG_HANDLER handler );

Image MyImage;
static jmp_buf Env;
static RAW_ADDR SavedStackLoc;
volatile int InRestart = TRUE;
volatile int check_sig;		// the signal which activated the checkpoint; used
							// by some routines to determine if this is a periodic
							// checkpoint (USR2) or a check & vacate (TSTP).
static size_t StackSaveSize;

static int
net_read(int fd, void *buf, int size)
{
	int		bytes_read;
	int		this_read;

	bytes_read = 0;
	do {
		this_read = read(fd, buf, size - bytes_read);
		if (this_read < 0) {
			return this_read;
		}
		bytes_read += this_read;
		buf += this_read;
	} while (bytes_read < size);
	return bytes_read;
}

void
Header::Init()
{
	magic = MAGIC;
	n_segs = 0;
}

void
Header::Display()
{
	DUMP( " ", magic, 0x%X );
	DUMP( " ", n_segs, %d );
}

void
SegMap::Init( const char *n, RAW_ADDR c, long l )
{
	strcpy( name, n );
	file_loc = -1;
	core_loc = c;
	len = l;
}

void
SegMap::Display()
{
	DUMP( " ", name, %s );
	printf( " file_loc = %Lu (0x%X)\n", file_loc, file_loc );
	printf( " core_loc = %Lu (0x%X)\n", core_loc, core_loc );
	printf( " len = %d (0x%X)\n", len, len );
}


void
Image::SetFd( int f )
{
	fd = f;
	pos = 0;
}

void
Image::SetFileName( char *ckpt_name )
{
		// Save the checkpoint file name
	file_name = new char [ strlen(ckpt_name) + 1 ];
	strcpy( file_name, ckpt_name );

	fd = -1;
	pos = 0;
}

void
Image::SetMode( int syscall_mode )
{
	if( syscall_mode & SYS_LOCAL ) {
		mode = STANDALONE;
	} else {
		mode = REMOTE;
	}
}


/*
  These actions must be done on every startup, regardless whether it is
  local or remote, and regardless whether it is an original invocation
  or a restart.
*/
extern "C"
void
_condor_prestart( int syscall_mode )
{
	struct sigaction action;
	sigset_t		 sig_mask;

	MyImage.SetMode( syscall_mode );

		// Initialize open files table
	InitFileState();

		// Install initial signal handlers
	_install_signal_handler( SIGTSTP, (SIG_HANDLER)Checkpoint );
	_install_signal_handler( SIGUSR2, (SIG_HANDLER)Checkpoint );
	_install_signal_handler( SIGUSR1, (SIG_HANDLER)Suicide );

	calc_stack_to_save();

}

extern "C" void
_install_signal_handler( int sig, SIG_HANDLER handler )
{
	int		scm;
	struct sigaction action;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	action.sa_handler = handler;
	sigemptyset( &action.sa_mask );
	/* We do not want "recursive" checkpointing going on.  So block SIGUSR2
		during a SIGTSTP checkpoint, and vice-versa.  -Todd Tannenbaum, 8/95 */
	if ( sig == SIGTSTP )
		sigaddset(&action.sa_mask,SIGUSR2);
	if ( sig == SIGUSR2 )
		sigaddset(&action.sa_mask,SIGTSTP);
	
	action.sa_flags = 0;

	if( sigaction(sig,&action,NULL) < 0 ) {
		perror( "sigaction" );
		exit( 1 );
	}

	SetSyscalls( scm );
}


/*
  Save checkpoint information about our process in the "image" object.  Note:
  this only saves the information in the object.  You must then call the
  Write() method to get the image transferred to a checkpoint file (or
  possibly moved to another process.
*/
void
Image::Save()
{
	RAW_ADDR	data_start, data_end;
	RAW_ADDR	stack_start, stack_end;
	ssize_t		pos;
	int			i;

	head.Init();

		// Set up data segment
	data_start = data_start_addr();
	data_end = data_end_addr();
	AddSegment( "DATA", data_start, data_end );

		// Set up stack segment
	find_stack_location( stack_start, stack_end );
	AddSegment( "STACK", stack_start, stack_end );

		// Calculate positions of segments in ckpt file
	pos = sizeof(Header) + head.N_Segs() * sizeof(SegMap);
	for( i=0; i<head.N_Segs(); i++ ) {
		pos = map[i].SetPos( pos );
	}

	if( pos < 0 ) {
		EXCEPT( "Internal error, ckpt size calculated is %d", pos );
	}

	dprintf( D_ALWAYS, "Size of ckpt image = %d bytes\n", pos );
	len = pos;

	valid = TRUE;
}

ssize_t
SegMap::SetPos( ssize_t my_pos )
{
	file_loc = my_pos;
	return file_loc + len;
}

void
Image::Display()
{
	int		i;

	printf( "===========\n" );
	printf( "Ckpt File Header:\n" );
	head.Display();
	for( i=0; i<head.N_Segs(); i++ ) {
		printf( "Segment %d:\n", i );
		map[i].Display();
	}
	printf( "===========\n" );
}

void
Image::AddSegment( const char *name, RAW_ADDR start, RAW_ADDR end )
{
	long	len = end - start;
	int idx = head.N_Segs();

	if( idx >= MAX_SEGS ) {
		fprintf( stderr, "Don't know how to grow segment map yet!\n" );
		exit( 1 );
	}
	head.IncrSegs();
	map[idx].Init( name, start, len );
}

char *
Image::FindSeg( void *addr )
{
	int		i;
	if( !valid ) {
		return NULL;
	}
	for( i=0; i<head.N_Segs(); i++ ) {
		if( map[i].Contains(addr) ) {
			return map[i].GetName();
		}
	}
	return NULL;
}

BOOL
SegMap::Contains( void *addr )
{
	return ((RAW_ADDR)addr >= core_loc) && ((RAW_ADDR)addr < core_loc + len);
}


#if defined(PVM_CHECKPOINTING)
extern "C" user_restore_pre(char *, int);
extern "C" user_restore_post(char *, int);

char	global_user_data[256];
#endif

/*
  Given an "image" object containing checkpoint information which we have
  just read in, this method actually effects the restart.
*/
void
Image::Restore()
{
	int		save_fd = fd;
	char	user_data[256];

#if defined(PVM_CHECKPOINTING)
	user_restore_pre(user_data, sizeof(user_data));
#endif
		// Overwrite our data segment with the one saved at checkpoint
		// time.
	RestoreSeg( "DATA" );

		// We have just overwritten our data segment, so the image
		// we are working with has been overwritten too.  Fortunately,
		// the only thing that has changed is the file descriptor.
	fd = save_fd;

#if defined(PVM_CHECKPOINTING)
	memcpy(global_user_data, user_data, sizeof(user_data));
#endif

		// Now we're going to restore the stack, so we move our execution
		// stack to a temporary area (in the data segment), then call
		// the RestoreStack() routine.
	ExecuteOnTmpStk( RestoreStack );

		// RestoreStack() also does the jump back to user code
	fprintf( stderr, "Error, should never get here\n" );
	exit( 1 );
}

void
Image::RestoreSeg( const char *seg_name )
{
	int		i;

	for( i=0; i<head.N_Segs(); i++ ) {
		if( strcmp(seg_name,map[i].GetName()) == 0 ) {
			if( (pos = map[i].Read(fd,pos)) < 0 ) {
				perror( "SegMap::Read()" );
				exit( 1 );
			} else {
				return;
			}
		}
	}
	fprintf( stderr, "Can't find segment \"%s\"\n", seg_name );
	exit( 1 );
}




void
RestoreStack()
{

#if defined(ALPHA)			
	unsigned int nbytes;		// 32 bit unsigned
#else
	unsigned long nbytes;		// 32 bit unsigned
#endif
	int		status;

	MyImage.RestoreSeg( "STACK" );

		// In remote mode, we have to send back size of ckpt informaton
	if( MyImage.GetMode() == REMOTE ) {
		nbytes = MyImage.GetLen();
		nbytes = htonl( nbytes );
		status = write( MyImage.GetFd(), &nbytes, sizeof(nbytes) );
		dprintf( D_ALWAYS, "USER PROC: CHECKPOINT IMAGE RECEIVED OK\n" );

		SetSyscalls( SYS_REMOTE | SYS_MAPPED );
	} else {
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
	}

#if defined(PVM_CHECKPOINTING)
	user_restore_post(global_user_data, sizeof(global_user_data));
#endif

	LONGJMP( Env, 1 );
}

int
Image::Write()
{
	if (fd == -1) {
		return Write( file_name );
	} else {
		return Write( fd );
	}
}


/*
  Set up a stream to write our checkpoint information onto, then write
  it.  Note: there are two versions of "open_ckpt_stream", one in
  "local_startup.c" to be linked with programs for "standalone"
  checkpointing, and one in "remote_startup.c" to be linked with
  programs for "remote" checkpointing.  Of course, they do very different
  things, but in either case a file descriptor is returned which we
  should access in LOCAL and UNMAPPED mode.
*/
int
Image::Write( const char *ckpt_file )
{
	int	fd;
	int	status;
	int	scm;
	int bytes_read;
	char	tmp_name[ _POSIX_PATH_MAX ];
#if defined(ALPHA)
	unsigned int  nbytes;		// 32 bit unsigned
#else
	unsigned long  nbytes;		// 32 bit unsigned
#endif

	if( ckpt_file == 0 ) {
		ckpt_file = file_name;
	}

		// Generate tmp file name
	dprintf( D_ALWAYS, "Checkpoint name is \"%s\"\n", ckpt_file );

		// Open the tmp file
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	tmp_name[0] = '\0';
	if( (fd=open_url(tmp_name,O_WRONLY|O_TRUNC|O_CREAT,len)) < 0 ) {
		sprintf( tmp_name, "%s.tmp", ckpt_file );
		dprintf( D_ALWAYS, "Tmp name is \"%s\"\n", tmp_name );
		if ((fd = open_ckpt_file(tmp_name, O_WRONLY|O_TRUNC|O_CREAT,
								len)) < 0)  {
			perror( "open_ckpt_file" );
			exit( 1 );
		}
	}

		// Write out the checkpoint
	if( Write(fd) < 0 ) {
		return -1;
	}

		// Have to check close() in AFS
	dprintf( D_ALWAYS, "About to close ckpt fd (%d)\n", fd );
	if( close(fd) < 0 ) {
		dprintf( D_ALWAYS, "Close failed!\n" );
		return -1;
	}
	dprintf( D_ALWAYS, "Closed OK\n" );

	SetSyscalls( scm );

		// We now know it's complete, so move it to the real ckpt file name
	dprintf( D_ALWAYS, "About to rename \"%s\" to \"%s\"\n",tmp_name,ckpt_file);
	if( rename(tmp_name,ckpt_file) < 0 ) {
		perror( "rename" );
		exit( 1 );
	}
	dprintf( D_ALWAYS, "Renamed OK\n" );

		// Report
	dprintf( D_ALWAYS, "USER PROC: CHECKPOINT IMAGE SENT OK\n" );

		// In remote mode we update the shadow on our image size
	if( MyImage.GetMode() == REMOTE ) {
		report_image_size( (MyImage.GetLen() + KILO - 1) / KILO );
	}

	return 0;
}

/*
  Write our checkpoint "image" to a given file descriptor.  At this level
  it makes no difference whether the file descriptor points to a local
  file, a remote file, or another process (for direct migration).
*/
int
Image::Write( int fd )
{
	int		i;
	ssize_t	pos = 0;
	ssize_t	nbytes;
	ssize_t ack;
	int		status;

		// Write out the header
	if( (nbytes=write(fd,&head,sizeof(head))) < 0 ) {
		return -1;
	}
	pos += nbytes;
	dprintf( D_ALWAYS, "Wrote headers OK\n" );

		// Write out the SegMaps
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=write(fd,&map[i],sizeof(map[i]))) < 0 ) {
			return -1;
		}
		pos += nbytes;
		dprintf( D_ALWAYS, "Wrote SegMap[%d] OK\n", i );
	}
	dprintf( D_ALWAYS, "Wrote all SegMaps OK\n" );

		// Write out the Segments
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=map[i].Write(fd,pos)) < 0 ) {
			dprintf( D_ALWAYS, "Write() of segment %d failed\n", i );
			dprintf( D_ALWAYS, "errno = %d, nbytes = %d\n", errno, nbytes );
			return -1;
		}
		pos += nbytes;
		dprintf( D_ALWAYS, "Wrote Segment[%d] OK\n", i );
	}
	dprintf( D_ALWAYS, "Wrote all Segments OK\n" );

		/* When using the stream protocol the shadow echo's the number
		   of bytes transferred as a final acknowledgement. */
	if( _condor_in_file_stream ) {
		status = net_read( fd, &ack, sizeof(ack) );
		if( status < 0 ) {
			EXCEPT( "Can't read final ack from the shadow" );
		}

		ack = ntohl( ack );		// Ack is in network byte order, fix here
		if( ack != len ) {
			EXCEPT( "Ack - expected %d, but got %d\n", len, ack );
		}
	}

	return 0;
}


/*
  Read in our checkpoint "image" from a given file descriptor.  The
  descriptor could point to a local file, a remote file, or another
  process (in the case of direct migration).  Here we only read in
  the image "header" and the maps describing the segments we need to
  restore.  The Restore() function will do the rest.
*/
int
Image::Read()
{
	int		i;
	ssize_t	nbytes;
	char	buf[100];

		// Make sure we have a valid file descriptor to read from
	if( fd < 0 && file_name && file_name[0] ) {
		if( (fd=open_url(file_name,O_RDONLY,0)) < 0 ) {
			if( (fd=open_ckpt_file(file_name,O_RDONLY,0)) < 0 ) {
				sprintf(buf, "open_ckpt_file(%s)", file_name);
				perror( buf );
				exit( 1 );
			}
		}
	}

		// Read in the header
	if( (nbytes=net_read(fd,&head,sizeof(head))) < 0 ) {
		return -1;
	}
	pos += nbytes;

		// Read in the segment maps
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=net_read(fd,&map[i],sizeof(SegMap))) < 0 ) {
			return -1;
		}
		pos += nbytes;
	}

	return 0;
}

void
Image::Close()
{
	if( fd < 0 ) {
		fprintf( stderr, "Image::Close - file not open\n" );
		abort();
	}
	close( fd );
	/* The next checkpoint is going to assume the fd is -1, so set it here */
	fd = -1;
}

ssize_t
SegMap::Read( int fd, ssize_t pos )
{
	ssize_t nbytes;
	char *orig_brk;
	char *cur_brk;
	char	*ptr;
	size_t	bytes_to_go;
	size_t	read_size;


	if( pos != file_loc ) {
		fprintf( stderr, "Checkpoint sequence error\n" );
		exit( 1 );
	}
	if( strcmp(name,"DATA") == 0 ) {
		orig_brk = (char *)sbrk(0);
		if( orig_brk < (char *)(core_loc + len) ) {
			brk( (char *)(core_loc + len) );
		}
		cur_brk = (char *)sbrk(0);
	}

		// This overwrites an entire segment of our address space
		// (data or stack).  Assume we have been handed an fd which
		// can be read by purely local syscalls, and we don't need
		// to need to mess with the system call mode, fd mapping tables,
		// etc. as none of that would work considering we are overwriting
		// them.
	bytes_to_go = len;
	ptr = (char *)core_loc;
	while( bytes_to_go ) {
		read_size = bytes_to_go > 4096 ? 4096 : bytes_to_go;
		nbytes =  syscall( SYS_read, fd, (void *)ptr, read_size );
		if( nbytes < 0 ) {
			return -1;
		}
		bytes_to_go -= nbytes;
		ptr += nbytes;
	}

	return pos + len;
}

ssize_t
SegMap::Write( int fd, ssize_t pos )
{
	if( pos != file_loc ) {
		fprintf( stderr, "Checkpoint sequence error\n" );
	}
	return write(fd,(void *)core_loc,(size_t)len);
}

extern "C" {

/*
  This is the signal handler which actually effects a checkpoint.  This
  must be implemented as a signal handler, since we assume the signal
  handling code provided by the system will save and restore important
  elements of our context (register values, etc.).  A process wishing
  to checkpoint itself should generate the correct signal, not call this
  routine directory, (the ckpt()) function does this.
  8/95: And now, SIGTSTP means "checkpoint and vacate", and other signal
  that gets here (aka SIGUSR2) means checkpoint and keep running (a 
  periodic checkpoint). -Todd Tannenbaum
*/
void
Checkpoint( int sig, int code, void *scp )
{
	int		scm;

	dprintf( D_ALWAYS, "Entering Checkpoint()\n" );

		// No sense trying to do a checkpoint in the middle of a
		// restart, just quit leaving the current ckpt entact.
	if( InRestart ) {
		if ( sig == SIGTSTP )
			Suicide();
		else
			return;
	}

	check_sig = sig;

	if( MyImage.GetMode() == REMOTE ) {
		scm = SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
		if ( MyImage.GetFd() != -1 ) {
			// Here we make _certain_ that fd is -1.  on remote checkpoints,
			// the fd is always new since we open a fresh TCP socket to the
			// shadow.  I have detected some buggy behavior where the remote
			// job prematurely exits with a status 4, and think that it is
			// related to the fact that fd is _not_ -1 here, so we make
			// certain.  Hopefully the real bug will be found someday and
			// this "patch" can go away...  -Todd 11/95
			dprintf(D_ALWAYS,"WARNING: fd is %d for remote checkpoint, should be -1\n",MyImage.GetFd());
			MyImage.SetFd( -1 );
		}
	} else {
		scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	}


	if ( sig == SIGTSTP ) {
		dprintf( D_ALWAYS, "Got SIGTSTP\n" );
	} else {
		dprintf( D_ALWAYS, "Got SIGUSR2\n" );
	}

	if( SETJMP(Env) == 0 ) {	// Checkpoint
		dprintf( D_ALWAYS, "About to save MyImage\n" );
		InRestart = TRUE;	// not strictly true, but needed in our saved data
#ifdef SAVE_SIGSTATE
		dprintf( D_ALWAYS, "About to save signal state\n" );
		condor_save_sigstates();
		dprintf( D_ALWAYS, "Done saving signal state\n" );
#endif
		SaveFileState();
		MyImage.Save();
		MyImage.Write();
		if ( sig == SIGTSTP ) {
			/* we have just checkpointed; now time to vacate */
			dprintf( D_ALWAYS,  "Ckpt exit\n" );
			SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
			terminate_with_sig( SIGUSR2 );
			/* should never get here */
		} else {
			/* we have just checkpointed, but this is a periodic checkpoint.
			 * so, update the shadow with accumulated CPU time info if we
			 * are not standalone, and then continue running. -Todd Tannenbaum */
			if ( MyImage.GetMode() == REMOTE ) {

				// first, reset the fd to -1.  this is normally done in
				// a call in remote_startup, but that will not get called
				// before the next periodic checkpoint so we must clear
				// it here.
				MyImage.SetFd( -1 );

				/* now update shadow with CPU time info.  unfortunately, we need
				 * to convert to struct rusage here in the user code, because
				 * clock_tick is platform dependent and we don't want CPU times
				 * messed up if the shadow is running on a different architecture */
				struct tms posix_usage;
				struct rusage bsd_usage;
				long clock_tick;

				scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
				memset(&bsd_usage,0,sizeof(struct rusage));
				times( &posix_usage );
#if defined(OSF1)
			    clock_tick = CLK_TCK;
#else
        		clock_tick = sysconf( _SC_CLK_TCK );
#endif
				bsd_usage.ru_utime.tv_sec = posix_usage.tms_utime / clock_tick;
				bsd_usage.ru_utime.tv_usec = posix_usage.tms_utime % clock_tick;
				(bsd_usage.ru_utime.tv_usec) *= 1000000 / clock_tick;
				bsd_usage.ru_stime.tv_sec = posix_usage.tms_stime / clock_tick;
				bsd_usage.ru_stime.tv_usec = posix_usage.tms_stime % clock_tick;
				(bsd_usage.ru_stime.tv_usec) *= 1000000 / clock_tick;
				SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
				(void)REMOTE_syscall( CONDOR_send_rusage, (void *) &bsd_usage );
				SetSyscalls( scm );
			}
					
			dprintf(D_ALWAYS, "Periodic Ckpt complete, doing a virtual restart...\n");
			LONGJMP( Env, 1);
		}
	} else {					// Restart
		if ( check_sig == SIGTSTP ) {
			scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
			patch_registers( scp );
			MyImage.Close();

			if( MyImage.GetMode() == REMOTE ) {
				SetSyscalls( SYS_REMOTE | SYS_MAPPED );
			} else {
				SetSyscalls( SYS_LOCAL | SYS_MAPPED );
			}
			RestoreFileState();
			syscall( SYS_write, 1, "Done restoring files state\n", 27 );
		} else {
			patch_registers( scp );
		}
#ifdef SAVE_SIGSTATE
		syscall( SYS_write, 1, "About to restore signal state\n", 30 );
		condor_restore_sigstates();
		syscall( SYS_write, 1, "Done restoring signal state\n", 28 );
#endif
		SetSyscalls( scm );
		InRestart = FALSE;
		syscall( SYS_write, 1, "About to return to user code\n", 29 );
		return;
	}
}

void
init_image_with_file_name( char *ckpt_name )
{
	MyImage.SetFileName( ckpt_name );
}

void
init_image_with_file_descriptor( int fd )
{
	MyImage.SetFd( fd );
}


/*
  Effect a restart by reading in an "image" containing checkpointing
  information and then overwriting our process with that image.
*/
void
restart( )
{
	InRestart = TRUE;

	MyImage.Read();
	MyImage.Restore();
}

}	// end of extern "C"



/*
  Checkpointing must by implemented as a signal handler.  This routine
  generates the required signal to invoke the handler.
*/
void
ckpt()
{
	int		scm;
	sigset_t	n_sigmask;
	sigset_t	o_sigmask;

	sigemptyset(&n_sigmask);
	sigaddset(&n_sigmask, SIGTSTP);
	sigaddset(&n_sigmask, SIGUSR1);
	dprintf( D_ALWAYS, "About to send CHECKPOINT signal to SELF\n" );
	SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	sigprocmask(SIG_UNBLOCK, &n_sigmask, &o_sigmask);
	kill( getpid(), SIGTSTP );
}

/*
** Some FORTRAN compilers expect "_" after the symbol name.
*/
extern "C" {
void
ckpt_() {
    ckpt();
}
}

/*
  Arrange to terminate abnormally with the given signal.  Note: the
  expectation is that the signal is one whose default action terminates
  the process - could be with a core dump or not, depending on the sig.
*/
void
terminate_with_sig( int sig )
{
	sigset_t	mask;
	pid_t		my_pid;
	struct sigaction act;

		// Make sure all system calls handled "straight through"
	SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		// Make sure we have the default action in place for the sig
	if( sig != SIGKILL && sig != SIGSTOP ) {
		act.sa_handler = (SIG_HANDLER)SIG_DFL;
		// mask everything so no user-level sig handlers run
		sigfillset( &act.sa_mask );
		act.sa_flags = 0;
		errno = 0;
		if( sigaction(sig,&act,0) < 0 ) {
			EXCEPT( "sigaction" );
		}
	}

		// Send ourself the signal
	my_pid = getpid();
	dprintf( D_ALWAYS, "About to send signal %d to process %d\n", sig, my_pid );
	if( kill(my_pid,sig) < 0 ) {
		EXCEPT( "kill" );
	}

		// Wait to die... and mask all sigs except the one to kill us; this
		// way a user's sig won't sneak in on us - Todd 12/94
	sigfillset( &mask );
	sigdelset( &mask, sig );
	sigsuspend( &mask );

		// Should never get here
	EXCEPT( "Should never get here" );

}

/*
  We have been requested to exit.  We do it by sending ourselves a
  SIGKILL, i.e. "kill -9".
*/
void
Suicide()
{
	terminate_with_sig( SIGKILL );
}

static void
find_stack_location( RAW_ADDR &start, RAW_ADDR &end )
{
	if( SP_in_data_area() ) {
		dprintf( D_ALWAYS, "Stack pointer in data area\n" );
		if( StackGrowsDown() ) {
			end = stack_end_addr();
			start = end - StackSaveSize;
		} else {
			start = stack_start_addr();
			end = start + StackSaveSize;
		}
	} else {
		start = stack_start_addr();
		end = stack_end_addr();
	}
}

extern "C" double atof( const char * );

const size_t	MEG = (1024 * 1024);

void
calc_stack_to_save()
{
	char	*ptr;

	ptr = getenv( "CONDOR_STACK_SIZE" );
	if( ptr ) {
		StackSaveSize = (size_t) (atof(ptr) * MEG);
	} else {
		StackSaveSize = MEG * 2;	// default 2 megabytes
	}
}

/*
  Return true if the stack pointer points into the "data" area.  This
  will often be the case for programs which utilize threads or co-routine
  packages.
*/
static int
SP_in_data_area()
{
	RAW_ADDR	data_start, data_end;
	RAW_ADDR	SP;

	data_start = data_start_addr();
	data_end = data_end_addr();

	if( StackGrowsDown() ) {
		SP = stack_start_addr();
	} else {
		SP = stack_end_addr();
	}

	return data_start <= SP && SP <= data_end;
}

extern "C" {
void
_condor_save_stack_location()
{
	SavedStackLoc = stack_start_addr();
}
}
