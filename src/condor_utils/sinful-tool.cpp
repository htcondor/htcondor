#include "condor_common.h"
#include "condor_debug.h"
#include "condor_sinful.h"

#include <stdio.h>

int printOrFail( char const * in ) {
	if( in ) {
		fprintf( stdout, "%s\n", in );
		return 0;
	} else {
		return 1;
	}
}

int main( int argc, char * argv[] ) {
	if( argc != 3 ) {
		fprintf( stderr, "Usage: %s <command> <sinful>\n", argv[0] );
		return 2;
	}

	Sinful s( argv[2] );
	if( strcmp( argv[1], "valid" ) == 0 ) {
		fprintf( stdout, "%s\n", s.valid() ? "valid" : "invalid" );
		return s.valid() ? 0 : 1;
	}


	if( ! s.valid() ) {
		fprintf( stderr, "String '%s' is not a valid Sinful.\n", argv[2] );
		return 2;
	}

	if( strcmp( argv[1], "host" ) == 0 ) {
		return printOrFail( s.getHost() );
	} else if( strcmp( argv[1], "port" ) == 0 ) {
		return printOrFail( s.getPort() );
	} else if( strcmp( argv[1], "ccb" ) == 0 ) {
		return printOrFail( s.getCCBContact() );
	} else if( strcmp( argv[1], "private-addr" ) == 0 ) {
		return printOrFail( s.getPrivateAddr() );
	} else if( strcmp( argv[1], "private-name" ) == 0 ) {
		return printOrFail( s.getPrivateNetworkName() );
	} else if( strcmp( argv[1], "shared-port" ) == 0 ) {
		return printOrFail( s.getSharedPortID() );
	} else if( strcmp( argv[1], "alias" ) == 0 ) {
		return printOrFail( s.getAlias() );
	} else if( strcmp( argv[1], "no-udp" ) == 0 ) {
		if( s.noUDP() ) { fprintf( stdout, "no UDP\n" ); return 0; } else { return 1; }
	} else if( strcmp( argv[1], "param-count" ) == 0 ) {
		fprintf( stdout, "%d\n", s.numParams() );
		return 0;
	} else if( strcmp( argv[1], "addrs" ) == 0 ) {
		std::vector< condor_sockaddr > * addrs = s.getV1Addrs();
		if( addrs == NULL ) {
			return 1;
		}
		if( addrs->size() == 0 ) {
			delete addrs;
			return 1;
		}
		for( unsigned i = 0; i < addrs->size(); ++i ) {
			fprintf( stdout, "%s\n", (*addrs)[i].to_ip_and_port_string().c_str() );
		}
		delete addrs;
	} else {
		fprintf( stderr, "Unrecognized command '%s'.\n", argv[1] );
		return 2;
	}

	return 0;
}
