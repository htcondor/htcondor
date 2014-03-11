#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "subsystem_info.h"

#include <string>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "PipeBuffer.h"
#include "Queue.h"

#include <curl/curl.h>

#include <vector>
#include <map>

pthread_mutex_t bigGlobalMutex = PTHREAD_MUTEX_INITIALIZER;

void configureSignals();
static void * workerFunction( void * ptr );

// Yes, globals are evil, and yes, I could wrap these and the queue in
// a struct and pass that to thread instead, but why bother?
std::string pandaURL;
std::string vomsProxy;
std::string vomsCAPath;
std::string vomsCACert;
int timeout = 0;

int main( int /* argc */, char ** /* argv */ ) {
	configureSignals();

	// This incantation makes dprintf() work.
	set_mySubSystem( "pandad", SUBSYSTEM_TYPE_DAEMON );
	config();
	dprintf_config( "pandad" );
	const char * debug_string = getenv( "DebugLevel" );
	if( debug_string && * debug_string ) {
		set_debug_flags( debug_string, 0 );
	}
	dprintf( D_ALWAYS, "STARTING UP\n" );


	param( pandaURL, "PANDA_URL" );
	param( vomsProxy, "PANDA_VOMS_PROXY" );
	if( pandaURL.empty() || vomsProxy.empty() ) {
		dprintf( D_ALWAYS, "You must define both PANDA_URL and PANDA_VOMS_PROXY for pandad to function.\n" );
		exit( 1 );
	}

	param( vomsCAPath, "PANDA_VOMS_CA_PATH" );
	param( vomsCACert, "PANDA_VOMS_CA_FILE" );
	if( vomsCAPath.empty() && vomsCACert.empty() ) {
		dprintf( D_ALWAYS, "One of PANDA_VOMS_CA_[PATH|CERT] is probably necessary, but neither is set.\n" );
	}

	timeout = param_integer( "PANDA_UPDATE_TIMEOUT" );

	// curl_global_init() is not thread-safe.
	CURLcode crv = curl_global_init( CURL_GLOBAL_ALL );
	if( crv != 0 ) {
		dprintf( D_ALWAYS, "Failed curl_global_init(), aborting.\n" );
		exit( 1 );
	}


	// To make sure that we're the only thread running, take the big
	// global mutex before creating any others.
	pthread_mutex_lock( & bigGlobalMutex );

	// A locking queue.
	Queue< std::string > queue( 1024, & bigGlobalMutex );

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
			dprintf( D_ALWAYS, "stdin closed, draining queue...\n" );
			queue.drain();
			dprintf( D_ALWAYS, "stdin closed, ... done.  Exiting.\n" );
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

void configureSignals() {
	sigset_t sigSet;
	struct sigaction sigAct;

	sigemptyset( & sigSet );
	sigaddset( &sigSet, SIGTERM );
	sigaddset( &sigSet, SIGQUIT );
	sigaddset( &sigSet, SIGPIPE );

	sigemptyset( &sigAct.sa_mask );
	sigAct.sa_flags = 0;

	sigAct.sa_handler = quit;
	sigaction( SIGTERM, &sigAct, 0 );
	sigaction( SIGQUIT, &sigAct, 0 );

	sigprocmask( SIG_UNBLOCK, & sigSet, NULL );
} // end configureSignals()

#else // ! defined( WIN32 )

void configureSignals() { }

#endif

typedef std::map< std::string, std::string > Command;
typedef std::map< std::string, Command > CommandSet;
CommandSet addCommands, updateCommands, removeCommands;

void constructCommand( const std::string & line ) {
	size_t nextSpace;
	size_t currentSpace = -1;
	std::vector< std::string > argv;
	do {
		nextSpace = line.find( ' ', currentSpace + 1 );
		argv.push_back( line.substr( currentSpace + 1, nextSpace - (currentSpace + 1) ) );
		currentSpace = nextSpace;
	} while( currentSpace != std::string::npos );

	std::string command = argv[0];
	std::string globalJobID = argv[1];
	if( command == "ADD" ) {
		Command & c = addCommands[ globalJobID ];
		c[ "globaljobid" ] = globalJobID;
		c[ "condorid" ] = argv[2];
	} else if( command == "UPDATE" ) {
		Command & c = updateCommands[ globalJobID ];
		c[ "globaljobid" ] = globalJobID;
		c[ argv[2] ] = argv[3];
	} else if( command == "REMOVE" ) {
		Command & c = removeCommands[ globalJobID ];
		c[ "globaljobid" ] = globalJobID;
	} else {
		dprintf( D_ALWAYS, "workerFunction() ignoring unknown command (%s).\n", command.c_str() );
	}
} // end constructCommand()

std::string generatePostString( const CommandSet & commandSet ) {
	std::string postFieldString = "[ ";
	CommandSet::const_iterator i = commandSet.begin();
	for( ; i != commandSet.end(); ++i ) {
		const std::string & globalJobID = i->first;
		const Command & c = i->second;

		postFieldString += "{ ";

		Command::const_iterator j = c.begin();
		for( ; j != c.end(); ++j ) {
			postFieldString += "\"" + j->first + "\": ";
			postFieldString += "\"" + j->second + "\", ";
		}
		// TO DO: Determine what, if anything, 'wmsid' ought to be.
		postFieldString += "\"wmsid\": \"1001\"";
		postFieldString += " }, ";
	}
	// Remove trailing ', '.
	postFieldString.erase( postFieldString.length() - 2 );
	postFieldString += " ]";

	return postFieldString;
} // end generatePostString()

#define SET_REQUIRED_CURL_OPTION( A, B, C ) { \
	CURLcode rv##B = curl_easy_setopt( A, B, C ); \
	if( rv##B != CURLE_OK ) { \
		dprintf( D_ALWAYS, "curl_easy_setopt( %s ) failed (%d): '%s', aborting.\n", \
			#B, rv##B, curl_easy_strerror( rv##B ) ); \
		exit( 1 ); \
	} \
}

bool sendCommands( CURL * curl, const std::string & url, const CommandSet & commandSet, char * curlErrorBuffer, unsigned long & responseCode ) {
	if( commandSet.size() == 0 ) { return false; }
	std::string postString = generatePostString( commandSet );

	dprintf( D_FULLDEBUG, "Sending commands to URL '%s'.\n", url.c_str() );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_URL, url.c_str() );
	dprintf( D_FULLDEBUG, "Sending POST '%s'.\n", postString.c_str() );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_POSTFIELDS, postString.c_str() );

	pthread_mutex_unlock( & bigGlobalMutex );
	CURLcode rv = curl_easy_perform( curl );
	pthread_mutex_lock( & bigGlobalMutex );

	if( rv != CURLE_OK ) {
		dprintf( D_ALWAYS, "Ignoring curl_easy_perform() error (%d): '%s'\n", rv, curl_easy_strerror( rv ) );
		dprintf( D_ALWAYS, "curlErrorBuffer = '%s'\n", curlErrorBuffer );
		return false;
	}

	rv = curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, & responseCode );
	if( rv != CURLE_OK ) {
		dprintf( D_ALWAYS, "Ignoring curl_easy_getinfo() error (%d): '%s'\n", rv, curl_easy_strerror( rv ) );
		dprintf( D_ALWAYS, "curlErrorBuffer = '%s'\n", curlErrorBuffer );
		return false;
	}

	return true;
} // end sendCommands()

// Required for debugging.  Stolen from ec2_gahp/amazonCommands.cpp.
size_t appendToString( const void * ptr, size_t size, size_t nmemb, void * str ) {
	if( size == 0 || nmemb == 0 ) { return 0; }

	std::string source( (const char *)ptr, size * nmemb );
	std::string * ssptr = (std::string *)str;
	ssptr->append( source );

	return (size * nmemb);
}

static void * workerFunction( void * ptr ) {
	Queue< std::string > * queue = (Queue< std::string > *)ptr;
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
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_SSLCERT, vomsProxy.c_str() );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_SSLKEY, vomsProxy.c_str() );
  	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_POST, 1 );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_NOSIGNAL, 1 );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_TIMEOUT, timeout );

	std::string resultString;
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_WRITEFUNCTION, & appendToString );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_WRITEDATA, & resultString );

	// No, this makes no sense.  Yes, it's how their API works.
	struct curl_slist * headers = NULL;
	headers = curl_slist_append( headers, "Content-Type: application/json" );
	SET_REQUIRED_CURL_OPTION( curl, CURLOPT_HTTPHEADER, headers );

	if( ! vomsCAPath.empty() ) {
		SET_REQUIRED_CURL_OPTION( curl, CURLOPT_CAPATH, vomsCAPath.c_str() );
	}
	if( ! vomsCACert.empty() ) {
		SET_REQUIRED_CURL_OPTION( curl, CURLOPT_CAINFO, vomsCACert.c_str() );
	}

	// The trailing slash is required.
	std::string API = pandaURL + "/api-auth/htcondorapi";
	std::string addJob = API + "/addjob/";
	std::string updateJob = API + "/updatejob/";
	std::string removeJob = API + "/removejob/";


	for( ; ; ) {
		//
		// Batch up all available commands.
		//
		do {
			constructCommand( queue->dequeue() );
		} while( ! queue->empty() );

		// Send all of the add commands.
		unsigned long responseCode = 0;
		bool rc = sendCommands( curl, addJob, addCommands, curlErrorBuffer, responseCode );
		if( rc && responseCode != 201 ) {
			dprintf( D_ALWAYS, "addJob() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
		}

		// Send all the update commands.
		rc = sendCommands( curl, updateJob, updateCommands, curlErrorBuffer, responseCode );
		if( rc && responseCode != 202 ) {
			dprintf( D_ALWAYS, "updateJob() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
		}

		// Send all the remove commands.
		rc = sendCommands( curl, removeJob, removeCommands, curlErrorBuffer, responseCode );
		if( rc && responseCode != 202 ) {
			dprintf( D_ALWAYS, "removeJob() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
		}

		// Clear the batched commands.
		addCommands.clear();
		updateCommands.clear();
		removeCommands.clear();

		pthread_mutex_unlock( & bigGlobalMutex );
		sleep( 1 );
		pthread_mutex_lock( & bigGlobalMutex );
	}


	curl_easy_cleanup( curl ); curl = NULL;
	curl_slist_free_all( headers ); headers = NULL;
	return NULL;
} // end workerFunction()
