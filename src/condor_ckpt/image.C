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

#if !defined(Solaris)
#define _POSIX_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(LINUX)
#include <sys/mman.h>
extern "C" {
	caddr_t MMAP(caddr_t, size_t, int, int, int, off_t);
};
#endif

#if defined(IRIX53)
#define MA_SHARED	0x0008	/* mapping is a shared or mapped object */
#include <sys/mman.h>		// for mmap()
#include <sys/syscall.h>        // for syscall()
#include <sys/time.h>
#include <values.h>
#endif
#if defined(Solaris)
#if !defined(Solaris251)
#include </usr/ucbinclude/sys/rusage.h>	// for rusage
#endif
#include <netconfig.h>		// for setnetconfig()
#include <sys/mman.h>		// for mmap()
#include <sys/syscall.h>        // for syscall()
#include <sys/time.h>
#include <values.h>
#endif
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
extern "C"	int	syscall(int ...);
extern "C"	int	SYSCALL(int ...);

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
#elif defined(Solaris) && defined(sun4m)
    #define htonl(x)        (x)
    #define ntohl(x)        (x)
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
extern "C" int open_ckpt_file( const char *name, int flags, size_t n_bytes );

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
SegMap::Init( const char *n, RAW_ADDR c, long l, int p )
{
	strcpy( name, n );
	file_loc = -1;
	core_loc = c;
	len = l;
	prot = p;
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
	MyImage.SetMode( syscall_mode );

		// Initialize open files table
	InitFileState();

		// Install initial signal handlers
	_install_signal_handler( SIGTSTP, (SIG_HANDLER)Checkpoint );
	_install_signal_handler( SIGUSR2, (SIG_HANDLER)Checkpoint );
	_install_signal_handler( SIGUSR1, (SIG_HANDLER)Suicide );

	calc_stack_to_save();

	/* On the first call to setnetconfig(), space is malloc()'ed to store
	   the net configuration database.  This call is made by Solaris during
	   a socket() call, which we do inside the Checkpoint signal handler.
	   So, we call setnetconfig() here to do the malloc() before we are
	   in the signal handler.  (Doing a malloc() inside a signal handler
	   can have nasty consequences.)  -Jim B.  */
	/* Note that the floating point code below is needed to initialize
	   this process as a process which potentially uses floating point
	   registers.  Otherwise, the initialization is possibly not done
	   on restart, and we will lose floats on context switches.  -Jim B. */
#if defined(Solaris)
	setnetconfig();
#if defined(sun4m)
	float x=23, y=14, z=256;
	if ((x+y)>z) {
		EXCEPT( "Internal error: Solaris floating point test failed\n");
	}
	z=x*y;
#endif
#endif
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
#if !defined(Solaris) && !defined(LINUX) && !defined(IRIX53)
	RAW_ADDR	stack_start, stack_end;
#else
	RAW_ADDR	addr_start, addr_end;
	int             numsegs, prot, rtn, stackseg;
#endif
	RAW_ADDR	data_start, data_end;
	ssize_t		pos;
	int			i;


	head.Init();

#if !defined(Solaris) && !defined(LINUX) && !defined(IRIX53)

		// Set up data segment
	data_start = data_start_addr();
	data_end = data_end_addr();
	AddSegment( "DATA", data_start, data_end, 0 );

		// Set up stack segment
	find_stack_location( stack_start, stack_end );
	AddSegment( "STACK", stack_start, stack_end, 0 );

#else

	// Note: the order in which the segments are put into the checkpoint
	// file is important.  The DATA segment must be first, followed by
	// all shared library segments, and then followed by the STACK
	// segment.  There are two reasons for this: (1) restoring the 
	// DATA segment requires the use of sbrk(), which is in libc.so; if
	// we mess up libc.so, our calls to sbrk() will fail; (2) we 
	// restore segments in order, and the STACK segment must be restored
	// last so that we can immediately return to user code.  - Jim B.

	numsegs = num_segments();

#if !defined(IRIX53) && !defined(Solaris)

	// data segment is saved and restored as before, using sbrk()
	data_start = data_start_addr();
	data_end = data_end_addr();
	dprintf( D_ALWAYS, "data start = 0x%lx, data end = 0x%lx\n",
			data_start, data_end );
	AddSegment( "DATA", data_start, data_end, 0 );

#else

	// sbrk() doesn't give reliable values on IRIX53 and Solaris
	// use ioctl info instead
	data_start=MAXLONG;
	data_end=0;
	for( i=0; i<numsegs; i++ ) {
		rtn = segment_bounds(i, addr_start, addr_end, prot);
		if (rtn == 3) {
			if (data_start > addr_start)
				data_start = addr_start;
			if (data_end < addr_end)
				data_end = addr_end;
		}
	}
	AddSegment( "DATA", data_start, data_end, prot );

#endif	

	for( i=0; i<numsegs; i++ ) {
		rtn = segment_bounds(i, addr_start, addr_end, prot);
		switch (rtn) {
		case -1:
			EXCEPT( "Internal error, segment_bounds"
				"returned -1");
			break;
		case 0:
#if defined(LINUX)
			addr_end=find_correct_vm_addr(addr_start, addr_end, prot);
#endif
			AddSegment( "SHARED LIB", addr_start, addr_end, prot);
			break;
		case 1:
			break;		// don't checkpoint text segment
		case 2:
			stackseg = i;	// don't add STACK segment until the end
			break;
		case 3:
			break;		// don't add DATA segment again
		default:
			EXCEPT( "Internal error, segment_bounds"
				"returned unrecognized value");
		}
	}	
	// now add stack segment
	rtn = segment_bounds(stackseg, addr_start, addr_end, prot);
	AddSegment( "STACK", addr_start, addr_end, prot);
	dprintf( D_ALWAYS, "stack start = 0x%lx, stack end = 0x%lx\n",
			addr_start, addr_end);
	dprintf( D_ALWAYS, "Current segmap dump follows\n");
	display_prmap();

#endif

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
Image::AddSegment( const char *name, RAW_ADDR start, RAW_ADDR end, int prot )
{
	long	len = end - start;
	int idx = head.N_Segs();

	if( idx >= MAX_SEGS ) {
		fprintf( stderr, "Don't know how to grow segment map yet!\n" );
		exit( 1 );
	}
	head.IncrSegs();
	map[idx].Init( name, start, len, prot );
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
	user_restore_pre(user_data, sizeof(user_data));
#endif
		// Overwrite our data segment with the one saved at checkpoint
		// time.
//	RestoreSeg( "DATA" );

		// Overwrite our data segment with the one saved at checkpoint
		// time *and* restore any saved shared libraries.
	RestoreAllSegsExceptStack();



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

/* don't assume we have libc.so in a good state right now... - Jim B. */
static int mystrcmp(const char *str1, const char *str2)
{
	while (*str1 != '\0' && *str2 != '\0' && *str1 == *str2) {
		str1++;
		str2++;
	}
	return (int) *str1 - *str2;
}

void
Image::RestoreSeg( const char *seg_name )
{
	int		i;

	for( i=0; i<head.N_Segs(); i++ ) {
		if( mystrcmp(seg_name,map[i].GetName()) == 0 ) {
			if( (pos = map[i].Read(fd,pos)) < 0 ) {
				perror( "SegMap::Read()" );
				exit( 1 );
			} else {
				return;
			}
		}
	}
	dprintf( D_ALWAYS, "Can't find segment \"%s\"\n", seg_name );
	exit( 1 );
}

void Image::RestoreAllSegsExceptStack()
{
	int		i;
	int		save_fd = fd;

#if defined(Solaris) || defined(IRIX53)
	dprintf( D_ALWAYS, "Current segmap dump follows\n");
	display_prmap();
#endif
	for( i=0; i<head.N_Segs(); i++ ) {
		if( mystrcmp("STACK",map[i].GetName()) != 0 ) {
			if( (pos = map[i].Read(fd,pos)) < 0 ) {
				perror( "SegMap::Read()" );
				exit( 1 );
			}
		}
		else if (i<head.N_Segs()-1) {
			dprintf( D_ALWAYS, "Checkpoint file error: STACK is not the "
					"last segment in ckpt file.\n");
			exit( 1 );
		}
		fd = save_fd;
	}

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
	return Write( file_name );
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
	if( (fd=open_url(ckpt_file,O_WRONLY|O_TRUNC|O_CREAT,len)) < 0 ) {
		sprintf( tmp_name, "%s.tmp", ckpt_file );
		dprintf( D_ALWAYS, "Tmp name is \"%s\"\n", tmp_name );
		if ((fd = open_ckpt_file(tmp_name, O_WRONLY|O_TRUNC|O_CREAT,
								len)) < 0)  {
			if (check_sig == SIGUSR2) { // periodic checkpoint
				dprintf( D_ALWAYS,
						"open_ckpt_file failed, aborting periodic ckpt\n" );
				return -1;
			}
			else {
				perror( "open_ckpt_file" );
				exit( 1 );
			}
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
	if (tmp_name[0] != '\0') {
		dprintf(D_ALWAYS, "About to rename \"%s\" to \"%s\"\n",
				tmp_name, ckpt_file);
		if( rename(tmp_name,ckpt_file) < 0 ) {
			if (check_sig == SIGUSR2) { // periodic checkpoint
				dprintf( D_ALWAYS,
						"rename failed, aborting periodic ckpt\n" );
				return -1;
			}
			else {
				perror( "rename" );
				exit( 1 );
			}
		}
		dprintf( D_ALWAYS, "Renamed OK\n" );
	}

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

		// Make sure we have a valid file descriptor to read from
	if( fd < 0 && file_name && file_name[0] ) {
		if( (fd=open_url(file_name,O_RDONLY,0)) < 0 ) {
			if( (fd=open_ckpt_file(file_name,O_RDONLY,0)) < 0 ) {
				perror( "open_ckpt_file" );
				exit( 1 );
			}
		}
	}

		// Read in the header
	if( (nbytes=net_read(fd,&head,sizeof(head))) < 0 ) {
		return -1;
	}
	pos += nbytes;
	dprintf( D_ALWAYS, "Read headers OK\n" );

		// Read in the segment maps
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=net_read(fd,&map[i],sizeof(SegMap))) < 0 ) {
			return -1;
		}
		pos += nbytes;
		dprintf( D_ALWAYS, "Read SegMap[%d] OK\n", i );
	}
	dprintf( D_ALWAYS, "Read all SegMaps OK\n" );

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
	long	saved_len = len;
	int 	saved_prot = prot;
	RAW_ADDR	saved_core_loc = core_loc;

	if( pos != file_loc ) {
		fprintf( stderr, "Checkpoint sequence error\n" );
		exit( 1 );
	}

	if( mystrcmp(name,"DATA") == 0 ) {
		orig_brk = (char *)sbrk(0);
		if( orig_brk < (char *)(core_loc + len) ) {
			brk( (char *)(core_loc + len) );
		}
		cur_brk = (char *)sbrk(0);
	}

#if defined(Solaris) || defined(IRIX53) || defined(LINUX)
	else if ( mystrcmp(name,"SHARED LIB") == 0) {
		int zfd, segSize = len;
		if ((zfd = SYSCALL(SYS_open, "/dev/zero", O_RDWR)) == -1) {
			fprintf( stderr, "Unable to open /dev/zero in read/write mode.\n");
			perror("open");
			exit( 1 );
		}

	  /* Some notes about mmap:
	     - The MAP_FIXED flag will ensure that the memory allocated is
	       exactly what was requested.
	     - Both the addr and off parameters must be aligned and sized
	       according to the value returned by getpagesize() when MAP_FIXED
	       is used.  If the len parameter is not a multiple of the page
	       size for the machine, then the system will automatically round
	       up. 
	     - Protections must allow writing, so that the dll data can be
	       copied into memory. 
	     - Memory should be private, so we don't mess with any other
	       processes that might be accessing the same library. */

//		fprintf(stderr, "Calling mmap(loc = 0x%lx, size = 0x%lx, "
//			"prot = %d, fd = %d, offset = 0)\n", core_loc, segSize,
//			prot|PROT_WRITE, zfd);
#if defined(Solaris)
		if ((MMAP((caddr_t)core_loc, (size_t)segSize,
				prot|PROT_WRITE,
				MAP_PRIVATE|MAP_FIXED, zfd,
				(off_t)0)) == MAP_FAILED) {
#elif defined(IRIX53)
		if (MMAP((caddr_t)saved_core_loc, (size_t)segSize,
					 (saved_prot|PROT_WRITE)&(~MA_SHARED),
					 MAP_PRIVATE|MAP_FIXED, zfd,
					 (off_t)0) == -1) {
#elif defined(LINUX)
		long status;
		if ((status=(long)MMAP((void *)core_loc, (size_t)segSize,
				prot|PROT_WRITE,
				MAP_PRIVATE|MAP_FIXED, zfd,
				(off_t)0)) == -1) {
#endif
			perror("mmap");
			fprintf(stderr, "Attempted to mmap /dev/zero at "
				"address 0x%lx, size 0x%lx\n", saved_core_loc,
				segSize);
			fprintf(stderr, "Current segmap dump follows\n");
			display_prmap();
			exit(1);
		}

		/* WARNING: We have potentially just overwritten libc.so.  Do
		   not make calls that are defined in this (or any other)
		   shared library until we restore all shared libraries from
		   the checkpoint (i.e., use mystrcmp and SYSCALL).  -Jim B. */

		if (SYSCALL(SYS_close, zfd) < 0) {
			fprintf( stderr, "Unable to close /dev/zero file descriptor.\n" );
			perror("close");
			exit(1);
		}
	}		
#endif		

		// This overwrites an entire segment of our address space
		// (data or stack).  Assume we have been handed an fd which
		// can be read by purely local syscalls, and we don't need
		// to need to mess with the system call mode, fd mapping tables,
		// etc. as none of that would work considering we are overwriting
		// them.
	bytes_to_go = saved_len;
	ptr = (char *)saved_core_loc;
	while( bytes_to_go ) {
		read_size = bytes_to_go > 4096 ? 4096 : bytes_to_go;
#if defined(Solaris) || defined(IRIX53) || defined(LINUX)
		nbytes =  SYSCALL(SYS_read, fd, (void *)ptr, read_size );
#else
		nbytes =  syscall( SYS_read, fd, (void *)ptr, read_size );
#endif
		if( nbytes < 0 ) {
			fprintf(stderr, "in Segmap::Read(): fd = %d, read_size=%d\n", fd,
				read_size);
			fprintf(stderr, "Error=%d, core_loc=%x\n", errno, core_loc);
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
	dprintf( D_ALWAYS, "write(fd=%d,core_loc=0x%lx,len=0x%lx)\n",
			fd, core_loc, len );
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
	int		scm, p_scm;
	int		do_full_restart = 1; // set to 0 for periodic checkpoint

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
#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
	int		wait_up = 1;
	while( wait_up )
		;
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

				p_scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
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
				SetSyscalls( p_scm );
				
			}
			do_full_restart = 0;
			dprintf(D_ALWAYS, "Periodic Ckpt complete, doing a virtual restart...\n");
			LONGJMP( Env, 1);
		}
	} else {					// Restart
		if ( do_full_restart ) {
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
