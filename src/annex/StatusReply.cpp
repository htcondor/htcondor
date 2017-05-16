#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "StatusReply.h"

void
printClassAds( unsigned count, const std::map< std::string, std::string > & instances, const std::string & annexID ) {
	// Compute the summary information for the annex ad.
	std::map< std::string, unsigned > statusCounts;
	std::map< std::string, std::vector< std::string > > statusInstanceList;
	for( auto i = instances.begin(); i != instances.end(); ++i ) {
		if( statusCounts.count( i->second ) == 0 ) {
			statusCounts[ i->second ] = 0;
		}
		statusCounts[ i->second ] += 1;

		statusInstanceList[ i->second ].push_back( i->first );
	}

	// Print the annex ad.
	ClassAd annexAd;
	annexAd.Assign( "MyType", "Annex" );
	annexAd.Assign( "AnnexID", annexID );
	annexAd.Assign( "TotalInstances", count );

	for( auto i = statusCounts.begin(); i != statusCounts.end(); ++i ) {
		std::string attr = i->first;
		if( attr == "[in pool]" ) { attr = "InPool"; }

		bool firstInWord = true;
		for( unsigned i = 0; i < attr.size(); ++i ) {
			if(! isalpha( attr[i] )) {
				attr.erase( i, 1 );
				--i;
				firstInWord = true;
				continue;
			}

			if( firstInWord ) {
				attr[i] = toupper( attr[i] );
				firstInWord = false;
			}
		}

		// If these ever become evaluated ClassAds, we could stop tracking
		// these and instead just make the attribute value 'size(...)'
		annexAd.Assign( ("NumInstances" + attr).c_str(), i->second );
		if( attr == "InPool" ) { continue; }

		std::string expr;
		std::vector< std::string > & instanceList = statusInstanceList[ i->first ];
		formatstr( expr, "{ \"%s\"", instanceList[0].c_str() );
		for( unsigned j = 1; j < instanceList.size(); ++j ) {
			formatstr( expr, "%s, \"%s\"", expr.c_str(), instanceList[j].c_str() );
		}
		expr += " }";
		annexAd.AssignExpr( (attr + "InstancesList").c_str(), expr.c_str() );
	}

	fPrintAd( stdout, annexAd );

	// Print an ad for each instance?
}

void
printHumanReadable( unsigned count, const std::map< std::string, std::string > & instances, const std::string & ) {
	std::map< std::string, unsigned > statusCounts;
	for( auto i = instances.begin(); i != instances.end(); ++i ) {
		if( statusCounts.count( i->second ) == 0 ) {
			statusCounts[ i->second ] = 0;
		}
		statusCounts[ i->second ] += 1;
	}

	fprintf( stdout, "%-14.14s %5.5s\n", "STATE", "COUNT" );
	for( auto i = statusCounts.begin(); i != statusCounts.end(); ++i ) {
		fprintf( stdout, "%-14.14s %5u\n", i->first.c_str(), i->second );
	}
	fprintf( stdout, "%-14.14s %5u\n", "TOTAL", count );

	std::map< std::string, std::vector< std::string > > instanceIDsByStatus;
	for( auto i = instances.begin(); i != instances.end(); ++i ) {
		instanceIDsByStatus[ i->second ].push_back( i->first );
	}

	if( statusCounts[ "[in pool]" ] != count ) {
		fprintf( stdout, "\n" );
		fprintf( stdout, "Instances not in the pool, grouped by state:\n" );
		for( auto i = instanceIDsByStatus.begin(); i != instanceIDsByStatus.end(); ++i ) {
			if( i->first == "[in pool]" ) { continue; }
			fprintf( stdout, "%s", i->first.c_str() );
			unsigned column = i->first.length();
			for( unsigned j = 0; j < i->second.size(); ++j ) {
				column += i->second[j].length() + 1;
				if( column >= 80 ) {
					fprintf( stdout, "\n" );
					column = i->first.length();
					fprintf( stdout, "%*.*s", column, column, "" );
					column += i->second[j].length() + 1;
				}
				fprintf( stdout, " %s", i->second[j].c_str() );
			}
			fprintf( stdout, "\n" );
		}
	}
}

int
StatusReply::operator() () {
	dprintf( D_FULLDEBUG, "StatusReply::operator()\n" );

	if( reply ) {
		std::string resultString;
		reply->LookupString( ATTR_RESULT, resultString );
		CAResult result = getCAResultNum( resultString.c_str() );

		if( result == CA_SUCCESS ) {
			std::map< std::string, std::string > instances;

			std::string iName;
			unsigned count = 0;
			do {
				formatstr( iName, "Instance%d", count );

				std::string instanceID;
				scratchpad->LookupString( (iName + ".instanceID").c_str(), instanceID );
				if( instanceID.empty() ) { break; }
				++count;

				// fprintf( stderr, "Found instance ID %s.\n", instanceID.c_str() );

				std::string status;
				scratchpad->LookupString( (iName + ".status").c_str(), status );
				ASSERT(! status.empty());
				instances[ instanceID ] = status;
			} while( true );

			if( count == 0 ) {
				fprintf( stdout, "Found no machines in that annex.\n" );
				goto cleanup;
			}

			std::string annexID, annexName;
			scratchpad->LookupString( "AnnexID", annexID );
			annexName = annexID.substr( 0, annexID.find( "_" ) );

			CondorQuery q( STARTD_AD );
			std::string constraint;
			formatstr( constraint, "AnnexName == \"%s\"", annexName.c_str() );
			q.addANDConstraint( constraint.c_str() );

			ClassAdList cal;
			CondorError errStack;
			CollectorList * collectors = CollectorList::create();
			/* QueryResult qr = */ collectors->query( q, cal, & errStack );
			delete collectors;

			cal.Rewind();
			ClassAd * cad = NULL;
			while( (cad = cal.Next()) != NULL ) {
				std::string name, instanceID, aName;
				cad->LookupString( "Name", name );
				cad->LookupString( "EC2InstanceID", instanceID );
				cad->LookupString( "AnnexName", aName );

				instances[ instanceID ] = "[in pool]";
			}

			if( wantClassAds ) {
				printClassAds( count, instances, annexID );
			} else {
				printHumanReadable( count, instances, annexID );
			}
		} else {
			std::string errorString;
			reply->LookupString( ATTR_ERROR_STRING, errorString );
			if( errorString.empty() ) {
				fprintf( stderr, "Status check failed (%s) without an error string.\n", resultString.c_str() );
			} else {
				fprintf( stderr, "%s\n", errorString.c_str() );
			}
		}

		cleanup:
		delete reply;
		reply = NULL;
	}

	if( gahp ) {
		daemonCore->Cancel_Timer( gahp->getNotificationTimerId() );

		delete gahp;
		gahp = NULL;
	}

	if( scratchpad ) {
		delete scratchpad;
		scratchpad = NULL;
	}

	return TRUE;
}

int
StatusReply::rollback() {
	dprintf( D_FULLDEBUG, "StatusReply::rollback() -- calling operator().\n" );
	return (* this)();
}
