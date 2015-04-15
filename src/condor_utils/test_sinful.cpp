#include "condor_common.h"
#include "condor_sinful.h"

#include <stdio.h>

//
// ...
//

bool verbose = false;
#define REQUIRE( condition ) \
	if(! ( condition )) { \
		fprintf( stderr, "Failed requirement '%s' on line %d.\n", #condition, __LINE__ ); \
		return 1; \
	} else if( verbose ) { \
		fprintf( stdout, "Passed requirement '%s' on line %d.\n", #condition, __LINE__ ); \
	}

int main( int, char ** ) {
	return 0;
}
