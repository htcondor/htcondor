//
// TO DO: Rather than being entirely interrupt-driven, we should instead buffer
// up the commands and send one of each (ADD, UPDATE, REMOVE; in that order)
// once a second.  Be sure to do all command construction before the first
// mutex release, so that we don't get confused about how far into the queue
// we are.
//
// TO DO: Pick some large but not excessive maximum size for the queue; throw
// away updates that don't fit.
//
// TO DO: Consider reimplementing Queue with a ring buffer.
//

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

// Required for debugging.  Stolen from ec2_gahp/amazonCommands.cpp.
size_t appendToString( const void * ptr, size_t size, size_t nmemb, void * str ) {
	if( size == 0 || nmemb == 0 ) { return 0; }

	std::string source( (const char *)ptr, size * nmemb );
	std::string * ssptr = (std::string *)str;
	ssptr->append( source );

	return (size * nmemb);
}

#define SET_REQUIRED_CURL_OPTION( A, B, C ) { \
	CURLcode rv##B = curl_easy_setopt( A, B, C ); \
	if( rv##B != CURLE_OK ) { \
		dprintf( D_ALWAYS, "curl_easy_setopt( %s ) failed (%d): '%s', aborting.\n", \
			#B, rv##B, curl_easy_strerror( rv##B ) ); \
		exit( 1 ); \
	} \
}

static void * workerFunction( void * ptr ) {
	Queue< std::string > * queue = (Queue< std::string > *)ptr;

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
		std::string line = queue->dequeue();
		dprintf( D_FULLDEBUG, "workerFunction() handling '%s'\n", line.c_str() );

		std::string url;
		std::vector< std::pair< std::string, std::string > > postFields;

		size_t nextSpace;
		size_t currentSpace = -1;
		std::vector< std::string > argv;
		do {
			nextSpace = line.find( ' ', currentSpace + 1 );
			argv.push_back( line.substr( currentSpace + 1, nextSpace - (currentSpace + 1) ) );
			currentSpace = nextSpace;
		} while( currentSpace != std::string::npos );

		std::string command = argv[0];
		if( command == "ADD" ) {
			url = addJob;
			postFields.push_back( std::make_pair( "globaljobid", argv[1] ) );
			postFields.push_back( std::make_pair( "condorid", argv[2] ) );
		} else if( command == "UPDATE" ) {
			url = updateJob;
			postFields.push_back( std::make_pair( "globaljobid", argv[1] ) );
			postFields.push_back( std::make_pair( argv[2], argv[3] ) );
		} else if( command == "REMOVE" ) {
			url = removeJob;
			postFields.push_back( std::make_pair( "globaljobid", argv[1] ) );
		} else {
			dprintf( D_ALWAYS, "workerFunction() ignoring unknown command (%s).\n", command.c_str() );
			continue;
		}

		// TO DO: Determine what, if anything, this ought to be; or change the API.
		postFields.push_back( std::make_pair( "wmsid", "1001" ) );

		std::string postFieldString = "[ { ";
		postFieldString += "\"" + postFields[0].first + "\": ";
		postFieldString += "\"" + postFields[0].second + "\"";
		for( unsigned i = 1; i < postFields.size(); ++i ) {
			postFieldString += ", \"" + postFields[i].first + "\": ";
			postFieldString += "\"" + postFields[i].second + "\"";
		}
		postFieldString += " } ]";
		dprintf( D_FULLDEBUG, "workerFunction() POSTing '%s'\n", postFieldString.c_str() );
		SET_REQUIRED_CURL_OPTION( curl, CURLOPT_POSTFIELDS, postFieldString.c_str() );

		dprintf( D_FULLDEBUG, "workerFunction() using URL '%s'\n", url.c_str() );
		SET_REQUIRED_CURL_OPTION( curl, CURLOPT_URL, url.c_str() );

		CURLcode rv = curl_easy_perform( curl );
		if( rv != CURLE_OK ) {
			dprintf( D_ALWAYS, "workerFunction() ignoring curl_easy_perform() error (%d): '%s'\n", rv, curl_easy_strerror( rv ) );
			dprintf( D_ALWAYS, "workerFunction() curlErrorBuffer = '%s'\n", curlErrorBuffer );
			continue;
		}

		unsigned long responseCode = 0;
		rv = curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, & responseCode );
		if( rv != CURLE_OK ) {
			// To be more user-friendly, we could set and report CURLOPT_ERRORBUFFER.
			dprintf( D_ALWAYS, "workerFunction() ignoring curl_easy_getinfo() error (%d): '%s'\n", rv, curl_easy_strerror( rv ) );
			continue;
		}

		if( command == "ADD" && responseCode != 201 ) {
			dprintf( D_ALWAYS, "workerFunction() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
		} else if( command == "UPDATE" && responseCode != 202 ) {
			dprintf( D_ALWAYS, "workerFunction() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
		} else if ( command == "REMOVE" && responseCode != 202 ) {
			dprintf( D_ALWAYS, "workerFunction() ignoring non-OK (%ld) response '%s'.\n", responseCode, resultString.c_str() );
		}
	}


	curl_easy_cleanup( curl ); curl = NULL;
	curl_slist_free_all( headers ); headers = NULL;
	return NULL;
} // end workerFunction()
