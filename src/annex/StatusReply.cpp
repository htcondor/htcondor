#include "condor_common.h"
#include "condor_config.h"
#include "condor_md.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "StatusReply.h"

static void
printClassAds(	const std::map< std::string, std::string > & instances,
				const std::string & annexID,
				ClassAd * command,
				std::map< std::string, std::string > & annexes );

static void
printClassAdsSummary(	const std::map< std::string, std::string > & instances,
						ClassAd * command,
						std::map< std::string, std::string > & annexes ) {
	std::map< std::string, std::map< std::string, std::string > > instanceListsByAnnex;
	for( auto i = instances.begin(); i != instances.end(); ++i ) {
		const std::string & status = i->second;
		const std::string & instanceID = i->first;
		const std::string & annexName = annexes[ instanceID ];

		instanceListsByAnnex[ annexName ][ instanceID ] = status;
	}

	std::map< std::string, std::string > dummy;
	for( auto i = instanceListsByAnnex.begin(); i != instanceListsByAnnex.end(); ++i ) {
		const std::string & annexName = i->first;
		std::map< std::string, std::string > & instanceList = i->second;
		printClassAds( instanceList, annexName, command, dummy );
		fprintf( stdout, "\n" );
	}
}


static void
printClassAds(	const std::map< std::string, std::string > & instances,
				const std::string & annexID,
				ClassAd * command,
				std::map< std::string, std::string > & annexes ) {
	if( annexID.empty() ) {
		printClassAdsSummary( instances, command, annexes );
		return;
	}

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
	annexAd.Assign( "AnnexID", annexID );
	annexAd.Assign( "TotalInstances", instances.size() );

	// Add the attributes necessary to insert it into the collector.  The
	// name must be unique, but the annex ID is only is only unique per-
	// credential.  What we actually want to uniquify with is the root
	// account ID, but we don't have that.  Instead, we'll hash the
	// public (access) key ID.
	annexAd.Assign( "MyType", "Annex" );

	std::string publicKeyFile;
	param( publicKeyFile, "ANNEX_DEFAULT_ACCESS_KEY_FILE" );
	command->LookupString( "PublicKeyFile", publicKeyFile );

	Condor_MD_MAC mmc;
	if(! mmc.addMDFile( publicKeyFile.c_str() )) {
		fprintf( stderr, "Failed to hash the access (public) key file, aborting.\n" );
		return;
	}
	unsigned char * md = mmc.computeMD();
	std::string md5;
	for( int i = 0; i < MAC_SIZE; ++i ) {
		formatstr_cat( md5, "%02x", (int)(md[i]) );
	}
	free( md );

	std::string name;
	formatstr( name, "%s [%s]", annexID.c_str(), md5.c_str() );
	annexAd.Assign( ATTR_NAME, name );

	for( auto i = statusCounts.begin(); i != statusCounts.end(); ++i ) {
		std::string attr = i->first;
		if( attr == "in-pool" ) { attr = "InPool"; }

		bool firstInWord = true;
		for( unsigned n = 0; n < attr.size(); ++n ) {
			if(! isalpha( attr[n] )) {
				attr.erase( n, 1 );
				--n;
				firstInWord = true;
				continue;
			}

			if( firstInWord ) {
				attr[n] = toupper( attr[n] );
				firstInWord = false;
			}
		}

		// If these ever become evaluated ClassAds, we could stop tracking
		// these and instead just make the attribute value 'size(...)'
		annexAd.Assign( ("NumInstances" + attr), i->second );
		if( attr == "InPool" ) { continue; }

		std::string expr;
		std::vector< std::string > & instanceList = statusInstanceList[ i->first ];
		formatstr( expr, "{ \"%s\"", instanceList[0].c_str() );
		for( unsigned j = 1; j < instanceList.size(); ++j ) {
			formatstr( expr, "%s, \"%s\"", expr.c_str(), instanceList[j].c_str() );
		}
		expr += " }";
		annexAd.AssignExpr( (attr + "InstancesList"), expr.c_str() );
	}

	std::string ncaFormat;
	classad::ClassAdUnParser caup;
	caup.Unparse( ncaFormat, & annexAd );
	dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", ncaFormat.c_str() );

	fPrintAd( stdout, annexAd );
}

static void
printHumanReadableSummary(	std::map< std::string, std::string > & instances,
							std::map< std::string, std::string > & annexes ) {
	// Do some aggregation.
	std::set< std::string > statuses;
	std::map< std::string, unsigned > total;
	std::map< std::string, std::map< std::string, unsigned > > output;
	for( auto i = annexes.begin(); i != annexes.end(); ++i ) {
		const std::string & annexName = i->second;
		const std::string & instanceID = i->first;

		ASSERT( instances.count( instanceID ) > 0 );
		const std::string & status = instances[ instanceID ];
		statuses.insert( status );

		if( output.count( annexName ) == 0
		 || output[ annexName ].count( status ) == 0 ) {
			output[ annexName ][ status ] = 0;
		}
		output[ annexName ][ instances[ instanceID ] ] += 1;

		if( total.count( annexName ) == 0 ) {
			total[ annexName ] = 0;
		}
		total[ annexName ] += 1;
	}

	// Print the [annex-]NAME TOTAL <status1> ... <statusN> table.
	bool instancesNotInPool = false;
	unsigned longestStatus = 0;
	fprintf( stdout, "%-27.27s %5.5s", "NAME", "TOTAL" );
	for( auto i = statuses.begin(); i != statuses.end(); ++i ) {
		fprintf( stdout, " %s", i->c_str() );
		if( i->length() > longestStatus ) { longestStatus = i->length(); }
	}
	fprintf( stdout, "\n" );

	std::string auditString;
	for( auto i = total.begin(); i != total.end(); ++i ) {
		unsigned annexTotal = i->second;
		const std::string & annexName = i->first;

		formatstr( auditString, "%s%s: %u total,", auditString.c_str(),
			annexName.c_str(), annexTotal );
		fprintf( stdout, "%-27.27s %5u", annexName.c_str(), annexTotal );
		for( auto j = statuses.begin(); j != statuses.end(); ++j ) {
			auto & as = output[ annexName ];
			const std::string & status = * j;

			if( as.count( status ) == 0 ) {
				formatstr( auditString, "%s %u %s,", auditString.c_str(),
					0, status.c_str() );
				fprintf( stdout, " %*s", (int)status.length(), "0" );
			} else {
				if( status != "in-pool" ) { instancesNotInPool += as[ status ]; }
				formatstr( auditString, "%s %u %s,", auditString.c_str(),
					as[ status ], status.c_str() );
				fprintf( stdout, " %*d", (int)status.length(), as[ status ] );
			}
		}
		auditString.erase( auditString.length() - 1 );
		auditString += "; ";
		fprintf( stdout, "\n" );
	}
	auditString.erase( auditString.length() - 2 );
	dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", auditString.c_str() );
	auditString.clear();

	if(! instancesNotInPool) {
		return;
	}

	// Print the table separator.
	fprintf( stdout, "\n" );

	// FIXME: Should this formatted as a table, or just as a series of lines?
	// Print the [annex-]NAME STATE INSTANCES... table.
	fprintf( stdout, "%-27.27s %-*s INSTANCES...\n", "NAME", longestStatus, "STATUS" );
	for( auto i = output.begin(); i != output.end(); ++i ) {
		const std::string & annexName = i->first;

		auto & as = i->second;
		for( auto j = as.begin(); j != as.end(); ++j ) {
			const std::string & status = j->first;
			if( status == "in-pool" ) { continue; }

			formatstr( auditString, "%s%s %s:", auditString.c_str(),
				annexName.c_str(), status.c_str() );
			fprintf( stdout, "%-27.27s %-*s", annexName.c_str(), longestStatus, status.c_str() );
			for( auto k = instances.begin(); k != instances.end(); ++k ) {
				const std::string & instanceID = k->first;
				const std::string & instanceStatus = k->second;

				if( status == instanceStatus && annexName == annexes[ instanceID ] ) {
					formatstr( auditString, "%s %s", auditString.c_str(),
						instanceID.c_str() );
					fprintf( stdout, " %s", instanceID.c_str() );
				}
			}
			auditString += "; ";
			fprintf( stdout, "\n" );
		}
	}

	if(! auditString.empty()) {
		auditString.erase( auditString.length() - 2 );
		dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", auditString.c_str() );
	}
}

static void
printHumanReadable( std::map< std::string, std::string > & instances,
					const std::string & annexID,
					std::map< std::string, std::string > & annexes ) {
	if( annexID.empty() ) {
		printHumanReadableSummary( instances, annexes );
		return;
	}

	std::string auditString;

	std::map< std::string, unsigned > statusCounts;
	for( auto i = instances.begin(); i != instances.end(); ++i ) {
		if( statusCounts.count( i->second ) == 0 ) {
			statusCounts[ i->second ] = 0;
		}
		statusCounts[ i->second ] += 1;
	}

	fprintf( stdout, "%-14.14s %5.5s\n", "STATE", "COUNT" );
	for( auto i = statusCounts.begin(); i != statusCounts.end(); ++i ) {
		formatstr( auditString, "%s%s %u, ", auditString.c_str(), i->first.c_str(), i->second );
		fprintf( stdout, "%-14.14s %5u\n", i->first.c_str(), i->second );
	}
	formatstr( auditString, "%stotal %zu", auditString.c_str(), instances.size() );
	fprintf( stdout, "%-14.14s %5zu\n", "TOTAL", instances.size() );

	std::map< std::string, std::vector< std::string > > instanceIDsByStatus;
	for( auto i = instances.begin(); i != instances.end(); ++i ) {
		instanceIDsByStatus[ i->second ].push_back( i->first );
	}

	if( statusCounts[ "in-pool" ] != instances.size() ) {
		formatstr( auditString, "%s; ", auditString.c_str() );
		fprintf( stdout, "\n" );
		fprintf( stdout, "Instances not in the pool, grouped by state:\n" );
		for( auto i = instanceIDsByStatus.begin(); i != instanceIDsByStatus.end(); ++i ) {
			if( i->first == "in-pool" ) { continue; }
			formatstr( auditString, "%s%s ", auditString.c_str(), i->first.c_str() );
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
				formatstr( auditString, "%s%s ", auditString.c_str(), i->second[j].c_str() );
				fprintf( stdout, " %s", i->second[j].c_str() );
			}
			formatstr( auditString, "%s; ", auditString.substr( 0, auditString.length() - 1 ).c_str() );
			fprintf( stdout, "\n" );
		}
		auditString.erase( auditString.length() - 2 );
	}

	dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", auditString.c_str() );
}

int
StatusReply::operator() () {
	dprintf( D_FULLDEBUG, "StatusReply::operator()\n" );

	if( reply ) {
		std::string resultString;
		reply->LookupString( ATTR_RESULT, resultString );
		CAResult result = getCAResultNum( resultString.c_str() );

		if( result == CA_SUCCESS ) {
			std::map< std::string, std::string > annexes;
			std::map< std::string, std::string > instances;

			std::string iName;
			unsigned count = 0;
			do {
				formatstr( iName, "Instance%d", count );

				std::string instanceID;
				scratchpad->LookupString( (iName + ".instanceID"), instanceID );
				if( instanceID.empty() ) { break; }
				++count;

				// fprintf( stderr, "Found instance ID %s.\n", instanceID.c_str() );

				std::string status;
				scratchpad->LookupString( (iName + ".status"), status );
				ASSERT(! status.empty());
				instances[ instanceID ] = status;

				std::string annexName;
				scratchpad->LookupString( (iName + ".annexName"), annexName );
				ASSERT(! annexName.empty() );
				annexes[ instanceID ] = annexName;
			} while( true );

			std::string annexID, annexName;
			scratchpad->LookupString( "AnnexID", annexID );
			annexName = annexID.substr( 0, annexID.find( "_" ) );

			if( count == 0 ) {
				std::string errorString;
				if( annexName.empty() ) {
					errorString = "Found no instances in any annex.";
				} else {
					errorString = "Found no machines in that annex.";
				}

				dprintf( D_AUDIT | D_IDENT | D_PID, getuid(), "%s\n", errorString.c_str() );
				fprintf( stdout, "%s\n", errorString.c_str() );
				goto cleanup;
			}

			CondorQuery q( STARTD_AD );
			std::string constraint;
			if( annexName.empty() ) {
				formatstr( constraint, "IsAnnex" );
			} else {
				formatstr( constraint, ATTR_ANNEX_NAME " == \"%s\"", annexName.c_str() );
			}
			q.addANDConstraint( constraint.c_str() );

			ClassAdList cal;
			CondorError errStack;
			CollectorList * collectors = CollectorList::create();
			QueryResult qr = collectors->query( q, cal, & errStack );
			if (qr != Q_OK) {
				fprintf( stderr, "Status check failed -- cannot contact collector\n");
			}
			delete collectors;

			cal.Rewind();
			ClassAd * cad = NULL;
			while( (cad = cal.Next()) != NULL ) {
				std::string name, instanceID, aName;
				cad->LookupString( ATTR_NAME, name );
				cad->LookupString( "EC2InstanceID", instanceID );
				cad->LookupString( ATTR_ANNEX_NAME, aName );

				instances[ instanceID ] = "in-pool";
			}

			if( wantClassAds ) {
				printClassAds( instances, annexID, command, annexes );
			} else {
				printHumanReadable( instances, annexID, annexes );
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
