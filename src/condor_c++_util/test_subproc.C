#include <stdio.h>
#include "subproc.h"


main( int argc, char *argv[] )
{
	SubProc	p( "/usr/ucb/cat", "/etc/motd", "r" );
	SubProc	q( "/usr/ucb/dd", "of=/tmp/mike ibs=1 obs=1", "w" );
	FILE	*fp;
	char	buf[ 1024 ];
	int		i;

	p.display();
	fp = p.run();
	p.display();

	while( fgets(buf,sizeof(buf),fp) ) {
		printf( "Got: %s", buf );
	}

	p.terminate();
	fp = NULL;
	p.display();

	fp = q.run();
	q.display();
	for( i=0; i<5; i++ ) {
		fprintf( fp, "yes\n" );
	}
	fflush( fp );
	q.terminate();

	fp = execute_program( "/usr/ucb/cat", "/etc/motd", "r" );
	while( fgets(buf,sizeof(buf),fp) ) {
		printf( "Got: %s", buf );
	}
	terminate_program();

	fp = execute_program( "/usr/ucb/dd", "of=/tmp/mike.1 ibs=1 obs=1", "w" );
	for( i=0; i<5; i++ ) {
		fprintf( fp, "yes\n" );
	}
	terminate_program();
}
