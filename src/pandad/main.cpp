#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "condor_uid.h"

#include <string>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "PipeBuffer.h"
#include "TimeSensitiveQueue.h"

#include <curl/curl.h>

#include <vector>
#include <map>

#include "safe_open.h"

pthread_mutex_t bigGlobalMutex = PTHREAD_MUTEX_INITIALIZER;

// Performance metrics.
unsigned badCommandCount = 0;
unsigned commandsSentCount = 0;

unsigned curlCalledCount = 0;
unsigned curlFailureCount = 0;
unsigned badResponseCodeCount = 0;

template< class T >
void updateStatisticsLog( const TimeSensitiveQueue<T> & queue, bool forceUpdate = false );

void configureSignals();
static void * workerFunction( void * ptr );

// Yes, globals are evil, and yes, I could wrap these and the queue in
// a struct and pass that to thread instead, but why bother?
bool reconfigure = false;
std::string pandaURL;
int timeout = 0;

int main( int /* argc */, char ** /* argv */ ) {
	configureSignals();

	// This incantation makes dprintf() work.
	set_priv_initialize();
	set_mySubSystem( "pandad", SUBSYSTEM_TYPE_DAEMON );
	config();
	dprintf_config( "pandad" );
	const char * debug_string = getenv( "DebugLevel" );
	if( debug_string && * debug_string ) {
		set_debug_flags( debug_string, 0 );
	}
	dprintf( D_ALWAYS, "STARTING UP\n" );

	param( pandaURL, "PANDA_URL" );
	if( pandaURL.empty() ) {
		dprintf( D_ALWAYS, "You must define PANDA_URL for pandad to function.\n" );
		exit( 1 );
	}

	timeout = param_integer( "PANDA_UPDATE_TIMEOUT" );

	int queueSize = param_integer( "PANDA_QUEUE_SIZE" );


	// curl_global_init() is not thread-safe.
	CURLcode crv = curl_global_init( CURL_GLOBAL_ALL );
	if( crv != 0 ) {
		dprintf( D_ALWAYS, "Failed curl_global_init(), aborting.\n" );
		exit( 1 );
	}


	// To make sure that we're the only thread running, take the big
	// global mutex before creating any others.
	pthread_mutex_lock( & bigGlobalMutex );

	// A time-sensitive locking queue.
	TimeSensitiveQueue< std::string > queue( queueSize, & bigGlobalMutex );

	int gracePeriod = param_integer( "PANDA_QUEUE_GRACE" );
	if( ! queue.allowGracePeriod( gracePeriod ) ) {
		dprintf( D_ALWAYS, "Unable to allow queue a %d-second grace period, exiting.\n", gracePeriod );
		exit( 1 );
	}

	// We only need one worker thread.
	pthread_t workerThread;
	int rv = pthread_create( & workerThread, NULL, workerFunction, & queue );
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable to create worker thread (%d), exiting.\n", rv );
		exit( 1 );
	}
	rv = pthread_detach( workerThread );
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable to detach worker thread (%d), exiting.\n", rv );
		exit( 1 );
	}


	std::string * line = NULL;
	PipeBuffer pb( STDIN_FILENO, & bigGlobalMutex );
	for( ;; ) {
		while( (line = pb.getNextLine()) != NULL ) {
			dprintf( D_FULLDEBUG, "getNextLine() = %s\n", line->c_str() );
			queue.enqueue( * line );
			delete line;
		}

		if( pb.isError() || pb.isEOF() ) {
			dprintf( D_ALWAYS, "stdin closed, trying to log final statistics...\n" );
			updateStatisticsLog( queue, true );
			dprintf( D_ALWAYS, "stdin closed, ... done.\n" );

			//
			// Don't even bother to try to drain the queue; the master will
			// kill us first, and it's not worth explaining to users.
			//
			// printf( D_ALWAYS, "stdin closed, draining queue...\n" );
			// queue.drain();
			// dprintf( D_ALWAYS, "stdin closed, ... done.\n" );

			dprintf( D_ALWAYS, "stdin closed, exit()ing.\n" );
			exit( 0 );
		}
	}
} // end main()

#if ! defined( WIN32 )

#include <signal.h>

void quit( int signal ) {
	// POSIX exit() is not signal-safe.
	_exit( signal );
}

void sighup( int signal ) {
	if( signal != SIGHUP ) { return; }
	reconfigure = true;
}

void configureSignals() {
	// Stolen from the GAHPs.
	sigset_t sigSet;
	struct sigaction sigAct;

	sigemptyset( &sigAct.sa_mask );
	sigAct.sa_flags = 0;
	sigAct.sa_handler = quit;
	sigaction( SIGTERM, &sigAct, 0 );
	sigaction( SIGQUIT, &sigAct, 0 );

	sigemptyset( & sigSet );
	sigaddset( &sigSet, SIGTERM );
	sigaddset( &sigSet, SIGQUIT );
	sigaddset( &sigSet, SIGPIPE );
	sigprocmask( SIG_UNBLOCK, & sigSet, NULL );

	// Handle SIGHUP.
	sigemptyset( & sigAct.sa_mask );
	sigAct.sa_flags = 0;
	sigAct.sa_handler = sighup;
	sigaction( SIGHUP, & sigAct, 0 );

	sigemptyset( & sigSet );
	sigaddset( & sigSet, SIGHUP );
	sigprocmask( SIG_UNBLOCK, & sigSet, NULL );
} // end configureSignals()

#else // ! defined( WIN32 )

void configureSignals() { }

#endif

typedef std::map< std::string, std::string > Command;
typedef std::map< std::string, Command > CommandSet;
CommandSet addCommands, updateCommands, removeCommands;

std::string & doValueCleanup( std::string & value ) {
	//
	// We're sent unparse()d ClassAd values, but we need to send JSON.
	//
	if( value[0] == '"' ) {
		if( value[ value.length() - 1 ] != '"' ) {
			dprintf( D_ALWAYS, "String value '%s' lacks trailing \", appending.\n", value.c_str() );
			value += "\"";
		}

		// All ClassAd escapes are legal JSON escapes, except for single
		// quote and the octal escapes.  The former shouldn't be escaped
		// and the latter must be converted into hex.  All ClassAd bytes
		// are between U+0020 and U+007E (with U+005C = '\' already being
		// escaped by unparse()), so they pass through unchanged.

		size_t index = 1;
		while( index < value.length() - 1 ) {
			if( value[ index ] != '\\' ) { index += 1; continue; }
			char c = value[ index + 1 ];
			// Ignore escaped escapes.
			if( c == '\\' ) { index += 2; continue; }
			// Convert escaped single quotes.
			if( c == '\'' ) { value.replace( index, 2, "'" ); index += 1; continue; }
			// Convert octal escapes.  The job queue may never generate these.
			if( ! isdigit( c ) ) { index += 2; continue; }

			const char * left = value.c_str();
			left = left + index + 1;
			char * right;
			unsigned long octal = strtoul( left, & right, 8 );
			if( right == left) {
				dprintf( D_ALWAYS, "Detected invalid octal escape, escaping.\n" );
				value.replace( index, 2, "\\\\" );
				index += 2;
				continue;
			}
			std::string cStr;
			formatstr( cStr, "\\u%04lX", octal );
			value.replace( index, right - left + 1, cStr.c_str() );
			index += right - left + 1;
		}
	} else if( isdigit( value[0] ) ) {
		// Sometimes integers are represented as floats with lots of trailing
		// zeroes, which can cause some parsers grief.  Convert them.
		size_t point = value.find( '.' );
		if( point == std::string::npos ) { return value; }
		for( size_t i = point + 1; i < value.length(); ++i ) {
			if( value[i] != '0' ) { return value; }
		}

		// If we get here, then everything after the point must have been 0,
		// so we can safely truncate the string.
		value.erase( point, std::string::npos );
	} else {
		// A literal value.
		if( value == "null" ) { return value; }
		else if( value == "false" ) { return value; }
		else if( value == "true" ) { return value; }
		else {
			// JSON does not permit any other literal.
			dprintf( D_FULLDEBUG, "Converting illegal literal '%s' into string.\n", value.c_str() );

			// Unfortunately, the alphabet for ClassAd expressions is less
			// limited than the alphabet for ClassAd strings.
			size_t index = 0;
			while( index < value.length() ) {
				unsigned char c = value[ index ];

				// Escape control characters.  This code should never run.
				if( c <= 0x1F ) {
					std::string cStr;
					formatstr( cStr, "\\u%04lX", (unsigned long)c );
					value.replace( index, 1, cStr.c_str() );
					index += cStr.length();
					continue;
				}

				// The quotation mark and the backslash must be escaped.  All
				// other escapes are optional, so we won't generate them.
				if( c == '"' ) {
					value.replace( index, 1, "\\\"" );
					index += 2;
					continue;
				}

				if( c == '\\' ) {
					value.replace( index, 1, "\\\\" );
					continue;
				}

				++index;
			}
			value = '"' + value + '"';

			dprintf( D_FULLDEBUG, "Converted illegal literal into '%s'.\n", value.c_str() );
		}
	}
	return value;
}


void addRemainingPairs( Command & c, std::vector< std::string > & argv, size_t index ) {
	for( ; index + 1 < argv.size(); index += 2 ) {
		// Panda should redefine their schema so that 'submitted'
		// is TIMESTAMP, not DATETIME.
		if( argv[index] == "submitted" ) {
			time_t submitTime = atol( argv[index + 1].c_str() );
			char buffer[] = "\"YYYY-MM-DD HH:MM:SS\"";
			struct tm * ts = localtime( & submitTime );
			// Include the quotes because TIMESTAMP is a string to JSON.
			strftime( buffer, sizeof( buffer ), "\"%Y-%m-%d %H:%M:%S\"", ts );
			argv[index + 1] = buffer;
		}

		c[ argv[index] ] = doValueCleanup( argv[index + 1] );
	}
}

void constructCommand( const std::string & line ) {
	//
	// Before parsing the command, make sure that it wasn't mangled by
	// an aborted partial write.  All writes from the schedd are of the
	// form '\v[ADD|UPDATE|REMOVE](\t<arg>)+\n'.  We know we have the newline
	// because we feed the queue line-by-line.  If we find only one \v in
	// the entire line, at the beginning of the line, then the write wasn't
	// aborted, and it should be safe to parse.
	//
	size_t firstVerticalTab = line.find( '\v' );
	if( firstVerticalTab != 0 ) {
		++badCommandCount;
		return;
	}

	size_t nextSpace;
	// The first character is '\v', so we can start looking at 0 + 1.
	size_t currentSpace = 0;
	std::vector< std::string > argv;
	do {
		// We know that the last character in line is '\n', so
		// currentSpace + 1 is always a valid index.
		nextSpace = line.find( '\t', currentSpace + 1 );
		argv.push_back( line.substr( currentSpace + 1, nextSpace - (currentSpace + 1) ) );
		currentSpace = nextSpace;
	} while( currentSpace != std::string::npos );

	#define PRIMARY_KEY "id"
	std::string command = argv[0];
	std::string globalJobID = argv[1];
	if( command == "ADD" ) {
		Command & c = addCommands[ globalJobID ];
		c[ PRIMARY_KEY ] = globalJobID;
		c[ "condorid" ] = argv[2];
		addRemainingPairs( c, argv, 3 );
	} else if( command == "UPDATE" ) {
		// Coalesce updates to a new job with the job creation commmand.
		if( addCommands.find( globalJobID ) != addCommands.end() ) {
			Command & c = addCommands[ globalJobID ];
			addRemainingPairs( c, argv, 2 );
		} else {
			Command & c = updateCommands[ globalJobID ];
			c[ PRIMARY_KEY ] = globalJobID;
			addRemainingPairs( c, argv, 2 );
		}
	} else if( command == "REMOVE" ) {
		Command & c = removeCommands[ globalJobID ];
		c[ PRIMARY_KEY ] = globalJobID;
		addRemainingPairs( c, argv, 2 );
	} else {
		dprintf( D_ALWAYS, "workerFunction() ignoring unknown command (%s).\n", command.c_str() );
		return;
	}
} // end constructCommand()

#define SET_REQUIRED_CURL_OPTION( A, B, C ) { \
	CURLcode rv##B = curl_easy_setopt( A, B, C ); \
	if( rv##B != CURLE_OK ) { \
		dprintf( D_ALWAYS, "curl_easy_setopt( %s ) failed (%d): '%s', aborting.\n", \
			#B, rv##B, curl_easy_strerror( rv##B ) ); \
		exit( 1 ); \
	} \
}

template< class T >
void updateStatisticsLog( const TimeSensitiveQueue<T> & queue, bool forceUpdate ) {
	static unsigned previousQueueFullCount = UINT_MAX;
	static unsigned previousBadCommandCount = UINT_MAX;
	static unsigned previousCurlFailureCount = UINT_MAX;
	static unsigned previousBadResponseCodeCount = UINT_MAX;

	if(	previousQueueFullCount == queue.queueFullCount
		  && previousBadCommandCount == badCommandCount
		  && previousCurlFailureCount == curlFailureCount
		  && previousBadResponseCodeCount == badResponseCodeCount
		  && (! forceUpdate)
		 ) {
			return;
	}
	previousQueueFullCount = queue.queueFullCount;
	previousBadCommandCount = badCommandCount;
	previousCurlFailureCount = curlFailureCount;
	previousBadResponseCodeCount = badResponseCodeCount;

	std::string statisticsLog = "/tmp/pandaStatisticsLog";
	param( statisticsLog, "PANDA_STATISTICS_LOG" );

	int fd = safe_create_keep_if_exists_follow( statisticsLog.c_str(), O_CREAT | O_APPEND | O_WRONLY | O_NONBLOCK, 00600 );
	if( fd == -1 ) {
		return;
	}

	FILE * psl = fdopen( fd, "a" );
	if( psl == NULL ) {
		return;
	}

	time_t now = time( NULL );
	struct tm * ns = localtime( & now );
	char nowString[72] = "YYYY-MM-DD HH:MM:SS";
	snprintf( nowString, sizeof( nowString ), "%4d-%02d-%02d %02d:%02d:%02d",
			  ns->tm_year + 1900, ns->tm_mon + 1, ns->tm_mday,
			  ns->tm_hour, ns->tm_min, ns->tm_sec );

	fprintf( psl, "[%s] queueFullCount: %u\n", nowString, queue.queueFullCount );
	fprintf( psl, "[%s] enqueueCallCount: %u\n", nowString, queue.enqueueCallCount );
	fprintf( psl, "[%s] %.3f%% queue success rate.\n", nowString,
		(1 - (queue.queueFullCount / (double)queue.enqueueCallCount)) * 100 );

	fprintf( psl, "[%s] badCommandCount: %u\n", nowString, badCommandCount );
	fprintf( psl, "[%s] commandsSentCount: %u\n", nowString, commandsSentCount );
	fprintf( psl, "[%s] %.3f%% command success rate.\n", nowString,
		(1 - (badCommandCount / (double)commandsSentCount )) * 100 );

	fprintf( psl, "[%s] curlFailureCount: %u\n", nowString, curlFailureCount );
	fprintf( psl, "[%s] badResponseCodeCount: %u\n", nowString, badResponseCodeCount );
	fprintf( psl, "[%s] curlCalledCount: %u\n", nowString, curlCalledCount );
	fprintf( psl, "[%s] %.3f%% total curl success rate.\n", nowString,
		(1 - ((curlFailureCount + badResponseCodeCount) / (double)curlCalledCount)) * 100 );

	fprintf( psl, "[%s]\n", nowString );
	fclose( psl );
}

template< class T >
bool sendPostField( const std::string & postString, CURL * curl, const std::string & url, const std::string & verb, char * curlErrorBuffer, unsigned long & responseCode, const TimeSensitiveQueue< T > & queue ) {
dprintf( D_ALWAYS, "sendPostField(): body length %zu, url %s, verb %s.\n", postString.length(), url.c_str(), verb.c_str() );
	++curlCalledCount;

	dprintf( D_FULLDEBUG, "Sending commands to URL '%s'.\n", url.c_str() );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_URL, url.c_str() );
	dprintf( D_FULLDEBUG, "Setting verb to '%s'.\n", verb.c_str() );
  	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_CUSTOMREQUEST, verb.c_str() );
	dprintf( D_FULLDEBUG, "Sending %s '%s'.\n", verb.c_str(), postString.c_str() );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_POSTFIELDS, postString.c_str() );

	pthread_mutex_unlock( & bigGlobalMutex );
	updateStatisticsLog( queue );
	CURLcode rv = curl_easy_perform( curl );
	pthread_mutex_lock( & bigGlobalMutex );

	if( rv != CURLE_OK ) {
		dprintf( D_ALWAYS, "Ignoring curl_easy_perform() error (%d): '%s'\n", rv, curl_easy_strerror( rv ) );
		dprintf( D_ALWAYS, "curlErrorBuffer = '%s'\n", curlErrorBuffer );
		dprintf( D_ALWAYS, "Sending %s (url) %s (verb) %s (postString).\n", url.c_str(), verb.c_str(), postString.c_str() );

		++curlFailureCount;
		return false;
	}

	rv = curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, & responseCode );
	if( rv != CURLE_OK ) {
		dprintf( D_ALWAYS, "Ignoring curl_easy_getinfo() error (%d): '%s'\n", rv, curl_easy_strerror( rv ) );
		dprintf( D_ALWAYS, "curlErrorBuffer = '%s'\n", curlErrorBuffer );
		return false;
	}

	return true;
}

template< class T >
bool sendCommands( CURL * curl, const std::string & url, const std::string & verb, const CommandSet & commandSet, char * curlErrorBuffer, unsigned long & responseCode, const TimeSensitiveQueue< T > & queue ) {
	if( commandSet.size() == 0 ) { return false; }

	unsigned commandsInString = 0;
	std::string postFieldString = "[ ";
	CommandSet::const_iterator i = commandSet.begin();
	for( ; i != commandSet.end(); ++i ) {
		const Command & c = i->second;
		postFieldString += "{ ";

		Command::const_iterator j = c.begin();
		for( ; j != c.end(); ++j ) {
			postFieldString += "\"" + j->first + "\": ";
			postFieldString += j->second + ", ";
		}

		// Remove trailing ', '.
		postFieldString.erase( postFieldString.length() - 2 );
		postFieldString += " }, ";
		++commandsInString;

		// If increased to 1024 * 1024, the PanDA service loses events.
		if( postFieldString.length() > 512 * 1024) {
			// Remove trailing ', '.
			postFieldString.erase( postFieldString.length() - 2 );
			postFieldString += " ]";

			commandsSentCount += commandsInString;
			if( ! sendPostField( postFieldString, curl, url, verb, curlErrorBuffer, responseCode, queue ) ) {
				return false;
			}

			commandsInString = 0;
			postFieldString = "[ ";
		}
	}

	// Send what remains, if anything.
	if( commandsInString > 0 ) {
		postFieldString.erase( postFieldString.length() - 2 );
		postFieldString += " ]";

		commandsSentCount += commandsInString;
		return sendPostField( postFieldString, curl, url, verb, curlErrorBuffer, responseCode, queue );
	}

	return true;
}

// Required for debugging.  Stolen from ec2_gahp/amazonCommands.cpp.
size_t appendToString( const void * ptr, size_t size, size_t nmemb, void * str ) {
	if( size == 0 || nmemb == 0 ) { return 0; }

	std::string source( (const char *)ptr, size * nmemb );
	std::string * ssptr = (std::string *)str;
	ssptr->append( source );

	return (size * nmemb);
}

static void * workerFunction( void * ptr ) {
	TimeSensitiveQueue< std::string > * queue = (TimeSensitiveQueue< std::string > *)ptr;
	pthread_mutex_lock( & bigGlobalMutex );

	// Do the command-independent curl set-up.
	CURL * curl = curl_easy_init();
	if( curl == NULL ) {
		dprintf( D_ALWAYS, "Failed curl_easy_init(), aborting.\n" );
		exit( 1 );
	}

	char curlErrorBuffer[CURL_ERROR_SIZE];
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_ERRORBUFFER, curlErrorBuffer );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_NOPROGRESS, 1 );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_NOSIGNAL, 1 );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_TIMEOUT, timeout );

	std::string resultString;
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_WRITEFUNCTION, & appendToString );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_WRITEDATA, & resultString );

	// No, this makes no sense.  Yes, it's how their API works.
	struct curl_slist * headers = NULL;
	headers = curl_slist_append( headers, "Content-Type: application/json" );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_HTTPHEADER, headers );

	std::string addJobVerb = "POST";
	std::string updateJobVerb = "PUT";
	std::string removeJobVerb = "DELETE";


	for( ; ; ) {
		//
		// Wait for the queue to fill up.  By sleeping instead of blocking,
		// we give the other thread a chance to insert more than one read
		// into the queue at a time.
		//
		while( queue->empty() ) {
			pthread_mutex_unlock( & bigGlobalMutex );
			sleep( 1 );
			pthread_mutex_lock( & bigGlobalMutex );
		}

		//
		// Empty the qeueue.
		//
		// We have to handle long sends in sendCommands() -- we otherwise
		// don't know if each PUT's corresponding POST has made it to the
		// server yet (and their API requires that it has).
		//
		while( ! queue->empty() ) {
			constructCommand( queue->dequeue() );
		}

		//
		// It's (presently) safe to resize the queue at any time, since it's
		// a non-destructive operation, but since the could change, do it
		// now, while we know the queue is empty.
		//
		if( reconfigure ) {
			config();
			unsigned newSize = param_integer( "PANDA_QUEUE_SIZE" );
			dprintf( D_ALWAYS, "SIGHUP: Queue size is now %u.\n", newSize );
			queue->resize( newSize );

			timeout = param_integer( "PANDA_UPDATE_TIMEOUT" );
			dprintf( D_ALWAYS, "SIGHUP: Timeout is now %d seconds.\n", timeout );

			std::string statisticsLog = "/tmp/pandaStatisticsLog";
			param( statisticsLog, "PANDA_STATISTICS_LOG" );
			dprintf( D_ALWAYS, "SIGHUP: Statistics log file is now '%s'.\n", statisticsLog.c_str() );

			// As presently implemented, calling allowGracePeriod() a second
			// time will always fail, so we can't handle reconfiguring
			// PANDA_QUEUE_GRACE.  If someone complained, we could maybe
			// make it work.

			reconfigure = false;
		}

		// Send all of the add commands.
		resultString = "";
		unsigned long responseCode = 0;
		bool rc = sendCommands( curl, pandaURL, addJobVerb, addCommands, curlErrorBuffer, responseCode, * queue );
		if( rc ) {
			dprintf( D_FULLDEBUG, "addJob() response code was %ld.\n", responseCode );
			if( responseCode != 201 ) {
				dprintf( D_ALWAYS, "addJob() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
				++badResponseCodeCount;
			}
		}

		// Send all the update commands.
		resultString = "";
		rc = sendCommands( curl, pandaURL, updateJobVerb, updateCommands, curlErrorBuffer, responseCode, * queue );
		if( rc ) {
			dprintf( D_FULLDEBUG, "updateJob() response code was %ld.\n", responseCode );
			if( responseCode != 200 ) {
				dprintf( D_ALWAYS, "updateJob() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
				++badResponseCodeCount;
			}
		}

		// Send all the remove commands.
		resultString = "";
		rc = sendCommands( curl, pandaURL, removeJobVerb, removeCommands, curlErrorBuffer, responseCode, * queue );
		if( rc ) {
			dprintf( D_FULLDEBUG, "removeJob() response code was %ld.\n", responseCode );
			if( responseCode != 204 ) {
				dprintf( D_ALWAYS, "removeJob() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
				++badResponseCodeCount;
			}
		}

		// Clear the batched commands.
		addCommands.clear();
		updateCommands.clear();
		removeCommands.clear();
	}


	curl_easy_cleanup( curl ); curl = NULL;
	curl_slist_free_all( headers ); headers = NULL;
	return NULL;
} // end workerFunction()
