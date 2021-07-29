#include "condor_common.h"
#include "condor_debug.h"
#include "condor_sinful.h"
#include "stl_string_utils.h"

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
#if defined(WINDOWS)
	if( argc < 3 ) {
		fprintf( stderr, "Usage: %s <command> <sinful>\n", argv[0] );
		return 2;
	}

	// Windows doesn't have argument arrays, so anything with spaces in
	// it will get broken up.  Bypass everything.  Cargo culted from TJ.

	std::string cmdline;
	formatstr(cmdline, "%S", GetCommandLineW() );
	const char * sinful = cmdline.c_str();
	int numSpaces = 0;
	for( int i = 0; i < cmdline.length() && numSpaces < 2; ++i ) {
		if( cmdline[i] == ' ' ) {
			sinful = cmdline.c_str()+i;
			numSpaces += 1;
		}
	}
	if( numSpaces != 2 || sinful[1] == '\0' ) {
		fprintf( stderr, "Usage: %s <command> <sinful>\n", argv[0] );
		return 2;
	}
	sinful = &sinful[1];
	sinful = strdup( sinful );
	cmdline.clear();
#else
	if( argc != 3 ) {
		fprintf( stderr, "Usage: %s <command> <sinful>\n", argv[0] );
		return 2;
	}

	char * sinful = argv[2];
#endif

	Sinful s( sinful );
	if( strcmp( argv[1], "valid" ) == 0 ) {
		fprintf( stdout, "%s\n", s.valid() ? "valid" : "invalid" );
		return s.valid() ? 0 : 1;
	}


	if( ! s.valid() ) {
		fprintf( stderr, "The string '%s' is not a valid Sinful.\n", sinful );
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
		std::vector< condor_sockaddr > * addrs = s.getAddrs();
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
	} else if( strcmp( argv[1], "sinful" ) == 0 ) {
		fprintf( stdout, "%s\n", s.getSinful() );
	} else if( strcmp( argv[1], "v1" ) == 0 ) {
		fprintf( stdout, "%s\n", s.getV1String() );
	} else {
		fprintf( stderr, "Unrecognized command '%s'.\n", argv[1] );
		return 2;
	}

	return 0;
}
