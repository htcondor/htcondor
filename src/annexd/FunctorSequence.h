#ifndef _CONDOR_FUNCTOR_SEQUENCE_H
#define _CONDOR_FUNCTOR_SEQUENCE_H

// #include "condor_common.h"
// #include "condor_daemon_core.h"
//
// #include <queue>
//
// #include "Functor.h"
// #include "FunctorSequence.h"

//
// std::queue< Functor * > sequence = { functorA, functorB, functorC };
// FunctorSequence * fs = new FunctorSequence( sequence, cleanupFunctor );
// timerID = daemonCore->Register_Timer( 0, TIMER_NEVER,
//     (void (Service::*)()) & FunctorSequence::operator(),
//     description, fs );
//
// A FunctorSequence calls a sequence of functors on a timer, returning
// to DaemonCore between calls.  Each functor is called (on the timer
// firing) until it stops returning KEEP_STREAM.  If the functor return
// PASS_STREAM, the next functor in the sequence will be called the next
// time the timer fires.  After the last functor in the sequence returns
// PASS_STREAM, or any functor returns anything other than KEEP_STREAM or
// PASS_STREAM, the cleanup functor is called once (on the next timer
// tick).  The cleanup functor must delete the stream and cancel the
// timer if it so desires.
//

class FunctorSequence : public Service {
	public:
		FunctorSequence( const std::deque< Functor * > & s, Functor * l ) :
			sequence(s), last(l), current(NULL) { }
		virtual ~FunctorSequence() { }

		void operator() ();

	private:
		std::queue< Functor * > sequence;
		Functor * last;

		Functor * current;
};

#endif /* _CONDOR_FUNCTOR_SEQUENCE_H */
