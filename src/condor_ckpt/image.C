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


Image MyImage;
static jmp_buf Env;

static int CkptMode;

Header::Header()
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

SegMap::SegMap()
{
	Init( "XXXXXXXXXXXXX", -1, -1 );
}

SegMap::SegMap( const char *n, RAW_ADDR c, long l )
{
	Init( n, c, l );
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

Image::Image()
{
	Init( "", -1 );
}

Image::Image( int fd )
{
	Init( "", fd );
}

Image::Image( char *ckpt_name )
{
	Init( ckpt_name, -1 );
}

void
Image::SetFd( int f )
{
	fd = f;
}

void
Image::SetFileName( char *ckpt_name )
{
		// Save the checkpoint file name
	file_name = new char [ strlen(ckpt_name) + 1 ];
	strcpy( file_name, ckpt_name );
}

void
Image::Init( char *ckpt_name, int ckpt_fd )
{
	struct sigaction action;

		// Save the checkpoint file name
	file_name = new char [ strlen(ckpt_name) + 1 ];
	strcpy( file_name, ckpt_name );

		// Initialize saved image data structures
	max_segs = SEG_INCR;
	map = new SegMap[max_segs];
	valid = FALSE;

		// set up to catch SIGTSTP and do a checkpoint
	action.sa_handler = (SIG_HANDLER)Checkpoint;
	sigemptyset( &action.sa_mask );
	action.sa_flags = 0;

		// install the SIGTSTP handler
	if( sigaction(SIGTSTP,&action,NULL) < 0 ) {
		perror( "sigaction" );
		exit( 1 );
	}

	fd = ckpt_fd;
	pos = 0;
}



extern char *etext;
extern char *edata;
void
Image::Save()
{
	RAW_ADDR	data_start, data_end;
	RAW_ADDR	stack_start, stack_end;
	ssize_t		pos;
	int			i;

		// Set up data segment
	data_start = data_start_addr();
	data_end = data_end_addr();
	AddSegment( "DATA", data_start, data_end );

		// Set up stack segment
	stack_start = stack_start_addr();
	stack_end = stack_end_addr();
	AddSegment( "STACK", stack_start, stack_end );

		// Calculate positions of segments in ckpt file
	pos = sizeof(Header) + head.N_Segs() * sizeof(SegMap);
	for( i=0; i<head.N_Segs(); i++ ) {
		pos = map[i].SetPos( pos );
	}

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
	DUMP( "", max_segs, %d );
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

	if( idx >= max_segs ) {
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



void
Image::Restore()
{
	int		save_fd = fd;

		// Overwrite our data segment with the one saved at checkpoint
		// time.
	RestoreSeg( "DATA" );

		// We have just overwritten our data segment, so the image
		// we are working with has been overwritten too.  Fortunately,
		// the only thing that has changed is the file descriptor.
	fd = save_fd;

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
	MyImage.RestoreSeg( "STACK" );
	LONGJMP( Env, 1 );
}

int
Image::Write()
{
	return Write( file_name );
}

int
Image::Write( const char *ckpt_file )
{
	int	fd;
	int	status;

	if( ckpt_file == 0 ) {
		ckpt_file == file_name;
	}

	if( (fd=open(ckpt_file,O_CREAT|O_TRUNC|O_WRONLY,0664)) < 0 ) {
		perror( "open" );
		exit( 1 );
	}
	status = Write( fd );
	(void)close( fd );

	return status;
}

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

		// Write out the SegMaps
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=write(fd,&map[i],sizeof(map[i]))) < 0 ) {
			return -1;
		}
		pos += nbytes;
	}

		// Write out the Segments
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=map[i].Write(fd,pos)) < 0 ) {
			return -1;
		}
		pos += nbytes;
	}
	return 0;
}


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

void
Checkpoint( int sig, int code, void *scp )
{

	SetSyscalls( CkptMode );
	dprintf( D_ALWAYS, "Got SIGTSTP\n" );
	if( SETJMP(Env) == 0 ) {
		dprintf( D_ALWAYS, "About to save MyImage\n" );
		SaveFileState();
		MyImage.Save();
		MyImage.Write();
		dprintf( D_ALWAYS,  "Ckpt exit\n" );
		SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
		exit( 0 );
	} else {
		patch_registers( scp );
		SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
		MyImage.Close();
		SetSyscalls( CkptMode );
		RestoreFileState();
		// SetSyscalls( SYS_LOCAL | SYS_MAPPED );
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

void
restart( )
{
	// SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	MyImage.Read();
	MyImage.Restore();
}

}	// end of extern "C"



void
ckpt()
{
	int		scm;

	CkptMode = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	kill( getpid(), SIGTSTP );
}
