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
#include "condor_version.h"
#include "condor_mmap.h"

#if defined(Solaris)
#include <netconfig.h>		// for setnetconfig()
#endif

#include "condor_syscalls.h"
#include "condor_sys.h"
#include "image.h"
#include "file_table_interf.h"
#include "condor_debug.h"
#include "condor_ckpt_mode.h"
#include "signals_control.h"
#include "subsystem_info.h"
#include "gtodc.h"

extern int _condor_in_file_stream;

const int KILO = 1024;

extern "C" void mydprintf(int foo, const char *fmt, ...);

extern "C" void report_image_size( int );
extern "C"	int	SYSCALL(int ...);

#ifdef SAVE_SIGSTATE
extern "C" void _condor_save_sigstates();
extern "C" void _condor_restore_sigstates();
#endif

#if defined(COMPRESS_CKPT)
#include "zlib.h"
extern "C" {
	int condor_malloc_init_size();
	void condor_malloc_init(void *start);
	char *condor_malloc(size_t);
	void condor_free(void *);
	void *condor_morecore(int);
	static /*struct*/ z_stream *zstr = NULL;
	unsigned char *zbuf = Z_NULL;
	const int zbufsize = 64*1024;
}
#endif

#if defined(OSF1)
	extern "C" unsigned int htonl( unsigned int );
	extern "C" unsigned int ntohl( unsigned int );
#elif defined(HPUX)
#	include <netinet/in.h>
#elif defined(Solaris)
	#define htonl(x)		(x)
	#define ntohl(x)		(x)
#elif defined(LINUX)
	#include <netinet/in.h>
#else
	extern "C" unsigned long htonl( unsigned long );
	extern "C" unsigned long ntohl( unsigned long );
#endif

#if defined(Solaris)
/* for undocumented calls in GETPAGESIZE() */
#include <sys/sysconfig.h>
#endif

void terminate_with_sig( int sig );
static void find_stack_location( RAW_ADDR &start, RAW_ADDR &end );
static int SP_in_data_area();
static void calc_stack_to_save();
extern "C" void _install_signal_handler( int sig, SIG_HANDLER handler );
extern "C" int open_ckpt_file( const char *name, int flags, size_t n_bytes );
extern "C" int get_ckpt_mode( int sig );
extern "C" int get_ckpt_speed( );

Image MyImage;
static jmp_buf Env;
static RAW_ADDR SavedStackLoc;
volatile int InRestart = TRUE;
volatile int check_sig;		// the signal which activated the checkpoint; used
							// by some routines to determine if this is a periodic
							// checkpoint (USR2) or a check & vacate (TSTP).
static size_t StackSaveSize;
unsigned int _condor_numrestarts = 0;
int condor_compress_ckpt = 1; // compression off(0) or on(1)
int condor_slow_ckpt = 0;

/* these are the remote system calls we use in this file */
extern "C" int REMOTE_CONDOR_send_rusage(struct rusage *use_p);

// set SubSystem for Condor components which expect it
DECL_SUBSYSTEM("JOB", SUBSYSTEM_TYPE_TOOL );

static int
net_read(int fd, void *buf, int size)
{
	int		bytes_read;
	int		this_read;

	bytes_read = 0;
	do {
		this_read = read(fd, buf, size - bytes_read);
		if (this_read <= 0) {
			return -1;
		}
		bytes_read += this_read;
		buf = (void *) ( (char *) buf + this_read );
	} while (bytes_read < size);
	return bytes_read;
}

#if defined(COMPRESS_CKPT)
void *condor_map_seg(void *base, size_t size)
{
	int zfd;
	if ((zfd = SYSCALL(SYS_open, "/dev/zero", O_RDWR)) == -1) {
		dprintf( D_ALWAYS,
				 "Unable to open /dev/zero in read/write mode.\n");
		dprintf( D_ALWAYS, "open: %s\n", strerror(errno));
		Suicide();
	}

	int flags;
	if (base == NULL) {
		flags = MAP_PRIVATE;
	} else {
		flags = MAP_PRIVATE|MAP_FIXED;
	}
	base = MMAP((MMAP_T)base, (size_t)size, PROT_READ|PROT_WRITE,
				flags, zfd, (off_t)0);
	if (base == MAP_FAILED) {
		dprintf(D_ALWAYS, "mmap: %s", strerror(errno));
		Suicide();
	}
	return base;
}

// TODO: deallocate segments on negative incr
void *condor_morecore(int incr)
{
	// begin points to the start of our heap segment
	// corestart points to the start of the allocated portion of the segment
	// *coreend points to the end of the allocated portion of the segment
	// *segend points to the end of our allocated segment
	// coreend and segend are stored at the start of the segment because
	//   we don't want them to be overwritten on a restart
	static void *begin = NULL, *corestart = NULL,
		**coreend = NULL, **segend = NULL;
	static int pagesize = -1;

	if (pagesize == -1) {
		pagesize = getpagesize();
	}
	
	if (begin == NULL) {
		begin = MyImage.FindAltHeap();
		int malloc_static_data = condor_malloc_init_size();
		int segincr =
			(((incr+malloc_static_data+
			   (2*sizeof(void *)))/pagesize)+1)*pagesize;
		begin = condor_map_seg(begin, segincr);
		corestart = (void *) (
			(int)begin+(int)(2*sizeof(void *))+(int)malloc_static_data ); 
		condor_malloc_init((void *)((int)begin+(int)(2*sizeof(void *))));
		coreend = (void **)begin;
		segend = (void **)((int)begin+(int)sizeof(void *));
		*segend = (void *)((int)begin+(int)segincr);
		*coreend = (void *)((int)corestart+(int)incr);
		return corestart;
	} else if (incr == 0) {
		return *coreend;
	} else {
		void *old_break = *coreend;
		*coreend = (void *)((int)*coreend + (int)incr);
		if (*coreend > *segend) {
			int segincr = (int)((((int)*coreend-(int)*segend)/(int)pagesize)+1)*(int)pagesize;
			if ((int)*coreend+(int)segincr-(int)begin > ALT_HEAP_SIZE) {
				dprintf(D_ALWAYS,
						"fatal error: exceeded ALT_HEAP_SIZE of %d bytes!\n",
						ALT_HEAP_SIZE);
				Suicide();
			}
			if (condor_map_seg(*segend, segincr) != *segend) {
				dprintf(D_ALWAYS, "failed to allocate contiguous segments in "
						"condor_morecore!\n");
				Suicide();
			}
			*segend = (void *)((int)*segend + (int)segincr);
		}
		return old_break;
	}
}

void *
zalloc(voidpf opaque, uInt items, uInt size)
{
	return condor_malloc(items*size);
}

void
zfree(voidpf opaque, voidpf address)
{
	condor_free(address);
}
#endif

void
Header::Init()
{
	if (condor_compress_ckpt)
		magic = COMPRESS_MAGIC;
	else
		magic = MAGIC;
	n_segs = 0;
	alt_heap = 0;
}

void
Header::ResetMagic()
{
	if (condor_compress_ckpt)
		magic = COMPRESS_MAGIC;
	else
		magic = MAGIC;
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
SegMap::MSync()
{
	/* We use msync() to commit our dirty pages to swap space periodically.
	   This reduces the number of dirty pages that need to be swapped out
	   if we are suspended, reducing the cost of a suspend operation.
	   We would like to use MS_ASYNC, like this:
		 if (msync((char *)core_loc, len, MS_ASYNC) < 0) {
		   dprintf( D_ALWAYS, "msync(%x, %d) failed with errno = %d\n",
		   core_loc, len, errno );
		 }
	   Unfortunately, this seems to overwhelm some systems with write
	   requests (possibly because MS_ASYNC is rarely used, so kernel
	   developers don't work to get it right), so instead we use MS_SYNC
	   below to write pagesize chunks synchronously.  This may be less
	   efficient, but it is better than hanging the machine.
	*/
	int pagesize = getpagesize();
	for (int i = 0; i < len; i += pagesize) {
		if (msync((char *)core_loc+i, pagesize, MS_SYNC) < 0) {
			dprintf( D_ALWAYS, "msync(%lx, %d) failed with errno = %d\n",
					 (unsigned long)core_loc+i, pagesize, errno );
		}
	}
}

void
SegMap::Display()
{
	DUMP( " ", name, %s );

	printf(	" file_loc = %lld (0x%llX)\n",
			(long long)file_loc,
			(long long)file_loc );
	printf(	" core_loc = %lu (0x%lX)\n",
			(unsigned long)core_loc,
			(unsigned long)core_loc );

	printf( " len = %ld (0x%lX)\n", len, len );
}


void
Image::SetFd( int f )
{
	fd = f;
	pos = 0;
}

void
Image::SetFileName( const char *ckpt_name )
{
	static bool done_once = false;
	if(done_once && file_name) 
		delete[] file_name;

	// Save the checkpoint file name
	file_name = new char [ strlen(ckpt_name) + 1 ];
	if(file_name == NULL) {
		dprintf(D_ALWAYS,
				"Internal error: Could not allocate memory for ckpt_filename\n");
		Suicide();
	}
	strcpy( file_name, ckpt_name );

	fd = -1;
	pos = 0;
	done_once = true;
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

#if defined(COMPRESS_CKPT)
/* 
** Return the start of the alternate heap, used internally by the
** checkpointing library.  The alternate heap is always located in
** the free area above the heap, leaving ALT_HEAP_SIZE of space between
** the start of the alt heap and the end of the free area.  It is
** essential that this heap be located in the same place after a
** restart, so our static pointer to the alt heap is not trashed.
** We never save the alt heap at checkpoint time.
*/
void *
Image::FindAltHeap()
{
	if (head.AltHeap() > 0) return (void *)head.AltHeap();

	void *datatop = 0, *freetop = (void *)-1;

	for (int i=0; i < head.N_Segs(); i++) {
		if (strcmp(map[i].GetName(), "DATA") == MATCH) {
			void *ptr = (void *)((int)map[i].GetLoc() + (int)map[i].GetLen());
			if (ptr > datatop) {
				datatop = ptr;
			}
		}
	}

	for (int i=0; i < head.N_Segs(); i++) {
		if (strcmp(map[i].GetName(), "DATA") != MATCH) {
			void *ptr = (void *) map[i].GetLoc();
			if (ptr > datatop && ptr < freetop) {
				freetop = ptr;
			}
		}
	}

	// if we didn't find anything above the heap, we don't want to
	// return (void *)-1 - ALT_HEAP_SIZE, because this address might
	// be too big on 64 bit architectures, so we instead return
	// datatop + RESERVED_HEAP - ALT_HEAP_SIZE
	if (freetop == (void *)-1) {
		freetop = (void *)((int)datatop + (int)RESERVED_HEAP);
	}

	// make sure that we begin on a page boundary
	int pagesize = getpagesize();
	freetop = (void *)(((RAW_ADDR)freetop/pagesize)*pagesize);
	head.AltHeap((RAW_ADDR)freetop - ALT_HEAP_SIZE);
	return (void *)head.AltHeap();
}
#endif


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

	_condor_file_table_init();

		// Install initial signal handlers
	_install_signal_handler( SIGTSTP, (SIG_HANDLER)Checkpoint );
	_install_signal_handler( SIGUSR2, (SIG_HANDLER)Checkpoint );

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
	float x=23, y=14, z=256;
	if ((x+y)>z) {
		dprintf(D_ALWAYS,
				"Internal error: Solaris floating point test failed\n");
		Suicide();
	}
	z=x*y;
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
		dprintf(D_ALWAYS, "can't install sighandler for sig %d: %s\n",
				sig, strerror(errno));
		Suicide();
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
#if !defined(HAS_DYNAMIC_USER_JOBS)
	RAW_ADDR	stack_start, stack_end;
#else
	RAW_ADDR	addr_start, addr_end;
	int             numsegs, prot, rtn, stackseg=-1;
#endif
	RAW_ADDR	data_start, data_end;
	ssize_t		position;
	int			i;

#if defined(COMPRESS_CKPT)
	RAW_ADDR	alt_heap = head.AltHeap();
#endif

	head.Init();

#if defined(COMPRESS_CKPT)
	head.AltHeap(alt_heap);
#endif

#if !defined(HAS_DYNAMIC_USER_JOBS)

		// Set up data segment
	data_start = data_start_addr();
	data_end = data_end_addr();
	dprintf(D_CKPT, "Adding a DATA segment: start[0x%x], end [0x%x]\n",
		(unsigned long)data_start, (unsigned long)data_end);
	AddSegment( "DATA", data_start, data_end, 0 );

		// Set up stack segment
	find_stack_location( stack_start, stack_end );
	dprintf(D_CKPT, "Adding a STACK segment: start[0x%x], end [0x%x]\n",
		(unsigned long)stack_start, (unsigned long)stack_end);
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

	
	dprintf(D_CKPT, "About to ask the OS for segments...\n");
	numsegs = num_segments();
	dprintf(D_CKPT, "I should have %d segments...\n", numsegs);
/*	display_prmap();*/

#if !defined(Solaris)

	// data segment is saved and restored as before, using sbrk()
	data_start = data_start_addr();
	data_end = data_end_addr();
/*	printf( "Data start = 0x%x, data end = 0x%x\n",*/
/*			data_start, data_end );*/
	AddSegment( "DATA", data_start, data_end, 0 );

#else

	// sbrk() doesn't give reliable values on Solaris
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
	dprintf(D_CKPT, "I just added the data segment\n");

#endif	

	for( i=0; i<numsegs; i++ ) {
		rtn = segment_bounds(i, addr_start, addr_end, prot);
		switch (rtn) {
		case -1:
			dprintf( D_ALWAYS, "Internal error, segment_bounds returned -1\n");
			Suicide();
			break;
		case 0:
			dprintf(D_CKPT, "Adding SHARED LIB\n");
			if (addr_start != head.AltHeap()) {	// don't ckpt alt heap
				AddSegment( "SHARED LIB", addr_start, addr_end, prot);
				dprintf(D_CKPT, "\tlen:[0x%x]\n", 
					addr_end - addr_start);
			}
			break;
		case 1:
			dprintf(D_CKPT, "Skipping Text Segment\n");
			break;		// don't checkpoint text segment
		case 2:
			dprintf(D_CKPT, "Adding Stack Segment\n");
			stackseg = i;	// don't add STACK segment until the end
			break;
		case 3:
			dprintf(D_CKPT, "Don't add DATA segment again\n");
			break;		// don't add DATA segment again
		default:
			dprintf( D_ALWAYS, "Internal error, segment_bounds"
					 "returned unrecognized value\n");
			Suicide();
		}
	}	

	if(stackseg==-1) {
		dprintf(D_ALWAYS,"Image::Save: Never found stackseg!\n");
		Suicide();
	}

	// now add stack segment
	rtn = segment_bounds(stackseg, addr_start, addr_end, prot);
	AddSegment( "STACK", addr_start, addr_end, prot);
	dprintf( D_CKPT, "stack start = 0x%x, stack end = 0x%x\n",
			addr_start, addr_end);

#endif

		// Calculate positions of segments in ckpt file
	position = sizeof(Header) + head.N_Segs() * sizeof(SegMap);
	for( i=0; i<head.N_Segs(); i++ ) {
		position = map[i].SetPos( position );
		dprintf( D_CKPT,"Pos: %d\n",position);
	}

	if( position < 0 ) {
		dprintf( D_ALWAYS, "Internal error, ckpt size calculated is %d\n",
													position );
		Suicide();
	}

	dprintf( D_ALWAYS, "Size of ckpt image = %d bytes\n", position );
	len = position;

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
	RAW_ADDR	length = end - start;
	int idx = head.N_Segs();

	if( idx >= MAX_SEGS ) {
		dprintf( D_ALWAYS, "Don't know how to grow segment map yet!\n" );
		Suicide();
	}

	dprintf(D_CKPT, 
		"Image::AddSegment: name=[%s], start=[%p], end=[%p], length=[0x%x], "
		"prot=[0x%x]\n", 
		name, (void*)start, (void*)end, (unsigned long)length, prot);

	head.IncrSegs();
	map[idx].Init( name, start, length, prot );
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


#define USER_DATA_SIZE 256
#if defined(PVM_CHECKPOINTING)
extern "C" int user_restore_pre(char *, int);
extern "C" int user_restore_post(char *, int);
char	global_user_data[USER_DATA_SIZE];
#endif

/*
  Given an "image" object containing checkpoint information which we have
  just read in, this method actually effects the restart.
*/
void
Image::Restore()
{
	int		save_fd = fd;
	char	user_data[USER_DATA_SIZE];


#if defined(PVM_CHECKPOINTING)
	user_restore_pre(user_data, sizeof(user_data));
#endif

#if defined(COMPRESS_CKPT)
	// If the checkpoint header contains an alt heap pointer, then we must
	// create an alt heap, because the alt heap pointer is going to be
	// restored, and it has to point to something.
	if (head.AltHeap() > 0) {
		condor_morecore(0);
	}
	if (head.Magic() == COMPRESS_MAGIC) {
		if (head.AltHeap() == 0) {
			dprintf( D_ALWAYS, "Checkpoint is compressed but no alt heap is "
					 "specified!  Aborting restart.\n" );
			fprintf( stderr, "CONDOR ERROR: Checkpoint header is malformed. "
					 "Compression is specified without a corresponding alt "
					 "heap.\n" );
			exit( 1 );
		}
		dprintf( D_ALWAYS, "Reading compressed segments...\n" );
		zstr = (z_stream *) condor_malloc(sizeof(z_stream));
		if (!zstr) {
			dprintf( D_ALWAYS, "out of memory in condor_malloc!\n");
			Suicide();
		}
		zbuf = (unsigned char *)condor_malloc(zbufsize);
		if (!zbuf) {
			dprintf( D_ALWAYS, "out of memory in condor_malloc!\n");
			Suicide();
		}
		zstr->zalloc = zalloc;
		zstr->zfree = zfree;
		zstr->opaque = Z_NULL;
		if (inflateInit(zstr) != Z_OK) {
			dprintf( D_ALWAYS, "zlib (inflateInit): %s\n", zstr->msg );
			Suicide();
		}
	}
#endif

		// Overwrite our data segment with the one saved at checkpoint
		// time *and* restore any saved shared libraries.

	RestoreAllSegsExceptStack();

		// We have just overwritten our data segment, so the image
		// we are working with has been overwritten too.... restore into the
		// data segment the value of the fd from this stack segment.
	fd = save_fd;

		// We want the gettimeofday() cache to be reset up in terms of the
		// submission machine time, so reinitialize the cache to default
		// and let it lazily refill when gettimeofday() is called again.
		// Performing this call effectively "forgets" about the checkpointed
		// information in the cache after resumption. It will be resetup at
		// the next gettimeofday() call.
	_condor_gtodc_init(_condor_global_gtodc);

#if defined(PVM_CHECKPOINTING)
	memcpy(global_user_data, user_data, sizeof(user_data));
#endif

		// Now we're going to restore the stack, so we move our execution
		// stack to a temporary area (in the data segment), then call
		// the RestoreStack() routine.
	dprintf(D_CKPT, "About to execute on TmpStk\n");
	ExecuteOnTmpStk( RestoreStack );

		// RestoreStack() also does the jump back to user code
	dprintf( D_ALWAYS, "Error: reached code past the restore point!\n" );
	Suicide();
}

#if defined(COMPRESS_CKPT) && defined(HAS_DYNAMIC_USER_JOBS)
/* zlib uses memcpy, but we can't assume that libc.so is in a good state,
   so we provide our own version... - Jim B. */
extern "C" {
void *memcpy(void *s1, const void *s2, size_t n)
{
	char *cs1 = NULL;
	char *cs2 = NULL;
	void *r = s1;

	cs1 = (char*)s1;
	cs2 = (char*)s2;

	while (n > 0) {
		*cs1++ = *cs2++;
		n--;
	}
	return r;
}
void *_memcpy(void *s1, const void *s2, size_t n)
{
	return memcpy(s1, s2, n);
}
}
#endif

/* don't assume we have libc.so in a good state right now... - Jim B. */
static int mystrcmp(const char *str1, const char *str2)
{
	while (*str1 != '\0' && *str2 != '\0' && *str1 == *str2) {
		str1++;
		str2++;
	}
	return (int) *str1 - *str2;
}

#if defined(HAS_DYNAMIC_USER_JOBS)
extern "C"  int GETPAGESIZE(void);

static int GETPAGESIZE(void)
{
#if defined(SYS_getpagesize)
	return SYSCALL(SYS_getpagesize);
#elif defined(SYS_sysconfig)
	return SYSCALL(SYS_sysconfig, _CONFIG_PAGESIZE);
#else
#error "Please port me.  I need a safe method of getting the pagesize."
#endif
}
#endif

void
Image::RestoreSeg( const char *seg_name )
{
	int		i;

	for( i=0; i<head.N_Segs(); i++ ) {
		if( mystrcmp(seg_name,map[i].GetName()) == 0 ) {
			if( (pos = map[i].Read(fd,pos)) < 0 ) {
				dprintf(D_ALWAYS, "SegMap::Read() failed!\n");
				Suicide();
			} else {
				return;
			}
		}
	}
	dprintf( D_ALWAYS, "Can't find segment \"%s\"\n", seg_name );
	fprintf( stderr, "CONDOR ERROR: can't find segment \"%s\" on restart\n",
			 seg_name );
	exit( 1 );
}

void Image::RestoreAllSegsExceptStack()
{
	int		i;
	int		save_fd = fd;

#if defined(Solaris)
	dprintf( D_ALWAYS, "Current segmap dump follows\n");
	display_prmap();
#endif
	for( i=0; i<head.N_Segs(); i++ ) {
		if( mystrcmp("STACK",map[i].GetName()) != 0 ) {
			if( (pos = map[i].Read(fd,pos)) < 0 ) {
				dprintf(D_ALWAYS, "SegMap::Read() failed!\n" );
				Suicide();
			}
		}
		else if (i<head.N_Segs()-1) {
			dprintf( D_ALWAYS, "Checkpoint file error: STACK is not the "
					"last segment in ckpt file.\n");
			fprintf( stderr, "CONDOR ERROR: STACK is not the last segment "
					 " in ckpt file.\n" );
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

	dprintf(D_CKPT, "RestoreStack() Entrance!\n");
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

#if defined(COMPRESS_CKPT)
	if (zstr) {
		int rval = inflateEnd(zstr);
		if (rval != Z_OK) {
			dprintf( D_ALWAYS, "zlib (inflateEnd): %d\n", rval );
			Suicide();
		}
		condor_free(zstr);
		zstr = NULL;
		condor_free(zbuf);
		zbuf = Z_NULL;
	}
#endif

	dprintf(D_CKPT, "RestoreStack() Exit!\n");
	
	LONGJMP( Env, 1 );
}

int
Image::Write()
{
	dprintf( D_ALWAYS, "Image::Write(): fd %d file_name %s\n",
			 fd, file_name?file_name:"(NULL)");
	if (fd == -1) {
		return Write( file_name );
	} else {
		return Write( fd );
	}
}

void
Image::MSync()
{
	for (int i=0; i < head.N_Segs(); i++) {
		map[i].MSync();
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
	int	file_d;
	int	scm;
	char	tmp_name[ _POSIX_PATH_MAX ];

	if( ckpt_file == 0 ) {
		ckpt_file = file_name;
	}

	head.ResetMagic();		// set magic according to compression mode

		// Generate tmp file name
	dprintf( D_ALWAYS, "Checkpoint name is \"%s\"\n", ckpt_file );

		// Open the tmp file
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	tmp_name[0] = '\0';
	/* For now we comment out the open_url() call because we are currently
	 * inside of a signal handler.  POSIX says malloc() is not safe to call
	 * in a signal handler; open_url calls malloc() and this messes up
	 * checkpointing on SGI IRIX6 bigtime!!  so it is commented out for
	 * now until we get rid of all mallocs in open_url() -Todd T, 2/97

	 * In ripping out the IRIX code, this comment was discovered and now
	 * needs further exploration as to what it means to have commented out
	 * this open_url() call. -psilord 2/08.
	 
	 */
	// if( (file_d=open_url(ckpt_file,O_WRONLY|O_TRUNC|O_CREAT)) < 0 ) {
		sprintf( tmp_name, "%s.tmp", ckpt_file );
		dprintf( D_ALWAYS, "Tmp name is \"%s\"\n", tmp_name );
#if defined(COMPRESS_CKPT)
		/* If we are writing a compressed checkpoint, we don't send the file
		   size, since we don't know it yet.  The file transfer code accepts
		   a file size of -1 to mean that the size is unknown. */
		if ((file_d = open_ckpt_file(tmp_name, O_WRONLY|O_TRUNC|O_CREAT,
								 condor_compress_ckpt ? -1 : len)) < 0)  {
#else
		if ((file_d = open_ckpt_file(tmp_name, O_WRONLY|O_TRUNC|O_CREAT,
								 len)) < 0)  {
#endif
			dprintf( D_ALWAYS, "ERROR:open_ckpt_file failed, aborting ckpt\n");
			SetSyscalls(scm);
			return -1;
		}
	// }  // this is the matching brace to the open_url; see comment above

		// Write out the checkpoint
	if( Write(file_d) < 0 ) {
		SetSyscalls(scm);
		return -1;
	}

		// Have to check close() in AFS
	dprintf( D_ALWAYS, "About to close ckpt fd (%d)\n", file_d );
	if( close(file_d) < 0 ) {
		dprintf( D_ALWAYS, "Close failed!\n" );
		SetSyscalls(scm);
		return -1;
	}
	dprintf( D_ALWAYS, "Closed OK\n" );

	SetSyscalls( scm );

		// We now know it's complete, so move it to the real ckpt file name
	if (tmp_name[0] != '\0') {
		dprintf(D_ALWAYS, "About to rename \"%s\" to \"%s\"\n",
				tmp_name, ckpt_file);
		if( rename(tmp_name,ckpt_file) < 0 ) {
			dprintf( D_ALWAYS, "rename failed, aborting ckpt\n" );
			return -1;
		}
		dprintf( D_ALWAYS, "Renamed OK\n" );
	}

		// Report
	dprintf( D_ALWAYS, "USER PROC: CHECKPOINT IMAGE SENT OK\n" );

	return 0;
}

/*
  Write our checkpoint "image" to a given file descriptor.  At this level
  it makes no difference whether the file descriptor points to a local
  file, a remote file, or another process (for direct migration).
*/
int
Image::Write( int file_d )
{
	int		i;
	int		position = 0;
	int		nbytes;
	int		ack;
	int		status;

#if defined(COMPRESS_CKPT)
	if (condor_compress_ckpt) {
		condor_morecore(0);			// initialize alt heap before we write head
	}
#endif

		// Write out the header
	if( (nbytes=write(file_d,&head,sizeof(head))) < 0 ) {
		return -1;
	}
	position += nbytes;
	dprintf( D_ALWAYS, "Wrote headers OK\n" );

		// Write out the SegMaps
	if( (nbytes=write(file_d,map,sizeof(SegMap)*head.N_Segs()))
		!= sizeof(SegMap)*head.N_Segs() ) {
		return -1;
	}
	position += nbytes;
	dprintf( D_ALWAYS, "Wrote all SegMaps OK\n" );

#if defined(COMPRESS_CKPT)
	if (condor_compress_ckpt) {
		dprintf( D_ALWAYS, "Writing compressed segments...\n" );
		zstr = (z_stream *) condor_malloc(sizeof(z_stream));
		if (!zstr) {
			dprintf( D_ALWAYS, "out of memory in condor_malloc!\n");
			Suicide();
		}
		zbuf = (unsigned char *) condor_malloc(zbufsize);
		if (!zbuf) {
			dprintf( D_ALWAYS, "out of memory in condor_malloc!\n");
			Suicide();
		}
		zstr->zalloc = zalloc;
		zstr->zfree = zfree;
		zstr->opaque = Z_NULL;
		if (deflateInit(zstr, Z_DEFAULT_COMPRESSION) != Z_OK) {
			dprintf( D_ALWAYS, "zlib (deflateInit): %s\n", zstr->msg );
			Suicide();
		}
	}
#endif

		// Write out the Segments
	for( i=0; i<head.N_Segs(); i++ ) {
/*		map[i].Display();*/
		if( (nbytes=map[i].Write(file_d,position)) < 0 ) {
			dprintf( D_ALWAYS, "Write() Segment[%d] of type %s -> FAILED\n", i,
				map[i].GetName() );
			dprintf( D_ALWAYS, "errno = %d, nbytes = %d\n", errno, nbytes );
			return -1;
		}
		position += nbytes;
		dprintf( D_ALWAYS, "Wrote Segment[%d] of type %s -> OK\n", i, map[i].GetName() );
	}

#if defined(COMPRESS_CKPT)
	if (condor_compress_ckpt) {
		int bytes_to_go, rval;
		unsigned char *ptr;

		zstr->next_out = zbuf;
		zstr->avail_out = zbufsize;
		if (deflate(zstr, Z_FINISH) != Z_STREAM_END) {
			dprintf( D_ALWAYS, "zlib (deflate): %s\n", zstr->msg );
			Suicide();
		}
		for (bytes_to_go = (zbufsize-zstr->avail_out), ptr = zbuf;
			 bytes_to_go > 0;
			 bytes_to_go -= rval, ptr += rval, zstr->avail_out += rval) {
			rval = write(file_d,ptr,bytes_to_go);
			if (rval < 0) {
				dprintf(D_ALWAYS, "write failed with errno %d in "
						"SegMap::Write\n", errno);
				Suicide();
			}
		}
		if (zstr->avail_out != zbufsize) {
			dprintf(D_ALWAYS, "deflate logic error, avail_out (%d) != "
					"zbufsize (%d)\n", zstr->avail_out, zbufsize);
			Suicide();
		}

		zstr->avail_out = 0;
		zstr->next_out = Z_NULL;
		condor_free(zbuf);
		zbuf = Z_NULL;
	
		rval = deflateEnd(zstr);
		if (rval != Z_OK) {
			dprintf( D_ALWAYS, "zlib (deflateEnd): %d\n", rval );
			Suicide();
		}
		condor_free(zstr);
		zstr = NULL;
	}
#endif

	dprintf( D_ALWAYS, "Wrote all Segments OK\n" );

		/* When using the stream protocol the shadow echo's the number
		   of bytes transferred as a final acknowledgement.  If, however,
		   we wrote a compressed checkpoint, then our peer won't know
		   that we have finished until we close the socket, so we can't
		   expect the acknowledgement. */
#if defined(COMPRESS_CKPT)
	if( _condor_in_file_stream && !condor_compress_ckpt ) {
#else
	if( _condor_in_file_stream ) {
#endif
		status = net_read( file_d, &ack, sizeof(ack) );
		if( status < 0 ) {
			dprintf( D_ALWAYS, "Can't read final ack from the shadow\n" );
			return -1;
		}

		ack = ntohl( ack );	// Ack is in network byte order, fix here
		if( ack != len ) {
			dprintf( D_ALWAYS, "Ack - expected %d, but got %d\n", len, ack );
			return -1;
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
	int		nbytes;

		// Make sure we have a valid file descriptor to read from
	if( fd < 0 && file_name && file_name[0] ) {
//		if( (fd=open_url(file_name,O_RDONLY)) < 0 ) {
			if( (fd=open_ckpt_file(file_name,O_RDONLY,0)) < 0 ) {
				dprintf( D_ALWAYS, "open_ckpt_file failed: %s",
						 strerror(errno));
				return -1;
			}
//		}		// don't use URL library -- Jim B.
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
/*		map[i].Display();*/
		pos += nbytes;
		dprintf( D_ALWAYS, "Read SegMap[%d](%s) OK\n", i, map[i].GetName() );
	}
	dprintf( D_ALWAYS, "Read all SegMaps OK\n" );

	return 0;
}

void
Image::Close()
{
	if( fd < 0 ) {
		dprintf( D_ALWAYS, "Image::Close - file not open!\n" );
	}
	close( fd );
	/* The next checkpoint is going to assume the fd is -1, so set it here */
	fd = -1;
}

ssize_t
SegMap::Read( int fd, ssize_t pos )
{
	int		nbytes;
	char *orig_brk;
	char *cur_brk;
	char	*ptr;
	int		bytes_to_go;
	int		read_size;
	long	saved_len = len;
	RAW_ADDR	saved_core_loc = core_loc;
	
	if( pos != file_loc ) {
		/* I haven't the time to fix up our dprintf implementation in 
			the checkpointing library to handle >32bit quantities. */
		if (pos > UINT_MAX) {
			dprintf( D_ALWAYS, "Checkpoint sequence error at a position "
				"greater than UINT_MAX. Sorry.\n");
		} else {
			dprintf( D_ALWAYS, "Checkpoint sequence error (%d != %u)\n", 
				pos, (unsigned int)file_loc );
		}
		Suicide();
	}

	dprintf(D_CKPT, "Restoring a %s segment\n", name);

	if( mystrcmp(name,"DATA") == 0 ) {
		orig_brk = (char *)sbrk(0);
		if( orig_brk < (char *)(core_loc + len) ) {
			brk( (char *)(core_loc + len) );
		}
		cur_brk = (char *)sbrk(0);
		dprintf(D_ALWAYS, 
			"Found a DATA block, increasing heap from 0x%p to 0x%p\n", 
			orig_brk, cur_brk);
	}

#if defined(HAS_DYNAMIC_USER_JOBS)
	else if ( mystrcmp(name,"SHARED LIB") == 0) {
		//these variables declared here b/c only used in this $if defined
		int segSize = len;
		int zfd;
		char *segLoc;
		
		segLoc = (char *)core_loc;
		if ((zfd = SYSCALL(SYS_open, "/dev/zero", O_RDWR)) == -1) {
			dprintf( D_ALWAYS,
					 "Unable to open /dev/zero in read/write mode.\n");
			dprintf( D_ALWAYS, "open: %s\n", strerror(errno));
			Suicide();
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

		int page_mask = GETPAGESIZE()-1;
		int align_offset = (long)segLoc & page_mask;
		if (align_offset) {
				// Our addr parameter (segLoc) is not aligned.  We
				// must have migrated to a machine with a smaller
				// pagesize than the machine where we checkpointed.
			dprintf(D_ALWAYS, "Unaligned ckpt segment at 0x%x!  "
					"Performing alignment...\n", segLoc);
			segSize += align_offset;
			segLoc -= align_offset;
				// But we can't just blindly align the address,
				// because we don't want to mmap() over previously
				// restored segments.  We also can't make any
				// assumption about the segments being ordered by
				// address in our checkpoint.  So, we take a look at
				// the first and last page using mprotect.  If we can
				// set the permissions on the pages, then they're
				// already mapped in and we shouldn't remap over them.
				// It's not a big deal that we're setting all
				// permissions on these pages because we basically do
				// that anyway in the restart process, since we need
				// the pages to be writeable to restore from
				// checkpoint.
			if (SYSCALL(SYS_mprotect, segLoc, GETPAGESIZE(),
						PROT_READ|PROT_WRITE|PROT_EXEC) == 0) {
				dprintf(D_ALWAYS, "First page already mapped.\n");
				segLoc += GETPAGESIZE();
				segSize -= GETPAGESIZE();
			}
			char *lastPage = segLoc + ((segSize-1) & ~(page_mask));
			if (segSize > 0 &&
				SYSCALL(SYS_mprotect, lastPage, GETPAGESIZE(),
						PROT_READ|PROT_WRITE|PROT_EXEC) == 0) {
				dprintf(D_ALWAYS, "Last page already mapped.\n");
				segSize -= GETPAGESIZE();
			}
		}

		if (segSize > 0) {		// our pages may already be mapped in
			if ((MMAP((MMAP_T)segLoc, (size_t)segSize,
					  prot|PROT_WRITE,
					  MAP_PRIVATE|MAP_FIXED, zfd,
					  (off_t)0)) == MAP_FAILED) {
				dprintf(D_ALWAYS, "mmap: %s", strerror(errno));
				dprintf(D_ALWAYS, "Attempted to mmap /dev/zero at "
						"address 0x%x, size 0x%x\n", segLoc,
						segSize);
				dprintf(D_ALWAYS, "Current segmap dump follows\n");
				display_prmap();
				Suicide();
			}
		} // if (segSize > 0)

		/* WARNING: We have potentially just overwritten libc.so.  Do
		   not make calls that are defined in this (or any other)
		   shared library until we restore all shared libraries from
		   the checkpoint (i.e., use mystrcmp and SYSCALL).  -Jim B. */

		if (SYSCALL(SYS_close, zfd) < 0) {
			dprintf( D_ALWAYS,
					 "Unable to close /dev/zero file descriptor.\n" );
			dprintf( D_ALWAYS, "close: %s\n", strerror(errno));
			Suicide();
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

	dprintf(D_ALWAYS, 
		"About to overwrite %d bytes starting at 0x%p(%s)\n", 
			bytes_to_go, ptr, name);

#if defined(COMPRESS_CKPT)
	if (zstr) {
		/*struct*/ z_stream *saved_zstr = zstr;
		unsigned char *saved_zbuf = zbuf;
		saved_zstr->next_out = (unsigned char *)saved_core_loc;
		saved_zstr->avail_out = saved_len;
		while (saved_zstr->avail_out > 0) {
			// make at least 32 bytes available
			if (saved_zstr->avail_in < 32) {
				if (saved_zstr->avail_in > 0) {
					memcpy(saved_zbuf, saved_zstr->next_in,
						   saved_zstr->avail_in);
				}
				nbytes = SYSCALL(SYS_read, fd,
								 saved_zbuf+saved_zstr->avail_in,
								 zbufsize-saved_zstr->avail_in);
				if (nbytes > 0) {
					saved_zstr->avail_in += nbytes;
				}
				saved_zstr->next_in = saved_zbuf;
			}
			if (inflate(saved_zstr, Z_PARTIAL_FLUSH) != Z_OK &&
				saved_zstr->avail_out > 0) {
				dprintf( D_ALWAYS, "zlib (inflate): %s\n", saved_zstr->msg );
				Suicide();
			}
		}
		zstr = saved_zstr;
		zbuf = saved_zbuf;
		return pos + saved_len;
	}
#endif

	while( bytes_to_go ) {
		read_size = bytes_to_go > 65536 ? 65536 : bytes_to_go;
#if defined(HAS_DYNAMIC_USER_JOBS)
		nbytes =  SYSCALL(SYS_read, fd, (void *)ptr, read_size );
#else
		nbytes =  syscall( SYS_read, fd, (void *)ptr, read_size );
#endif
		if( nbytes <= 0 ) {
			dprintf(D_ALWAYS, "in Segmap::Read(): fd = %d, read_size=%d\n", fd,
				read_size);
			dprintf(D_ALWAYS, "core_loc=%lx, nbytes=%d, errno=%d(%s)\n",
				(unsigned long)core_loc, nbytes, errno, strerror(errno));
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
		/* I haven't the time to fix up our dprintf implementation in
			the checkpointing library to handle >32bit quantities. */
		if (pos > UINT_MAX) {
			dprintf( D_ALWAYS, "Checkpoint sequence error at a position "
				"greater than UINT_MAX. Sorry.\n");
		} else {
			dprintf( D_ALWAYS, "Checkpoint sequence error (%d != %u)\n", 
				pos, (unsigned int)file_loc );
		}
		Suicide();
	}
#if defined(COMPRESS_CKPT)
	if (condor_compress_ckpt) {
		int bytes_to_go, rval, old_avail_in;
		unsigned char *ptr;

		zstr->next_in = (unsigned char *)core_loc;
		zstr->avail_in = len;
		while (zstr->avail_in > 0) {
			zstr->next_out = zbuf;
			zstr->avail_out = zbufsize;
			old_avail_in = zstr->avail_in;
			if (deflate(zstr, Z_PARTIAL_FLUSH) != Z_OK) {
				dprintf( D_ALWAYS, "zlib (deflate): %s\n", zstr->msg );
				Suicide();
			}
			for (bytes_to_go = (zbufsize-zstr->avail_out), ptr = zbuf;
				 bytes_to_go > 0;
				 bytes_to_go -= rval, ptr += rval, zstr->avail_out += rval) {
				// note: bytes_to_go <= 65536 (bufsize)
				rval = write(fd,ptr,bytes_to_go);
				if (rval < 0) {
					dprintf(D_ALWAYS, "write failed with errno %d in "
							"SegMap::Write\n", errno);
					Suicide();
				}
			}
			if (condor_slow_ckpt) {
				// write condor_slow_ckpt KB per second
				sleep(((old_avail_in-zstr->avail_in)/
					   (condor_slow_ckpt*KILO))+1);
			}
			if (zstr->avail_out != zbufsize) {
				dprintf(D_ALWAYS, "deflate logic error, avail_out (%d) != "
						"zbufsize (%d)\n", zstr->avail_out, zbufsize);
				Suicide();
			}
		}
		return len;
	}
#endif
	dprintf( D_ALWAYS, "write(fd=%d,core_loc=0x%x,len=0x%x)\n",
			fd, core_loc, len );

	int bytes_to_go = len, nbytes;

	char *ptr = (char *)core_loc;
	while (bytes_to_go) {
		size_t write_size;
		if (condor_slow_ckpt && bytes_to_go > (condor_slow_ckpt*KILO)) {
			// write condor_slow_ckpt KB per second
			write_size = condor_slow_ckpt*KILO;
		} else {
			write_size = bytes_to_go;
		}
		nbytes = write(fd,(void *)ptr,write_size);
		dprintf(D_CKPT, "I wrote %d bytes with write...\n", nbytes);
		if ( nbytes < 0 ) {
			dprintf( D_ALWAYS, "in SegMap::Write(): fd = %d, write_size=%d\n",
					 fd, bytes_to_go );
			dprintf( D_ALWAYS, "errno=%d, core_loc=%p\n", errno, ptr );
			return -1;
		}
		if (condor_slow_ckpt) sleep(1);
		bytes_to_go -= nbytes;
		ptr += nbytes;
	}
	return len;
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
	int		write_result;
	sigset_t	mask;

		// Block ckpt signals while we are ckpting
	mask = _condor_signals_disable();

		// If checkpointing is temporarily disabled, remember that and return
	if( _condor_ckpt_is_disabled() )  {
		_condor_ckpt_defer(sig);
		_condor_signals_enable(mask);
		return;
	}

	if( InRestart ) {
		if ( sig == SIGTSTP ) {
			Suicide();		// if we're supposed to vacate, kill ourselves
		} else {
			_condor_signals_enable(mask);
			return;			// if periodic ckpt or we're currently ckpting
		}
	} else {
		InRestart = TRUE;
	}

	check_sig = sig;

	if( MyImage.GetMode() == REMOTE ) {
		scm = SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
#if !defined(PVM_CHECKPOINTING)
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
#endif

		/* now update shadow with CPU time info.  unfortunately, we need
		 * to convert to struct rusage here in the user code, because
		 * clock_tick is platform dependent and we don't want CPU times
		 * messed up if the shadow is running on a different architecture */
		/* We do this before we write the checkpoint now, so the shadow
		 * can send usage info to the checkpoint server with the checkpoint.
		 * The usage is committed with the checkpoint. -Jim B. */
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
		(void)REMOTE_CONDOR_send_rusage( &bsd_usage );
		SetSyscalls( p_scm );

	} else {
		scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	}

	if ( sig == SIGTSTP ) {
		dprintf( D_ALWAYS, "Got SIGTSTP\n" );
	} else {
		dprintf( D_ALWAYS, "Got SIGUSR2\n" );
	}

#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
	int		wait_up = 1;
	while( wait_up )
		;
#endif

		// These two things must be done BEFORE saving of the process image.
		_condor_save_sigstates();
		dprintf( D_ALWAYS, "Saved signal state.\n");

		dprintf( D_ALWAYS, "About to save file state\n");
		_condor_file_table_checkpoint();
		dprintf( D_ALWAYS, "Done saving file state\n");

	if( SETJMP(Env) == 0 ) {	// Checkpoint
			// First, take a snapshot of our memory image to prepare
			// for the checkpoint and accurately report our image size.

			// WARNING!!!! Once we have taken this snapshot, we MUST not do
			// WARNING!!!! any libc memory managment anymore because it could
			// WARNING!!!! screw up the segment boundaries.
			// WARNING!!!! We should't be doing ANY memory management in the
			// WARNING!!!! Checkpoint function call because we are in a signal
			// WARNING!!!! handler anyway.
			// WARNING!!!! We need to inspect the code to see if this
			// WARNING!!!! happens. which I suspect it does... psilord 9/24/01
		dprintf( D_ALWAYS, "About to update MyImage\n" );
		MyImage.Save();

			// If we're in REMOTE mode, report our status and ask the
			// shadow for instructions.  We need to do this before we
			// start checkpointing our file state or memory image,
			// since the shadow may tell us not to checkpoint at all.
			// It may just want a status update.
		if( MyImage.GetMode() == REMOTE ) {
				// Give the shadow our updated image size
			report_image_size( (MyImage.GetLen() + KILO - 1) / KILO );

				// Report some file table statistics here???

				// Get checkpoint parameters from the shadow.
				// At some point, this may also include some instructions
				// for manipulating the file table without doing a full
				// checkpoint.
			int mode = get_ckpt_mode(check_sig);
			if (mode > 0) {
				condor_compress_ckpt = (mode&CKPT_MODE_USE_COMPRESSION)?1:0;
				if (mode&CKPT_MODE_SLOW) {
					condor_slow_ckpt = get_ckpt_speed();
					dprintf(D_ALWAYS, "Checkpointing at %d KB/s.\n",
							condor_slow_ckpt);
				} else {
					condor_slow_ckpt = 0;
				}
				if (mode&CKPT_MODE_MSYNC) {
					dprintf(D_ALWAYS,
							"Performing an msync() on all dirty pages...\n");
					MyImage.MSync();
				}
				if (mode&CKPT_MODE_ABORT) {
					dprintf(D_ALWAYS,
							"Checkpoint aborted by shadow request.\n");

					// if I'm checkpointing on a vacate when WantCheckpoint
					// has been specified to false, then simply commit suicide
					// since I don't want to waste the time of the checkpoint
					// which could be significant.
					if (check_sig == SIGTSTP) {
						dprintf( D_ALWAYS, "Checkpoint during a vacate while "
											"WantCheckpoint = False, aborting "
											"the checkpoint and commiting "
											"Suicide().\n");
						SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
						Suicide();
					}

					// We can't just return here.  We need to cleanup
					// anything we've done above first.

					dprintf( D_ALWAYS, "About to restore file state\n");
					_condor_file_table_resume();
					dprintf( D_ALWAYS, "Done restoring file state\n" );

					dprintf( D_ALWAYS, "About to restore signal state\n" );
					_condor_restore_sigstates();

					SetSyscalls( scm );
					dprintf( D_ALWAYS, "About to return to user code\n" );
					InRestart = FALSE;
					_condor_signals_enable(mask);
					return;
				}
			} else {
				condor_compress_ckpt = condor_slow_ckpt = 0;
			}
		}

		dprintf( D_ALWAYS, "About to write checkpoint\n");
		write_result = MyImage.Write();
		if ( sig == SIGTSTP ) {
			/* we have just checkpointed; now time to vacate */
			dprintf( D_ALWAYS,  "Ckpt exit\n" );
			SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
			if ( write_result == 0 ) {
				terminate_with_sig( SIGUSR2 );
/*				terminate_with_sig( SIGKILL );*/
			} else {
				dprintf(D_ALWAYS, "Write failed with [%d]\n", write_result);
				Suicide();
			}
			/* should never get here */
			dprintf(D_ALWAYS, "You should never see this line in the log!\n");
		} else {
			if ( MyImage.GetMode() == REMOTE ) {

				// first, reset the fd to -1.  this is normally done in
				// a call in remote_startup, but that will not get called
				// before the next periodic checkpoint so we must clear
				// it here.
				MyImage.SetFd( -1 );

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

			int mode = get_ckpt_mode(0);
			if (mode > 0) {
				if (mode&CKPT_MODE_MSYNC) {
					dprintf(D_ALWAYS,
							"Performing an msync() on all dirty pages...\n");
					MyImage.MSync();
				}
				if (mode&CKPT_MODE_ABORT) {
					dprintf(D_ALWAYS, "Restart aborted by shadow request.\n");
					Suicide();
				}
			}
		} else {
			patch_registers( scp );
		}

		dprintf( D_ALWAYS, "About to restore file state\n");
		_condor_file_table_resume();
		dprintf( D_ALWAYS, "Done restoring file state\n" );

		// Here we check if we received checkpoint&exit signal
		// while all this was going on.  
		SetSyscalls(SYS_LOCAL | SYS_UNRECORDED);
		sigset_t pending_sig_set;
		sigpending( &pending_sig_set );
		if ( sigismember(&pending_sig_set,SIGTSTP) ) {
			dprintf(D_ALWAYS,
				"Received a SIGTSTP while checkpointing or restarting.\n");
			if ( do_full_restart ) {
				// Here we were restarting, and while our checkpoint
				// signal was perhaps blocked we were asked to checkpoint
				// and exit.
				// May as well do a suicide.
				Suicide();
			} else {
				// Here we just completed a periodic checkpoint, and
				// while our checkpoint signal was blocked we received
				// a checkpoint and exit.  Since we just finished 
				// writing out a checkpoint, we may as well exit with
				// SIGUSR2 which tells the starter we exited OK after
				// a checkpoint.
				terminate_with_sig(SIGUSR2);
			}
		}
		_condor_numrestarts++;
		SetSyscalls( scm );

		/*
			WARNING: The restoring of signal states must be the last
			thing that happens in a checkpoint restart.  Once this 
			function returns, the user's signal handlers are installed
			and the saved signal state is replaced, so nothing that
			is signal-unsafe can follow this restore.
		*/

		dprintf( D_ALWAYS, "About to restore signal state\n" );
		_condor_restore_sigstates();

		dprintf( D_ALWAYS, "About to return to user code\n" );
		InRestart = FALSE;
		_condor_signals_enable(mask);
		return;
	}
}

void
init_image_with_file_name( const char *ckpt_name )
{
	_condor_ckpt_disable();
	MyImage.SetFileName( ckpt_name );
	_condor_ckpt_enable();
}

void
init_image_with_file_descriptor( int fd )
{
	MyImage.SetFd( fd );
}

/*
  Effect a restart by reading in an "image" containing checkpointing
  information and then overwriting our process with that image.
  All signals are disabled while this is in progress.
*/
void
restart( )
{
	sigset_t mask;

	sigfillset( &mask );
	sigprocmask( SIG_SETMASK, &mask, 0 );

	InRestart = TRUE;
	if (MyImage.Read() < 0) Suicide();
	MyImage.Restore();
}

}	// end of extern "C"



/*
  Checkpointing must by implemented as a signal handler.  This routine
  generates the required signal to invoke the handler.
  ckpt() = periodic ckpt
  ckpt_and_exit() = ckpt and "vacate"
*/
extern "C" {
void
ckpt()
{
	int		scm;

	dprintf( D_ALWAYS, "About to send CHECKPOINT signal to SELF\n" );
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	kill( getpid(), SIGUSR2 );
	SetSyscalls( scm );
}
void
ckpt_and_exit()
{
	int		scm;

	dprintf( D_ALWAYS, "About to send CHECKPOINT and EXIT signal to SELF\n" );
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	kill( getpid(), SIGTSTP );
	SetSyscalls( scm );
}

/*
** Some FORTRAN compilers expect "_" after the symbol name.
*/
void
ckpt_() {
	ckpt();
}
void
ckpt_and_exit_() {
	ckpt_and_exit();
}

/*
** Some FORTRAN compilers expect "__" after the name!
*/

void ckpt__()
{
	ckpt();
}

void ckpt_and_exit__()
{
	ckpt_and_exit();
}


}   /* end of extern "C" */

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

	/* Note: If InRestart, avoid accessing any non-local data, as it
	   may be corrupt.  This includes calling dprintf().  Also avoid
	   calling any libc functions, since libc might be corrupt during
	   a restart. Since sigaction and sigsuspend are defined in
	   signals_support.C, they should be safe to call. -Jim B. */

		// Make sure all system calls handled "straight through"
	if (!InRestart) SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		// Make sure we have the default action in place for the sig
	if( sig != SIGKILL && sig != SIGSTOP ) {
		act.sa_handler = (SIG_HANDLER)SIG_DFL;
		// mask everything so no user-level sig handlers run
		sigfillset( &act.sa_mask );
		act.sa_flags = 0;
		errno = 0;
		if( sigaction(sig,&act,0) < 0 ) {
			if (!InRestart) dprintf(D_ALWAYS, "sigaction: %s\n",
									strerror(errno));
			Suicide();
		}
	}

		// Send ourself the signal
	my_pid = SYSCALL(SYS_getpid);
	if (!InRestart) {
		dprintf( D_ALWAYS, "About to send signal %d to process %d\n",
				sig, my_pid );
	}

	// Sleep for 1 second to allow our debug socket to drain.  This is
	// very important for debugging when Suicide() is called, because if
	// our debug message doesn't arrive at the shadow, we won't know why
	// the job died.  Note that we don't necessarily have access to any
	// libc functions here, so we must use SYSCALL(SYS_something, ...).
#if defined(SYS_sleep)
	SYSCALL(SYS_sleep, 1);
#elif defined(SYS__newselect)
	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;
	SYSCALL(SYS__newselect, 0, NULL, NULL, NULL, &t);
#elif defined(SYS_select)
	struct timeval t;
	t.tv_sec = 1;
	t.tv_usec = 0;
	SYSCALL(SYS_select, 0, NULL, NULL, NULL, &t);
#elif defined(SYS_nanosleep)
	struct timespec t;
	t.tv_sec = 1;
	t.tv_nsec = 0;
	SYSCALL(SYS_nanosleep, &t, NULL);
#else
#error "Please port me!  I need a sleep system call."
#endif

	if( SYSCALL(SYS_kill, my_pid, sig) < 0 ) {
		EXCEPT( "kill" );
	}

		// Wait to die... and mask all sigs except the one to kill us; this
		// way a user's sig won't sneak in on us - Todd 12/94
		// If sig is SIGKILL, we just busy loop since the sig will be SIGKILL
		// if we are called from Suicide(), and Suicide() could be called
		// from inside our restart code before sigfillset(), sigsuspend(), etc,
		// have been mapped to anything (thus we'd exit with a SEGV instead). 
		// -Todd 12/99, "5 yrs later I'm still messing with this f(*ing code!"
	if( sig != SIGKILL && sig != SIGSTOP ) {
		sigfillset( &mask );
		sigdelset( &mask, sig );
		sigsuspend( &mask );
	} else {
		while(1);
	}

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

	// NRL 2002-04-22: In an ideal world, this would be something like:
	//   ptr = getenv( EnvGetName( ENV_STACKSIZE ) );
	// However, this causes all kinds of linkage problems for programs
	// that are condor_compiled, and I just don't want to go there.
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

void mydprintf(int foo, const char *fmt, ...)
{
		va_list args;

		va_start(args, fmt);
		vfprintf(stdout, fmt, args);
		va_end(args);
		fflush(0);

		foo = foo;
}

} /* extern "C" */

