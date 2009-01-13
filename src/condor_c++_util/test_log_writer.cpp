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
#include "condor_distribution.h"
#include "write_user_log.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "condor_random_num.h"
#include "condor_string.h"
#include "simple_arg.h"
#include "stat_wrapper.h"
#include <stdio.h>
#include <unistd.h>
#include <math.h>

static const char *	VERSION = "0.9.5";
DECL_SUBSYSTEM( "TEST_LOG_WRITER", SUBSYSTEM_TYPE_TOOL );

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity {
	VERB_NONE = 0,
	VERB_ERROR,
	VERB_WARNING,
	VERB_INFO,
	VERB_VERBOSE,
	VERB_ALL
};

struct Options
{
	bool			isXml;
	const char *	logFile;
	int				numExec;
	int				sleep_seconds;
	int				sleep_useconds;
	int				cluster;
	int				proc;
	int				subproc;
	int				numProcs;
	double			randomProb;		// Probability of 'random' events
	bool			stork;
	const char *	submitNote;
	Verbosity		verbosity;
	const char *	genericEventStr;
	const char *    persistFile;

	int				maxRotations;	// # of rotations limit
	int				maxSequence;	// Sequence # limit
	long			maxGlobalSize;	// Limit max size of global file
	long			maxUserSize;	// Limit max size of user log file

	int				num_forks;
	int				fork_cluster_step;
};

class EventInfo
{
public:
	EventInfo( const Options &opts, int cluster, int proc, int subproc ) {
		m_opts = &opts;
		m_cluster = cluster;
		m_proc = proc;
		m_subproc = subproc;
		m_eventp = NULL;
		m_name = NULL;
		m_note = NULL;
		m_is_large = false;
	}
	~EventInfo( void ) {
		Reset( );
	}
	void Reset( void ) {
		if ( m_eventp ) {
			delete m_eventp;
			m_eventp = NULL;
		}
		if ( m_name ) {
			free( const_cast<char *>(m_name) );
			m_name = NULL;
		}
		if ( m_note ) {
			free( const_cast<char *>(m_note) );
			m_note = NULL;
		}
	}

	bool IsValid( void ) const {
		return ( m_eventp  &&  m_name );
	};

	ULogEventNumber GetEventNumber( void ) const;

	ULogEvent *GetEvent( void ) const { return m_eventp; };
	ULogEvent *SetEvent( ULogEvent *event ) { return m_eventp = event; };

	const char *GetName( void ) const { return m_name; };
	void SetName( const char *name ) { m_name = strdup(name); };

	const char *GetNote( void ) const { return m_note; };
	void SetNote( const char *note ) { m_note = strdup(note); };

	ULogEvent *GenEvent( void );
	ULogEvent *GenEvent( ULogEventNumber );

	bool WriteEvent( UserLog &log );

	bool NextCluster( void ) { m_cluster++; };
	bool NextProc( void ) { m_proc++; };
	bool NextSubProc( void ) { m_subproc++; };

	ULogEvent *GenEventBasic( const char *name, ULogEvent *event );
	ULogEvent *GenEventSubmit( void );
	ULogEvent *GenEventTerminate( void );
	ULogEvent *GenEventExecute( void );
	ULogEvent *GenEventExecutableError( void );
	ULogEvent *GenEventCheckpoint( void );
	ULogEvent *GenEventJobEvicted( void );
	ULogEvent *GenEventImageSize( void );
	ULogEvent *GenEventShadowException( void );
	ULogEvent *GenEventJobAborted( void );
	ULogEvent *GenEventJobSuspended( void );
	ULogEvent *GenEventJobHeld( void );
	ULogEvent *GenEventJobReleased( void );
	ULogEvent *GenEventGeneric( void );

private:
	const Options	*m_opts;
	int				 m_cluster;
	int				 m_proc;
	int				 m_subproc;
	ULogEvent		*m_eventp;
	const char		*m_name;
	const char		*m_note;
	bool			 m_is_large;

	bool GenIsLarge( void );
	int GetSize( int mult = 4096 ) const;
};

class UserLogTest : public UserLog
{
public:
    UserLogTest(const Options &,
				const char *owner, const char *file,
				int clu, int proc, int subp,
				bool xml = XML_USERLOG_DEFAULT );
	virtual ~UserLogTest( void ) { };

	virtual bool globalRotationStarting( long size );
	virtual void globalRotationEvents( int events );
	virtual void globalRotationComplete( int, int, const MyString &  );

private:
	const Options	&m_opts;
	int				 m_rotations;
};

bool
CheckArgs(int argc, const char **argv, Options &opts);

bool // false == okay, true == error
ForkJobs( const Options &opts );

bool // false == okay, true == error
WriteEvents(Options &opts, UserLogTest &writer, int &num, int &sequence );

static const char *timestr( void );
static unsigned randint( unsigned maxval );

// Simple term signal handler
static bool	global_done = false;
void handle_sig(int /*sig*/)
{
	printf( "Got signal; shutting down\n" );
	global_done = true;
}

int
main(int argc, const char **argv)
{

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("test_log_writer");
	DebugFlags = D_ALWAYS;

	bool	error = false;
	Options	opts;

	error = CheckArgs(argc, argv, opts);
	if ( error ) {
		fprintf( stderr, "Command line error\n" );
		exit( 1 );
	}

	int		num_events = 0;
	int		sequence = 0;
	if ( opts.num_forks ) {
		error = ForkJobs( opts );
	}
	else {
		UserLogTest writer( opts,
							"owner", opts.logFile,
							opts.cluster, opts.proc, opts.subproc,
							opts.isXml );
		int		max_proc = opts.proc + opts.numProcs - 1;
		for( int proc = opts.proc; proc <= max_proc; proc++ ) {
			writer.setGlobalProc( proc );
			error = WriteEvents( opts, writer, num_events, sequence );
			if ( error || global_done ) {
				break;
			}
		}
	}

	if ( error  &&  (opts.verbosity >= VERB_ERROR) ) {
		fprintf(stderr, "test_log_writer FAILED\n");
	}
	else if ( opts.verbosity >= VERB_INFO ) {
		printf( "wrote %d events\n", num_events );
		printf( "global sequence %d\n", sequence );
	}

	return (int) error;
}

bool
CheckArgs(int argc, const char **argv, Options &opts)
{
	bool	status = false;

	const char *	usage =
		"Usage: test_log_writer [options] <filename>\n"
		"  --cluster <number>: Starting cluster %d (default = getpid())\n"
		"  --proc <number>: Starting proc %d (default = 0)\n"
		"  --subproc <number>: Starting subproc %d (default = 0)\n"
		"  --jobid <c.p.s>: combined -cluster, -proc, -subproc\n"
		"  --fork <number>: fork off <number> processes\n"
		"  --fork-cluster-step <number>: with --fork: step # of cluster #"
		" (default = 1000)\n"
		"\n"
		"  --num-exec <number>: number of execute events to write / proc\n"
		"  -n|--num-procs <num>: Number of procs (default:10) (-1:no limit)\n"
		"\n"
		"  --max-rotations <num>: stop after <number> rotations\n"
		"  --max-sequence <num>: stop when sequence <number> written\n"
		"  --max-global <num>: stop when global log size >= <num> bytes\n"
		"  --max-user <num>: stop when user log size >= <num> bytes\n"
		"    All of these default to '--num-exec 100000 --num-procs 1'\n"
		"\n"
		"  --generic <string>: Write generic event\n"
		"  -p|--persist <file>: persist writer state to file (for jobid gen)\n"
		"  --sleep <number>: how many seconds to sleep between events\n"
		"  --no-sleep: Don't sleep at all between events\n"
		"  --stork: simulate Stork (-1 for proc and subproc)\n"
		"  --random <percent>: gen other random events every <percent> times\n"
		"  --submit_note <string>: submit event note\n"
		"\n"
		"  -d|--debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  -q|quiet: quiet all messages\n"
		"  -v: increase verbose level by 1\n"
		"  --verbosity <number|name>: set verbosity level (default is ERROR)\n"
		"    names: NONE=0 ERROR WARNING INFO VERBOSE ALL\n"
		"  --version: print the version number and compile date\n"
		"  -h|--usage: print this message and exit\n"
		"\n"
		"  --xml: write the log in XML\n"
		"\n"
		"  <filename>: the log file to write to\n";

	opts.isXml				= false;
	opts.logFile			= NULL;
	opts.numExec			= 1;
	opts.cluster			= -1;
	opts.proc				= -1;
	opts.subproc			= -1;
	opts.numProcs			= 10;
	opts.sleep_seconds		= 5;
	opts.sleep_useconds		= 0;
	opts.stork				= false;
	opts.randomProb			= 0.0;
	opts.submitNote			= "";
	opts.verbosity			= VERB_ERROR;
	opts.genericEventStr	= NULL;
	opts.persistFile		= NULL;

	opts.maxRotations		= -1;	// disable max # of rotations limit
	opts.maxSequence		= -1;	// disable max sequence # limit
	opts.maxGlobalSize		= -1;	// disable max global log size limit
	opts.maxUserSize		= -1;	// disable max user log size limit

	opts.num_forks			= 0;
	opts.fork_cluster_step	= 1000;

	int			 argno = 1;
	while ( (argno < argc) & (status == 0) ) {
		SimpleArg	arg( argv, argc, argno );

		if ( arg.Error() ) {
			printf("%s", usage);
			status = true;
		}

		if ( arg.Match( "cluster") ) {
			if ( ! arg.getOpt(opts.cluster) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match('d', "debug") ) {
			if ( arg.hasOpt() ) {
				set_debug_flags( const_cast<char *>(arg.getOpt()) );
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match('j', "jobid") ) {
			if ( arg.hasOpt() ) {
				const char *opt = arg.getOpt();
				if ( *opt == '.' ) {
					sscanf( opt, ".%d.%d", &opts.proc, &opts.subproc );
				}
				else {
					sscanf( opt, "%d.%d.%d",
							&opts.cluster, &opts.proc, &opts.subproc );
				}
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("generic") ) {
			if ( !arg.getOpt(opts.genericEventStr) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match('n', "num-exec") ) {
			if ( ! arg.getOpt(opts.numExec) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("num-procs") ) {
			if ( ! arg.getOpt(opts.numProcs) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("proc") ) {
			if ( ! arg.getOpt(opts.proc) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("max-rotations") ) {
			if ( ! arg.getOpt(opts.maxRotations) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts.numExec = 100000;
				opts.numProcs = 1;
			}

		} else if ( arg.Match("max-global") ) {
			if ( ! arg.getOpt(opts.maxGlobalSize) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts.numExec = 100000;
				opts.numProcs = 1;
			}

		} else if ( arg.Match("max-user") ) {
			if ( ! arg.getOpt(opts.maxUserSize) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts.numExec = 100000;
				opts.numProcs = 1;
			}

		} else if ( arg.Match( 'p', "persist") ) {
			if ( !arg.getOpt(opts.persistFile) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("sleep") ) {
			double	sec;
			if ( arg.getOpt(sec) ) {
				opts.sleep_seconds  = (int) floor( sec );
				opts.sleep_useconds =
					(int) (1e6 * ( sec - opts.sleep_seconds ) );
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("no-sleep") ) {
			opts.sleep_seconds  = 0;
			opts.sleep_useconds = 0;

		} else if ( arg.Match( "stork") ) {
			opts.stork = true;

		} else if ( arg.Match("subproc") ) {
			if ( arg.getOpt(opts.subproc) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match('f', "fork") ) {
			if ( !arg.getOpt(opts.num_forks) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("fork-cluster-step") ) {
			if ( !arg.getOpt(opts.fork_cluster_step) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("random") ) {
			double	percent;
			if ( arg.getOpt(percent) ) {
				opts.randomProb = percent / 100.0;
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("submit_note") ) {
			if ( !arg.getOpt(opts.submitNote) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if( arg.Match( 'h', "usage") ) {
			printf("%s", usage);
			status = STATUS_CANCEL;

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
				else if ( !strcasecmp(s, "VERBOSE" ) ) {
					opts.verbosity = VERB_VERBOSE;
				}
				else if ( !strcasecmp(s, "ALL" ) ) {
					opts.verbosity = VERB_ALL;
				}
				else {
					fprintf(stderr, "Unknown %s '%s'\n", arg.Arg(), s );
					printf("%s", usage);
					status = true;
				}
			}
			else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match('v') ) {
			int		v = (int) opts.verbosity;
			opts.verbosity = (Verbosity) (v + 1);

		} else if ( arg.Match('q', "quiet" ) ) {
			opts.verbosity = VERB_NONE;

		} else if ( arg.Match("version") ) {
			printf("test_log_writer: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else if ( arg.Match("xml") ) {
			opts.isXml = true;

		} else if ( !arg.ArgIsOpt() ) {
			arg.getOpt( opts.logFile );

		} else {
			fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
			printf("%s", usage);
			status = true;
		}
		argno = arg.Index();
	}

	if ( status == STATUS_OK && opts.logFile == NULL ) {
		fprintf(stderr, "Log file must be specified\n");
		printf("%s", usage);
		status = true;
	}

	// Read the persisted file (if specified)
	if ( opts.persistFile ) {
		FILE	*fp = safe_fopen_wrapper( opts.persistFile, "r" );
		if ( fp ) {
			int		cluster, proc, subproc;
			if ( 3 == fscanf( fp, "%d.%d.%d", &cluster, &proc, &subproc ) ) {
				if ( opts.cluster < 0 ) opts.cluster = cluster;
				if ( opts.proc < 0 )    opts.proc    = proc;
				if ( opts.subproc < 0 ) opts.subproc = subproc;
			}
			fclose( fp );
		}
	}

	// Set defaults for the cluster, proc & subproc
	if ( opts.cluster < 0 ) opts.cluster = getpid();
	if ( opts.proc < 0 )    opts.proc    = 0;
	if ( opts.subproc < 0 ) opts.subproc = 0;

	// Stork sets these to -1
	if ( opts.stork ) {
		opts.proc = -1;
		opts.subproc = -1;
	}

	// Update the persisted file (if specified)
	if ( opts.persistFile ) {
		FILE	*fp = safe_fopen_wrapper( opts.persistFile, "w" );
		if ( fp ) {
			fprintf( fp, "%d.%d.%d", opts.cluster+1, opts.proc, opts.subproc );
			fclose( fp );
		}
	}

	return status;
}

void handle_sigchild(int /*sig*/ )
{
}

bool
ForkJobs( const Options & /*opts*/ )
{
	fprintf( stderr, "--fork: Not implemented\n" );
	return true;

#if 0
	signal( SIGCHLD, handle_sigchild );
	for( int num = 0;  num < opts.num_forks;  num++ ) {
		// TODO
	}
#endif
}

long
getUserLogSize( const Options &opts )
{
	if ( NULL == opts.logFile ) {
		return 0;
	}
	StatWrapper	swrap( opts.logFile );
	if ( swrap.Stat() ) {
		return -1L;			// What should we do here????
	}
	return swrap.GetBuf()->st_size;
}

bool
WriteEvents( Options &opts, UserLogTest &writer, int &events, int &sequence )
{
	bool		error = false;
	int			cluster = writer.getGlobalCluster();
	int			proc = writer.getGlobalProc();
	int			subproc = writer.getGlobalSubProc();

	EventInfo	event( opts, cluster, proc, subproc );

	signal( SIGTERM, handle_sig );
	signal( SIGQUIT, handle_sig );
	signal( SIGINT, handle_sig );

		//
		// Write the submit event.
		//
	event.GenEventSubmit( );
	error = event.WriteEvent( writer );
	event.Reset( );
	if ( !error ) {
		events++;
	}

		//
		// Write a single generic event
		//
	if ( opts.genericEventStr ) {
		event.GenEventGeneric( );
		if ( event.WriteEvent( writer ) ) {
			error = true;
		}
		else {
			events++;
		}
		event.Reset( );
	}

		//
		// Write execute events.
		//
	if ( opts.verbosity >= VERB_VERBOSE ) {
		printf( "Writing %d events for job %d.%d.%d\n",
				opts.numExec, cluster, proc, subproc );
	}
	for ( int exec = 0; ( (opts.numExec<0) || (exec<opts.numExec) ); ++exec ) {
		if ( global_done ) {
			break;
		}

		event.GenEvent( );
		if ( event.WriteEvent( writer ) ) {
			error = true;
		}
		else {
			events++;
			event.NextProc( );
		}
		event.Reset( );

		if ( ( opts.maxGlobalSize > 0 ) && 
			 ( writer.getGlobalLogSize() > opts.maxGlobalSize ) ) {
			printf( "Maximum global log size limit hit\n" );
			global_done = true;
		}

		if ( ( opts.maxUserSize > 0 ) && 
			 ( getUserLogSize(opts) > opts.maxUserSize ) ) {
			printf( "Maximum user log size limit hit\n" );
			global_done = true;
		}

		if ( opts.sleep_seconds ) {
			sleep( opts.sleep_seconds);
		}
		if ( opts.sleep_useconds ) {
			usleep( opts.sleep_useconds);
		}
	}

		//
		// Write the terminated event.
		//
	event.GenEventTerminate( );
	if ( event.WriteEvent( writer ) ) {
		error = true;
	}
	else {
		events++;
	}
	sequence = writer.getGlobalSequence( );

	return error;
}

static const char *
timestr( void )
{
	static char	tbuf[64];
	time_t now = time( NULL );
	strncpy( tbuf, ctime( &now ), sizeof(tbuf) );
	tbuf[sizeof(tbuf)-1] = '\0';
	if ( strlen(tbuf) ) {
		tbuf[strlen(tbuf)-1] = '\0';
	}
	return tbuf;
}

static unsigned
randint( unsigned maxval )
{
	return get_random_uint() % maxval;
}


// **********************
// Event Info class
// **********************
ULogEventNumber
EventInfo::GetEventNumber( void ) const
{
	if ( m_eventp == NULL )
		return (ULogEventNumber) -1;
	return m_eventp->eventNumber;
}

ULogEvent *
EventInfo::GenEvent( void )
{
	// Select the event type
	double	randval = get_random_float( );

	// Special case: execute event
	if ( randval > m_opts->randomProb ) {
		return GenEventExecute( );
	}

	ULogEvent	*eventp = NULL;
	while( NULL == eventp ) {
		unsigned	rand_event = randint(29);
		ULogEventNumber eventType = (ULogEventNumber) rand_event;

		eventp = GenEvent( eventType );
	}

	return eventp;
}

bool
EventInfo::WriteEvent( UserLog &log )
{
	if ( m_opts->verbosity >= VERB_ALL ) {
		printf("Writing %s event %s @ %s\n",
			   m_name, m_note ? m_note : "", timestr() );
	}

	if ( !log.writeEvent( m_eventp ) ) {
		if ( m_opts->verbosity >= VERB_ERROR ) {
			fprintf(stderr, "Error writing log event\n");
		}
		return true;
	}
	return false;
}

ULogEvent *
EventInfo::GenEvent( ULogEventNumber eventType )
{
	switch( eventType ) {

	case ULOG_SUBMIT:
		return NULL;

	case ULOG_EXECUTE:
		return GenEventExecute( );

	case ULOG_EXECUTABLE_ERROR:
		return GenEventExecutableError( );

	case ULOG_CHECKPOINTED:
		return GenEventCheckpoint( );

	case ULOG_JOB_EVICTED:
		return GenEventJobEvicted( );

	case ULOG_IMAGE_SIZE:
		return GenEventImageSize( );

	case ULOG_SHADOW_EXCEPTION:
		return GenEventShadowException( );

	case ULOG_JOB_ABORTED:
		return GenEventJobAborted( );

	case ULOG_JOB_SUSPENDED:
		return GenEventJobSuspended( );

	case ULOG_JOB_UNSUSPENDED:
		return GenEventBasic( "Unsuspended", new JobUnsuspendedEvent );

	case ULOG_JOB_HELD:
		return GenEventJobHeld( );

	case ULOG_JOB_RELEASED:
		return GenEventJobReleased( );

#  if 0
	case ULOG_JOB_DISCONNECTED:
		return GenEventBasic( "Job disconnected", new JobDisconnectedEvent );
#  endif

#  if 0
	case ULOG_JOB_RECONNECTED:
		return GenEventBasic( "Job reconnected", new JobReconnectedEvent );
#  endif

#  if 0
	case ULOG_JOB_RECONNECT_FAILED:
		return GenEventBasic( "Job reconnect failed",
							  new JobReconnectFailedEvent );
#  endif

	default:
		break;
	}	// switch

	return NULL;
}

ULogEvent *
EventInfo::GenEventBasic( const char *name, ULogEvent *event )
{
	SetName( name );
	return SetEvent( event );
}

ULogEvent *
EventInfo::GenEventSubmit( void )
{
	char	buf[128];
	SetName( "Submit" );

	snprintf( buf, sizeof(buf), "(%d.%d.%d)", m_cluster, m_proc, m_subproc );
	SetNote( buf );

	SubmitEvent	*e = new SubmitEvent;
	strcpy(e->submitHost, "<128.105.165.12:32779>");

		// Note: the line below is designed specifically to work with
		// Kent's dummy stork_submit script for testing DAGs with
		// DATA nodes.
	e->submitEventLogNotes  = strnewp(m_opts->submitNote);
	e->submitEventUserNotes = strnewp("User info");

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventExecute( void )
{
	SetName( "Execute" );
	SetNote( "<128.105.165.12:32779>" );

	ExecuteEvent	*e = new ExecuteEvent;
	strcpy(e->executeHost, GetNote() );

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventTerminate( void )
{
	char	buf[128];
	SetName( "Terminate" );

	JobTerminatedEvent *e = new JobTerminatedEvent;
	e->normal = true;
	e->returnValue = 0;
	e->signalNumber = 0;
	e->sent_bytes = GetSize( );
	e->recvd_bytes = GetSize( );
	e->total_sent_bytes = GetSize( );
	e->total_recvd_bytes = GetSize( );

	snprintf( buf, sizeof(buf), "(%d.%d.%d)", m_cluster, m_proc, m_subproc );
	SetNote( buf );

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventExecutableError( void )
{
	SetName( "Executable Error" );

	ExecutableErrorEvent *e = new ExecutableErrorEvent;
	e->errType = CONDOR_EVENT_BAD_LINK;

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventCheckpoint( void )
{
	SetName( "Checkpoint" );
	CheckpointedEvent *e = new CheckpointedEvent;
	e->sent_bytes = GetSize( );

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventJobEvicted( void )
{
	SetName( "Evicted" );
	JobEvictedEvent *e = new JobEvictedEvent;
	e->setReason("EVICT");
	e->setCoreFile("corefile");
	e->checkpointed = randint(10) > 8;
	e->sent_bytes = GetSize( );
	e->recvd_bytes = GetSize( );

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventImageSize( void )
{
	SetName( "Job Image Size" );
	JobImageSizeEvent *e = new JobImageSizeEvent;
	e->size = GetSize( );

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventShadowException( void )
{
	SetName( "Shadow Exception" );
	ShadowExceptionEvent *e = new ShadowExceptionEvent;
	e->sent_bytes = GetSize( );
	e->recvd_bytes = GetSize( );
	e->message[0] = '\0';
	strncat(e->message,"EXCEPTION", 15);

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventJobAborted( void )
{
	SetName( "Job aborted" );
	JobAbortedEvent *e = new JobAbortedEvent;
	e->setReason("ABORT");

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventJobSuspended( void )
{
	SetName( "Suspended" );
	JobSuspendedEvent *e = new JobSuspendedEvent;
	e->num_pids = randint(99);

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventJobHeld( void )
{
	SetName( "Job held" );
	JobHeldEvent *e = new JobHeldEvent;
	e->setReason("HELD");
	e->setReasonCode(404);
	e->setReasonSubCode(0xff);

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventJobReleased( void )
{
	SetName( "Job released" );
	JobReleasedEvent *e = new JobReleasedEvent;
	e->setReason("RELEASED");

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventGeneric( void )
{
	SetName( "Generic" );

	GenericEvent	*e = new GenericEvent;
	strncpy(e->info, m_opts->genericEventStr, sizeof(e->info) );
	e->info[sizeof(e->info)-1] = '\0';

	return SetEvent( e );
}

bool
EventInfo::GenIsLarge( void )
{
	m_is_large = (get_random_float() >= 0.8);
	return m_is_large;
}

int
EventInfo::GetSize( int mult ) const
{
	if ( m_is_large ) {
		return randint( mult*1024 );
	}
	else {
		return randint( mult );
	}
}


// **************************
//  Rotating user log class
// **************************
UserLogTest::UserLogTest(const Options &opts,
						 const char *owner, const char *file,
						 int clu, int proc, int subp, bool xml ) 
		: UserLog( owner, file, clu, proc, subp, xml ),
		  m_opts( opts ),
		  m_rotations( 0 )
{
	// Do nothing
}

bool
UserLogTest::globalRotationStarting( long filesize )
{
	if ( m_opts.verbosity >= VERB_INFO ) {
		printf( "rotation starting, file size is %ld\n", filesize );
	}
	return true;
}

void
UserLogTest::globalRotationEvents( int events )
{
	if ( m_opts.verbosity >= VERB_INFO ) {
		printf( "Rotating: %d events counted\n", events );
	}
}

void
UserLogTest::globalRotationComplete( int num_rotations,
									 int sequence,
									 const MyString & /*id*/ )
{
	m_rotations++;
	if ( m_opts.verbosity >= VERB_INFO ) {
		printf( "rotation complete: %d %d %d\n",
				m_rotations, num_rotations, sequence );
	}
	if ( ( m_opts.maxRotations >= 0 ) &&
		 ( m_rotations >= m_opts.maxRotations ) ) {
		printf( "Max # of rotations hit: shutting down\n" );
		global_done = true;
	}
	if ( ( m_opts.maxSequence >= 0 ) &&
		 ( sequence >= m_opts.maxSequence ) ) {
		printf( "Max sequence # hit: shutting down\n" );
		global_done = true;
	}
}
