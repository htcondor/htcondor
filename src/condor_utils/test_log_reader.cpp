/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#include "read_user_log.h"
#if ENABLE_STATE_DUMP
#  include "read_user_log_state.h"
#endif
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "subsystem_info.h"
#include "simple_arg.h"
#include <stdio.h>
#ifdef WIN32
// condor_sys_nt.h and signal.h disagree on the value of SIGABRT, so just undef it in this code.
#undef SIGABRT
#include <signal.h>
#undef SIGABRT
#endif

static const char *	VERSION = "0.9.5";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity{ VERB_NONE = 0, VERB_ERROR, VERB_WARNING, VERB_INFO, VERB_ALL };

struct Options
{
	const char *	logFile;
	const char *	persistFile;
	bool			readPersist;
	bool			writePersist;
	bool			rotation;
	int				maxRotations;
	bool			readOnly;
	bool			dumpState;
	bool			missedCheck;
	bool			exitAfterInit;
	bool			isEventLog;
	bool			checkFileStatus;
	int				maxExec;
	bool			exit;
	int				stop;
	int				sleep;
	int				term;
	Verbosity		verbosity;
};

Status
CheckArgs(int argc, const char **argv, Options &opts);

Status
ReadEvents( Options &opts, int &numEvents );

void
ReportError( const ReadUserLog &reader );

const char *timestr( time_t );

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
	set_debug_flags(NULL, D_ALWAYS);

	set_mySubSystem( "TEST_LOG_READER", false, SUBSYSTEM_TYPE_TOOL );

		// initialize to read from config file
	config();

		// Set up the dprintf stuff...
	dprintf_set_tool_debug("TEST_LOG_READER", 0);

	int		result = 0;
	int		events = 0;

	Options	opts;
	Status tmpStatus = CheckArgs(argc, argv, opts);
	if ( STATUS_CANCEL == tmpStatus ) {
		exit( 0 );
	}
	else if ( STATUS_OK != tmpStatus ) {
		printf( "Error parsing command line\n" );
		exit( 1 );
	}

	tmpStatus = ReadEvents(opts, events);
	if ( tmpStatus == STATUS_ERROR ) {
		printf( "Error from ReadEvents: %d\n", (int)tmpStatus );
		result = 1;
	}

	if ( opts.verbosity >= VERB_INFO ) {
		printf( "Total of %d events read\n", events );
	}

	if ( result != 0 && opts.verbosity >= VERB_ERROR ) {
		printf( "test_log_reader FAILED\n" );
	}

	printf( "test_log_reader: exiting with status %d\n", result );
	exit( result );
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
		"  --persist|-p <filename>: file to persist to/from\n"
#     if ENABLE_STATE_DUMP
		"  --dump-state: dump the persisted reader state after reading it\n"
#     endif
		"  --p-ro|--p-rw|--p-wo: Set persitent state to "
		"read-only/read-write/write-only\n"
		"  --init-only: exit after initialization\n"
		"  --rotation|-r <n>: enable rotation handling, set max #\n"
		"  --no-rotation: disable rotation handling\n"
		"  --no-sleep: No sleep between events\n"
		"  --sleep <number>: how many seconds to sleep between events\n"
		"  --stop: Send myself a SIGSTOP when no events available\n"
		"  --exit|-x: Exit when no events available\n"
		"  --eventlog|-e: Setup to read the EventLog\n"
		"  --check-file-status: Check the file status, print when changed\n"
		"  --ro: Read-only access to log file (disables locking)\n"
		"  --no-term: No limit on terminte events\n"
		"  --term <number>: number of terminate events to exit after\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number|name>: set verbosity level (default is ERROR)\n"
		"    names: NONE=0 ERROR WARNING INFO ERROR\n"
		"  --version: print the version number and compile date\n"
		"  <filename>: the log file to read\n";

	opts.logFile = NULL;
	opts.persistFile = NULL;
	opts.readPersist = false;
	opts.writePersist = false;
	opts.readOnly = false;
	opts.maxExec = 0;
	opts.isEventLog = false;
	opts.checkFileStatus = false;
	opts.exit = false;
	opts.sleep = 5;
	opts.stop = 0;
	opts.term = 1;
	opts.verbosity = VERB_ERROR;
	opts.rotation = false;
	opts.maxRotations = 0;
	opts.exitAfterInit = false;
	opts.dumpState = false;
	opts.missedCheck = false;

	int		argno = 1;
	while ( (argno < argc) & (status == STATUS_OK) ) {
		SimpleArg	arg( argv, argc, argno );

		if ( arg.Error() ) {
			printf("%s", usage);
			status = STATUS_ERROR;
		}

		if ( arg.Match('d', "debug") ) {
			if ( arg.hasOpt() ) {
				const char	*flags;
				arg.getOpt( flags );
				set_debug_flags( const_cast<char *>(flags), 0 );
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

		} else if ( arg.Match('e', "eventlog") ) {
			opts.isEventLog = true;

		} else if ( arg.Match("ro") ) {
			opts.readOnly = true;

		} else if ( arg.Match('p', "persist") ) {
			if ( arg.hasOpt() ) {
				arg.getOpt( opts.persistFile );
				opts.readPersist = true;
				opts.writePersist = true;
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

#     if ENABLE_STATE_DUMP
		} else if ( arg.Match("dump-state") ) {
			opts.dumpState = true;
#     endif

		} else if ( arg.Match("check-file-status") ) {
			opts.checkFileStatus = true;

		} else if ( arg.Match("init-only") ) {
			opts.exitAfterInit = true;

		} else if ( arg.Match("p-ro") ) {
			opts.readPersist = true;
			opts.writePersist = false;

		} else if ( arg.Match("p-rw") ) {
			opts.readPersist = true;
			opts.writePersist = true;

		} else if ( arg.Match( 'x', "exit") ) {
			opts.exit = true;

		} else if ( arg.Match("stop") ) {
			opts.stop++;

		} else if ( arg.Match( 'r', "rotation") ) {
			if ( arg.getOpt( opts.maxRotations ) ) {
				opts.rotation = opts.maxRotations > 0;
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

		} else if ( arg.Match("no-sleep") ) {
			opts.sleep = 0;

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
			int		v = (int) opts.verbosity;
			opts.verbosity = (Verbosity) (v + 1);

		} else if ( arg.Match("verbosity") ) {
			if ( arg.isOptInt() ) {
				int		verb;
				arg.getOpt(verb);
				opts.verbosity = (Verbosity) verb;
			}
			else if ( arg.hasOpt() ) {
				const char	*s;
				arg.getOpt( s );
				if ( !strcasecmp(s, "NONE" ) ) {
					opts.verbosity = VERB_NONE;
				}
				else if ( !strcasecmp(s, "ERROR" ) ) {
					opts.verbosity = VERB_ERROR;
				}
				else if ( !strcasecmp(s, "WARNING" ) ) {
					opts.verbosity = VERB_WARNING;
				}
				else if ( !strcasecmp(s, "INFO" ) ) {
					opts.verbosity = VERB_INFO;
				}
				else if ( !strcasecmp(s, "ALL" ) ) {
					opts.verbosity = VERB_ALL;
				}
				else {
					fprintf(stderr, "Unknown %s '%s'\n", arg.Arg(), s );
					printf("%s", usage);
					status = STATUS_ERROR;
				}
			}
			else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = STATUS_ERROR;
			}

		} else if ( arg.Match("version") ) {
			printf("test_log_reader: %s\n", VERSION);
			status = STATUS_CANCEL;

		} else if ( arg.Match("p-wo") ) {
			opts.readPersist = false;
			opts.writePersist = true;

		} else if ( !arg.ArgIsOpt() ) {
			arg.getOpt(opts.logFile);

		} else {
			fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
			printf("%s", usage);
			status = STATUS_ERROR;
		}
		argno = arg.Index( );
	}

	if ( status == STATUS_OK &&
		 !opts.readPersist &&
		 !opts.isEventLog &&
		 opts.logFile == NULL ) {
		fprintf(stderr, "Log file must be specified if not restoring state\n");
		printf("%s", usage);
		status = STATUS_ERROR;
	}

	return status;
}

Status
ReadEvents(Options &opts, int &totalEvents)
{
	Status					 result = STATUS_OK;
	int						 numEvents = 0;

	// No events yet!
	totalEvents = 0;

	// Create & initialize the state
	ReadUserLog::FileState	state;
	ReadUserLog::InitFileState( state );

	ReadUserLog	reader;

	// Initialize the reader from the persisted state
	if ( opts.readPersist ) {
		int	fd = safe_open_wrapper_follow( opts.persistFile, O_RDONLY, 0 );
		if ( fd >= 0 ) {
			if ( read( fd, state.buf, state.size ) != state.size ) {
				fprintf( stderr, "Failed reading persistent file\n" );
				return STATUS_ERROR;
			}
			close( fd );

			bool istatus;
			if ( opts.maxRotations ) {
				istatus = reader.initialize( state,
											 opts.maxRotations,
											 opts.readOnly );
			}
			else {
				istatus = reader.initialize( state,
											 opts.readOnly );
			}
			if ( ! istatus ) {
				fprintf( stderr, "Failed to initialize from state\n" );
				ReportError( reader );
				return STATUS_ERROR;
			}
			printf( "Initialized log reader from state %s\n",
					opts.persistFile );
		}

#     if ENABLE_STATE_DUMP
		if ( opts.dumpState ) {
			ReadUserLogState	rstate(state, 60);
			std::string			str;

			rstate.GetStateString( state, str, "Restore File State" );
			puts( str.Value() );
		}
#     endif

	}

	// If, after the above, the reader isn't initialized, do so now
	if ( !reader.isInitialized() ) {
		bool		 istatus = false;
		const char	*type = "None";
		char		 buf[64];
		if ( opts.isEventLog ) {
			istatus = reader.initialize( );
			type = "EventLog";
		}
		else if ( opts.maxRotations <= 1 ) {
			istatus = reader.initialize( opts.logFile,
										 opts.rotation,
										 opts.rotation );
			type = opts.rotation ? "file (rotation)" : "file (no rotations)";
		}
		else {
			istatus = reader.initialize( opts.logFile,
										 opts.maxRotations,
										 opts.rotation,
										 opts.readOnly );
			snprintf( buf, sizeof(buf), "file (%s/%d)",
					  opts.rotation ? "rotation" : "no rotations",
					  opts.maxRotations );
			type = buf;
		}
		if ( !istatus ) {
			fprintf( stderr, "Failed to initialize with %s\n", type );
			return STATUS_ERROR;
		}
		printf( "Initialized with %s\n", type );
	}

	// --init-only ?
	if ( opts.exitAfterInit ) {
		printf( "Exiting after init (due to --init-only)\n" );
		return STATUS_OK;
	}

	signal( SIGTERM, handle_sig );
	signal( SIGQUIT, handle_sig );
	signal( SIGINT, handle_sig );

	int						execEventCount = 0;
	int						termEventCount = 0;
	bool					done = (opts.term == 0);
	bool					missedLast = false;
	int						prevCluster=999;
	int						prevProc=999;
	ReadUserLog::FileStatus	prevFstatus = (ReadUserLog::FileStatus) 999;

	while ( !done && !global_done ) {
		bool	empty = false;
		if ( opts.checkFileStatus ) {
			ReadUserLog::FileStatus	fstatus = reader.CheckFileStatus( empty );
			if ( fstatus != prevFstatus ) {
				const char	*s;
				switch( fstatus ) {
				case ReadUserLog::LOG_STATUS_ERROR:
					s = "ERROR";
					break;
				case ReadUserLog::LOG_STATUS_NOCHANGE:
					s = "NOCHANGE";
					break;
				case ReadUserLog::LOG_STATUS_GROWN:
					s = "GROWN";
					break;
				case ReadUserLog::LOG_STATUS_SHRUNK:
					s = "SHRUNK";
					break;
				default:
					s = "unknown";
					break;
				}
				if ( opts.verbosity >= VERB_INFO ) {
					printf( "New status: %d/%s%s\n",
							(int) fstatus, s, empty ? " [empty]" : "" );
				}
				prevFstatus = fstatus;
			}
		}

		ULogEvent			*event = NULL;
		ULogEventOutcome	 outcome;
		if ( empty ) {
			outcome = ULOG_NO_EVENT;
		}
		else {
			outcome = reader.readEvent(event);
		}

		if ( outcome == ULOG_OK ) {
			if ( opts.verbosity >= VERB_ALL ) {
				printf( "Got an event from %d.%d.%d @ %s",
						event->cluster, event->proc, event->subproc,
						timestr(event->GetEventclock()) );
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

			if ( missedLast ) {
				ReadUserLogStateAccess		paccess( state );
				ReadUserLog::FileState		nstate;
				ReadUserLog::InitFileState( nstate );
				ReadUserLogStateAccess		naccess( nstate );

				long						diff_pos, diff_enum;
				char						puniq[256], nuniq[256];
				int							pseq, nseq;

				paccess.getLogPositionDiff( paccess, diff_pos );
				paccess.getEventNumberDiff( paccess, diff_enum );
				paccess.getUniqId( puniq, sizeof(puniq) );
				paccess.getSequenceNumber( pseq );
				naccess.getUniqId( nuniq, sizeof(nuniq) );
				paccess.getSequenceNumber( nseq );
				printf( "Missed: %ld bytes, %ld events\n"
						"  Previous Uniq=%s, seq=%d\n"
						"  Current  Uniq=%s, seq=%d\n",
						diff_pos, diff_enum,
						puniq, pseq,
						nuniq, nseq );
			}

			numEvents++;
			totalEvents++;
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
					result = STATUS_ERROR;
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

			// Store off the persisted state
			if ( opts.writePersist && reader.GetFileState( state ) ) {
				int	fd = safe_open_wrapper_follow( opts.persistFile,
											O_WRONLY|O_CREAT,
#ifdef WIN32
											_S_IWRITE);
#else
											S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
#endif
				if ( fd >= 0 ) {
					if ( write( fd, state.buf, state.size ) != state.size ) {
						fprintf( stderr, "Failed writing persistent file\n" );
					}
					close( fd );
				}
			}

		} else if ( outcome == ULOG_NO_EVENT ) {
			if ( opts.verbosity >= VERB_ALL ) {
				printf( "No events available\n" );
			}
			if ( opts.stop > 0 ) {
				if ( opts.verbosity >= VERB_INFO ) {
					printf( "Read %d events\n", numEvents );
				}
				printf( "\n*** Sending SIGSTOP to myself (PID %d) ***\n",
						getpid() );
				fflush( stdout );
#ifdef WIN32
				ExitProcess(SIGSTOP);
#else
				kill( getpid(), SIGSTOP );
#endif
				opts.stop--;
				numEvents = 0;
				printf( "\n*** Continued after stop ***\n");
			}
			else if ( opts.exit ) {
				done = true;
			}
			else {
				sleep(opts.sleep);
			}
			missedLast = false;

		} else if ( outcome == ULOG_MISSED_EVENT ) {
			printf( "\n*** Missed event(s) ***\n" );
			missedLast = true;

		} else if ( outcome == ULOG_RD_ERROR || outcome == ULOG_UNK_ERROR ) {
			if ( opts.verbosity >= VERB_ERROR ) {
				fprintf(stderr, "Error reading event @ # %d / %d\n",
						numEvents, totalEvents );
			}
			result = STATUS_ERROR;
		}

		delete event;
	}

	reader.GetFileState( state );
#  if ENABLE_STATE_DUMP
	if ( opts.dumpState ) {
		ReadUserLogState	rstate(state, 60);
		std::string			str;

		rstate.GetStateString( state, str, "Final File State" );
		puts( str.Value() );
	}
#  endif

	if ( opts.writePersist ) {
		fputs( "\nStoring final state...", stdout );
		int	fd = safe_open_wrapper_follow( opts.persistFile,
									O_WRONLY|O_CREAT,
#ifdef WIN32
									_S_IWRITE);
#else
									S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
#endif
		if ( fd >= 0 ) {
			if ( write( fd, state.buf, state.size ) != state.size ) {
				fputs( "Failed writing persistent file\n", stderr );
			}
			close( fd );
		}
		fputs( "  Done\n", stdout );
	}

	ReadUserLog::UninitFileState( state );

	if ( opts.verbosity >= VERB_INFO ) {
		printf( "Read %d events\n", numEvents );
	}

	return result;
}

void
ReportError( const ReadUserLog &reader )
{
	ReadUserLog::ErrorType	 error;
	const char				*error_str;
	unsigned				 line_num;

	reader.getErrorInfo( error, error_str, line_num );
	fprintf( stderr, "  %s (#%d) @ line %d\n",
			 error_str, error, line_num );
}

const char *timestr( time_t time )
{
	static char	tbuf[64];
	strncpy( tbuf, asctime( localtime(&time) ), sizeof(tbuf) );
	tbuf[sizeof(tbuf)-1] = '\0';
	if ( strlen(tbuf) ) {
		tbuf[strlen(tbuf)-1] = '\0';
	}
	return tbuf;
}
