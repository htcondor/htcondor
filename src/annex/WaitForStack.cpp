#include "condor_common.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "WaitForStack.h"

int
WaitForStack::operator() () {
	dprintf( D_FULLDEBUG, "WaitForStack::operator()\n" );

	if(! introduced) {
		fprintf( stdout, "Creating %s..", stackDescription.c_str() );
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
		// Determine if the stack is done yet.  If it is, move on.  Otherwise,
		// update the user and then wait ten seconds and check again.
		if( stackStatus == "CREATE_COMPLETE" || stackStatus == "UPDATE_COMPLETE" ) {
			fprintf( stdout, " complete.\n" ); fflush( stdout );

			for( auto i = outputs.begin(); i != outputs.end(); ++i ) {
				scratchpad->Assign( i->first, i->second );
			}

			reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
			rc = PASS_STREAM;
		} else if( stackStatus == "CREATE_IN_PROGRESS" ) {
			fprintf( stdout, "." ); fflush( stdout );
			daemonCore->Reset_Timer( cfGahp->getNotificationTimerId(), 10, TIMER_NEVER );
			return KEEP_STREAM;
		} else {
			fprintf( stdout, "\n" );

			std::string message;
			formatstr( message, "Stack creation failed with status '%s'.", stackStatus.c_str() );
			dprintf( D_ALWAYS, "%s\n", message.c_str() );

			reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
			reply->Assign( ATTR_ERROR_STRING, message );
			rc = FALSE;
		}
	} else {
		std::string message;
		formatstr( message, "Failed to check status of stack: '%s' (%d): '%s'.",
			errorCode.c_str(), rc, cfGahp->getErrorString() );
		dprintf( D_ALWAYS, "%s\n", message.c_str() );

		reply->Assign( ATTR_RESULT, getCAResultString( CA_FAILURE ) );
		reply->Assign( ATTR_ERROR_STRING, message );
		rc = FALSE;
	}

	daemonCore->Reset_Timer( cfGahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return rc;
}


int
WaitForStack::rollback() {
	dprintf( D_FULLDEBUG, "WaitForStack::rollback()\n" );

	// We didn't do anything, so we don't have anything to undo.

	daemonCore->Reset_Timer( cfGahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
