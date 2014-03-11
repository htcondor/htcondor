// TO DO: It would be much less user-hostile if we autorenewed the proxy
// in question.  (It may make more logical sense to do this in the pandad,
// but it shouldn't have the privileges to do that.)

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
	{ "JobPrio", "pri" },
	{ "Owner", "owner" },
	{ "QDate", "submitted" },
	{ "RemoteWallClockTime", "run_time" },
    { "ResidentSetSize", "size" },
	{ "Out", "p_stdout" },
	{ "Err", "p_stderr" },
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

		// It turns out that new job ads don't contain the GlobalJobID,
		// which mean we can't use this notification for Panda.
		void newClassAd( const char * ) { }
		void destroyClassAd( const char * key );
		void setAttribute( const char * key, const char * attribute, const char * value );
		void deleteAttribute( const char * key, const char * name );

	private:
		bool shouldIgnoreJob( const char * key, int & cluster, int & proc );
		bool getGlobalJobID( int cluster, int proc, std::string & globalJobID );
		bool shouldIgnoreAttribute( const char * attribute );

		void addPandaJob( const char * key, const char * globalJobID );
		void updatePandaJob( const char * key, const char * globalJobID, const char * attribute, const char * value );
		void removePandaJob( const char * key, const char * globalJobID );

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

 PandadClassAdLogPlugin::PandadClassAdLogPlugin() : ClassAdLogPlugin() {
	std::string binary;
	param( binary, "PANDA" );

	const char * arguments[] = { binary.c_str(), NULL };
	pandad = my_popenv( arguments, "w", 0 );

	if( pandad == NULL ) {
		dprintf( D_ALWAYS, "PandadClassAdLogPlugin: failed to start pandad, monitor will not be updated.\n" );

		pandad = fopen( DEVNULL, "w" );
	}
}

PandadClassAdLogPlugin::~PandadClassAdLogPlugin() {
	my_pclose( pandad );
}

void PandadClassAdLogPlugin::destroyClassAd( const char * key ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PandadClassAdLogPlugin::destroyClassAd( %s )\n", key );

	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID ) ) {
		dprintf( D_ALWAYS, "PandadClassAdLogPlugin::destroyClassAd( %s ) failed to find global job ID.\n", key );
		return;
	}

	removePandaJob( key, globalJobID.c_str() );
}

void PandadClassAdLogPlugin::setAttribute( const char * key, const char * attribute, const char * value ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PandadClassAdLogPlugin::setAttribute( %s, %s, %s )\n", key, attribute, value );

	if( strcmp( attribute, "GlobalJobId" ) == 0 ) {
		addPandaJob( key, value );

		// If we're adding this job for the first time, update the attributes
		// that we care about -- it could have some, if this a schedd restart.
		ClassAd * classAd = ScheddGetJobAd( cluster, proc );
		if( classAd == NULL ) {
			dprintf( D_ALWAYS, "PandadClassAdLogPlugin::setAttribute( %s, %s, %s ) unable to update attributes after adding job: no job ad found.\n", key, attribute, value );
			return;
		}

		ClassAd::const_iterator i = classAd->begin();
		for( ; i != classAd->end(); ++i ) {
			std::string attribute = i->first;
			if( shouldIgnoreAttribute( attribute.c_str() ) ) { continue; }

			ExprTree * valueExpr = i->second;
			std::string valueString;
			classad::ClassAdUnParser unparser;
			unparser.Unparse( valueString, valueExpr );

			updatePandaJob( key, value, attribute.c_str(), valueString.c_str() );
		}
	} else {
		std::string globalJobID;
		if( ! getGlobalJobID( cluster, proc, globalJobID ) ) {
			dprintf( D_ALWAYS, "PandadClassAdLogPlugin::setAttribute( %s, %s, %s ) failed to find global job ID.\n", key, attribute, value );
			return;
		}
		updatePandaJob( key, globalJobID.c_str(), attribute, value );
	}
}

void PandadClassAdLogPlugin::deleteAttribute( const char * key, const char * attribute ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PandadClassAdLogPlugin::deleteAttribute( %s, %s )\n", key, attribute );

	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID ) ) {
		dprintf( D_ALWAYS, "PandadClassAdLogPlugin::deleteAttribute( %s, %s ) failed to find global job ID.\n", key, attribute );
		return;
	}

	// The global job ID is Panda's primary key, so we can't do any
	// further updates to this job after it;s deleted.
	if( strcmp( attribute, "GlobalJobId" ) == 0 ) {
		removePandaJob( key, globalJobID.c_str() );
	} else {
		updatePandaJob( key, globalJobID.c_str(), attribute, NULL );
	}
}

bool PandadClassAdLogPlugin::shouldIgnoreJob( const char * key, int & cluster, int & proc ) {
	if ( sscanf( key, "%d.%d", & cluster, & proc ) != 2 ) { return true; }

	// Ignore the spurious 0.0 ad we get on startup.
	if( cluster == 0 && proc == 0 ) { return true; }

	// Ignore the <clusterId>.-1 ad for the grid manager.
	if( proc == -1 ) { return true; }

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
	dprintf( D_FULLDEBUG, "PANDA: considering '%s'\n", attribute );
	if( CondorToPandaMap::contains( attribute ) ) { return false; }
	return true;
}


// Because the schedd is single-threaded, and we only call this function
// once per fprintf(), it can safely return out of a static buffer.
char * unquote( const char * quotedString ) {
	static const unsigned bufferSize = 128;
	static char buffer[bufferSize];
	if( quotedString == NULL ) {
		dprintf( D_ALWAYS, "PandadClassAdLogPlugin: Attempted to unqoute NULL string.\n" );
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
	dprintf( D_FULLDEBUG, "addPandaJob( %s, %s )\n", condorJobID, globalJobID );
	fprintf( pandad, "ADD %s %s\n", unquote( globalJobID ), condorJobID );
	fflush( pandad );
}

// FIXME: If value is NULL, remove the attribute.  (Panda may not support this.)
void PandadClassAdLogPlugin::updatePandaJob( const char * condorJobID, const char * globalJobID, const char * attribute, const char * value ) {
	if( value == NULL ) { return; }
	dprintf( D_FULLDEBUG, "updatePandaJob( %s, %s, %s, %s )...\n", condorJobID, globalJobID, attribute, value );
	if( shouldIgnoreAttribute( attribute ) ) { return; }
	dprintf( D_FULLDEBUG, "... will not ignore attribute\n", condorJobID, globalJobID, attribute, value );
	fprintf( pandad, "UPDATE %s %s %s\n", unquote( globalJobID ), CondorToPandaMap::map( attribute ), value );
	fflush( pandad );
}

void PandadClassAdLogPlugin::removePandaJob( const char * condorJobID, const char * globalJobID ) {
	dprintf( D_FULLDEBUG, "removePandaJob( %s, %s )\n", condorJobID, globalJobID );
	fprintf( pandad, "REMOVE %s\n", unquote( globalJobID ) );
	fflush( pandad );
}
