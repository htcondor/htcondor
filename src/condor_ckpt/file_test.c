#define _POSIX_SOURCE

#include <fcntl.h>
#include "stdlib.h"
#include "c_proto.h"


int
main( int argc, char *argv[] )
{
	int		scm;
	int		i;
	int		fd1, fd2, fd3, fd4, fd5, fd6;

	for( i=0; i<3; i++ ) {
		if( PreOpen(i) < 0 ) {
			perror( "PreOpen" );
			exit( 1 );
		}
	}

	scm = SetSyscalls( SYS_MAPPED );

	fd1 = MarkFileOpen( 3, "foo", O_RDONLY, FI_RSC );
	fd2 = MarkFileOpen( 4, "bar", O_WRONLY, FI_RSC );
	fd3 = MarkFileOpen( 5, "glarch", O_RDWR, FI_RSC );

	DoDup2( fd2, 12 );
	DoDup2( fd2, 13 );
	DumpOpenFds();
	MarkFileClosed( fd2 );
	DumpOpenFds();
	MarkFileClosed( 12 );
	DumpOpenFds();

	MarkFileClosed( 0 );
	MarkFileClosed( 1 );
	MarkFileClosed( fd1 );
	MarkFileClosed( fd2 );
	MarkFileClosed( fd3 );
	MarkFileClosed( 13 );
	DumpOpenFds();


	/*
	fd4 = MarkFileOpen( 3, "alpha", O_RDONLY, FI_DIRECT );
	fd5 = MarkFileOpen( 4, "bravo", O_WRONLY, FI_DIRECT );
	fd6 = MarkFileOpen( 5, "charlie", O_RDWR, FI_DIRECT );


	MarkFileClosed( fd1 );
	MarkFileClosed( fd2 );
	MarkFileClosed( fd3);

	DumpOpenFds();

	MarkFileClosed( fd4 );
	MarkFileClosed( fd5 );
	MarkFileClosed( fd6);

	DumpOpenFds();
	*/

	exit( 0 );
	return 0;
}
