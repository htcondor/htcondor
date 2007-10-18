/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "user_log.c++.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "MyString.h"
#include <stdio.h>

static const char *	VERSION = "0.9.2";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity { VERB_NONE = 0, VERB_ERROR, VERB_WARNING, VERB_ALL };

struct Arguments {
	const char *	logFile;
	const char *	persistFile;
	bool			readPersist;
	bool			writePersist;
	bool			rotation;
	bool			dumpState;
	bool			missedCheck;
	bool			exitAfterInit;
	int				maxExec;
	int				sleep;
	int				term;
	int				verbosity;
};

Status
CheckArgs(int argc, char **argv, Arguments &args);

int // 0 == okay, 1 == error
ReadEvents(Arguments &args);

const char *timestr( struct tm &tm );

// Simple term signal handler
static bool	global_done = false;
void handle_sig(int sig)
{
	(void) sig;
	printf( "Got signal; shutting down\n" );
	global_done = true;
}

int
main(int argc, char **argv)
{
	DebugFlags = D_ALWAYS;

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("TEST_LOG_READER");

	int		result = 0;

	Arguments	args;

	Status tmpStatus = CheckArgs(argc, argv, args);

	if ( tmpStatus == STATUS_OK ) {
		result = ReadEvents(args);
	} else if ( tmpStatus == STATUS_ERROR ) {
		result = 1;
	}

	if ( result != 0 && args.verbosity >= VERB_ERROR ) {
		fprintf(stderr, "test_log_reader FAILED\n");
	}

	return result;
}

Status
CheckArgs(int argc, char **argv, Arguments &args)
{
	Status	status = STATUS_OK;

	const char *	usage =
		"Usage: test_log_reader [options] <filename>\n"
		"  -debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  -maxexec <number>: maximum number of execute events to read\n"
		"  -misscheck: Enable missed event checking "
		"(valid only with test writer)\n"
		"  -persist <filename>: file to persist to/from (implies -rotation)\n"
		"  -persist-dump: dump the persistent file state after reading it\n"
		"  -ro|-rw|-wo: Set persitent state to "
		"read-only/read-write/write-only\n"
		"  -init-only: exit after initialization\n"
		"  -rotation: enable rotation handling\n"
		"  -no-rotation: disable rotation handling\n"
		"  -sleep <number>: how many seconds to sleep between events\n"
		"  -term <number>: number of terminate events to exit after\n"
		"  -usage: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  -verbosity <number>: set verbosity level (default is 1)\n"
		"  -version: print the version number and compile date\n"
		"  <filename>: the log file to read\n";

	args.logFile = NULL;
	args.persistFile = NULL;
	args.readPersist = false;
	args.writePersist = false;
	args.maxExec = 0;
	args.sleep = 5;
	args.term = 1;
	args.verbosity = 1;
	args.rotation = false;
	args.exitAfterInit = false;
	args.dumpState = false;
	args.missedCheck = false;

	for ( int index = 1; index < argc; ++index ) {
		if ( !strcmp(argv[index], "-debug") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -debug argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				set_debug_flags( argv[index] );
			}

		} else if ( !strcmp(argv[index], "-maxexec") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -maxexec argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.maxExec = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-misscheck") ) {
			args.missedCheck = true;

		} else if ( !strcmp(argv[index], "-persist") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -persist argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.persistFile = argv[index];
				args.rotation = true;
				args.readPersist = true;
				args.writePersist = true;
			}

		} else if ( !strcmp(argv[index], "-persist-dump") ) {
			args.dumpState = true;

		} else if ( !strcmp(argv[index], "-init-only") ) {
			args.exitAfterInit = true;

		} else if ( !strcmp(argv[index], "-ro") ) {
			args.readPersist = true;
			args.writePersist = false;

		} else if ( !strcmp(argv[index], "-rw") ) {
			args.readPersist = true;
			args.writePersist = true;

		} else if ( !strcmp(argv[index], "-rotation") ) {
			args.rotation = true;


		} else if ( !strcmp(argv[index], "-no-rotation") ) {
			args.rotation = false;


		} else if ( !strcmp(argv[index], "-sleep") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -sleep argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.sleep = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-term") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -term argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.term = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-usage") ) {
			printf("%s", usage);
			status = STATUS_CANCEL;

		} else if ( !strcmp(argv[index], "-v") ) {
			args.verbosity++;

		} else if ( !strcmp(argv[index], "-verbosity") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -verbosity argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				args.verbosity = atoi(argv[index]);
			}

		} else if ( !strcmp(argv[index], "-version") ) {
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else if ( !strcmp(argv[index], "-wo") ) {
			args.readPersist = false;
			args.writePersist = true;

		} else if ( argv[index][0] != '-' ) {
			args.logFile = argv[index];

		} else {
			fprintf(stderr, "Unrecognized argument: <%s>\n", argv[index]);
			printf("%s", usage);
			status = STATUS_ERROR;
		}
	}

	if ( status == STATUS_OK && args.logFile == NULL ) {
		fprintf(stderr, "Log file must be specified\n");
		printf("%s", usage);
		status = STATUS_ERROR;
	}

	return status;
}

int
ReadEvents(Arguments &args)
{
	int		result = 0;

	// Create & initialize the state
	ReadUserLog::FileState	state;
	ReadUserLog::InitFileState( state );

	ReadUserLog	log;

	// Initialize the reader from the persisted state
	if ( args.readPersist ) {
		int	fd = safe_open_wrapper( args.persistFile, O_RDONLY, 0 );
		if ( fd >= 0 ) {
			if ( read( fd, state.buf, state.size ) != state.size ) {
				fprintf( stderr, "Failed reading persistent file\n" );
				return STATUS_ERROR;
			}
			close( fd );
			if ( !log.initialize( state, args.rotation ) ) {
				fprintf( stderr, "Failed to initialize from state\n" );
				return STATUS_ERROR;
			}
			printf( "Initialized log reader from state %s\n",
					args.persistFile );
		}
		if ( args.dumpState ) {
			MyString	str;
			log.FormatFileState( str, "Restore File State" );
			puts( str.GetCStr() );
		}
	}

	// If, after the above, the reader isn't initialized, do so now
	if ( !log.isInitialized() ) {
		if ( !log.initialize( args.logFile, args.rotation, args.rotation ) ) {
			fprintf( stderr, "Failed to initialize with file\n" );
			return STATUS_ERROR;
		}
	}

	// -init-only ?
	if ( args.exitAfterInit ) {
		printf( "Exiting after init (due to -init-only)\n" );
		return STATUS_OK;
	}

	signal( SIGTERM, handle_sig );
	signal( SIGQUIT, handle_sig );
	signal( SIGINT, handle_sig );

	int		executeEventCount = 0;
	int		terminatedEventCount = 0;
	bool	done = false;
	bool	missedLast = false;
	int		prevCluster=999, prevProc=999, prevSubproc=999;

	while ( !done && !global_done ) {
		ULogEvent	*event = NULL;

		ULogEventOutcome	outcome = log.readEvent(event);
		if ( outcome == ULOG_OK ) {
			if ( args.verbosity >= VERB_ALL ) {
				printf( "Got an event from %d.%d.%d @ %s",
						event->cluster, event->proc, event->subproc,
						timestr(event->eventTime) );
			}

			// Store off the persisted state
			if ( args.writePersist && log.GetFileState( state ) ) {
				int	fd = safe_open_wrapper( args.persistFile,
											O_WRONLY|O_CREAT,
											S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
				if ( fd >= 0 ) {
					if ( write( fd, state.buf, state.size ) != state.size ) {
						fprintf( stderr, "Failed writing persistent file\n" );
					}
					close( fd );
				}
			}

			if (args.missedCheck ) {
				bool isPrevCluster = ( prevCluster == event->cluster );
				bool isPrevProc = (  ( prevProc   == event->proc )  ||
									 ( prevProc+1 == event->proc )  );
				if ( missedLast && isPrevCluster && isPrevProc ) {
					printf( "\n** Bad missed **\n" );
					global_done = true;
				}
				else if ( (!missedLast) && isPrevCluster && (!isPrevProc) ) {
					printf( "\n** Undetected missed event **\n" );
					global_done = true;
					
				}
			}
			prevCluster = event->cluster;
			prevProc = event->proc;
			prevSubproc = event->subproc;

			switch ( event->eventNumber ) {
			case ULOG_SUBMIT:
				if ( args.verbosity >= VERB_ALL ) {
					printf(" (submit)\n");
				}
				missedLast = false;
				break;

			case ULOG_EXECUTE:
				if ( args.verbosity >= VERB_ALL ) {
					printf(" (execute)\n");
				}
				if ( args.maxExec != 0 &&
						++executeEventCount > args.maxExec ) {
					if ( args.verbosity >= VERB_ERROR ) {
						fprintf(stderr, "Maximum number of execute "
								"events (%d) exceeded\n", args.maxExec);
					}
					result = 1;
					done = true;
				}
				missedLast = false;
				break;

			case ULOG_JOB_TERMINATED:
				if ( args.verbosity >= VERB_ALL ) {
					printf(" (terminated)\n");
				}
				if ( args.term != 0 && ++terminatedEventCount >= args.term ) {
					if ( args.verbosity >= VERB_ALL ) {
						printf( "Reached terminated event limit (%d); %s\n",
								args.term, "exiting" );
					}
					done = true;
				}
				missedLast = false;
				break;

			case ULOG_GENERIC:
			{
				const GenericEvent	*generic =
					dynamic_cast <const GenericEvent*>( event );
				if ( ! generic ) {
					fprintf( stderr, "Can't pointer cast generic event!\n" );
				}
				else {
					int	l = strlen( generic->info );
					char	*p = (char*)generic->info+l-1;
					while( l && isspace(*p) ) {
						*p = '\0';
						p--;
						l--;
					}
					printf( " (generic): '%s'\n", generic->info );
				}
				break;
			}

			default:
				if ( args.verbosity >= VERB_ALL ) {
					const char *name = event->eventName( );
					printf(" (%s)\n", name ? name : "UNKNOWN" );
				}
				break;

			}

		} else if ( outcome == ULOG_NO_EVENT ) {
			sleep(args.sleep);
			missedLast = false;

		} else if ( outcome == ULOG_MISSED_EVENT ) {
			printf( "\n*** Missed event ***\n" );
			missedLast = true;

		} else if ( outcome == ULOG_RD_ERROR || outcome == ULOG_UNK_ERROR ) {
			if ( args.verbosity >= VERB_ERROR ) {
				fprintf(stderr, "Error reading event\n");
			}
			result = 1;
		}

		delete event;
	}

	if ( args.dumpState ) {
		MyString	str;
		log.FormatFileState( str, "Final File State" );
		puts( str.GetCStr() );
	}
	if ( args.writePersist ) {
		fputs( "\nStoring final state...", stdout );
		int	fd = safe_open_wrapper( args.persistFile,
									O_WRONLY|O_CREAT,
									S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
		if ( fd >= 0 ) {
			if ( write( fd, state.buf, state.size ) != state.size ) {
				fputs( "Failed writing persistent file\n", stderr );
			}
			close( fd );
		}
		fputs( "  Done\n", stdout );
	}

	return result;
}

const char *timestr( struct tm &tm )
{
	static char	tbuf[64];
	strncpy( tbuf, asctime( &tm ), sizeof(tbuf) );
	tbuf[sizeof(tbuf)-1] = '\0';
	if ( strlen(tbuf) ) {
		tbuf[strlen(tbuf)-1] = '\0';
	}
	return tbuf;
}
