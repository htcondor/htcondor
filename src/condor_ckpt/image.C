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
#include "fcntl.h"
#include <string.h>
#include "condor_syscalls.h"
#include "image.h"
#include "file_table_interf.h"

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

#ifndef SIG_DFL
#include <signal.h>
#endif

extern int errno;
int _Ckpt_Via_TCP_Stream;

const int KILO = 1024;

extern "C" int open_write_stream( const char * ckpt_file, size_t n_bytes );
extern "C" void report_image_size( int );

#if defined(OSF1)
	extern "C" unsigned int htonl( unsigned int );
	extern "C" unsigned int ntohl( unsigned int );
#elif defined(AIX32)
#	include <net/nh.h>
#else
	extern "C" unsigned long htonl( unsigned long );
	extern "C" unsigned long ntohl( unsigned long );
#endif

void terminate_with_sig( int sig );
void Suicide();

Image MyImage;
static jmp_buf Env;
static RAW_ADDR SavedStackLoc;


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

		// set up to catch SIGTSTP and do a checkpoint
	action.sa_handler = (SIG_HANDLER)Checkpoint;
	sigemptyset( &action.sa_mask );
	action.sa_flags = 0;

		// install the SIGTSTP handler
	if( sigaction(SIGTSTP,&action,NULL) < 0 ) {
		perror( "sigaction - TSTP handler" );
		exit( 1 );
	}

		// install the SIGUSR1 handler
	action.sa_handler = (SIG_HANDLER)Suicide;
	if( sigaction(SIGUSR1,&action,NULL) < 0 ) {
		perror( "sigaction - USR1 handler" );
		exit( 1 );
	}


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
	if( SavedStackLoc ) {
		stack_start = SavedStackLoc;
	} else {
		stack_start = stack_start_addr();
	}
	stack_end = stack_end_addr();
	AddSegment( "STACK", stack_start, stack_end );

		// Calculate positions of segments in ckpt file
	pos = sizeof(Header) + head.N_Segs() * sizeof(SegMap);
	for( i=0; i<head.N_Segs(); i++ ) {
		pos = map[i].SetPos( pos );
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
extern "C" user_restore_pre();
extern "C" user_restore_post(int);

int		global_user_data;
#endif

/*
  Given an "image" object containing checkpoint information which we have
  just read in, this method actually effects the restart.
*/
void
Image::Restore()
{
	int		save_fd = fd;
	int		user_data;

#if defined(PVM_CHECKPOINTING)
	user_data = user_restore_pre();
#endif
		// Overwrite our data segment with the one saved at checkpoint
		// time.
	RestoreSeg( "DATA" );

		// We have just overwritten our data segment, so the image
		// we are working with has been overwritten too.  Fortunately,
		// the only thing that has changed is the file descriptor.
	fd = save_fd;

#if defined(PVM_CHECKPOINTING)
	global_user_data = user_data;
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
	user_restore_post(global_user_data);
#endif

	LONGJMP( Env, 1 );
}

int
Image::Write()
{
	return Write( file_name );
}


/*
  Set up a stream to write our checkpoint information onto, then write
  it.  Note: there are two versions of "open_write_stream", one in
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
#if defined(ALPHA)
	unsigned int  nbytes;		// 32 bit unsigned
#else
	unsigned long  nbytes;		// 32 bit unsigned
#endif

	if( ckpt_file == 0 ) {
		ckpt_file = file_name;
	}

	if( (fd=open_write_stream(ckpt_file,len)) < 0 ) {
		perror( "open_write_stream" );
		exit( 1 );
	}

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	if( Write(fd) < 0 ) {
		return -1;
	}

		// In remote mode, our peer will send back the actual
		// number of bytes transferred as a check
	if( _Ckpt_Via_TCP_Stream ) {
		if( MyImage.GetMode() == REMOTE ) {
			bytes_read = read( fd, &nbytes, sizeof(nbytes) );
			nbytes = ntohl( nbytes );
		}
	}

	dprintf( D_ALWAYS, "About to close ckpt fd (%d)\n", fd );
	if( close(fd) < 0 ) {
		dprintf( D_ALWAYS, "Close failed!\n" );
		return -1;
	}
	dprintf( D_ALWAYS, "Closed OK\n" );

	dprintf( D_ALWAYS, "USER PROC: CHECKPOINT IMAGE SENT OK\n" );
	SetSyscalls( scm );


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
			return -1;
		}
		pos += nbytes;
		dprintf( D_ALWAYS, "Wrote Segment[%d] OK\n", i );
	}
	dprintf( D_ALWAYS, "Wrote all Segments OK\n" );
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

		// Make sure we have a valid file descriptor to read from
	if( fd < 0 && file_name && file_name[0] ) {
		if( (fd=open(file_name,O_RDONLY,0)) < 0 ) {
			perror( "open" );
			exit( 1 );
		}
	}

		// Read in the header
	if( (nbytes=read(fd,&head,sizeof(head))) < 0 ) {
		return -1;
	}
	pos += nbytes;

		// Read in the segment maps
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=read(fd,&map[i],sizeof(SegMap))) < 0 ) {
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
*/
void
Checkpoint( int sig, int code, void *scp )
{
	int		scm;


	if( MyImage.GetMode() == REMOTE ) {
		SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
	} else {
		SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	}


	dprintf( D_ALWAYS, "Got SIGTSTP\n" );
	if( SETJMP(Env) == 0 ) {
		dprintf( D_ALWAYS, "About to save MyImage\n" );
		SaveFileState();
		MyImage.Save();
		MyImage.Write();
		dprintf( D_ALWAYS,  "Ckpt exit\n" );
		SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
		terminate_with_sig( SIGUSR2 );
	} else {
		scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
		patch_registers( scp );
		MyImage.Close();

#if 1
		if( MyImage.GetMode() == REMOTE ) {
			SetSyscalls( SYS_REMOTE | SYS_MAPPED );
		} else {
			SetSyscalls( SYS_LOCAL | SYS_MAPPED );
		}
#else
		SetSyscalls( scm );
#endif
		syscall( SYS_write, 1, "About to restore files state\n", 29 );
		RestoreFileState();
		syscall( SYS_write, 1, "Done restoring files state\n", 27 );
		SetSyscalls( scm );
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
	// SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
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

	dprintf( D_ALWAYS, "About to send CHECKPOINT signal to SELF\n" );
	SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
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
	struct sigaction	act;
	pid_t		my_pid;

		// Make sure all system calls handled "straight through"
	SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		// Make sure we have the default action in place for the sig
	if( sig != SIGKILL && sig != SIGSTOP ) {
		act.sa_handler = (SIG_HANDLER)SIG_DFL;
		sigemptyset( &act.sa_mask );
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

		// Wait to die...
	sigemptyset( &mask );
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

extern "C" {
void
_condor_save_stack_location()
{
	SavedStackLoc = stack_start_addr();
}
}
