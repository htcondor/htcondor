//
// TO DO: It would be much less user-hostile if we autorenewed the proxy
// in question.  (It may make more logical sense to do this in the pandad,
// but it shouldn't have the privileges to do that.)
//

#include "condor_common.h"
#include "condor_debug.h"

#include "ClassAdLogPlugin.h"
#include "my_popen.h"
#include "condor_config.h"
#include "param_info.h"

// Make sure we get the schedd's version of GetJobAd, and not libcondorutil's.
extern ClassAd * ScheddGetJobAd( int, int, bool e = false, bool p = true );

// The things we can tell Panda about are:
// condorid, owner, submitted, run_time, st, pri, size, cmd, host, status,
// manager, executable, goodput, cpu_util, mbps, read_, write_, seek, xput,
// bufsize, blocksize, cpu_time, p_start_time, p_end_time, p_modif_time,
// p_factory, p_schedd, p_description, p_stdout, p_stderr

class CondorToPandaMap {
	public:
		static bool contains( const char * key );
		static const char * map( const char * key );

		typedef struct { const char * key; const char * value; } tuple;
};

/*
 * I don't know that HTCondor can acquire the following attributes.  Maybe
 * they're standard-universe specific?
 * { "", "mbps" }
 * { "", "read_" }
 * { "", "write_" }
 * { "", "seek" }
 * { "", "xput" }
 * { "", "bufsize" }
 * { "", "blocksize" }
 *
 * The following Panda attributes are derived and should be computed
 * in their database.  (I'm not sure why the first two are the same.)
 *	{ "", "goodput" }			// CPU_TIME / RUN_TIME
 *  { "", "cpu_util" }			// CPU_TIME / RUN_TIME
 *
 * I don't know what the following Panda attributes mean.
 *	{ "", "manager" }			//
 *	{ "", "p_schedd" }			//
 *	{ "", "p_factory" }			//
 *	{ "", "p_description" }		//
 *	{ "", "p_start_time" }		//
 *	{ "", "p_end_time" }		//
 *	{ "", "p_modif_time" }		//
 *
 * The following Panda attributes depend on more than one HTCondor attribute,
 * or aren't signalled by changes to job ad attributes.
 *	{ "", "host" }				// host submitted to, or host running on
 *	{ "", "cpu_time" }			// remote CPU (user+sys?) at last checkpoint
 *
 * The following tuples would duplicate HTCondor attributes in the map.
 *	{ "Cmd", "executable" }		// duplicates 'cmd'
 *
 * The following tuples would duplicate Panda attributes in the map.
 *	{ "ImageSize", "size " } 	// prefer 'MemoryUsage'
 *	{ "JobStatus", "status" }	// a different translation than for 'st'
 *
 * The remaining attributes are the usable 1-1 mapping.  We'll try to
 * convince Panda to do the translation on their side of the API.
 */
//	{ "JobStatus", "st" }
//  { "LastJobStatus", "st" }
const CondorToPandaMap::tuple sortedMap[] = {
	{ "Cmd", "cmd" },
	{ "Err", "p_stderr" },
	{ "JobPrio", "pri" },
	{ "Out", "p_stdout" },
	{ "Owner", "owner" },
	{ "QDate", "submitted" },
	{ "RemoteWallClockTime", "run_time" },
    { "ResidentSetSize", "size" },
};

bool CondorToPandaMap::contains( const char * key ) {
	return CondorToPandaMap::map( key ) != NULL;
}

const char * CondorToPandaMap::map( const char * key ) {
	const CondorToPandaMap::tuple * t = BinaryLookup<CondorToPandaMap::tuple>( sortedMap, sizeof( sortedMap ) / sizeof( CondorToPandaMap::tuple ), key, strcasecmp );
	if( t != NULL ) { return t->value; }
	return NULL;
}

// ----------------------------------------------------------------------------

class PandadClassAdLogPlugin : public ClassAdLogPlugin {
	public:
		PandadClassAdLogPlugin();
		~PandadClassAdLogPlugin();

		void earlyInitialize() { }
		void initialize() { }
		void shutdown() { }

		void beginTransaction();
		void newClassAd( const char * key );
		void destroyClassAd( const char * key );
		void setAttribute( const char * key, const char * attribute, const char * value );
		void deleteAttribute( const char * key, const char * name );
		void endTransaction();

	private:
		bool shouldIgnoreJob( const char * key, int & cluster, int & proc );
		bool getGlobalJobID( int cluster, int proc, std::string & globalJobID );
		bool shouldIgnoreAttribute( const char * attribute );

		void addPandaJob( const char * key, const char * globalJobID );
		void updatePandaJob( const char * globalJobID, const char * attribute, const char * value );
		void removePandaJob( const char * globalJobID );

		int lowestSeenCluster;
		int highestKnownCluster;

		FILE *	pandad;
};

// Required by plug-in API.  (Only if linked into the schedd?)
static PandadClassAdLogPlugin instance;

// ----------------------------------------------------------------------------

#if defined( WIN32 )
	#define DEVNULL "\\device\\null"
#else
	#define DEVNULL "/dev/null"
#endif // defined( WIN32 )

PandadClassAdLogPlugin::PandadClassAdLogPlugin() : ClassAdLogPlugin(), lowestSeenCluster( INT_MAX ), highestKnownCluster( 0 ) {
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
}

PandadClassAdLogPlugin::~PandadClassAdLogPlugin() {
	my_pclose( pandad );
}

void PandadClassAdLogPlugin::beginTransaction() {
	lowestSeenCluster = INT_MAX;

	//
	// If we want Panda to see job operations on jobs that were in the
	// queue before we started, we could probe the job queue to find the
	// highest known cluster ID.
	//
}

void PandadClassAdLogPlugin::newClassAd( const char * key ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: newClassAd( %s )\n", key );

	if( cluster <= highestKnownCluster ) {
		dprintf( D_ALWAYS, "PANDA: newClassAd() ignoring spurious call.\n" );
		return;
	}

	//
	// We see new class ads before they contain any useful information.  Wait
	// until after the transaction that created them completes to try to
	// inform Panda about them.  Because we know cluster IDs increase
	// monotonically, we can just store the lowest one we see during a given
	// transaction and scan upwards from there when the transaction completes.
	//
	if( cluster < lowestSeenCluster ) {
		lowestSeenCluster = cluster;
	}
}

void PandadClassAdLogPlugin::destroyClassAd( const char * key ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }
	if( proc == -1 ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: destroyClassAd( %s )\n", key );

	//
	// If we're deleting a job that was created in this transaction, we
	// won't send a create or update event for it when the transaction ends;
	// therefore don't send a delete event, either.
	//
	if( cluster > highestKnownCluster ) {
		return;
	}

	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID ) ) {
		dprintf( D_ALWAYS, "PANDA: destroyClassAd( %s ) failed to find global job ID.\n", key );
		return;
	}

	removePandaJob( globalJobID.c_str() );
}

void PandadClassAdLogPlugin::setAttribute( const char * key, const char * attribute, const char * value ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: setAttribute( %s, %s, %s ).\n", key, attribute, value );

	//
	// If this is an update to an existing job, update the job.  Otherwise,
	// it's an update to a job created in this transaction.  Delay updating
	// it until the whole job is available (when the transaction ends).
	//
	if( cluster > highestKnownCluster ) {
		return;
	}

	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID ) ) {
		dprintf( D_ALWAYS, "PANDA: setAttribute( %s, %s, %s ) failed to find global job ID.\n", key, attribute, value );
		return;
	}
	if( shouldIgnoreAttribute( attribute ) ) { return; }
	updatePandaJob( globalJobID.c_str(), attribute, value );
}

void PandadClassAdLogPlugin::deleteAttribute( const char * key, const char * attribute ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: deleteAttribute( %s, %s )\n", key, attribute );

	//
	// If we're deleting an attribute from an ad that was created in the
	// current transaction, we simply won't send it when we do our update
	// after the transaction ends.
	//
	if( cluster > highestKnownCluster ) {
		return;
	}

	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID ) ) {
		dprintf( D_ALWAYS, "PANDA: deleteAttribute( %s, %s ) failed to find global job ID.\n", key, attribute );
		return;
	}

	//
	// We need to the global job ID to call removePandaJob(), so do so
	// while we still can.
	//
	updatePandaJob( globalJobID.c_str(), attribute, NULL );
	if( strcmp( attribute, "GlobalJobId" ) == 0 ) {
		removePandaJob( globalJobID.c_str() );
	}
}

//
// After the end of a transaction, scan the job queue from the lowest
// new cluster ID we saw until we run out, adding and updating the jobs
// to Panda as we go.  (TO DO: Change the pandad so that we can combine
// the add and update operations here.)
//
void PandadClassAdLogPlugin::endTransaction() {

	for( int cluster = lowestSeenCluster; ; ++cluster ) {
		dprintf( D_FULLDEBUG, "PANDA: looking at cluster %d\n", cluster );
		ClassAd * clusterAd = ScheddGetJobAd( cluster, -1 );
		for( int proc = 0; ; ++proc ) {
			dprintf( D_FULLDEBUG, "PANDA: looking at proc [%d] %d\n", cluster, proc );
			ClassAd * jobAd = ScheddGetJobAd( cluster, proc );
			if( jobAd == NULL ) {
				// While not all queue management users create a cluster ad
				// (so it's OK for it to be NULL), they do have to start their
				// proc IDs at zero.
				if( proc == 0 ) { return; }
				break;
			}
			highestKnownCluster = cluster;

			std::string globalJobID;
			if( ! jobAd->LookupString( "GlobalJobId", globalJobID ) ) {
				dprintf( D_ALWAYS, "PANDA: endTransaction() found job without global job ID, ignoring it.\n" );
				continue;
			}

			if( globalJobID.empty() ) {
				dprintf( D_ALWAYS, "PANDA: endTransaction() found job with empty lobal job ID, ignoring it.\n" );
				continue;
			}

			char condorJobID[ PROC_ID_STR_BUFLEN ];
			ProcIdToStr( cluster, proc, condorJobID );
			addPandaJob( condorJobID, globalJobID.c_str() );


			ExprTree * valueExpr = NULL;
			const char * attribute = NULL;

			// Not all users of queue management create cluster ads.  However,
			// if they don't, the job ad proper will have all of the attributes.
			if( clusterAd != NULL ) {
				clusterAd->ResetExpr();
				while( clusterAd->NextExpr( attribute, valueExpr ) ) {
					dprintf( D_FULLDEBUG, "PANDA: endTransaction() found %s in cluster ad.\n", attribute );
					if( shouldIgnoreAttribute( attribute ) ) { continue; }

					std::string valueString;
					classad::ClassAdUnParser unparser;
					unparser.Unparse( valueString, valueExpr );
					updatePandaJob( globalJobID.c_str(), attribute, valueString.c_str() );
				}
			}

			jobAd->ResetExpr();
			while( jobAd->NextExpr( attribute, valueExpr ) ) {
				dprintf( D_FULLDEBUG, "PANDA: endTransaction() found %s in job ad.\n", attribute );
					if( shouldIgnoreAttribute( attribute ) ) { continue; }

					std::string valueString;
					classad::ClassAdUnParser unparser;
					unparser.Unparse( valueString, valueExpr );
					updatePandaJob( globalJobID.c_str(), attribute, valueString.c_str() );
			}
		}
	}
}

bool PandadClassAdLogPlugin::shouldIgnoreJob( const char * key, int & cluster, int & proc ) {
	if ( sscanf( key, "%d.%d", & cluster, & proc ) != 2 ) { return true; }

	// Ignore the spurious 0.0 ad we get on startup.
	if( cluster == 0 && proc == 0 ) { return true; }

	return false;
}

bool PandadClassAdLogPlugin::getGlobalJobID( int cluster, int proc, std::string & globalJobID ) {
	ClassAd * classAd = ScheddGetJobAd( cluster, proc );
	if( ! classAd ) {
		return false;
	}

	if( ! classAd->LookupString( "GlobalJobId", globalJobID ) ) {
		return false;
	}

	if( globalJobID.empty() ) {
		return false;
	}

	return true;
}

bool PandadClassAdLogPlugin::shouldIgnoreAttribute( const char * attribute ) {
	if( CondorToPandaMap::contains( attribute ) ) { return false; }
	return true;
}


// Because the schedd is single-threaded, and we only call this function
// once per fprintf(), it can safely return out of a static buffer.
char * unquote( const char * quotedString ) {
	static const unsigned bufferSize = 128;
	static char buffer[bufferSize];
	if( quotedString == NULL ) {
		dprintf( D_ALWAYS, "PANDA: Attempted to unqoute NULL string.\n" );
		buffer[0] = '\0';
		return buffer;
	}

	size_t i = 0;
	for( ; i < bufferSize && quotedString[i] != '\0'; ++i ) {
		buffer[i] = quotedString[i];
	}
	buffer[i] = '\0';

	if( i > 0 && buffer[i - 1] == '"' ) { buffer[i - 1] = '\0'; }
	if( buffer[0] == '"' ) { return & buffer[1]; }
	return &buffer[0];
}

void PandadClassAdLogPlugin::addPandaJob( const char * condorJobID, const char * globalJobID ) {
	dprintf( D_FULLDEBUG, "PANDA: addPandaJob( %s, %s )\n", condorJobID, globalJobID );
	fprintf( pandad, "\vADD %s %s\n", unquote( globalJobID ), condorJobID );
	fflush( pandad );
}

// TO DO: If value is NULL, remove the attribute.  (Panda may not support this.)
void PandadClassAdLogPlugin::updatePandaJob( const char * globalJobID, const char * attribute, const char * value ) {
	if( value == NULL ) { return; }
	dprintf( D_FULLDEBUG, "PANDA: updatePandaJob( %s, %s, %s )\n", unquote( globalJobID ), CondorToPandaMap::map( attribute ), value );
	fprintf( pandad, "\vUPDATE %s %s %s\n", unquote( globalJobID ), CondorToPandaMap::map( attribute ), value );
	fflush( pandad );
}

void PandadClassAdLogPlugin::removePandaJob( const char * globalJobID ) {
	dprintf( D_FULLDEBUG, "PANDA: removePandaJob( %s )\n", globalJobID );
	fprintf( pandad, "\vREMOVE %s\n", unquote( globalJobID ) );
	fflush( pandad );
}
