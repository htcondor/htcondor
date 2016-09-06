#include "condor_common.h"
#include "condor_daemon_core.h"

#include <queue>

#include "Functor.h"
#include "FunctorSequence.h"

void FunctorSequence::operator() () {
	if( current == NULL ) {
		if(! sequence.empty()) {
			current = sequence.front();
			sequence.pop();
		} else {
			int r = (* last)();
			if( r != KEEP_STREAM ) {
				delete this;
			}
			return;
		}
	}

	if( current != NULL ) {
		int r = (*current)();
		switch(r) {
			default:
				while(! sequence.empty()) { sequence.pop(); }
			case PASS_STREAM:
				current = NULL;
			case KEEP_STREAM:
				return;
		}
	}
}
