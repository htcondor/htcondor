#include "condor_common.h"
#include "condor_daemon_core.h"
#include "dc_coroutines.h"

#include "sig_name.h"

using namespace condor;

dc::AwaitableDeadlineReaper::AwaitableDeadlineReaper() {
	reaperID = daemonCore->Register_Reaper(
		"AwaitableDeadlineReaper::reaper",
		[=, this](int p, int s) { return this->reaper(p, s); },
		"AwaitableDeadlineReaper::reaper"
	);
}


dc::AwaitableDeadlineReaper::~AwaitableDeadlineReaper() {
	// Do NOT destroy() the_coroutine here.  The coroutine may still
	// needs its state, because the lifetime of this object could be
	// shorter than the lifetime of the coroutine.  (For example, a
	// coroutine could declare two locals of this type, one of which
	// has a longer lifetime than the other.)

	// Cancel the reaper.  (Which holds a pointer to this.)
	if( reaperID != -1 ) {
		daemonCore->Cancel_Reaper( reaperID );
	}

	// Cancel any timers.  (Each holds a pointer to this.)
	for( auto [timerID, dummy] : timerIDToPIDMap ) {
		daemonCore->Cancel_Timer( timerID );
	}
}


bool
dc::AwaitableDeadlineReaper::born( pid_t pid, time_t timeout ) {
	auto [dummy, inserted] = pids.insert(pid);
	if(! inserted) { return false; }
	// dprintf( D_ZKM, "Inserted %d into %p\n", pid, & pids );

	// Register a timer for this process.
	int timerID = daemonCore->Register_Timer(
		timeout, TIMER_NEVER,
		(TimerHandlercpp) & AwaitableDeadlineReaper::timer,
		"AwaitableDeadlineReaper::timer",
		this
	);
	timerIDToPIDMap[timerID] = pid;

	return true;
}


int
dc::AwaitableDeadlineReaper::reaper( pid_t pid, int status ) {
	ASSERT(pids.contains(pid));

	// We will never hear from this process again, so forget about it.
	pids.erase(pid);

	// Make sure we don't hear from the timer.
	for( auto [a_timerID, a_pid] : timerIDToPIDMap ) {
		if( a_pid == pid ) {
			daemonCore->Cancel_Timer(a_timerID);
			timerIDToPIDMap.erase(a_timerID);
			break;
		}
	}

	the_pid = pid;
	timed_out = false;
	the_status = status;
	ASSERT(the_coroutine);
	the_coroutine.resume();

	return 0;
}


void
dc::AwaitableDeadlineReaper::timer( int timerID ) {
	ASSERT(timerIDToPIDMap.contains(timerID));
	int pid = timerIDToPIDMap[timerID];
	ASSERT(pids.contains(pid));

	// We don't remove the PID; it's up to the co_await'ing function
	// to decide what to do when the timer fires.  This does mean that
	// you'll get another event if you kill() a timed-out child, but
	// because we can safely remove the timer in the reaper, you won't
	// get a timer event after a reaper event.

	the_pid = pid;
	timed_out = true;
	the_status = -1;
	ASSERT(the_coroutine);
	the_coroutine.resume();
}


// Arguably this section should be in its own file, along with its
// entry in the header.

#include "checkpoint_cleanup_utils.h"

cr::void_coroutine
spawnCheckpointCleanupProcessWithTimeout( int cluster, int proc, ClassAd * jobAd, time_t timeout ) {
	dc::AwaitableDeadlineReaper logansRun;

	std::string error;
	// Always initialize PIDs to 0 rather than -1 to avoid accidentally
	// kill()ing all (of your) processes on the system.
	int spawned_pid = 0;
	bool rv = spawnCheckpointCleanupProcess(
		cluster, proc, jobAd, logansRun.reaper_id(),
		spawned_pid, error
	);
	if(! rv) { co_return /* false */; }

	logansRun.born( spawned_pid, timeout );
	auto [pid, timed_out, status] = co_await( logansRun );
	// This pointer could have been invalidated while we were co_await()ing.
	jobAd = NULL;

	if( timed_out ) {
		daemonCore->Shutdown_Graceful( pid );
		dprintf( D_TEST, "checkpoint clean-up proc %d timed out after %ld seconds\n", pid, timeout );
		// This keeps the awaitable deadline reaper alive until the process
		// we just killed is reaped, which prevents a log message about an
		// unknown process dying.
		std::ignore = co_await( logansRun );
	} else {
		dprintf( D_TEST, "checkpoint clean-up proc %d returned %d\n", pid, status );
	}
}


// ---------------------------------------------------------------------------


dc::AwaitableDeadlineSocket::AwaitableDeadlineSocket() {
}


dc::AwaitableDeadlineSocket::~AwaitableDeadlineSocket() {
	// See the commend  in ~AwaitableDeadlineReaper() for why we don't
	// destroy the_coroutine here.

	// Cancel any timers and any sockets.  (Each holds a pointer to this.)
	for( auto [timerID, socket] : timerIDToSocketMap ) {
		daemonCore->Cancel_Timer( timerID );
		daemonCore->Cancel_Socket( socket );
	}
}


bool
dc::AwaitableDeadlineSocket::deadline( Sock * sock, int timeout ) {
	auto [dummy, inserted] = sockets.insert(sock);
	if(! inserted) { return false; }

	// Register a timer for this socket.
	int timerID = daemonCore->Register_Timer(
		timeout, TIMER_NEVER,
		(TimerHandlercpp) & AwaitableDeadlineSocket::timer,
		"AwaitableDeadlineSocket::timer",
		this
	);
	timerIDToSocketMap[timerID] = sock;

    // This is a special undocumented hack; you can use an
    // AwaitableDeadlineSocket as a pure co-awaitable() way to
    // wibble in and out of the event loop.
    if( sock == NULL ) { return false; }

    // Register a handler for this socket.
    daemonCore->Register_Socket( sock, "peer description",
        (SocketHandlercpp) & dc::AwaitableDeadlineSocket::socket,
        "AwaitableDeadlineSocket::socket",
        this
    );

	return true;
}


void
dc::AwaitableDeadlineSocket::timer( int timerID ) {
	ASSERT(timerIDToSocketMap.contains(timerID));
	Sock * sock = timerIDToSocketMap[timerID];
	ASSERT(sockets.contains(sock));
	sockets.erase(sock);

	// Remove the socket listener.
	daemonCore->Cancel_Socket( sock );
	timerIDToSocketMap.erase(timerID);

	the_socket = sock;
	timed_out = true;
	ASSERT(the_coroutine);
	the_coroutine.resume();
}


int
dc::AwaitableDeadlineSocket::socket( Stream * s ) {
    Sock * sock = dynamic_cast<Sock *>(s);
    ASSERT(sock != NULL);

	ASSERT(sockets.contains(sock));
	sockets.erase(sock);

	// Make sure we don't hear from the timer.
	for( auto [a_timerID, a_sock] : timerIDToSocketMap ) {
		if( a_sock == sock ) {
		    daemonCore->Cancel_Socket(a_sock);
			daemonCore->Cancel_Timer(a_timerID);
			timerIDToSocketMap.erase(a_timerID);
			break;
		}
	}

	the_socket = sock;
	timed_out = false;
	ASSERT(the_coroutine);
	the_coroutine.resume();

	return KEEP_STREAM;
}


// ---------------------------------------------------------------------------


dc::AwaitableDeadlineSignal::AwaitableDeadlineSignal() {
}


dc::AwaitableDeadlineSignal::~AwaitableDeadlineSignal() {
	// See the commend  in ~AwaitableDeadlineReaper() for why we don't
	// destroy the_coroutine here.

	// Cancel any timers and any sockets.  (Each holds a pointer to this.)
	for( auto [timerID, signalPair] : timerIDToSignalMap ) {
		daemonCore->Cancel_Timer( timerID );
		daemonCore->Cancel_Signal( signalPair.first, signalPair.second );
	}
}


bool
dc::AwaitableDeadlineSignal::deadline( int signalNo, int timeout ) {
	// Register a timer for this signal.
	int timerID = daemonCore->Register_Timer(
		timeout, TIMER_NEVER,
		(TimerHandlercpp) & AwaitableDeadlineSignal::timer,
		"AwaitableDeadlineSignal::timer",
		this
	);

	auto handler = [=, this](int signal) -> int { return this->signal(signal); };
	auto destroyer = [=, this]() -> void { this->destroy(); };

	// Register a handler for this signal.
	int signalID = daemonCore->Register_Signal(
		signalNo, signalName(signalNo),
		handler, "AwaitableDeadlineSignal::signal",
		destroyer
	);
	timerIDToSignalMap[timerID] = std::make_pair(signalNo, signalID);

	return true;
}


void
dc::AwaitableDeadlineSignal::timer( int timerID ) {
	ASSERT(timerIDToSignalMap.contains(timerID));
	auto signalPair = timerIDToSignalMap[timerID];

	// Remove the signal handler.
	daemonCore->Cancel_Signal( signalPair.first, signalPair.second );
	timerIDToSignalMap.erase(timerID);

	the_signal = signalPair.first;
	timed_out = true;
	ASSERT(the_coroutine);
	the_coroutine.resume();
}


int
dc::AwaitableDeadlineSignal::signal( int signal ) {
	// Make sure we don't hear from the timer.
	for( auto [a_timerID, a_signal] : timerIDToSignalMap ) {
		if( a_signal.first == signal ) {
			// We otherwise won't (be able to) cancel the signal handler.
			daemonCore->Cancel_Signal(a_signal.first, a_signal.second);
			daemonCore->Cancel_Timer(a_timerID);
			timerIDToSignalMap.erase(a_timerID);
			break;
		}
	}

	the_signal = signal;
	timed_out = false;
	ASSERT(the_coroutine);
	the_coroutine.resume();

	return TRUE;
}
