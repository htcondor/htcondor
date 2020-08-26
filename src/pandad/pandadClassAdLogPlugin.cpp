#include "condor_common.h"
#include "condor_debug.h"

#include "ClassAdLogPlugin.h"
#include "my_popen.h"
#include "condor_config.h"
#include "param_info.h"

#include <sstream>
#include <set>

// Make sure we get the schedd's version of GetJobAd, and not libcondorutil's.
extern ClassAd * ScheddGetJobAd( int, int, bool e = false, bool p = true );

class PandadClassAdLogPlugin : public ClassAdLogPlugin {
	public:
		PandadClassAdLogPlugin();
		~PandadClassAdLogPlugin();

		void earlyInitialize() { }
		void initialize() { scheddInitialized = true; }
		void shutdown() { }

		void beginTransaction();
		void newClassAd( const char * key );
		void destroyClassAd( const char * key );
		void setAttribute( const char * key, const char * attribute, const char * value );
		void deleteAttribute( const char * key, const char * name );
		void endTransaction();

	private:
		bool getGlobalJobID( int cluster, int proc, std::string & globalJobID );
		bool shouldIgnoreJob( const char * key, int & cluster, int & proc );
		bool shouldIgnoreAttribute( const char * attribute );

		void addPandaJob( const char *  globalJobID, const char * condorJobID );
		void updatePandaJob( const char * globalJobID, const char * attribute, const char * value );
		void removePandaJob( const char * globalJobID );

		std::set< std::string > jobAttributes;

		FILE *	pandad;

		bool scheddInitialized;
};

// Required by plug-in API.  (Only if linked into the schedd?)
static PandadClassAdLogPlugin instance;

// ----------------------------------------------------------------------------

#if defined( WIN32 )
	#define DEVNULL "\\device\\null"
#else
	#define DEVNULL "/dev/null"
#endif // defined( WIN32 )

PandadClassAdLogPlugin::PandadClassAdLogPlugin() : ClassAdLogPlugin(), pandad( NULL ), scheddInitialized( false ) {
	std::string binary;
	param( binary, "PANDAD" );

	const char * arguments[] = { binary.c_str(), NULL };
	pandad = my_popenv( arguments, "w", 0 );

	// Never block the schedd.
	if( pandad != NULL ) {
		if( fcntl( fileno( pandad ), F_SETFL, O_NONBLOCK ) == -1 ) {
			dprintf( D_ALWAYS, "PANDA: failed to set pandad pipe to nonblocking, monitor will not be updated.\n" );
			pandad = NULL;
		}
	}

	if( pandad == NULL ) {
		dprintf( D_ALWAYS, "PANDA: failed to start pandad, monitor will not be updated.\n" );

		pandad = fopen( DEVNULL, "w" );
	}

	// This doesn't handle commas, but boy is it simple.
	std::string jaString;
	param( jaString, "PANDA_JOB_ATTRIBUTES" );
	if( ! jaString.empty() ) {
		std::istringstream jaStream( jaString );
		std::string attribute;
		while( std::getline( jaStream, attribute, ' ' ) ) {
			jobAttributes.insert( attribute );
		}
	}
}

PandadClassAdLogPlugin::~PandadClassAdLogPlugin() {
	my_pclose( pandad );
}

void PandadClassAdLogPlugin::beginTransaction() {
	dprintf( D_FULLDEBUG, "PANDA: beginTransaction()\n" );
}

void PandadClassAdLogPlugin::newClassAd( const char * key ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: newClassAd( %s )\n", key );

	std::string globalJobID;
	if( getGlobalJobID( cluster, proc, globalJobID ) ) {
		addPandaJob( globalJobID.c_str(), key );
	}

	//
	// Unfortunately, for a given cluster (2), condor_submit:
	//		(1) creates the cluster ad (22.-1)
	//		(2) creates the first job ad (22.0)
	//		(3) adds MyType, TargetType, and CurrentTime to it (22.0)
	//		(4) adds GlobalJobId to it (22.0)
	//		(5) starts populating the cluster ad (22.-1)
	//		(6) adds SUBMIT_ATTRS to the job ad (22.0)
	//		(7) mostly populates the cluster ad (22.-1)
	//		(8) sets LastJobStatus and JobStatus for the job ad (22.0)
	//		(9) finishes populating the cluster ad (22.-1)
	//		(a) sets the ProcId for the job ad (22.0)
	// repeats the following for each additional job in the cluster:
	//		(b) creates the next job ad
	//		(c) sets its GlobalJobId
	//		(d) sets its ProcId, LastJobStatus, JobStatus, and SUBMIT_EXPRS.
	//
	// This means that we can: not send the initial attribute-value pairs for
	// a 22.0; trap ProcId updates; xor pay attention to where transactions
	// begin and end.
	//
	// Since we never change assigned proc IDs, we know we won't update
	// with the initial values of the cluster ad more than once.
	//
	// Using the proc ID as a signal depends on the exact ordering of how
	// condor_submit does things.  However, since only condor_submit uses
	// cluster ads, that's not so bad, especially since this approach is
	// so much simpler and never ignores jobs.
	//
}

void PandadClassAdLogPlugin::setAttribute( const char * key, const char * attribute, const char * value ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: setAttribute( %s, %s, %s ).\n", key, attribute, value );

	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID ) ) { return; }

	// See comment in newClassAd(), above.
	if( strcmp( attribute, "ProcId" ) == 0 ) {
		ClassAd * clusterAd = ScheddGetJobAd( cluster, -1 );
		if( clusterAd != NULL ) {
			ExprTree * valueExpr = NULL;
			const char * attribute = NULL;

			for( auto itr = clusterAd->begin(); itr != clusterAd->end(); itr++ ) {
				attribute = itr->first.c_str();
				valueExpr = itr->second;
				dprintf( D_FULLDEBUG, "PANDA: found %s in cluster ad.\n", attribute );
				if( shouldIgnoreAttribute( attribute ) ) { continue; }
				std::string valueString;
				classad::ClassAdUnParser unparser;
				unparser.Unparse( valueString, valueExpr );
				updatePandaJob( globalJobID.c_str(), attribute, valueString.c_str() );
			}
		} else {
			dprintf( D_FULLDEBUG, "PANDA: Failed to find cluster ad for %d.%d\n", cluster, proc );
		}
	}

	if( shouldIgnoreAttribute( attribute ) ) { return; }
	updatePandaJob( globalJobID.c_str(), attribute, value );
}

void PandadClassAdLogPlugin::deleteAttribute( const char * key, const char * attribute ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: deleteAttribute( %s, %s )\n", key, attribute );

	std::string globalJobID;
	if( shouldIgnoreAttribute( attribute ) ) { return; }
	if( getGlobalJobID( cluster, proc, globalJobID ) ) {
		updatePandaJob( globalJobID.c_str(), attribute, NULL );
	}
}

void PandadClassAdLogPlugin::destroyClassAd( const char * key ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: destroyClassAd( %s )\n", key );

	std::string globalJobID;
	if( getGlobalJobID( cluster, proc, globalJobID ) ) {
		removePandaJob( globalJobID.c_str() );
	}
}

void PandadClassAdLogPlugin::endTransaction() {
	dprintf( D_FULLDEBUG, "PANDA: endTransaction()\n" );
}


bool PandadClassAdLogPlugin::shouldIgnoreJob( const char * key, int & cluster, int & proc ) {
	if ( sscanf( key, "%d.%d", & cluster, & proc ) != 2 ) { return true; }

	// Ignore the spurious 0.0 ad we get on startup.
	if( cluster == 0 && proc == 0 ) { return true; }

	// Ignore cluster ads.
	if( proc == -1 ) { return true; }

	return false;
}

bool PandadClassAdLogPlugin::shouldIgnoreAttribute( const char * attribute ) {
	if( jobAttributes.empty() ) { return false; }

	if( jobAttributes.find( attribute ) != jobAttributes.end() ) { return false; }
	return true;
}

//
// Because '\t' is escaped by ClassAds, we can use it as a separator.
//

void PandadClassAdLogPlugin::addPandaJob( const char * globalJobID, const char * condorJobID ) {
	dprintf( D_FULLDEBUG, "PANDA: addPandaJob( %s, %s )\n", condorJobID, globalJobID );
	fprintf( pandad, "\vADD\t\"%s\"\t\"%s\"\n", globalJobID, condorJobID );
	fflush( pandad );
}

void PandadClassAdLogPlugin::updatePandaJob( const char * globalJobID, const char * attribute, const char * value ) {
	if( value == NULL ) { value = "null"; }
	dprintf( D_FULLDEBUG, "PANDA: updatePandaJob( %s, %s, %s )\n", globalJobID, attribute, value );
	fprintf( pandad, "\vUPDATE\t\"%s\"\t%s\t%s\n", globalJobID, attribute, value );
	fflush( pandad );
}

void PandadClassAdLogPlugin::removePandaJob( const char * globalJobID ) {
	dprintf( D_FULLDEBUG, "PANDA: removePandaJob( %s )\n", globalJobID );
	fprintf( pandad, "\vREMOVE\t\"%s\"\n", globalJobID );
	fflush( pandad );
}

bool PandadClassAdLogPlugin::getGlobalJobID( int cluster, int proc, std::string & globalJobID ) {
	static char const * hostname = NULL;
	if( hostname == NULL ) {
		// We can't block on name resolution, so use what we've already got.
		hostname = param( "FULL_HOSTNAME" );

		if( hostname == NULL || strlen( hostname ) == 0 ) {
			hostname = param( "HOSTNAME" );
		}

		if( hostname == NULL || strlen( hostname ) == 0 ) {
			hostname = param( "IP_ADDRESS" );
		}

		if( hostname == NULL || strlen( hostname ) == 0 ) {
			dprintf( D_ALWAYS, "Unable to determine hostname portion of global job IDs, using '[unknown]'.\n" );
			hostname = "[unknown]";
		}
	}
	formatstr( globalJobID, "%s:%d.%d", hostname, cluster, proc );
	return true;
}
