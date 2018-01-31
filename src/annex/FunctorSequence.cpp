#include "condor_common.h"
#include "condor_daemon_core.h"
#include "classad_collection.h"

#include <queue>

#include "Functor.h"
#include "FunctorSequence.h"

FunctorSequence::FunctorSequence( const std::vector< Functor * > & s,
	Functor * l, ClassAdCollection * c, const std::string & cid,
	ClassAd * sp ) :
  sequence( s ), last( l ), current( 0 ), rollingBack( false ),
  commandState( c ), commandID( cid ), scratchpad( sp ) {
	ClassAd * commandState;
	if( c->Lookup( commandID, commandState ) ){
		commandState->LookupBool( "State_FS_rollingBack", rollingBack );
		commandState->LookupInteger( "State_FS_current", current );
	}

	std::string hk = commandID + "-scratchpad";
	c->Lookup( hk, scratchpad );
}

void
FunctorSequence::log() {
	if( commandState == NULL ) {
		dprintf( D_FULLDEBUG, "log() called without a log.\n" );
		return;
	}

	if( commandID.empty() ) {
		dprintf( D_FULLDEBUG, "log() called without a command ID.\n" );
		return;
	}

	commandState->BeginTransaction();
	{
		std::string currentString; formatstr( currentString, "%d", current );
		commandState->SetAttribute( commandID,
			"State_FS_current", currentString.c_str() );

		std::string rollingBackString = rollingBack ? "true" : "false";
		commandState->SetAttribute( commandID,
			"State_FS_rollingBack", rollingBackString.c_str() );

		// Should I call InsertOrUpdateAd() (from annexd.cpp)?
		std::string hk = commandID + "-scratchpad";
		commandState->NewClassAd( hk, scratchpad );
	}
	commandState->CommitTransaction();
}

void
FunctorSequence::deleteFunctors() {
	for( unsigned i = 0; i < sequence.size(); ++i ) {
		Functor * f = sequence[i];
		delete f;
	}
	if( last ) { delete last; }

	commandState->BeginTransaction();
	{
		std::string hk = commandID + "-scratchpad";
		commandState->DestroyClassAd( hk );
	}
	commandState->CommitTransaction();
}

void
FunctorSequence::operator() () {
	// If we've run past the end of the sequence, call the last functor.
	if( current >= (int) sequence.size() ) {
		int r = (* last)();
		if( r != KEEP_STREAM ) { deleteFunctors(); delete this; exit( 0 ); }
		return;
	}

	// If we've run past the end of the rollback, call the last functor.
	if( current < 0 ) {
		int r = last->rollback();
		if( r != KEEP_STREAM ) { deleteFunctors(); delete this; exit( 0 ); }
		return;
	}

	// Otherwise, call the current functor.
	Functor * f = sequence[ current ];

	int r = rollingBack ? f->rollback() : (* f)();
	switch( r ) {
		case PASS_STREAM:
			current += rollingBack ? -1 : 1;
		case KEEP_STREAM:
			return;
		default:
			// We don't stop a rollback for errors.
			if( rollingBack ) { current -= 1; }
			else { rollingBack = true; log(); }
			return;
	}
}

