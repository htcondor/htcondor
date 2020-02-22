#include "condor_common.h"
#include "condor_config.h"
#include "condor_md.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "CondorStatusReply.h"

static bool
exec_condor_status( int argc, char ** argv, unsigned subCommandIndex, const std::string & annexName ) {
	std::string csPath = argv[0];
	csPath.replace( csPath.find( "condor_annex" ), strlen( "condor_annex" ), "condor_status" );

	unsigned EXTRA_ARGS = 5;
	if(! annexName.empty()) {
		EXTRA_ARGS += 2;
	}
	char ** csArgv = (char **)malloc( (argc - subCommandIndex + EXTRA_ARGS + 1) * sizeof(char *) );
	if( csArgv == NULL ) { return false; }

	csArgv[0] = strdup( csPath.c_str() );
	csArgv[1] = strdup( "-annex" );
	csArgv[2] = strdup( "-sort" );
	csArgv[3] = strdup( "EC2InstanceID" );
	csArgv[4] = strdup( "-merge" );
	csArgv[5] = strdup( "-" );
	if(! annexName.empty()) {
		csArgv[6] = strdup( "-annex-name" );
		csArgv[7] = strdup( annexName.c_str() );
	}

	bool allDuplicated = true;
	for( unsigned i = 0; i < EXTRA_ARGS; ++i ) {
		if( csArgv[i] == NULL ) { allDuplicated = false; break; }
	}
	if(! allDuplicated) {
		for( unsigned i = 0; i < EXTRA_ARGS; ++i ) {
			if( csArgv[i] != NULL ) { free(csArgv[i]); }
		}
		free( csArgv );
		return false;
	}

	// Copy any trailing arguments.
	for( int i = subCommandIndex + 1; i < argc; ++i ) {
		csArgv[EXTRA_ARGS + (i - subCommandIndex)] = argv[i];
	}
	csArgv[argc - subCommandIndex + EXTRA_ARGS] = NULL;

	if( csPath[0] == '/' ) {
		int r = execv( csPath.c_str(), csArgv );
		free(csArgv);
		return r;
	} else {
		int r = execvp( csPath.c_str(), csArgv );
		free(csArgv);
		return r;
	}
}

int
CondorStatusReply::operator() () {
	dprintf( D_FULLDEBUG, "StatusReply::operator()\n" );

	if( reply ) {
		std::string resultString;
		reply->LookupString( ATTR_RESULT, resultString );
		CAResult result = getCAResultNum( resultString.c_str() );

		if( result == CA_SUCCESS ) {
			char adFileName[] = "temporary-ad-file-XXXXXX";
			int adFD = mkstemp( adFileName );
			if( adFD == -1 ) {
				fprintf( stderr, "Failed to create temporary file '%s', aborting.\n", adFileName );
				return TRUE;
			}
			unlink( adFileName );
			FILE * adFile = fdopen( adFD, "w" );

			std::string iName;
			unsigned count = 0;
			do {
				formatstr( iName, "Instance%d", count );

				std::string instanceID;
				scratchpad->LookupString( (iName + ".instanceID"), instanceID );
				if( instanceID.empty() ) { break; }
				++count;

				// fprintf( stderr, "Found instance ID %s.\n", instanceID.c_str() );

				ClassAd ad;
				ad.Assign( ATTR_MY_TYPE, "AnnexInstance" );
				ad.Assign( "EC2InstanceID", instanceID );
				ad.AssignExpr( ATTR_NAME, "EC2InstanceID" );

				std::string status;
				scratchpad->LookupString( (iName + ".status"), status );
				ASSERT(! status.empty());
				ad.Assign( ATTR_GRID_JOB_STATUS, status );

				std::string clientToken;
				scratchpad->LookupString( (iName + ".clientToken"), clientToken );
				if(! clientToken.empty()) {
					ad.Assign( "EC2ClientToken", clientToken );
				}

				std::string keyName;
				scratchpad->LookupString( (iName + ".keyName"), keyName );
				if(! keyName.empty()) {
					ad.Assign( ATTR_EC2_KEY_PAIR, keyName );
				}

				std::string stateReasonCode;
				scratchpad->LookupString( (iName + ".stateReasonCode"), stateReasonCode );
				if(! stateReasonCode.empty() && stateReasonCode != "NULL" ) {
					ad.Assign( ATTR_EC2_STATUS_REASON_CODE, stateReasonCode );
				}

				std::string publicDNSName;
				scratchpad->LookupString( (iName + ".publicDNSName"), publicDNSName );
				if(! publicDNSName.empty()) {
					ad.Assign( ATTR_MACHINE, publicDNSName );
				}

				std::string annexName;
				scratchpad->LookupString( (iName + ".annexName"), annexName );
				ASSERT(! annexName.empty() );
				ad.Assign( ATTR_ANNEX_NAME, annexName );

				fPrintAd( adFile, ad );
				fprintf( adFile, "\n" );
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
				fclose( adFile );
				goto cleanup;
			}

			fflush( adFile );
			rewind( adFile );
			if( dup2( adFD, 0 ) == -1 ) {
				fprintf( stderr, "Failed to set condor_status stdin, aborting.\n" );
				fclose( adFile );
				return FALSE;
			}
			if(! exec_condor_status( argc, argv, subCommandIndex, annexName )) {
				fprintf( stderr, "Failed to run condor_status, aborting.\n" );
				fclose( adFile );
				return FALSE;
			}

			// We can't actually get here (exec_condor_status() either
			// succeeds by not returning or fails), but Coverity doesn't
			// know that.
			fclose( adFile );
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
CondorStatusReply::rollback() {
	dprintf( D_FULLDEBUG, "StatusReply::rollback() -- calling operator().\n" );
	return (* this)();
}
