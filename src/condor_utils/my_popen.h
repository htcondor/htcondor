/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef __MY_POPEN__
#define __MY_POPEN__


FILE *my_popenv( const char *const argv [],
                 const char * mode,
                 int options ); // zero or more of MY_POPEN_OPT_* bits
#define MY_POPEN_OPT_WANT_STDERR  0x0001
#define MY_POPEN_OPT_FAIL_QUIETLY 0x0002 // failure is an option, don't dprintf or write to stderr for common errors

int my_pclose( FILE *fp );
// This calls my_pclose_ex() with its own arguments but has a
// backwards-compatible return code (like my_pclose()).
int my_pclose( FILE *fp, unsigned int timeout, bool kill_after_timeout );
int my_pclose_ex( FILE *fp, time_t timeout, bool kill_after_timeout );
// special return values from my_pclose_ex that are not exit statuses
#define MYPCLOSE_EX_NO_SUCH_FP     ((int)0xB4B4B4B4) // fp is not in my_popen tables mapping fp -> pid
#define MYPCLOSE_EX_I_KILLED_IT    ((int)0x99099909) // process was hard killed by my_pclose_ex so process exit value is unknown
#define MYPCLOSE_EX_STATUS_UNKNOWN ((int)0xDEADBEEF) // process was reaped by someone else, so exit status is unknown
#define MYPCLOSE_EX_STILL_RUNNING  ((int)0xBAADDEED) // process is still running (can only happen if kill_after_timeout==false)

int my_systemv( const char *const argv[] );

int my_spawnl( const char* cmd, ... );
int my_spawnv( const char* cmd, const char *const argv[] );

#if defined(WIN32)
// on Windows, expose the ability to use a raw command line
FILE *my_popen( const char *cmd, const char *mode, int options );
int my_system( const char *cmd );
#endif

// ArgList and Env versions only available from C++
#include "condor_arglist.h"
#include "env.h"
FILE *my_popen( const ArgList &args,
                const char * mode,
                int options, // see MY_POPEN_OPT_ flags above
                const Env *env_ptr = NULL,
                bool drop_privs = true,
				const char *write_data = NULL);
int my_system( const ArgList &args, const Env *env_ptr = NULL );


// run a command and return its output in a buffer
// the program is killed if the timeout expires.
// returns NULL and sets exit_status to errno if the timeout expires
// returns a null-terminated malloc'd buffer and sets exit_status
//   to the programs exit status if the program ran to completion
#define RUN_COMMAND_OPT_WANT_STDERR       0x001
#define RUN_COMMAND_OPT_USE_CURRENT_PRIVS 0x080
char* run_command(time_t timeout, const ArgList &args, int options, const Env* env_ptr, int *exit_status);

// Class to hold a my_popen stream can capture its output into a buffer
// And also provide a time limit on how long the program runs.
// aka "Backticks" (with timeout)
//
// Usage:
// use start_program(...) to start the process (calls my_popen)
// if start_program returns != 0, execution failed and the return value was the error
//
// use wait_for_output() or wait_for_exit() to buffer the output with a timeout
// then use close_program() to close the my_popen handle (also calls my_pkill if needed)
//
// then use output(), error_code(), exit_status(), etc to get the result.
// output is available only after the program exits.
//
// use clear() to reset the class so you can call start_program() again.
//
class MyPopenTimer {
public:
	static const int NOT_INTIALIZED=0xd01e;

	static const bool WITH_STDERR = true;
	static const bool WITHOUT_STDERR = false;

	static const bool DROP_PRIVS = true;
	static const bool KEEP_PRIVS = false;

	static const int ALREADY_RUNNING = -1;

	MyPopenTimer() : fp(NULL), status(0), error(NOT_INTIALIZED), begin_time(0), src(NULL, true), bytes_read(0), run_time(0) {}
	virtual ~MyPopenTimer();

	// prepare class for destruction or re-use.
	// rewinds output buffer but does not free it.
	void clear();

	// run a program and begin capturing it's output
	// returns 0 if program starts successfully
	// returns -1 if a program is already running
	// returns errno from my_popen if program does not start
	int start_program (
		const ArgList &args,
		bool also_stderr,
		const Env* env_ptr=NULL,
		bool drop_privs=true,
		const char * stdin_data=NULL);

	// Capture program output until the program exits or until timeout expires
	// returns output if program runs to completion and output is captured
	// returns NULL if there was an error or timeout.
	// When NULL is returned the program may still be running. You can
	//   use is_closed() to discover if it is still running
	//   use close_program() to terminate it
	// then use exit_status() and/or error_code() to find out what happened.
	// at that point Output() can be used to get whatever output was returned.
	const char* wait_for_output(time_t timeout);

	// capture program output until it exits or the timout expires
	// returns true and the exit code if the program runs to completion.
	// returns false if the timeout expired or there was an error reading the output.
	// when false is returned, the program (might) still be running.
	// use close_program to terminate it.
	// at that point Output() can be used to get whatever output was returned.
	bool wait_for_exit(time_t timeout, int *exit_status);

	// close the program if it is still running, sending a SIGTERM now and
	// a SIGKILL after wait_for_term seconds if it still has not exited.
	// return true if program exited on its own, false if it had be signaled.
	bool close_program(time_t wait_for_term);

	// a common use pattern, wait for output and then close the program, by force if necessary
	const char * wait_and_close(time_t timeout, time_t wait_for_term=1) {
		const char* ret = wait_for_output(timeout);
		close_program(wait_for_term);
		return ret;
	}

	// returns true if the program and FILE* handle have been closed, false if not.
	bool is_closed() const { return !fp; }

	// once is_closed() returns true, these can be used to query the final state.
	time_t began_at() const { return begin_time; }
	bool was_timeout() const { return error == ETIMEDOUT; }
	int exit_status() const { return status; }
	int error_code() const { return error; }
	MyStringCharSource& output() { return src; } // if you call this before is_close() is true, you must deal with truncated lines yourself.
	int output_size() const { return bytes_read; }
	int runtime() const { return run_time; }

	// returns "" if no error, strerror() or some other short string
	// if there was an error starting the program or reading output
	const char * error_str() const;

protected:
	FILE * fp;
	int    status;  // status value of the program
	int    error;   // error code from reading
	time_t begin_time;
	MyStringCharSource src;

	// returns 0 on success, error code on error
	int read_until_eof(time_t timeout);

private:
	int   bytes_read; // number of bytes read for this program
	int   run_time;   // time between start_program() and close_program()
	// disallow assignment and copy construction
	MyPopenTimer(const MyPopenTimer & that);
	MyPopenTimer& operator=(const MyPopenTimer & that);
};


#endif
