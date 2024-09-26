//
// #include "condor_common.h"
// #include "condor_daemon_core.h"
// #include "dc_coroutines.h"
//

#ifndef _CONDOR_DC_COROUTINES_H
#define _CONDOR_DC_COROUTINES_H

#include <exception>
#include <coroutine>

namespace condor {
namespace dc {

	//
	// An AwaitableDeadlineReaper allows you to co_await for a child process
	// to either exit or time out.  You can register any many child processes
	// as you like with a single ADR, but you can't have multiple ADRs in the
	// same coroutine (function) unless you ensure that a process from one
	// can't trigger either a timer or a callback reaper while you're waiting
	// for another.  Otherise, you'll blow an ASSERT().
	//
	//
	// AwaitableDeadlineReaper logansRun;
	// ...
	// logansRun.born( pid1, lifetime1 );
	// ...
	// logansRun.born( pid2, lifetime2 );
	//
	// while( logansRun.alive() > 0 ) {
	//     auto [pid, timed_out, status] = co_await( logansRun );
	//     if( timed_out ) { kill(pid, SIGTERM); ... }
	//     else if( status != 0 ) { ... }
	// }
	//

	class AwaitableDeadlineReaper : public Service {

		public:

			//
			// The API programmer will use.
			//

			AwaitableDeadlineReaper();
			virtual ~AwaitableDeadlineReaper();

			// Call when you've spawned a child process.
			bool born( pid_t pid, int timeout );

			// Useful as an argument to Create_Process().
			int reaper_id() const { return reaperID; }

			// How many born have not died?
			size_t living() const { return pids.size(); }

			// Is the given PID alive?
			bool contains( pid_t pid ) const { return pids.contains(pid); }


			//
			// The API for daemon core.
			//
			int reaper( pid_t pid, int status );
			void timer( int timerID );


			//
			// The API for the compiler.
			//
			// It seems like you should be able to move the await_*() functions
			// out of the struct returned by the function below and into this
			// class, but that does NOT work; it looks like the awaiter is
			// std::move()d (or the equivalent) at some point, which resets
			// the source, which is the object that the daemon core reaper and
			// timer handler have registered.
			//
			// As far as I can tell, this is completely undocumented.
			//
			// This code presently assumes that the taking the address of a
			// stack-allocated object in a coroutine and writing to it while
			// the coroutine is suspended is kosher.  This works because the
			// coroutine is _always_ invoked with its own stack and that
			// stack remains valid until the handle is destroyed.
			//
			auto operator co_await() {
				struct awaiter {
					dc::AwaitableDeadlineReaper * adr;

					// Because we always immediately resume the calling
					// coroutine when daemon core invokes the timer or reaper
					// callbacks, we will never have a pending event ready.
					bool await_ready() { return false; }

					// We've already registered the timer and the reaper at
					// this point, so just record the calling coroutine so we
					// can resume it later.
					void await_suspend( std::coroutine_handle<> h ) {
						adr->the_coroutine = h;
					}

					// The value of co_await'ing an AwaitableDeadlineReaper.
					std::tuple<int, bool, int> await_resume() {
						return std::make_tuple(
							adr->the_pid, adr->timed_out, adr->the_status
						);
					}
				};

				return awaiter{this};
			}


		private:

			int reaperID = -1;
			std::coroutine_handle<> the_coroutine;

			std::set<pid_t> pids;
			std::map<int, pid_t> timerIDToPIDMap;

			// Always initialize PIDs to 0; kill( -1, ... ) is a terrible
			// idea and we don't always check for it.
			pid_t the_pid = 0;
			int the_status = -1;
			bool timed_out = false;
	};


	//
	// Any function which calls co_await is a coroutine, and needs to have
	// a special return type which reflects that.  You can use this return
	// type for functions which would otherwise return void.  Do NOT use
	// this for functions which calls co_yield().
	//
	struct void_coroutine {
		struct promise_type;
		using handle_type = std::coroutine_handle<promise_type>;

		struct promise_type {
			std::exception_ptr exception;

			void_coroutine get_return_object() { return {}; }

			std::suspend_never initial_suspend() { return {}; }

			std::suspend_never final_suspend() noexcept {
				if( exception ) { std::rethrow_exception( exception ); }
				return {};
			}

			void unhandled_exception() { exception = std::current_exception(); }

			void return_void() {
				if( exception ) { std::rethrow_exception( exception ); }
			}
		};
	};


	//
	// An AwaitableDeadlineSocket allows you to co_await for a socket to
	// become hot or for time out to pass.
	//
	class AwaitableDeadlineSocket : public Service {

		public:

			//
			// The API programmer will use.
			//

			AwaitableDeadlineSocket();

			// The caller remains responsible for `sock`.
			bool deadline(Sock * sock, int timeout );

			void destroy() { if( the_coroutine ){ the_coroutine.destroy(); } }

			virtual ~AwaitableDeadlineSocket();

			//
			// The API for daemon core.
			//
			int socket( Stream * s );
			void timer( int timerID );


			//
			// The API for the compiler.
			//
			auto operator co_await() {
				struct awaiter {
					dc::AwaitableDeadlineSocket * ads;

					bool await_ready() { return false; }

					void await_suspend( std::coroutine_handle<> h ) {
						ads->the_coroutine = h;
					}

					// The value of co_await'ing an AwaitableDeadlineSocket.
					std::tuple<Sock *, bool> await_resume() {
						return std::make_tuple(
							ads->the_socket, ads->timed_out
						);
					}
				};

				return awaiter{this};
			}


		private:

			std::coroutine_handle<> the_coroutine;

            // Bookkeeping.
			std::set<Sock *> sockets;
			std::map<int, Sock *> timerIDToSocketMap;

            // The co_await() return values.
            Sock * the_socket = NULL;
			bool timed_out = false;
	};


} // end namespace dc
} // end namespace condor


condor::dc::void_coroutine
spawnCheckpointCleanupProcessWithTimeout(
    int cluster, int proc, ClassAd * jobAd, time_t timeout
);


#endif /* defined(_CONDOR_DC_COROUTINES_H) */
