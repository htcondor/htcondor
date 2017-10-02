#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

int
condor_off(	const char * annexName, int argc, char ** argv,
  unsigned subCommandIndex ) {
	std::string coPath = argv[0];
	coPath.replace( coPath.find( "condor_annex" ), strlen( "condor_annex" ),
		"condor_off" );

	unsigned EXTRA_ARGS = 1;
	if( annexName != NULL && annexName[0] != '\0' ) {
		EXTRA_ARGS += 1;
	}

	char ** coArgv = (char **)malloc( (argc - subCommandIndex + EXTRA_ARGS + 1) * sizeof(char *) );
	if( coArgv == NULL ) { return 1; }

	coArgv[0] = strdup( coPath.c_str() );
	coArgv[1] = strdup( "-annex" );
	if( annexName != NULL && annexName[0] != '\0' ) {
		coArgv[2] = strdup( annexName );
	}

	bool allDuplicated = true;
	for( unsigned i = 0; i < EXTRA_ARGS; ++i ) {
		if( coArgv[i] == NULL ) { allDuplicated = false; break; }
	}
	if(! allDuplicated) {
		for(unsigned i = 0; i < EXTRA_ARGS; ++i ) {
			if( coArgv[i] != NULL ) { free( coArgv[i] ); }
		}
		free( coArgv );
		return 1;
	}

	// Copy any trailing arguments.
	for( int i = subCommandIndex + 1; i < argc; ++i ) {
		coArgv[EXTRA_ARGS + (i - subCommandIndex)] = argv[i];
	}
	coArgv[argc - subCommandIndex + EXTRA_ARGS] = NULL;

	if( coPath[0] == '/' ) {
		int r = execv( coPath.c_str(), coArgv );
		free( coArgv );
		return r;
	} else {
		int r = execvp( coPath.c_str(), coArgv );
		free( coArgv );
		return r;
	}
}
