#ifndef _CONDOR_DC_COROUTINES_H
#define _CONDOR_DC_COROUTINES_H

#include <stdio.h>
#include <string>
#include <exception>
#include <coroutine>

#if THIS_IS_AN_EXAMPLE

//
// A C++ coroutine is any function which uses co_await, co_yield, or
// co_return.  Implementation weirdness means that the return type,
// in this case DC::protocol, determines how, exactly, the
// coroutine behaves.  In this case, a DC::protocol is also
// an int generator: it yields and returns an int and can be used as
// a truth value to determine if there are any more ints in the generated
// sequence.  See the example main() function for how to use a
// coroutine.  The implementation, if you're curious, is below that.
//
// Hopefully, this entire file will become obsolete in C++23.
//

DC::protocol
handle_blocking_protocol( const std::string & parameter ) {
	for( size_t i = 0; i < parameter.size(); ++i ) {
		co_yield (int)parameter[i];
	}
	co_return 0;
}


int
main( int argc, char ** argv ) {
	std::string s("example");
	auto h = handle_blocking_protocol(s);
	while( h ) { fprintf( stdout, "%c\n", h() ); }
	fprintf( stdout, "\n\n" );

	std::string t("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	std::string u("abcdefghij");

	auto i = handle_blocking_protocol(t);
	auto j = handle_blocking_protocol(u);

	while( true ) {
		if( i ) { fprintf( stdout, "%c\n", i() ); }
		if( j ) { fprintf( stdout, "%c\n", j() ); }
		if( (! i) && (! j) ) { break; }
	}


	return 0;
}
#endif /* THIS_IS_AN_EXAMPLE */


//
// Implementation
//

namespace DC {
	struct protocol {

		struct promise_type;
		using handle_type = std::coroutine_handle<promise_type>;

		struct promise_type {
			int value;
			std::exception_ptr exception;

			protocol get_return_object() { return protocol(handle_type::from_promise(*this)); }
			// In `auto h = handle_blocking_protocol()`, above, the fact that
			// this initial_suspend() returns `suspend_always` means that the
			// the body of handle_blocking_protocol() won't execute until
			// `if( h() )`, when operator bool() invokes it to generate the
			// next (or last) value in the sequence.
			std::suspend_always initial_suspend() { return {}; }
			std::suspend_always final_suspend() noexcept { return {}; }
			void unhandled_exception() { exception = std::current_exception(); }

			template <std::convertible_to<int> from_t>
			std::suspend_always yield_value( from_t && from ) {
				value = std::forward<from_t>(from);
				return {};
			}

			template <std::convertible_to<int> from_t>
			void return_value( from_t && from ) {
				value = std::forward<from_t>(from);
			}
		};

		protocol(handle_type h) : handle(h) { }
		~protocol() { handle.destroy(); }

		// So that `p()` returns the values yielded.
		int operator() () {
			call_generator();
			result_available = false;
			return std::move(handle.promise().value);
		}

		// So that `p` can be used as a loop condition.
		explicit operator bool() {
			call_generator();
			return ! handle.done();
		}

		// I'd work on adding begin() and end() and an iterator,
		// but that's (hopefully) coming along with the `generator`
		// template in C++23 and I don't think the loop below is
		// so awful that it will look out of place in DaemonCore.

		private:

			handle_type handle;
			bool result_available = false;

			void call_generator() {
				if(! result_available) {
					handle();

					if( handle.promise().exception ) {
						std::rethrow_exception( handle.promise().exception );
					}

					result_available = true;
				}
			}
	};

	typedef protocol Timer;
}

#endif /* defined(_CONDOR_DC_COROUTINES_H) */
