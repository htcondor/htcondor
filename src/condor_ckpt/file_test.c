#define _POSIX_SOURCE

#include <fcntl.h>
#include "stdlib.h"

#if defined(OSF1)
const char	*DICTIONARY = "/usr/lbin/spell/american";
#else
const char	*DICTIONARY = "/usr/dict/words";
#endif

void cat_one_line( int fd );


int
main( int argc, char *argv[] )
{
	int		scm;
	int		i;
	int		fd1, fd2, fd3, fd4, fd5, fd6;
	char	buf[20];

	if( argc > 1 ) {
		restart();
	}

	printf( "At Startup:\n" );
	DumpOpenFds();

	fd1 = open( DICTIONARY, O_RDONLY, 0 );
	if( fd1 < 0 ) {
		perror( "open" );
		exit( 1 );
	}
	printf( "Opened Dictionary as %d\n", fd1 );
	DumpOpenFds();

	if( (fd2 = dup2(fd1,4)) < 0 ) {
		perror( "dup" );
		exit( 1 );
	}
	printf( "Duped %d onto %d\n", fd1, fd2 );
	DumpOpenFds();

	if( close(fd1) < 0 ) {
		perror( "close" );
		exit( 1 );
	}
	printf( "Closed %d\n", fd1 );
	DumpOpenFds();

	printf( "Duping %d onto %d\n", fd2, 12 );
	fd3 = dup2( fd2, 12 );
	if( fd3 < 0 ) {
		perror( "dup" );
		exit( 1 );
	}
	DumpOpenFds();

	printf( "Reading from %d\n", fd3 );
	for( i=0; i<10; i++ ) {
		cat_one_line( fd3 );
		ckpt();
		DumpOpenFds();
	}
	write( 1, "\n", 1 );

	/*
	printf( "Closing %d\n", fd2 );
	if( close(fd2) < 0 ) {
		perror( "close" );
		exit( 1 );
	}
	DumpOpenFds();
	*/

	SaveFileState();
	printf( "Saved File State:\n" );
	DumpOpenFds();

	RestoreFileState();
	DumpOpenFds();

	exit( 0 );
	return 0;
}

void
cat_one_line( int fd )
{
	char	buf[1];

	for(;;) {
		if( read(fd,buf,sizeof(buf)) != sizeof(buf) ) {
			perror( "read" );
			exit( 1 );
		}
		write( 1, buf, sizeof(buf) );
		if( buf[0] == '\n' ) {
			return;
		}
	}
}
