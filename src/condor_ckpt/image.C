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

#ifndef SIG_DFL
#include <signal.h>
#endif


static Image MyImage;
static jmp_buf Env;

Header::Header()
{
	Init( -1, -1, "" );
}

Header::Header( int c, int p, const char *o )
{
	Init( c, p, o );
}

void
Header::Init( int c, int p, const char *o )
{
	magic = MAGIC;
	n_segs = 0;
	cluster = c;
	proc = p;
	strcpy( owner, o );
}

void
Header::Display()
{
	DUMP( " ", magic, 0x%X );
	DUMP( " ", n_segs, %d );
	DUMP( " ", cluster, %d );
	DUMP( " ", proc, %d );
	DUMP( " ", owner, "%s" );
}

SegMap::SegMap()
{
	Init( "", 0, 0L );
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
	struct sigaction action;

		// Initialize saved image data structures
	head.Init( -1, -1, "" );
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

	fd = -1;
	pos = 0;
}

int Cluster = 13;
int Proc = 13;
char Owner[] = "mike";

void
Image::Save()
{
	Save( Cluster, Proc, Owner );
}

extern char *etext;
extern char *edata;
void
Image::Save( int c, int p, const char *o )
{
	RAW_ADDR	data_start, data_end;
	RAW_ADDR	stack_start, stack_end;
	ssize_t		pos;
	int			i;

		// Set up header
	head.Init( c, p, o );

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
Image::Write( const char *file_name )
{
	int	fd;
	int	status;

	if( (fd=open(file_name,O_CREAT|O_TRUNC|O_WRONLY,0664)) < 0 ) {
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
Image::Read( const char *file_name )
{
	int	status;

	if( (fd=open(file_name,O_RDONLY,0)) < 0 ) {
		perror( "open" );
		exit( 1 );
	}
	status = Read( fd );

	return status;
}

int
Image::Read( int fd )
{
	int		i;
	ssize_t	nbytes;

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

	if( pos != file_loc ) {
		fprintf( stderr, "Checkpoint sequence error\n" );
		exit( 1 );
	}
	if( strcmp(name,"DATA") == 0 ) {
		brk( (char *)(core_loc + len) );
	}
	nbytes =  read(fd,(void *)core_loc,(size_t)len);
	if( nbytes < 0 ) {
		return -1;
	}
	return pos + nbytes;
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

	printf( "Got SIGTSTP\n" );
	if( SETJMP(Env) == 0 ) {
		printf( "About to save MyImage\n" );
		SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
		SaveFileState();
		MyImage.Save();
		MyImage.Write( "Ckpt.13.13" );
		printf( "Ckpt exit\n" );
		exit( 0 );
	} else {
		patch_registers( scp );
		MyImage.Close();
		RestoreFileState();
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
		syscall( SYS_write, 1, "About to return to user code\n", 29 );
		return;
	}
}

}	// end of extern "C"

void
restart()
{
	SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	MyImage.Read( "Ckpt.13.13" );
	MyImage.Restore();
}

void
ckpt()
{
	kill( getpid(), SIGTSTP );
}
