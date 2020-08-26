//
// Due to our policy of glorious failure, all three #includes are required
// in this order and at this location (before any system headers).
//
#include "condor_common.h"
#include "condor_debug.h"
#include "dc_startd.h"

#include "condor_config.h"

#include "match_prefix.h"

#include <stdio.h>
#include <errno.h>

int updateMachineAdAt( const char * const name, const char * const pool, const ClassAd & update, ClassAd & reply  ) {
	DCStartd startd( name, pool );
	if( ! startd.locate() ) {
		fprintf( stderr, "Unable to locate startd: %s\n", startd.error() );
		return 1;
	}

	if( ! startd.updateMachineAd( & update, & reply ) ) {
		fprintf( stderr, "Unable to update machine ad: %s\n", startd.error() );
		return 1;
	}

	return 0;
}

void version() {
}

void usage( const char * self ) {
	fprintf( stdout, "%s: [options] <path/to/update-ad>\n", self );
	fprintf( stdout, "Where options include:\n" );
	fprintf( stdout, "\t-pool\t\tWhich pool to query.\n" );
	fprintf( stdout, "\t-name\t\tWhich startd to ask for.\n" );
	fprintf( stdout, "\t-help\t\tPrint this help.\n" );
	fprintf( stdout, "\t-version\tPrint the HTCondor version.\n" );
	fprintf( stdout, "\n" );
}

int main( int argc, char ** argv ) {

	set_priv_initialize(); // allow uid switching if root
	config();

	char * name = NULL, * pool = NULL, * file = NULL;
	for( int i = 1; i < argc; ++i ) {
		if( is_arg_prefix( argv[i], "-help" ) ) {
			usage( argv[0] );
			return 0;
		} else
		if( is_arg_prefix( argv[i], "-version" ) ) {
			version();
			return 0;
		} else
		if( is_arg_prefix( argv[i], "-pool" ) ) {
			++i;
			if( i < argc ) {
				pool = argv[i];
			} else {
				usage( argv[0] );
				return 0;
			}
		} else
		if( is_arg_prefix( argv[i], "-name" ) ) {
			++i;
			if( i < argc ) {
				name = argv[i];
			} else {
				usage( argv[0] );
				return 0;
			}
			name = argv[i];
		} else {
			file = argv[i];
		}
	}

	FILE * f = NULL;
	if( file == NULL ) {
		file = strdup( "stdin" );
		f = stdin;
	} else {
		f = fopen( file, "r" );
	}

	if( f == NULL ) {
		fprintf( stderr, "Failed to open '%s', aborting (%d: %s)\n", file, errno, strerror( errno ) );
		return 1;
	}

	int isEOF = 0, isError = 0, isEmpty = 0;
	ClassAd update;
	InsertFromFile( f, update, "\n", isEOF, isError, isEmpty );
	if( isError || isEmpty ) {
		fprintf( stderr, "Failed to parse '%s', aborting.\n", file );
		//if( isEOF ) { fprintf( stderr, "File ended unexpectedly.  Please add a blank line the end of the file.\n" ); }
		if( isEmpty ) { fprintf( stderr, "File was empty.\n" ); }
		if( isError ) { fprintf( stderr, "Syntax error in file.\n" ); }
		return 1;
	}

	ClassAd reply;
	return updateMachineAdAt( name, pool, update, reply );
}

