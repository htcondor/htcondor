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

#include <iostream>
#include <sstream>
#include <set>

// Make sure we get the schedd's version of GetJobAd, and not libcondorutil's.
extern ClassAd * ScheddGetJobAd( int, int, bool e = false, bool p = true );

class CondorToPandaMap {
	public:
		static bool contains( const char * key );
		static const char * map( const char * key );

		typedef struct { const char * key; const char * value; } tuple;
};

//
// Map from HTCondor job attributes to PanDA monitor attributes.  Ideally,
// this list would be empty, and there'd also be no hard-coded attribute
// mangling in the pandad (which currently converts QDate from TIMESTAMP
// to DATETIME).  At that point, we could probably just add GlobalJobID
// to PANDA_REQUIRED_JOB_ATTRIBUTES and not have /any/ special cases.
//
const CondorToPandaMap::tuple sortedMap[] = {
	{ "Cmd", "cmd" },
	{ "Err", "p_stderr" },
	{ "JobDescription", "p_description" },
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
		void initialize() { scheddInitialized = true; }
		void shutdown() { }

		void beginTransaction();
		void newClassAd( const char * key );
		void destroyClassAd( const char * key );
		void setAttribute( const char * key, const char * attribute, const char * value );
		void deleteAttribute( const char * key, const char * name );
		void endTransaction();

	private:
		std::string generateAttributesAlwaysSent( ClassAd * jobAd, const char * attribute = NULL );
		bool userWantsToIgnoreJob( ClassAd * jobAd );
		bool shouldIgnoreJob( const char * key, int & cluster, int & proc );
		bool getGlobalJobID( int cluster, int proc, std::string & globalJobID, ClassAd * & jobAd );
		bool shouldIgnoreAttribute( const char * attribute );

		void addPandaJob( const char * key, const char * globalJobID, ClassAd * jobAd );
		void updatePandaJob( const char * globalJobID, const char * attribute, const char * value, ClassAd * jobAd );
		void removePandaJob( const char * globalJobID, ClassAd * jobAd );

		bool inTransaction;
		int lowestNewCluster;
		int highestNewCluster;

		std::set< std::string > jobAttributes;
		std::set< std::string > requiredAttributes;
		std::set< std::string > attributesAlwaysSent;

		ExprTree * jobFilter;

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

PandadClassAdLogPlugin::PandadClassAdLogPlugin() : ClassAdLogPlugin(), inTransaction( false ), lowestNewCluster( INT_MAX ), highestNewCluster( 0 ), jobFilter( NULL ), pandad( NULL ), scheddInitialized( false ) {
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
	std::string raString;
	param( raString, "PANDA_REQUIRED_JOB_ATTRIBUTES" );
	if( ! raString.empty() ) {
		std::istringstream raStream( raString );
		std::string attribute;
		while( std::getline( raStream, attribute, ' ' ) ) {
			requiredAttributes.insert( attribute );
		}
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

	std::string jfString;
	param( jfString, "PANDA_JOB_FILTER" );
	if( ! jfString.empty() ) {
		classad::ClassAdParser parser;
		if( ! parser.ParseExpression( jfString, jobFilter, true ) ) {
			dprintf( D_ALWAYS, "PANDA: Failed to parse job filter '%s', ignoring.\n", jfString.c_str() );
			jobFilter = NULL;
		}
	}

	// This doesn't handle commas, but boy is it simple.
	std::string aasString;
	param( aasString, "PANDA_ALWAYS_SEND_ATTRIBUTES" );
	if( ! aasString.empty() ) {
		std::istringstream aasStream( aasString );
		std::string attribute;
		while( std::getline( aasStream, attribute, ' ' ) ) {
			attributesAlwaysSent.insert( attribute );
		}
	}
}

PandadClassAdLogPlugin::~PandadClassAdLogPlugin() {
	my_pclose( pandad );
}

void PandadClassAdLogPlugin::beginTransaction() {
	inTransaction = true;
	lowestNewCluster = INT_MAX;
	highestNewCluster = 0;
}

void PandadClassAdLogPlugin::newClassAd( const char * key ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: newClassAd( %s )\n", key );

	//
	// We see new class ads before they contain any useful information.  Wait
	// until after the transaction that created them completes to try to
	// inform Panda about them.  Because we know cluster IDs increase
	// monotonically, we can just store the lowest one we see during a given
	// transaction and scan upwards from there when the transaction completes.
	// We need to store the highestNewCluster because we can skip cluster IDs.
	//
	// If we're not in a transaction, we can't do anything useful, but since
	// that should never happen, make a note of it in the log.
	//

	if( ! inTransaction ) {
		if( scheddInitialized ) { dprintf( D_ALWAYS, "PANDA: newClassAd( %s ) saw a new job outside of transaction.  Unable to process; will ignore.\n", key ); }
		return;
	}

	if( cluster < lowestNewCluster ) {
		lowestNewCluster = cluster;
	}
	if( cluster > highestNewCluster ) {
		highestNewCluster = cluster;
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
	// If we're not in a transaction, send the event and hope for the best.
	//
	if( inTransaction && cluster >= lowestNewCluster ) { return; }

	ClassAd * jobAd = NULL;
	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID, jobAd ) ) {
		if( scheddInitialized ) { dprintf( D_ALWAYS, "PANDA: destroyClassAd( %s ) failed to find global job ID.\n", key ); }
		return;
	}

	if( userWantsToIgnoreJob( jobAd ) ) { return; }
	removePandaJob( globalJobID.c_str(), jobAd );
}

void PandadClassAdLogPlugin::setAttribute( const char * key, const char * attribute, const char * value ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: setAttribute( %s, %s, %s ).\n", key, attribute, value );

	//
	// Ignore updates to clusters we'll be handling in EndTransaction().  We'll
	// always see newClassAd() before setAttribute() for the same ad, so we
	// won't miss anything this way even if the ads are created out of order.
	//
	// If we're not in a transaction, send the event and hope for the best.
	//
	if( inTransaction && cluster >= lowestNewCluster ) { return; }

	ClassAd * jobAd = NULL;
	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID, jobAd ) ) {
		if( scheddInitialized ) { dprintf( D_ALWAYS, "PANDA: setAttribute( %s, %s, %s ) failed to find global job ID.\n", key, attribute, value ); }
		return;
	}
	if( shouldIgnoreAttribute( attribute ) ) { return; }
	if( userWantsToIgnoreJob( jobAd ) ) { return; }
	updatePandaJob( globalJobID.c_str(), attribute, value, jobAd );
}

void PandadClassAdLogPlugin::deleteAttribute( const char * key, const char * attribute ) {
	int cluster = 0, proc = 0;
	if( shouldIgnoreJob( key, cluster, proc ) ) { return; }

	dprintf( D_FULLDEBUG, "PANDA: deleteAttribute( %s, %s )\n", key, attribute );

	//
	// Ignore updates to clusters we'll be handling in EndTransaction().  We'll
	// always see newClassAd() before deleteAttribute() for the same ad, so we
	// won't miss anything this way even if the ads are created out of order.
	//
	// If we're not in a transaction, send the event and hope for the best.
	//
	if( inTransaction && cluster >= lowestNewCluster ) { return; }

	ClassAd * jobAd = NULL;
	std::string globalJobID;
	if( ! getGlobalJobID( cluster, proc, globalJobID, jobAd ) ) {
		if( scheddInitialized ) { dprintf( D_ALWAYS, "PANDA: deleteAttribute( %s, %s ) failed to find global job ID.\n", key, attribute ); }
		return;
	}

	//
	// We need to the global job ID to call removePandaJob(), so do so
	// while we still can.
	//
	if( shouldIgnoreAttribute( attribute ) ) { return; }
	if( userWantsToIgnoreJob( jobAd ) ) { return; }
	updatePandaJob( globalJobID.c_str(), attribute, NULL, jobAd );
	if( strcmp( attribute, "GlobalJobId" ) == 0 ) {
		removePandaJob( globalJobID.c_str(), jobAd );
	}
}

//
// After the end of a transaction, scan the job queue from the lowest
// new cluster ID we saw until we run out, adding and updating the jobs
// to Panda as we go.
//
// The pandad coalesces updates for us.
//
void PandadClassAdLogPlugin::endTransaction() {
	if( ! inTransaction ) {
		if( scheddInitialized ) { dprintf( D_ALWAYS, "PANDA: endTransaction() called but we're not in a transaction.  Ignoring.\n" ); }
		return;
	}

	for( int cluster = lowestNewCluster; cluster <= highestNewCluster; ++cluster ) {
		dprintf( D_FULLDEBUG, "PANDA: looking at cluster %d\n", cluster );
		ClassAd * clusterAd = ScheddGetJobAd( cluster, -1 );
		for( int proc = 0; ; ++proc ) {
			dprintf( D_FULLDEBUG, "PANDA: looking at proc [%d] %d\n", cluster, proc );
			ClassAd * jobAd = ScheddGetJobAd( cluster, proc );
			if( jobAd == NULL ) {
				// We claim here that proc IDs can't be skipped.
				break;
			}

			std::string globalJobID;
			if( ! jobAd->LookupString( "GlobalJobId", globalJobID ) ) {
				if( scheddInitialized ) { dprintf( D_ALWAYS, "PANDA: endTransaction() found job without global job ID, ignoring it.\n" ); }
				continue;
			}

			if( globalJobID.empty() ) {
				if( scheddInitialized ) { dprintf( D_ALWAYS, "PANDA: endTransaction() found job with empty lobal job ID, ignoring it.\n" ); }
				continue;
			}

			//
			// We need to chain the child and parent ads together so we can
			// properly evaluate the job filter.  Since this also makes it
			// more convenient to do the required-attribute filter and update
			// the new ad, we'll do in unconditionally.  We must unchain the
			// ad before we call back into the schedd to preserve semantics.
			//
			if( clusterAd != NULL ) { jobAd->ChainToAd( clusterAd ); }
			if( userWantsToIgnoreJob( jobAd ) ) {
				jobAd->Unchain();
				continue;
			}

			char condorJobID[ PROC_ID_STR_BUFLEN ];
			ProcIdToStr( cluster, proc, condorJobID );
			addPandaJob( condorJobID, globalJobID.c_str(), jobAd );

			ExprTree * valueExpr = NULL;
			const char * attribute = NULL;

			jobAd->ResetExpr();
			while( jobAd->NextExpr( attribute, valueExpr ) ) {
				dprintf( D_FULLDEBUG, "PANDA: endTransaction() found %s in job ad.\n", attribute );
					if( shouldIgnoreAttribute( attribute ) ) { continue; }

					std::string valueString;
					classad::ClassAdUnParser unparser;
					unparser.Unparse( valueString, valueExpr );
					updatePandaJob( globalJobID.c_str(), attribute, valueString.c_str(), jobAd );
			}

			if( clusterAd != NULL ) { jobAd->Unchain(); }
		}
	}

	inTransaction = false;
}

bool PandadClassAdLogPlugin::shouldIgnoreJob( const char * key, int & cluster, int & proc ) {
	if ( sscanf( key, "%d.%d", & cluster, & proc ) != 2 ) { return true; }

	// Ignore the spurious 0.0 ad we get on startup.
	if( cluster == 0 && proc == 0 ) { return true; }

	// Ignore cluster ads.
	if( proc == -1 ) { return true; }

	return false;
}

bool PandadClassAdLogPlugin::userWantsToIgnoreJob( ClassAd * jobAd ) {
	assert( jobAd != NULL );

	if( jobFilter == NULL && requiredAttributes.empty() ) {
		return false;
	}

	if( jobFilter != NULL ) {
		classad::Value v;
		jobFilter->SetParentScope( jobAd );
		bool evaluated = jobAd->EvaluateExpr( jobFilter, v );
		if( ! evaluated ) {
			dprintf( D_ALWAYS, "PANDA: Ignoring job because filter failed to evaluate.\n" );
			return true;
		}

		bool b;
		if( v.IsBooleanValue( b ) && b ) {
			dprintf( D_FULLDEBUG, "PANDA: Job permitted by filter.\n" );
			return false;
		} else {
			dprintf( D_FULLDEBUG, "PANDA: Job forbidden by filter.\n" );
			return true;
		}
	}

	if( ! requiredAttributes.empty() ) {
		std::set< std::string >::iterator i = requiredAttributes.begin();
		for( ; i != requiredAttributes.end(); ++i ) {
			if( jobAd->Lookup( * i ) == NULL ) {
				dprintf( D_FULLDEBUG, "PANDA: Ignoring job because required attribute '%s' is missing.\n", i->c_str() );
				return true;
			}
		}

		return false;
	}

	dprintf( D_ALWAYS, "Ignoring job because it arrive at unreachable code.\n" );
	return true;
}

bool PandadClassAdLogPlugin::getGlobalJobID( int cluster, int proc, std::string & globalJobID, ClassAd * & jobAd ) {
	jobAd = ScheddGetJobAd( cluster, proc );
	if( jobAd == NULL ) {
		return false;
	}

	if( ! jobAd->LookupString( "GlobalJobID", globalJobID ) ) {
		return false;
	}

	if( globalJobID.empty() ) {
		return false;
	}

	return true;
}

bool PandadClassAdLogPlugin::shouldIgnoreAttribute( const char * attribute ) {
	if( CondorToPandaMap::contains( attribute ) ) { return false; }
	if( jobAttributes.find( attribute ) != jobAttributes.end() ) { return false; }
	return true;
}

std::string PandadClassAdLogPlugin::generateAttributesAlwaysSent( ClassAd * jobAd, const char * ignore ) {
	std::string aas = "";
	std::set< std::string >::iterator i = attributesAlwaysSent.begin();
	for( ; i != attributesAlwaysSent.end(); ++i ) {
		if( ignore != NULL && ignore == * i ) { continue; }
		std::string value;
		ExprTree * expr = jobAd->Lookup( * i );
		if( expr == NULL ) {
			dprintf( D_ALWAYS, "PANDA: You requested that attribute '%s' always be sent, but it doesn't exist in this ad.  You may need to set PANDA_REQUIRED_JOB_ATTIBUTES or adjust PANDA_JOB_FILTER.  Sending null, instead.\n", i->c_str() );
			aas += "\t" + *i + "\t" + "null";
			continue;
		}
		classad::ClassAdUnParser unparser;
		unparser.Unparse( value, expr );
		aas += "\t" + *i + "\t" + value;
	}
	return aas;
}

//
// Because '\t' is escaped by ClassAds, we can use it as a separator.
//

void PandadClassAdLogPlugin::addPandaJob( const char * condorJobID, const char * globalJobID, ClassAd * jobAd ) {
	dprintf( D_FULLDEBUG, "PANDA: addPandaJob( %s, %s )\n", condorJobID, globalJobID );
	fprintf( pandad, "\vADD\t\"%s\"\t\"%s\"%s\n", globalJobID, condorJobID, generateAttributesAlwaysSent( jobAd ).c_str() );
	fflush( pandad );
}

void PandadClassAdLogPlugin::updatePandaJob( const char * globalJobID, const char * attribute, const char * value, ClassAd * jobAd ) {
	if( value == NULL ) { value = "null"; }
	const char * mappedAttribute = CondorToPandaMap::map( attribute );
	if( mappedAttribute == NULL ) { mappedAttribute = attribute; }
	dprintf( D_FULLDEBUG, "PANDA: updatePandaJob( %s, %s, %s )\n", globalJobID, mappedAttribute, value );
	fprintf( pandad, "\vUPDATE\t\"%s\"\t%s\t%s%s\n", globalJobID, mappedAttribute, value, generateAttributesAlwaysSent( jobAd, attribute ).c_str() );
	fflush( pandad );
}

void PandadClassAdLogPlugin::removePandaJob( const char * globalJobID, ClassAd * jobAd ) {
	dprintf( D_FULLDEBUG, "PANDA: removePandaJob( %s )\n", globalJobID );
	fprintf( pandad, "\vREMOVE\t\"%s\"%s\n", globalJobID, generateAttributesAlwaysSent( jobAd ).c_str() );
	fflush( pandad );
}
