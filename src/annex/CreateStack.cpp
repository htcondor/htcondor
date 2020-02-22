#include "condor_common.h"
#include "compat_classad.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "CreateStack.h"

int
CreateStack::operator() () {
	dprintf( D_FULLDEBUG, "CreateStack::operator()\n" );

	// Handle dynamic data depencies.
	for( auto i = stackParameters.begin(); i != stackParameters.end(); ++i ) {
		if( i->second == "<scratchpad>" ) {
			scratchpad->LookupString( i->first, i->second );
		}
	}

	std::string errorCode;
	std::string stackID;
	int rc = cfGahp->create_stack(
				service_url, public_key_file, secret_key_file,

				stackName, stackURL, "CAPABILITY_IAM",
				stackParameters,

				stackID,
				errorCode );

	if( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
		// We expect to exit here the first time.
		return KEEP_STREAM;
	}

	if( rc == 0 ) {
		reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
		rc = PASS_STREAM;
	} else {
		if( strcasestr( cfGahp->getErrorString(), "<Code>AlreadyExistsException</Code>" ) != NULL ) {
			reply->Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );
			rc = PASS_STREAM;
		} else {
			std::string message;
			formatstr( message, "Stack creation failed: '%s' (%d): '%s'.",
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
CreateStack::rollback() {
	dprintf( D_FULLDEBUG, "CreateStack::rollback()\n" );

	// If we blow up in the middle of creating the stack, it's OK; we'll
	// notice when either (a) the user tries to create the stack again
	// later or (b) when the user doesn't notice the crash and tries to
	// start an annex and the validation fails.  (If the validation
	// succeeds, obviously we don't need to do anything; we just have
	// to make sure that the validation checks for the stack to be
	// CREATE_COMPLETE, and not just existent.)  For (a), if stack
	// creation fails because the stack already exists (that is,
	// CF_DESCRIBE_STACKS succeeds when passed the fixed stack name),
	// then we just continue as normal.
	//
	// This logic assumes that the stack was created to delete on failure.

	daemonCore->Reset_Timer( cfGahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
