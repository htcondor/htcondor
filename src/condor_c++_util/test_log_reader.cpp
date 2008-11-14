/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "user_log.c++.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "MyString.h"
#include "subsystem_info.h"
#include "simple_arg.h"
#include <stdio.h>

static const char *	VERSION = "0.9.3";

DECL_SUBSYSTEM( "TEST_LOG_READER", SUBSYSTEM_TYPE_TOOL );

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity { VERB_NONE = 0, VERB_ERROR, VERB_WARNING, VERB_ALL };

struct Options
{
	const char *	logFile;
	const char *	persistFile;
	bool			readPersist;
	bool			writePersist;
	bool			rotation;
	int				max_rotations;
	bool			dumpState;
	bool			missedCheck;
	bool			exitAfterInit;
	int				maxExec;
	bool			exit;
	int				sleep;
	int				term;
	int				verbosity;
};

Status
CheckArgs(int argc, const char **argv, Options &opts);

int // 0 == okay, 1 == error
ReadEvents(Options &opts);

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
main(int argc, const char **argv)
{
	DebugFlags = D_ALWAYS;

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("TEST_LOG_READER");

	int		result = 0;

	Options	opts;

	Status tmpStatus = CheckArgs(argc, argv, opts);
	
	if ( tmpStatus == STATUS_OK ) {
		result = ReadEvents(opts);
	} else if ( tmpStatus == STATUS_ERROR ) {
		result = 1;
	}

	if ( result != 0 && opts.verbosity >= VERB_ERROR ) {
		fprintf(stderr, "test_log_reader FAILED\n");
	}

	return result;
}

Status
CheckArgs(int argc, const char **argv, Options &opts)
{
	Status	status = STATUS_OK;

	const char *	usage =
		"Usage: test_log_reader [options] <filename>\n"
		"  --debug|-d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --max-exec <number>: maximum number of execute events to read\n"
		"  --miss-check: Enable missed event checking "
		"(valid only with test writer)\n"
		"  --persist|-p <filename>: file to persist to/from "
		"(implies --rotation)\n"
		"  --dump-state: dump the persisted reader state after reading it\n"
		"  --ro|--rw|--wo: Set persitent state to "
		"read-only/read-write/write-only\n"
		"  --init-only: exit after initialization\n"
		"  --rotation|-r <n>: enable rotation handling, set max #\n"
		"  --no-rotation: disable rotation handling\n"
		"  --sleep <number>: how many seconds to sleep between events\n"
		"  --exit|x: Exit when no event available\n"
		"  --no-term: No limit on terminte events\n"
		"  --term <number>: number of terminate events to exit after\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n"
		"  <filename>: the log file to read\n";

	opts.logFile = NULL;
	opts.persistFile = NULL;
	opts.readPersist = false;
	opts.writePersist = false;
	opts.maxExec = 0;
	opts.exit = false;
	opts.sleep = 5;
	opts.term = 1;
	opts.verbosity = 1;
	opts.rotation = false;
	opts.max_rotations = 0;
	opts.exitAfterInit = false;
	opts.dumpState = false;
	opts.missedCheck = false;

	for ( int index = 1; index < argc; ++index ) {
		SimpleArg	arg( argv, argc, index );

		if ( arg.Error() ) {
			printf("%s", usage);
			status = STATUS_ERROR;
		}

		if ( arg.Match('d', "debug") ) {
			if ( arg.hasOpt() ) {
				set_debug_flags( const_cast<char *>(arg.getOpt()) );
				index = arg.ConsumeOpt( );
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

		} else if ( arg.Match("max-exec") ) {
			if ( !arg.getOpt( opts.maxExec ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

		} else if ( arg.Match("miss-check") ) {
			opts.missedCheck = true;

		} else if ( arg.Match('p', "persist") ) {
			if ( arg.hasOpt() ) {
				arg.getOpt( opts.persistFile );
				opts.rotation = true;
				if ( opts.max_rotations == 0 ) {
					opts.max_rotations = 1;
				}
				opts.readPersist = true;
				opts.writePersist = true;
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

		} else if ( arg.Match("dump-state") ) {
			opts.dumpState = true;

		} else if ( arg.Match("init-only") ) {
			opts.exitAfterInit = true;

		} else if ( arg.Match("ro") ) {
			opts.readPersist = true;
			opts.writePersist = false;

		} else if ( arg.Match("rw") ) {
			opts.readPersist = true;
			opts.writePersist = true;

		} else if ( arg.Match("exit") ) {
			opts.exit = true;

		} else if ( arg.Match( 'r', "rotation") ) {
			if ( arg.getOpt( opts.max_rotations ) ) {
				opts.rotation = opts.max_rotations > 0;
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
			}

		} else if ( arg.Match("no-rotation") ) {
			opts.rotation = false;

		} else if ( arg.Match("sleep") ) {
			if ( !arg.getOpt( opts.sleep ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

		} else if ( arg.Match("no-term") ) {
			opts.term = -1;

		} else if ( arg.Match("term") ) {
			if ( !arg.getOpt( opts.term ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

		} else if ( ( arg.Match("usage") )		||
					( arg.Match('h') )			||
					( arg.Match("help") )  )	{
			printf("%s", usage);
			status = STATUS_CANCEL;

		} else if ( arg.Match('v') ) {
			opts.verbosity++;

		} else if ( arg.Match("verbosity") ) {
			if ( !arg.getOpt( opts.verbosity ) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

		} else if ( arg.Match("version") ) {
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else if ( arg.Match("wo") ) {
			opts.readPersist = false;
			opts.writePersist = true;

		} else if ( !arg.ArgIsOpt() ) {
			opts.logFile = arg.Arg();

		} else {
			fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
			printf("%s", usage);
			status = STATUS_ERROR;
		}
	}

	if ( status == STATUS_OK && !opts.readPersist && opts.logFile == NULL ) {
		fprintf(stderr, "Log file must be specified if not restoring state\n");
		printf("%s", usage);
		status = STATUS_ERROR;
	}

	return status;
}

int
ReadEvents(Options &opts)
{
	int		result = 0;

	// Create & initialize the state
	ReadUserLog::FileState	state;
	ReadUserLog::InitFileState( state );

	ReadUserLog	log;

	// Initialize the reader from the persisted state
	if ( opts.readPersist ) {
		int	fd = safe_open_wrapper( opts.persistFile, O_RDONLY, 0 );
		if ( fd >= 0 ) {
			if ( read( fd, state.buf, state.size ) != state.size ) {
				fprintf( stderr, "Failed reading persistent file\n" );
				return STATUS_ERROR;
			}
			close( fd );
			if ( !log.initialize( state, opts.max_rotations ) ) {
				fprintf( stderr, "Failed to initialize from state\n" );
				return STATUS_ERROR;
			}
			printf( "Initialized log reader from state %s\n",
					opts.persistFile );
		}
		if ( opts.dumpState ) {
			MyString	str;
			log.FormatFileState( state, str, "Restore File State (raw)" );
			puts( str.GetCStr() );

			log.FormatFileState( str, "Restore File State" );
			puts( str.GetCStr() );	
		}
	}

	// If, after the above, the reader isn't initialized, do so now
	if ( !log.isInitialized() ) {
		if ( !log.initialize( opts.logFile,
							  opts.max_rotations,
							  opts.rotation) ) {
			fprintf( stderr, "Failed to initialize with file\n" );
			return STATUS_ERROR;
		}
	}

	// --init-only ?
	if ( opts.exitAfterInit ) {
		printf( "Exiting after init (due to --init-only)\n" );
		return STATUS_OK;
	}

	signal( SIGTERM, handle_sig );
	signal( SIGQUIT, handle_sig );
	signal( SIGINT, handle_sig );

	int		execEventCount = 0;
	int		termEventCount = 0;
	bool	done = (opts.term == 0);
	bool	missedLast = false;
	int		prevCluster=999, prevProc=999, prevSubproc=999;

	while ( !done && !global_done ) {
		ULogEvent	*event = NULL;

		ULogEventOutcome	outcome = log.readEvent(event);
		if ( outcome == ULOG_OK ) {
			if ( opts.verbosity >= VERB_ALL ) {
				printf( "Got an event from %d.%d.%d @ %s",
						event->cluster, event->proc, event->subproc,
						timestr(event->eventTime) );
			}

			// Store off the persisted state
			if ( opts.writePersist && log.GetFileState( state ) ) {
				int	fd = safe_open_wrapper( opts.persistFile,
											O_WRONLY|O_CREAT,
											S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
				if ( fd >= 0 ) {
					if ( write( fd, state.buf, state.size ) != state.size ) {
						fprintf( stderr, "Failed writing persistent file\n" );
					}
					close( fd );
				}
			}

			if (opts.missedCheck ) {
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
				if ( opts.verbosity >= VERB_ALL ) {
					printf(" (submit)\n");
				}
				missedLast = false;
				break;

			case ULOG_EXECUTE:
				if ( opts.verbosity >= VERB_ALL ) {
					printf(" (execute)\n");
				}
				if ( opts.maxExec != 0 &&
						++execEventCount > opts.maxExec ) {
					if ( opts.verbosity >= VERB_ERROR ) {
						fprintf(stderr, "Maximum number of execute "
								"events (%d) exceeded\n", opts.maxExec);
					}
					result = 1;
					done = true;
				}
				missedLast = false;
				break;

			case ULOG_JOB_TERMINATED:
				if ( opts.verbosity >= VERB_ALL ) {
					printf(" (terminated)\n");
				}
				if ( (opts.term > 0) && (++termEventCount >= opts.term) ) {
					if ( opts.verbosity >= VERB_ALL ) {
						printf( "Reached terminated event limit (%d); %s\n",
								opts.term, "exiting" );
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
					char	*info = strdup( generic->info );
					int		 l = strlen( info );
					char	*p = info+l-1;
					while( l && isspace(*p) ) {
						*p = '\0';
						p--;
						l--;
					}
					printf( " (generic): '%s'\n", info );
					free( info );
				}
				break;
			}

			default:
				if ( opts.verbosity >= VERB_ALL ) {
					const char *name = event->eventName( );
					printf(" (%s)\n", name ? name : "UNKNOWN" );
				}
				break;

			}

		} else if ( outcome == ULOG_NO_EVENT ) {
			if ( opts.exit ) {
				done = true;
			}
			else {
				sleep(opts.sleep);
			}
			missedLast = false;

		} else if ( outcome == ULOG_MISSED_EVENT ) {
			printf( "\n*** Missed event ***\n" );
			missedLast = true;

		} else if ( outcome == ULOG_RD_ERROR || outcome == ULOG_UNK_ERROR ) {
			if ( opts.verbosity >= VERB_ERROR ) {
				fprintf(stderr, "Error reading event\n");
			}
			result = 1;
		}

		delete event;
	}

	log.GetFileState( state );
	if ( opts.dumpState ) {
		MyString	str;

		log.FormatFileState( state, str, "Final File State (raw)" );
		puts( str.GetCStr() );

		log.FormatFileState( str, "Final File State" );
		puts( str.GetCStr() );
	}
	if ( opts.writePersist ) {
		fputs( "\nStoring final state...", stdout );
		int	fd = safe_open_wrapper( opts.persistFile,
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

	ReadUserLog::UninitFileState( state );
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
