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
#include "simple_arg.h"
#include "stat_wrapper.h"
#include "read_user_log.h"
#include "user_log_header.h"
#include "condor_string.h"
#include "my_username.h"
#include <stdio.h>
#if defined(UNIX)
# include <unistd.h>
# define ENABLE_WORKERS
#endif
#include <math.h>
#include <vector>
#include <list>

#ifdef WIN32
# define usleep(_x_) Sleep((_x_)/1000)
#endif

static const char *	VERSION = "1.0.0";

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

enum Verbosity {
	VERB_NONE = 0,
	VERB_ERROR,
	VERB_WARNING,
	VERB_INFO,
	VERB_VERBOSE,
	VERB_ALL
};

class SharedOptions
{
public:
	SharedOptions( void );
	~SharedOptions( void );

public:
	const char *getName( void ) const { return m_name; };
	bool getXml( void ) const { return m_isXml; };
	double getRandomProb( void ) const { return m_randomProb; };
	Verbosity getVerbosity( void ) const { return m_verbosity; };
	bool Verbose( Verbosity v ) const { return (m_verbosity >= v); };

public:
	const char	*m_name;
	bool		 m_isXml;
	double		 m_randomProb;		// Probability of 'random' events
	Verbosity	 m_verbosity;
};

class WorkerOptions
{
public:
	WorkerOptions( const SharedOptions & );
	WorkerOptions( const SharedOptions &, const WorkerOptions & );
	~WorkerOptions( void );

public:
	const SharedOptions	&getShared( void ) const { return m_shared; };
	const char *getName( void ) const { return m_shared.getName(); };
	bool getXml( void ) const { return m_shared.getXml(); };
	double getRandomProb( void ) const {
		return m_shared.getRandomProb();
	};
	Verbosity getVerbosity( void ) const { return m_shared.getVerbosity(); };
	bool Verbose( Verbosity v ) const { return m_shared.Verbose(v); };

	const char *getLogFile( void ) const { return m_logFile; };
	int getNumExec( void ) const { return m_numExec; };
	int getSleepSeconds( void ) const { return m_sleep_seconds; };
	int getSleepUseconds( void ) const { return m_sleep_useconds; };

	int getCluster( void ) const {
		return m_cluster >= 0 ? m_cluster : getpid();
	};
	int getProc( void ) const {
		return m_proc >= 0 ? m_proc : 0;
	};
	int getSubProc( void ) const {
		return m_subProc >= 0 ? m_subProc : 0;
	};
	int getNumProcs( void ) const { return m_numProcs; };

	const char *getSubmitNote( void ) const { return m_submitNote; };
	const char *getGenericEventStr( void ) const { return m_genericEventStr; };
	const char *getPersistFile( void ) const { return m_persistFile; };
						
	int  getMaxRotations( void ) const { return m_maxRotations; };
	bool getMaxRotationStop( void ) const { return m_maxRotationStop; };
	int  getMaxSequence( void ) const { return m_maxSequence; };
	long getMaxGlobalSize( void ) const { return m_maxGlobalSize; };
	long getMaxUserSize( void ) const { return m_maxUserSize; };

public:
	const SharedOptions	&m_shared;
	const char			*m_logFile;
	int					 m_sleep_seconds;
	int					 m_sleep_useconds;
	int					 m_numExec;
	int					 m_cluster;
	int					 m_proc;
	int					 m_subProc;
	int					 m_numProcs;
	const char			*m_submitNote;
	const char			*m_genericEventStr;
	const char			*m_persistFile;
						
	int					 m_maxRotations;	// # of rotations limit
	bool				 m_maxRotationStop;	// Stop max rots from happening?
	int					 m_maxSequence;		// Sequence # limit
	long				 m_maxGlobalSize;	// Limit max size of global file
	long				 m_maxUserSize;		// Limit max size of user log file
};

class GlobalOptions
{
public:
	GlobalOptions( void );
	~GlobalOptions( void );

	bool parseArgs( int argc, const char *argv[] );

	const WorkerOptions *getWorkerOpts( unsigned num ) const {
		if ( num >= m_workerOptions.size() ) {
			return NULL;
		}
		return m_workerOptions[num];
	};
	const SharedOptions &getSharedOpts( void ) const {
		return m_shared;
	};
	int getNumWorkers( void ) const {
		return m_workerOptions.size();
	};
	const char *getName( void ) const { return m_shared.getName(); };
	bool getXml( void ) const { return m_shared.getXml(); };
	double getRandomProb( void ) const {
		return m_shared.getRandomProb();
	};
	Verbosity getVerbosity( void ) const { return m_shared.getVerbosity(); };
	bool Verbose( Verbosity v ) const { return m_shared.Verbose(v); };

private:
	std::vector<WorkerOptions *>	 m_workerOptions;	// List of worker's options
	SharedOptions			 m_shared;
};

class Worker
{
public:
	Worker( const WorkerOptions &, int num );
	~Worker( void );

	void setPid( int pid ) {
		m_pid = pid;
		if ( pid > 0 ) {
			m_alive = true;
		}
	};
	void setStatus( int status ) {
		m_alive = false;
		m_status = status;
	};
	bool Kill( int signum ) const;

	const WorkerOptions &getOptions( void ) const { return m_options; };
	int getNum( void ) const { return m_workerNum; };
	bool isAlive( void ) const { return m_alive; };
	int getPid( void ) const { return m_pid; };
	int getStatus( void ) const { return m_status; };

private:
	const WorkerOptions	&m_options;
	int					 m_workerNum;
	bool				 m_alive;
	int					 m_pid;
	int					 m_status;
};

class Workers
{
public:
	Workers( const GlobalOptions &options );
	~Workers( void );

	Worker *createWorkers( void );
	Worker *findWorker( pid_t pid );
	Worker *getWorker( unsigned num );
	bool waitForWorkers( int max_seconds );
	bool reapChild( pid_t pid, int status );
	bool signalWorkers( int signum );

	int numErrors( void ) const { return m_errors; };
	int numChildren( void ) const { return m_runningChildren; };
	int numWorkers( void ) const { return m_workers.size(); };

private:
	const GlobalOptions	&m_options;
	std::vector<Worker *>	 m_workers;
	int					 m_runningChildren;
	int					 m_errors;
};

class TestLogWriter : public WriteUserLog
{
public:
	TestLogWriter( Worker &worker, const WorkerOptions &m_options );
	~TestLogWriter( void ) { };

	bool globalRotationStarting( unsigned long size );
	void globalRotationEvents( int events );
	void globalRotationComplete( int, int, const std::string &  );

	bool WriteEvents( int &num, int &sequence );
	long getUserLogSize( void );

private:
	const WorkerOptions	&m_options;
	int					 m_rotations;
};

class EventInfo
{
public:
	EventInfo( const WorkerOptions &opts, int cluster, int proc, int subproc )
			: m_options( opts ),
			  m_cluster( cluster ),
			  m_proc( proc ),
			  m_subProc( subproc ),
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
	ULogEvent *GenEventRandom( void );

	bool WriteEvent( TestLogWriter &log );

	int NextCluster( void ) { return ++m_cluster; };
	int NextProc( void ) { return ++m_proc; };
	int NextSubProc( void ) { return ++m_subProc; };

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
	const WorkerOptions	&m_options;
	int					 m_cluster;
	int					 m_proc;
	int					 m_subProc;
	ULogEvent			*m_eventp;
	const char			*m_name;
	const char			*m_note;
	bool				 m_is_large;

	bool GenIsLarge( void );
	int GetSize( int mult = 4096 ) const;
};

static const char *timestr( void );
static unsigned randint( unsigned maxval );

// Simple term signal handler
static bool	global_done = false;
static Workers *global_workers = NULL;
void handle_sig(int sig)
{
	printf( "%d Got signal; shutting down\n", getpid() );
	if ( global_workers ) {
		global_workers->signalWorkers( sig );
	}
	global_done = true;
}

int
main(int argc, const char **argv)
{
	set_mySubSystem( "TEST_LOG_WRITER", false, SUBSYSTEM_TYPE_TOOL );

		// initialize to read from config file
	config();

		// Set up the dprintf stuff...
	dprintf_set_tool_debug("test_log_writer", 0);
	set_debug_flags(NULL, D_ALWAYS);

	bool			error = false;
	GlobalOptions	opts;

	error = opts.parseArgs( argc, argv );
	if ( error ) {
		fprintf( stderr, "Command line error\n" );
		exit( 1 );
	}

# if defined(UNIX)
	signal( SIGTERM, handle_sig );
	signal( SIGQUIT, handle_sig );
	signal( SIGINT, handle_sig );
# endif

	int			 num_events = 0;
	int			 sequence = 0;
	Workers		*workers;

	workers = new Workers( opts );

	Worker *worker = workers->createWorkers( );
	if ( error ) {
		fprintf( stderr, "Failed to create workers\n" );
		exit( 1 );
	}

	if ( worker ) {
		if ( workers->numChildren() ) {
			delete workers;
			workers = NULL;
		}

		const WorkerOptions	&wopts = worker->getOptions();
		TestLogWriter writer( *worker, wopts );
		int		max_proc = wopts.getProc() + wopts.getNumProcs() - 1;
		for( int proc = wopts.getProc();
			 ( (wopts.getNumProcs() < 0) || (proc <= max_proc) ); proc++ ) {
			writer.setGlobalProc( proc );
			error = writer.WriteEvents( num_events, sequence );
			if ( error || global_done ) {
				break;
			}
		}
	}

	if ( workers && workers->numChildren() ) {
		printf( "About to wait for workers\n" );
		global_workers = workers;
		error = workers->waitForWorkers( 0 );

		if ( workers->numErrors() ) {
			error = true;
		}
		global_workers = NULL;
		delete workers;
	}

	if ( error  &&  (opts.Verbose(VERB_ERROR) ) ) {
		fprintf(stderr, "test_log_writer FAILED\n");
	}
	else if ( opts.Verbose(VERB_INFO) ) {
		printf( "wrote %d events\n", num_events );
		printf( "global sequence %d\n", sequence );
	}

	return (int) error;
}


// *******************************
// **** SharedOptions methods ****
// *******************************
SharedOptions::SharedOptions( void )
{
	m_name				= NULL;
	m_isXml				= false;
	m_randomProb		= 0.0;
	m_verbosity			= VERB_ERROR;
}

SharedOptions::~SharedOptions( void )
{
}


// *******************************
// **** WorkerOptions methods ****
// *******************************
WorkerOptions::WorkerOptions( const SharedOptions &shared )
		: m_shared( shared ),
		  m_logFile( NULL ),
		  m_sleep_seconds( 5 ),
		  m_sleep_useconds( 0 ),
		  m_numExec( 1 ),
		  m_cluster( -1 ),
		  m_proc( -1 ),
		  m_subProc( -1 ),
		  m_numProcs( 10 ),
		  m_submitNote( "" ),
		  m_genericEventStr( NULL ),
		  m_persistFile( NULL ),

		  m_maxRotations( -1 ),			// disable max # of rotations limit
		  m_maxRotationStop( false ),	// disable max rotation stop
		  m_maxSequence( -1 ),			// disable max sequence # limit
		  m_maxGlobalSize( -1 ),		// disable max global log size limit
		  m_maxUserSize( -1 )			// disable max user log size limit
{
}

WorkerOptions::WorkerOptions( const SharedOptions &shared,
							  const WorkerOptions &other )
		: m_shared( shared ),
		  m_logFile( other.m_logFile ),
		  m_sleep_seconds( other.m_sleep_seconds ),
		  m_sleep_useconds( other.m_sleep_useconds ),
		  m_numExec( other.m_numExec ),
		  m_cluster( other.m_cluster ),
		  m_proc( other.m_proc ),
		  m_subProc( other.m_subProc ),
		  m_numProcs( other.m_numProcs ),
		  m_submitNote( other.m_submitNote ),
		  m_genericEventStr( other.m_genericEventStr ),
		  m_persistFile( other.m_persistFile ),

		  m_maxRotations( other.m_maxRotations ),
		  m_maxRotationStop( other.m_maxRotationStop ),
		  m_maxSequence( other.m_maxSequence ),
		  m_maxGlobalSize( other.m_maxGlobalSize ),
 		  m_maxUserSize( other.m_maxUserSize )
{
}

WorkerOptions::~WorkerOptions( void )
{
}


// *******************************
// **** GlobalOptions methods ****
// *******************************
GlobalOptions::GlobalOptions( void )
{
}

GlobalOptions::~GlobalOptions( void )
{
	for( unsigned num = 0;  num < m_workerOptions.size();  num++ ) {
		delete m_workerOptions[num];
	}
}

bool
GlobalOptions::parseArgs( int argc, const char **argv )
{
	bool	status = false;

	const char *	usage =
		"Usage: test_log_writer [options] <filename>\n"
# if defined(ENABLE_WORKERS)
		"  -w|--worker: Specify options are for next worker"
		" (default = global)\n"
# endif
		"  --cluster <number>: Starting cluster %d (default = getpid())\n"
		"  --proc <number>: Starting proc %d (default = 0)\n"
		"  --subproc <number>: Starting subproc %d (default = 0)\n"
		"  --jobid <c.p.s>: combined -cluster, -proc, -subproc\n"
# if defined(ENABLE_WORKERS)
		"  --fork <number>: fork off <number> processes\n"
		"  --fork-cluster-step <number>: with --fork: step # of cluster #"
		" (default = 1000)\n"
# endif
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
		"  --name <name>: Set creator name\n"
		"\n"
		"  <filename>: the log file to write to\n";

	WorkerOptions	*opts = new WorkerOptions( m_shared );
	m_workerOptions.push_back( opts );

	int			 argno = 1;
	while ( (argno < argc) & (status == 0) ) {
		SimpleArg	arg( argv, argc, argno );

		if ( arg.Error() ) {
			printf("%s", usage);
			status = true;
		}


		if ( arg.Match('d', "debug") ) {
			if ( arg.hasOpt() ) {
				const char	*flags;
				arg.getOpt( flags );
				set_debug_flags( const_cast<char *>(flags), 0 );
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
		}
		else if ( arg.Match( "cluster") ) {
			if ( ! arg.getOpt(opts->m_cluster) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
		}
# if defined(ENABLE_WORKERS)
		else if ( arg.Match('w', "worker") ) {	
			opts = new WorkerOptions( m_shared, *opts );
			m_workerOptions.push_back( opts );
			printf( "Created worker option: %zu\n", m_workerOptions.size() );
		}
# endif
		else if ( arg.Match('j', "jobid") ) {
			if ( arg.hasOpt() ) {
				const char *opt = arg.getOpt();
				if ( *opt == '.' ) {
					sscanf( opt, ".%d.%d", &opts->m_proc, &opts->m_subProc );
				}
				else {
					sscanf( opt, "%d.%d.%d",
							&opts->m_cluster,
							&opts->m_proc,
							&opts->m_subProc );
				}
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("generic") ) {
			if ( !arg.getOpt(opts->m_genericEventStr) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("num-exec") ) {
			if ( ! arg.getOpt(opts->m_numExec) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match('n', "num-procs") ) {
			if ( ! arg.getOpt(opts->m_numProcs) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("proc") ) {
			if ( ! arg.getOpt(opts->m_proc) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("max-rotations") ) {
			if ( ! arg.getOpt(opts->m_maxRotations) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts->m_numExec = 100000;
				opts->m_numProcs = 1;
			}

		}
		else if ( arg.Match("max-rotation-stop") ) {
			opts->m_maxRotationStop = true;

		}
		else if ( arg.Match("max-global") ) {
			if ( ! arg.getOpt(opts->m_maxGlobalSize) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts->m_numExec = 100000;
				opts->m_numProcs = 1;
			}

		}
		else if ( arg.Match("max-user") ) {
			if ( ! arg.getOpt(opts->m_maxUserSize) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
			else {
				opts->m_numExec = 100000;
				opts->m_numProcs = 1;
			}

		}
		else if ( arg.Match( 'p', "persist") ) {
			if ( !arg.getOpt(opts->m_persistFile) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("sleep") ) {
			double	sec;
			if ( arg.getOpt(sec) ) {
				opts->m_sleep_seconds  = (int) floor( sec );
				opts->m_sleep_useconds =
					(int) (1e6 * ( sec - opts->m_sleep_seconds ) );
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("no-sleep") ) {
			opts->m_sleep_seconds  = 0;
			opts->m_sleep_useconds = 0;

		}
		else if ( arg.Match("subproc") ) {
			if ( arg.getOpt(opts->m_subProc) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("random") ) {
			double	percent;
			if ( arg.getOpt(percent) ) {
				m_shared.m_randomProb = percent / 100.0;
			} else {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if ( arg.Match("submit_note") ) {
			if ( !arg.getOpt(opts->m_submitNote) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}

		}
		else if( arg.Match( 'h', "usage") ) {
			printf("%s", usage);
			status = STATUS_CANCEL;

		}
		else if ( arg.Match("verbosity") ) {
			if ( arg.isOptInt() ) {
				int		verb;
				arg.getOpt(verb);
				m_shared.m_verbosity = (Verbosity) verb;
			}
			else if ( arg.hasOpt() ) {
				const char	*s;
				arg.getOpt( s );
				if ( !strcasecmp(s, "NONE" ) ) {
					m_shared.m_verbosity = VERB_NONE;
				}
				else if ( !strcasecmp(s, "ERROR" ) ) {
					m_shared.m_verbosity = VERB_ERROR;
				}
				else if ( !strcasecmp(s, "WARNING" ) ) {
					m_shared.m_verbosity = VERB_WARNING;
				}
				else if ( !strcasecmp(s, "INFO" ) ) {
					m_shared.m_verbosity = VERB_INFO;
				}
				else if ( !strcasecmp(s, "VERBOSE" ) ) {
					m_shared.m_verbosity = VERB_VERBOSE;
				}
				else if ( !strcasecmp(s, "ALL" ) ) {
					m_shared.m_verbosity = VERB_ALL;
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

		}
		else if ( arg.Match('v') ) {
			int		v = (int) m_shared.m_verbosity;
			m_shared.m_verbosity = (Verbosity) (v + 1);

		}
		else if ( arg.Match('q', "quiet" ) ) {
			m_shared.m_verbosity = VERB_NONE;

		}
		else if ( arg.Match("version") ) {
			printf("test_log_writer: %s\n", VERSION);
			status = STATUS_CANCEL;

		}
		else if ( arg.Match("name") ) {
			if ( !arg.getOpt(m_shared.m_name) ) {
				fprintf(stderr, "Value needed for '%s'\n", arg.Arg() );
				printf("%s", usage);
				status = true;
			}
		}
		else if ( arg.Match("xml") ) {
			m_shared.m_isXml = true;

		}
		else if ( !arg.ArgIsOpt() ) {
			arg.getOpt( opts->m_logFile );

		}
		else {
			fprintf(stderr, "Unrecognized argument: '%s'\n", arg.Arg() );
			printf("%s", usage);
			status = true;
		}
		argno = arg.Index();
	}

	if ( status == STATUS_OK && opts->m_logFile == NULL ) {
		fprintf(stderr, "Log file must be specified\n");
		printf("%s", usage);
		status = true;
	}

	// Read the persisted file (if specified)
	if ( opts->m_persistFile ) {
		FILE	*fp = safe_fopen_wrapper_follow( opts->m_persistFile, "r" );
		if ( fp ) {
			int		cluster, proc, subproc;
			if ( 3 == fscanf( fp, "%d.%d.%d", &cluster, &proc, &subproc ) ) {
				if ( opts->m_cluster < 0 ) opts->m_cluster = cluster;
				if ( opts->m_proc < 0 )    opts->m_proc    = proc;
				if ( opts->m_subProc < 0 ) opts->m_subProc = subproc;
			}
			fclose( fp );
		}
	}

	// Update the persisted file (if specified)
	if ( opts->m_persistFile ) {
		FILE	*fp = safe_fopen_wrapper_follow( opts->m_persistFile, "w" );
		if ( fp ) {
			fprintf( fp, "%d.%d.%d",
					 opts->m_cluster+1, opts->m_proc, opts->m_subProc );
			fclose( fp );
		}
	}

	return status;
}

void
handle_sigchild(int /*sig*/ )
{
#if defined(UNIX)
	pid_t	pid;
	int		status;
	if ( !global_workers ) {
		return;
	}
	while( true ) {
		pid = waitpid( -1, &status, WNOHANG );
		if ( pid > 0 ) {
			global_workers->reapChild( pid, status );
		}
		else {
			return;
		}
	}
#endif
}

Worker::Worker( const WorkerOptions &options, int num )
		: m_options( options ),
		  m_workerNum( num ),
		  m_alive( false ),
		  m_pid( -1 ),
		  m_status( 0 )
{
}

Worker::~Worker( void )
{
	Kill( SIGKILL );
}

bool
Worker::Kill( int signum ) const
{
	if ( !m_alive || (m_pid <= 0) ) {
		return false;
	}
# if defined(UNIX)
	if ( kill(m_pid, signum) < 0 ) {
		return false;
	}
# endif
	return true;
}

Workers::Workers( const GlobalOptions &options )
		: m_options( options ),
		  m_runningChildren( 0 )
{
}

Workers::~Workers( void )
{
	signalWorkers( SIGKILL );
	waitForWorkers( 10 );
# if defined(UNIX)
	signal( SIGCHLD, SIG_DFL );
# endif
	for( unsigned num = 0;  num < m_workers.size();  num++ ) {
		delete m_workers[num];
	}
}

Worker *
Workers::createWorkers( void )
{
	if ( m_options.getNumWorkers() == 1 ) {
		Worker *worker = new Worker( *m_options.getWorkerOpts(0), 0 );
		m_workers.push_back( worker );
		return worker;
	}

# if defined(UNIX)
	signal( SIGCHLD, handle_sigchild );
	for( int num = 0;  num < m_options.getNumWorkers();  num++ ) {
		Worker *worker = new Worker( *m_options.getWorkerOpts(num), num );
		pid_t pid = fork( );

		// Parent
		if ( pid ) {
			if ( m_options.Verbose(VERB_VERBOSE) ) {
				printf( "Forked off child pid %d\n", pid );
			}
			worker->setPid( pid );
			m_workers.push_back( worker );
			m_runningChildren++;
		}
		else {
			if ( m_options.Verbose(VERB_VERBOSE) ) {
				printf( "I'm child pid %d\n", getpid() );
			}
			m_runningChildren = 0;
			for( unsigned tmp = 0;  tmp < m_workers.size();  tmp++ ) {
				m_workers[tmp]->setPid( 0 );
				delete m_workers[tmp];
			}
			m_workers.clear();
			return worker;
		}
	}
# endif
	return NULL;
}

Worker *
Workers::findWorker( pid_t pid )
{
	for( unsigned num = 0;  num < m_workers.size();  num++ ) {
		if ( m_workers[num]->getPid() == pid ) {
			return m_workers[num];
		}
	}
	return NULL;
}

Worker *
Workers::getWorker( unsigned num )
{
	if ( num >= m_workers.size() ) {
		return NULL;
	}
	return m_workers[num];
}

bool
Workers::signalWorkers( int signum )
{
	bool	error = false;
	for( unsigned num = 0;  num < m_workers.size();  num++ ) {
	  if ( !(m_workers[num]->Kill(signum)) ) {
			error = true;
		}
	}
	return error;
}

bool
Workers::reapChild( pid_t pid, int status )
{
	Worker	*worker = findWorker( pid );
	if ( NULL == worker ) {
		return false;
	}
	worker->setStatus( status );
	m_runningChildren--;
	return true;
}

bool
Workers::waitForWorkers( int max_seconds )
{
	if ( !m_runningChildren ) {
		return true;
	}
	if ( m_options.Verbose(VERB_VERBOSE) ) {
		printf( "Waiting for %d children max %d seconds\n",
				m_runningChildren, max_seconds );
	}
	time_t	end = time(NULL) + max_seconds;
	while( m_runningChildren ) {
		handle_sigchild( SIGCHLD );		// Just to make sure we don't miss one
		if ( max_seconds && (time(NULL) > end) ) {
			return (0 != m_runningChildren);
		}
		sleep(1);
	}

	if ( m_options.Verbose(VERB_VERBOSE) ) {
		printf( "All children done:" );
	}
	for( unsigned num = 0;  num < m_workers.size();  num++ ) {
		int	status = m_workers[num]->getStatus();
		if ( m_options.Verbose(VERB_VERBOSE) ) {
			printf( "child %d: exit:%d sig:%d\n",
					m_workers[num]->getPid(),
					WEXITSTATUS(status), WTERMSIG(status) );
		}
		if ( status ) {
			m_errors++;
		}
	}
	
	return false;
}


// **************************
//  Rotating user log class
// **************************
TestLogWriter::TestLogWriter( Worker & /*worker*/,
							  const WorkerOptions &options )
	: WriteUserLog(),
	  m_options( options ),
	  m_rotations( 0 )
{
	setEnableGlobalLog( true );
	initialize( options.getLogFile(), options.getCluster(), options.getProc(),
	            options.getSubProc() );
	if ( options.getXml() ) {
		setUseCLASSAD( 1 );
	}
	if ( options.getName() ) {
		setCreatorName( options.getName() );
	}
}

bool
TestLogWriter::globalRotationStarting( unsigned long filesize )
{
	int rotations = m_rotations + 1;

	if ( m_options.Verbose(VERB_INFO) ) {
		printf( "rotation %d starting, file size is %lu\n",
				rotations, filesize );
	}

	if ( ( m_options.m_maxRotations >= 0 ) &&
		 ( rotations >= m_options.m_maxRotations ) ) {
		printf( "Max # of rotations hit: shutting down\n" );
		global_done = true;
		if ( m_options.m_maxRotationStop ) {
			return false;
		}
	}

	m_rotations++;
	return true;
}

void
TestLogWriter::globalRotationEvents( int events )
{
	if ( m_options.Verbose(VERB_INFO) ) {
		printf( "Rotating: %d events counted\n", events );
	}
}

void
TestLogWriter::globalRotationComplete( int num_rotations,
									 int sequence,
									   const std::string & /*id*/ )
{
	if ( m_options.Verbose(VERB_INFO) ) {
		printf( "rotation complete: %d %d\n",
				num_rotations, sequence );
	}
	if ( ( m_options.m_maxSequence >= 0 ) &&
		 ( sequence >= m_options.m_maxSequence ) ) {
		printf( "Max sequence # hit: shutting down\n" );
		global_done = true;
	}
}

long
TestLogWriter::getUserLogSize( void )
{
	static StatWrapper	swrap;
	if ( NULL == m_options.getLogFile() ) {
		return 0;
	}
	if ( !swrap.IsInitialized() ) {
		swrap.SetPath( m_options.getLogFile() );
	}
	if ( swrap.Stat() ) {
		return -1L;			// What should we do here????
	}
	return swrap.GetBuf()->st_size;
}

bool
TestLogWriter::WriteEvents( int &events, int &sequence )
{
	bool		error = false;
	int			cluster = getGlobalCluster();
	int			proc = getGlobalProc();
	int			subproc = getGlobalSubProc();

	// Sanity check
	if (  ( ( m_options.getMaxGlobalSize() >= 0 ) ||
			( m_options.getMaxRotations() >= 0 ) ||
			( m_options.getMaxSequence() >= 0 ) ) &&
		  ( false == isGlobalEnabled() )  ) {
		fprintf( stderr, "Global option specified, but eventlog disabled!\n" );
		return false;
	}

	EventInfo	event( m_options, cluster, proc, subproc );

		//
		// Write the submit event.
		//
	event.GenEventSubmit( );
	error = event.WriteEvent( *this );
	event.Reset( );
	if ( !error ) {
		events++;
	}

		//
		// Write a single generic event
		//
	if ( m_options.m_genericEventStr ) {
		event.GenEventGeneric( );
		if ( event.WriteEvent( *this ) ) {
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
	if ( m_options.Verbose(VERB_VERBOSE) ) {
		printf( "Writing %d events for job %d.%d.%d\n",
				m_options.m_numExec, cluster, proc, subproc );
	}
	for ( int exec = 0;
		  ( (m_options.getNumExec()<0) || (exec<m_options.getNumExec()) );
		  ++exec ) {
		if ( global_done ) {
			break;
		}

		event.GenEventExecute( );
		if ( event.WriteEvent( *this ) ) {
			error = true;
		}
		else {
			events++;
		}
		event.Reset( );

		if ( !error && ( get_random_float_insecure() < m_options.getRandomProb() )  ) {
			event.GenEventRandom( );
			if ( event.WriteEvent( *this ) ) {
				printf( "Error writing event type %d\n",
						(int) event.GetEventNumber() );
				error = true;
			}
			else {
				events++;
			}
			event.Reset( );
		}
		if ( !error ) {
			event.NextProc( );
		}

		unsigned long	size;
		long			max_size;
		max_size = m_options.getMaxGlobalSize();

		if ( isGlobalEnabled() ) {
			if ( !getGlobalLogSize(size, true) ) {
				printf( "Error getting global log size!\n" );
				error = true;
			}
			else if ( (max_size > 0) && (size > (unsigned long)max_size) ) {
				printf( "Maximum global log size limit hit %ld > %lu\n",
						size, max_size );
				global_done = true;
			}
		}

		max_size = m_options.getMaxUserSize();
		if ( ( max_size > 0 ) && ( getUserLogSize() > max_size ) ) {
			printf( "Maximum user log size limit hit\n" );
			global_done = true;
		}

		if ( m_options.getSleepSeconds() ) {
			sleep( m_options.getSleepSeconds() );
		}
		if ( m_options.getSleepUseconds() ) {
			usleep( m_options.getSleepUseconds() );
		}
	}

		//
		// Write the terminated event.
		//
	event.GenEventTerminate( );
	if ( event.WriteEvent( *this ) ) {
		error = true;
	}
	else {
		events++;
	}

	if ( isGlobalEnabled() ) {
		sequence = getGlobalSequence( );
	}
	else {
		sequence = 0;
	}

	// If no rotations occurred, the writer did no rotations, and doesn't
	// know it's rotation #
	if ( isGlobalEnabled() && ( sequence == 0 ) ) {
		const char			*path = getGlobalPath();
		ReadUserLogHeader	header_reader;
		ReadUserLog			log_reader;

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
			printf( "Got sequence #%d from header\n", sequence );
		}
	}

	return error;
}

// **************************
//  static functions
// **************************
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
	return get_random_uint_insecure() % maxval;
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
	double	randval = get_random_float_insecure( );

	// Special case: execute event
	if ( randval < m_options.getRandomProb() ) {
		return GenEventExecute( );
	}
	else {
		return GenEventRandom( );
	}
}

ULogEvent *
EventInfo::GenEventRandom( void )
{
	ULogEvent	*eventp = NULL;
	while( NULL == eventp ) {
		unsigned	rand_event = randint(29);
		ULogEventNumber eventType = (ULogEventNumber) rand_event;

		eventp = GenEvent( eventType );
	}

	return eventp;
}

bool
EventInfo::WriteEvent( TestLogWriter &writer )
{
	if ( m_options.Verbose(VERB_ALL) ) {
		printf("Writing %s event %s @ %s\n",
			   m_name, m_note ? m_note : "", timestr() );
	}

	if ( !writer.writeEvent( m_eventp ) ) {
		if ( m_options.Verbose(VERB_ERROR) ) {
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

	snprintf( buf, sizeof(buf), "(%d.%d.%d)", m_cluster, m_proc, m_subProc );
	SetNote( buf );

	SubmitEvent	*e = new SubmitEvent;
	e->setSubmitHost( "<128.105.165.12:32779>");

		// Note: the line below is designed specifically to work with
		// Kent's dummy stork_submit script for testing DAGs with
		// DATA nodes.
	e->submitEventLogNotes  = m_options.m_submitNote;
	e->submitEventUserNotes = "User info";

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventExecute( void )
{
	SetName( "Execute" );
	SetNote( "<128.105.165.12:32779>" );

	ExecuteEvent	*e = new ExecuteEvent;
	e->setExecuteHost(GetNote());
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

	snprintf( buf, sizeof(buf), "(%d.%d.%d)", m_cluster, m_proc, m_subProc );
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
	e->core_file = "corefile";
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
	e->image_size_kb = GetSize( );
	e->memory_usage_mb = (GetSize() + 1023) / 1024;

	return SetEvent( e );
}

ULogEvent *
EventInfo::GenEventShadowException( void )
{
	SetName( "Shadow Exception" );
	ShadowExceptionEvent *e = new ShadowExceptionEvent;
	e->sent_bytes = GetSize( );
	e->recvd_bytes = GetSize( );
	e->setMessage("EXCEPTION");

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
	e->code = 404;
	e->subcode = 0xff;

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
	strncpy(e->info, m_options.m_genericEventStr, sizeof(e->info) - 1 );
	e->info[sizeof(e->info)-1] = '\0';

	return SetEvent( e );
}

bool
EventInfo::GenIsLarge( void )
{
	m_is_large = (get_random_float_insecure() >= 0.8);
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
