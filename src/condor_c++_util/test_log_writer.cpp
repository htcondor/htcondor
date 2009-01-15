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
#include "read_user_log.h"
#include "user_log_header.h"
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

class Options
{
public:
	Options( void );
	~Options( void ) { };

	bool			m_isXml;
	const char *	m_logFile;
	int				m_numExec;
	int				m_sleep_seconds;
	int				m_sleep_useconds;
	int				m_cluster;
	int				m_proc;
	int				m_subproc;
	int				m_numProcs;
	double			m_randomProb;		// Probability of 'random' events
	bool			m_stork;
	const char *	m_submitNote;
	Verbosity		m_verbosity;
	const char *	m_genericEventStr;
	const char *    m_persistFile;

	int				m_maxRotations;		// # of rotations limit
	bool			m_maxRotationStop;	// Stop max rotation from happening?
	int				m_maxSequence;		// Sequence # limit
	long			m_maxGlobalSize;	// Limit max size of global file
	long			m_maxUserSize;		// Limit max size of user log file

	int				m_num_forks;
	int				m_fork_cluster_step;
};

class EventInfo
{
public:
	EventInfo( const Options &opts, int cluster, int proc, int subproc )
			: m_opts( opts ),
			  m_cluster( cluster ),
			  m_proc( proc ),
			  m_subproc( subproc ),
			  m_eventp( NULL ),
			  m_name( NULL ),
			  m_note( NULL ),
			  m_is_large( false )
		{
		};
	~EventInfo( void ) {
		Reset( );
	};
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

	int NextCluster( void ) { return ++m_cluster; };
	int NextProc( void ) { return ++m_proc; };
	int NextSubProc( void ) { return ++m_subproc; };

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
	const Options	&m_opts;
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
	if ( opts.m_num_forks ) {
		error = ForkJobs( opts );
	}
	else {
		UserLogTest writer( opts,
							"owner", opts.m_logFile,
							opts.m_cluster, opts.m_proc, opts.m_subproc,
							opts.m_isXml );
		int		max_proc = opts.m_proc + opts.m_numProcs - 1;
		for( int proc = opts.m_proc; proc <= max_proc; proc++ ) {
			writer.setGlobalProc( proc );
			error = WriteEvents( opts, writer, num_events, sequence );
			if ( error || global_done ) {
				break;
			}
		}
	}

	if ( error  &&  (opts.m_verbosity >= VERB_ERROR) ) {
		fprintf(stderr, "test_log_writer FAILED\n");
	}
	else if ( opts.m_verbosity >= VERB_INFO ) {
		printf( "wrote %d events\n", num_events );
		printf( "global sequence %d\n", sequence );
	}

	return (int) error;
}

Options::Options( void )
{
	m_isXml				= false;
	m_logFile			= NULL;
	m_numExec			= 1;
	m_cluster			= -1;
	m_proc				= -1;
	m_subproc			= -1;
	m_numProcs			= 10;
	m_sleep_seconds		= 5;
	m_sleep_useconds	= 0;
	m_stork				= false;
	m_randomProb		= 0.0;
	m_submitNote		= "";
	m_verbosity			= VERB_ERROR;
	m_genericEventStr	= NULL;
	m_persistFile		= NULL;

	m_maxRotations		= -1;		// disable max # of rotations limit
	m_maxRotationStop	= false;	// disable max rotation stop
	m_maxSequence		= -1;		// disable max sequence # limit
	m_maxGlobalSize		= -1;		// disable max global log size limit
	m_maxUserSize		= -1;		// disable max user log size limit

	m_num_forks			= 0;
	m_fork_cluster_step	= 1000;
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
		"  --max-rotation-stop: prevent final rotation (default:off)\n"
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

	int			 argno = 1;
	while ( (argno < argc) & (status == 0) ) {
		SimpleArg	arg( argv, argc, argno );

		if ( arg.Error() ) {
			printf("%s", usage);
			status = true;
		}

		if ( arg.Match( "cluster") ) {
			if ( ! arg.getOpt(opts.m_cluster) ) {
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
					sscanf( opt, ".%d.%d", &opts.m_proc, &opts.m_subproc );
				}
				else {
					sscanf( opt, "%d.%d.%d",
							&opts.m_cluster, &opts.m_proc, &opts.m_subproc );
				}
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("generic") ) {
			if ( !arg.getOpt(opts.m_genericEventStr) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match('n', "num-exec") ) {
			if ( ! arg.getOpt(opts.m_numExec) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("num-procs") ) {
			if ( ! arg.getOpt(opts.m_numProcs) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("proc") ) {
			if ( ! arg.getOpt(opts.m_proc) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("max-rotations") ) {
			if ( ! arg.getOpt(opts.m_maxRotations) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts.m_numExec = 100000;
				opts.m_numProcs = 1;
			}

		} else if ( arg.Match("max-rotation-stop") ) {
			opts.m_maxRotationStop = true;

		} else if ( arg.Match("max-global") ) {
			if ( ! arg.getOpt(opts.m_maxGlobalSize) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts.m_numExec = 100000;
				opts.m_numProcs = 1;
			}

		} else if ( arg.Match("max-user") ) {
			if ( ! arg.getOpt(opts.m_maxUserSize) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts.m_numExec = 100000;
				opts.m_numProcs = 1;
			}

		} else if ( arg.Match( 'p', "persist") ) {
			if ( !arg.getOpt(opts.m_persistFile) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("sleep") ) {
			double	sec;
			if ( arg.getOpt(sec) ) {
				opts.m_sleep_seconds  = (int) floor( sec );
				opts.m_sleep_useconds =
					(int) (1e6 * ( sec - opts.m_sleep_seconds ) );
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("no-sleep") ) {
			opts.m_sleep_seconds  = 0;
			opts.m_sleep_useconds = 0;

		} else if ( arg.Match( "stork") ) {
			opts.m_stork = true;

		} else if ( arg.Match("subproc") ) {
			if ( arg.getOpt(opts.m_subproc) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match('f', "fork") ) {
			if ( !arg.getOpt(opts.m_num_forks) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("fork-cluster-step") ) {
			if ( !arg.getOpt(opts.m_fork_cluster_step) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("random") ) {
			double	percent;
			if ( arg.getOpt(percent) ) {
				opts.m_randomProb = percent / 100.0;
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		} else if ( arg.Match("submit_note") ) {
			if ( !arg.getOpt(opts.m_submitNote) ) {
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
				opts.m_verbosity = (Verbosity) verb;
			}
			else if ( arg.hasOpt() ) {
				const char	*s;
				arg.getOpt( s );
				if ( !strcasecmp(s, "NONE" ) ) {
					opts.m_verbosity = VERB_NONE;
				}
				else if ( !strcasecmp(s, "ERROR" ) ) {
					opts.m_verbosity = VERB_ERROR;
				}
				else if ( !strcasecmp(s, "WARNING" ) ) {
					opts.m_verbosity = VERB_WARNING;
				}
				else if ( !strcasecmp(s, "INFO" ) ) {
					opts.m_verbosity = VERB_INFO;
				}
				else if ( !strcasecmp(s, "VERBOSE" ) ) {
					opts.m_verbosity = VERB_VERBOSE;
				}
				else if ( !strcasecmp(s, "ALL" ) ) {
					opts.m_verbosity = VERB_ALL;
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
			int		v = (int) opts.m_verbosity;
			opts.m_verbosity = (Verbosity) (v + 1);

		} else if ( arg.Match('q', "quiet" ) ) {
			opts.m_verbosity = VERB_NONE;

		} else if ( arg.Match("version") ) {
			printf("test_log_writer: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else if ( arg.Match("xml") ) {
			opts.m_isXml = true;

		} else if ( !arg.ArgIsOpt() ) {
			arg.getOpt( opts.m_logFile );

		} else {
			fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
			printf("%s", usage);
			status = true;
		}
		argno = arg.Index();
	}

	if ( status == STATUS_OK && opts.m_logFile == NULL ) {
		fprintf(stderr, "Log file must be specified\n");
		printf("%s", usage);
		status = true;
	}

	// Read the persisted file (if specified)
	if ( opts.m_persistFile ) {
		FILE	*fp = safe_fopen_wrapper( opts.m_persistFile, "r" );
		if ( fp ) {
			int		cluster, proc, subproc;
			if ( 3 == fscanf( fp, "%d.%d.%d", &cluster, &proc, &subproc ) ) {
				if ( opts.m_cluster < 0 ) opts.m_cluster = cluster;
				if ( opts.m_proc < 0 )    opts.m_proc    = proc;
				if ( opts.m_subproc < 0 ) opts.m_subproc = subproc;
			}
			fclose( fp );
		}
	}

	// Set defaults for the cluster, proc & subproc
	if ( opts.m_cluster < 0 ) opts.m_cluster = getpid();
	if ( opts.m_proc < 0 )    opts.m_proc    = 0;
	if ( opts.m_subproc < 0 ) opts.m_subproc = 0;

	// Stork sets these to -1
	if ( opts.m_stork ) {
		opts.m_proc = -1;
		opts.m_subproc = -1;
	}

	// Update the persisted file (if specified)
	if ( opts.m_persistFile ) {
		FILE	*fp = safe_fopen_wrapper( opts.m_persistFile, "w" );
		if ( fp ) {
			fprintf( fp, "%d.%d.%d",
					 opts.m_cluster+1, opts.m_proc, opts.m_subproc );
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
	for( int num = 0;  num < opts.m_num_forks;  num++ ) {
		// TODO
	}
#endif
}

long
getUserLogSize( const Options &opts )
{
	if ( NULL == opts.m_logFile ) {
		return 0;
	}
	StatWrapper	swrap( opts.m_logFile );
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
	if ( opts.m_genericEventStr ) {
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
	if ( opts.m_verbosity >= VERB_VERBOSE ) {
		printf( "Writing %d events for job %d.%d.%d\n",
				opts.m_numExec, cluster, proc, subproc );
	}
	for ( int exec = 0; ( (opts.m_numExec<0) || (exec<opts.m_numExec) ); ++exec ) {
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

		if ( ( opts.m_maxGlobalSize > 0 ) && 
			 ( writer.getGlobalLogSize() > opts.m_maxGlobalSize ) ) {
			printf( "Maximum global log size limit hit\n" );
			global_done = true;
		}

		if ( ( opts.m_maxUserSize > 0 ) && 
			 ( getUserLogSize(opts) > opts.m_maxUserSize ) ) {
			printf( "Maximum user log size limit hit\n" );
			global_done = true;
		}

		if ( opts.m_sleep_seconds ) {
			sleep( opts.m_sleep_seconds);
		}
		if ( opts.m_sleep_useconds ) {
			usleep( opts.m_sleep_useconds);
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

	// If no rotations occurred, the writer did no rotations, and doesn't
	// know it's rotation #
	if ( sequence == 0 ) {
		const char			*path = writer.getGlobalPath();
		ReadUserLogHeader	header_reader;
		ReadUserLog			log_reader;
		printf( "Trying to get sequence # from header\n" );
		if ( !log_reader.initialize( path, false, false, true ) ) {
			fprintf( stderr, "Error reading eventlog header (initialize)\n" );
			error = true;
		}
		else if ( header_reader.Read( log_reader ) != ULOG_OK ) {
			fprintf( stderr, "Error reading header eventlog header\n" );
			error = true;
		}
		else {
			sequence = header_reader.getSequence( );
			printf( "Got %d from header\n", sequence );
		}
	}

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
	if ( randval > m_opts.m_randomProb ) {
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
	if ( m_opts.m_verbosity >= VERB_ALL ) {
		printf("Writing %s event %s @ %s\n",
			   m_name, m_note ? m_note : "", timestr() );
	}

	if ( !log.writeEvent( m_eventp ) ) {
		if ( m_opts.m_verbosity >= VERB_ERROR ) {
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
	e->submitEventLogNotes  = strnewp(m_opts.m_submitNote);
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
	strncpy(e->info, m_opts.m_genericEventStr, sizeof(e->info) );
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
	int rotations = m_rotations + 1;

	if ( m_opts.m_verbosity >= VERB_INFO ) {
		printf( "rotation %d starting, file size is %ld\n",
				rotations, filesize );
	}

	if ( ( m_opts.m_maxRotations >= 0 ) &&
		 ( rotations >= m_opts.m_maxRotations ) ) {
		printf( "Max # of rotations hit: shutting down\n" );
		global_done = true;
		if ( m_opts.m_maxRotationStop ) {
			return false;
		}
	}

	m_rotations++;
	return true;
}

void
UserLogTest::globalRotationEvents( int events )
{
	if ( m_opts.m_verbosity >= VERB_INFO ) {
		printf( "Rotating: %d events counted\n", events );
	}
}

void
UserLogTest::globalRotationComplete( int num_rotations,
									 int sequence,
									 const MyString & /*id*/ )
{
	if ( m_opts.m_verbosity >= VERB_INFO ) {
		printf( "rotation complete: %d %d\n",
				num_rotations, sequence );
	}
	if ( ( m_opts.m_maxSequence >= 0 ) &&
		 ( sequence >= m_opts.m_maxSequence ) ) {
		printf( "Max sequence # hit: shutting down\n" );
		global_done = true;
	}
}
