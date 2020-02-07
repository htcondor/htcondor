// FIXME: copyright notice

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <stdio.h>
#include <sys/inotify.h>
#include <poll.h>

#include "file_modified_trigger.h"
#include "read_user_log.h"
#include "wait_for_user_log.h"

void usage( char * self ) {
	fprintf( stdout, "%s: [-debug] <path/to/event-log> [<timeout in milliseconds>]\n", self );
}

int main( int argc, char ** argv ) {

	set_priv_initialize(); // allow uid switching if root
	config();

	if( argc < 2 ) {
		usage( argv[0] );
		return 1;
	}

	long timeout = -1;
	char * filename = NULL;
	int firstPositionalArgument = 1;
	// Optional arguments (must come first).
	if( strcmp( argv[1], "-debug" ) == 0 ) {
		dprintf_set_tool_debug( "TOOL", 0 );
		++firstPositionalArgument;
	}

	// Positional arguments.
	filename = argv[firstPositionalArgument];
	if( firstPositionalArgument + 1 < argc ) {
		char * endptr;
		timeout = strtol( argv[ firstPositionalArgument + 1 ], &endptr, 10 );
		if( endptr == argv[ firstPositionalArgument + 1 ] || endptr[0] != '\0' ) {
			usage( argv[0] );
			return 1;
		}
	}

	if( filename == NULL ) {
		usage( argv[0] );
		return 1;
	}

	WaitForUserLog wful( filename );
	if(! wful.isInitialized()) {
		fprintf( stderr, "Failed to initialize user log reader, aborting.\n" );
		return 1;
	}

	do {
		ULogEvent * event = NULL;
		ULogEventOutcome outcome = wful.readEvent( event, timeout );
		switch( outcome ) {
			case ULOG_OK: {
				ClassAd * classad = event->toClassAd();
				if( classad != NULL ) {
					fPrintAd( stdout, * classad );
					fprintf( stdout, "\n" );
					delete classad;
				} else {
					fprintf( stderr, "Could not convert event to classad, aborting.\n" );
					return 3;
				}
			} break;

			case ULOG_NO_EVENT:
				dprintf( D_ALWAYS, "Log reader found no event, will wait again...\n" );
			break;

			case ULOG_RD_ERROR:
				fprintf( stderr, "Log reader read error, aborting.\n" );
				return 2;
			break;

			case ULOG_MISSED_EVENT:
				fprintf( stderr, "Log reader missed an event, aborting.\n" );
				return 2;
			break;

			case ULOG_UNK_ERROR:
				fprintf( stderr, "Log reader unknown error, aborting.\n" );
				return 2;
			break;

			default:
				fprintf( stderr, "Unknown event outcome '%d', aborting.\n", outcome );
				return 2;
			break;
		}
		delete event;
	} while( true );
}
