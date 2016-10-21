#include "condor_common.h"
#include "condor_daemon_core.h"

#include <queue>

#include "Functor.h"
#include "FunctorSequence.h"

void
FunctorSequence::deleteFunctors() {
	for( unsigned i = 0; i < sequence.size(); ++i ) {
		Functor * f = sequence[i];
		delete f;
	}
	if( last ) { delete last; }
}

void
FunctorSequence::operator() () {
	// If we've run past the end of the sequence, call the last functor.
	if( current >= (int) sequence.size() ) {
		int r = (* last)();
		if( r != KEEP_STREAM ) { deleteFunctors(); delete this; }
		return;
	}

	// If we've run past the end of the rollback, call the last functor.
	if( current < 0 ) {
		int r = last->rollback();
		if( r != KEEP_STREAM ) { deleteFunctors(); delete this; }
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
			else { rollingBack = true; }
			return;
	}
}

