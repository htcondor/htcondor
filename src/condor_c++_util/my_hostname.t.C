#include "condor_common.h"
#include "my_hostname.h"


void
usage( char* me ) {
	fprintf( stderr, "Usage: %s -ip | -host | -fullhost | -sin\n", me );
	exit( 1 );
}


int
main( int argc, char* argv[] )
{
	if( argc != 2 ) {
		usage( argv[0] );
	}
	switch( argv[1][1] ) {
	case 'i':
		printf( "ip: %ul\n", my_ip_addr() );
		break;
	case 'h':
		printf( "host: %s\n", my_hostname() );
		break;
	case 'f':
		printf( "fullhost: %s\n", my_full_hostname() );
		break;
	case 's':
		printf( "sin_addr: %ul\n", (my_sin_addr())->s_addr );
		printf( "ntohl sin_addr: %ul\n", ntohl((my_sin_addr())->s_addr) );
		printf( "ntoa sin_addr: %s\n", inet_ntoa(*(my_sin_addr())) );
		break;
	default:
		usage( argv[0] );
		break;
	}
	return 0;
}
