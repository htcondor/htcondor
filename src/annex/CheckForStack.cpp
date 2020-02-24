#include "condor_common.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "CheckForStack.h"

int
CheckForStack::operator() () {
	dprintf( D_FULLDEBUG, "CheckForStack::operator()\n" );

	if(! introduced) {
		fprintf( stdout, "Checking for %s...", stackDescription.c_str() );
		introduced = true;
	}

	std::string stackStatus;
	std::map< std::string, std::string > outputs;
	std::string errorCode;
	int rc = cfGahp->describe_stacks(
				service_url, public_key_file, secret_key_file,
				stackName,
				stackStatus, outputs,
				errorCode );

	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		if( stackStatus == "CREATE_COMPLETE" || stackStatus == "UPDATE_COMPLETE" ) {
			fprintf( stdout, " OK.\n" ); fflush( stdout );

			for( auto i = outputs.begin(); i != outputs.end(); ++i ) {
				scratchpad->Assign( i->first, i->second );
			}

			reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
			rc = PASS_STREAM;
		} else {
			fprintf( stdout, " not OK ('%s').\n", stackStatus.c_str() ); fflush( stdout );

			std::string message;
			formatstr( message, "Missing %s.  Please log into your AWS account and delete each CloudFormation stack whose name starts with 'HTCondorAnnex-', then re-run 'condor_annex -setup'.", stackDescription.c_str() );
			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, message );
			rc = FALSE;
		}
	} else {
		if( strcasestr( cfGahp->getErrorString(), "<Code>ValidationError</Code>" ) != NULL ) {
			fprintf( stdout, " missing.\n" );  fflush( stdout );

			std::string message;
			formatstr( message, "Missing %s.  Please log into your AWS account and delete each CloudFormation stack whose name starts with 'HTCondorAnnex-', then re-run 'condor_annex -setup'.", stackDescription.c_str() );
			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, message );
			rc = FALSE;
		} else {
			std::string message;
			formatstr( message, " FAILED to check status of stack: '%s' (%d): '%s'.",
				errorCode.c_str(), rc, cfGahp->getErrorString() );
			dprintf( D_ALWAYS, "%s\n", message.c_str() );

			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, message );
			rc = FALSE;
		}
	}

	daemonCore->Reset_Timer( cfGahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return rc;
}


int
CheckForStack::rollback() {
	dprintf( D_FULLDEBUG, "CheckForStack::rollback()\n" );

	// We didn't do anything, so we don't have anything to undo.

	daemonCore->Reset_Timer( cfGahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
