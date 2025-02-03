/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


#ifndef DAGMAN_DEBUG_H
#define DAGMAN_DEBUG_H

#include "condor_header_features.h"
#include <string>

/** @name debug.h
    
    This debug module offers the user (or programmer) a way to dynamically
    to debug messages on and off without having to recompile.
    
    My motivation for writing this stemmed from my annoyance of
    #if defined(DEBUG) lines littering source code, making it quite
    unreadable.  Beyond that, having to constantly change a Makefile and
    recompile just to add or subtract debug message was a real hassle.
    
    This module gives the programmer a way to offer a dynamic mechanism
    for printing debug messages.  Not only that, it offers a verbosity scale,
    so to speak.  Message can be assigned a severity, and will only be
    printed if it is as or more severe than the desired output level.
    
    Some pitfalls to avoid:  Because all messages are dynamic, they
    are compiled in with your code, whether of not you see them.  So
    even if you never run the program at level 5, message at level 5
    and above will make your executable larger, even though they never print.
    
    As with assertions or any conditionally compiled/executed code, you
    should never include side effects in your debug messages, since you
    don't really know if they will execute.  Since my functions are
    not conditional macros, side effects will probably be OK, but that
    doesn't mean you should do it.
    
    debug_println (DEBUG_DEBUG, "Error reading element %d", x++);
    
    is an example of unwise programming.
    
    This implementation only address output to stdio.  I yet thought about
    how to design it to work with stderr.  Should there be a debug_level
    for each output stream?  I don't want it to get too complicated.
    
    @author Colby O'Donnell <colbster@cs.wisc.edu>
*/
//@{
  
/** The fundamental data type for the debug module.
    A debug level is meant to express the verbosity of any print
    operation to the screen.  The programmer is welcome to use
    values greater than 4, as I could see a real benefit to having
    multiple levels of debug.  The programmer could assign level 5
    to printing only in outer loops, and level 6 to inner loops.
*/
enum debug_level {
  /** Debug level is unset */                DEBUG_UNSET   = -1,
  /** NEVER output, except for usage info */ DEBUG_SILENT  = 0,
  /** Very quiet, only severe errors */      DEBUG_QUIET   = 1,
  /** Errors and warnings */                 DEBUG_NORMAL  = 2,
  /** Normal output, errors, and warnings */ DEBUG_VERBOSE = 3,
  /** Basic debug output */                  DEBUG_DEBUG_1 = 4,
  /** Outer Loop Debug */                    DEBUG_DEBUG_2 = 5,
  /** Inner Loop Debug */                    DEBUG_DEBUG_3 = 6,
  /** Rarely Used Debug */                   DEBUG_DEBUG_4 = 7
};

///
typedef enum debug_level debug_level_t;

/** The current threshhold of debug output.  You output is only printed
    if the current debug_level is equal or greater than the level of your
    message.
*/
extern debug_level_t debug_level;

/** The name of the current executable.  basename (argv[0]) from main().
 */
extern const char *        debug_progname;

/** Debug wrapper for the printf() function.  Your message will be printed
    only if the debug_level of the program is equal or greater than
    the level of your message.  For example, if you call:

      debug_printf (DEBUG_VERBOSE, "Warning, this might cause a problem");

    You are requesting this message to be printed if the current debug level
    is at least DEBUG_VERBOSE.  If the debug_level was DEBUG_NORMAL or
    below, you would not see the message printed.

    @param level This message is intended for output at debug 'level' or above
    @param fmt Refer to printf() documentation
    @param ... Refer to printf() documentation
    @return Refer to printf() documentation
*/
void debug_printf( debug_level_t level, const char *fmt, ... ) CHECK_PRINTF_FORMAT(2,3);
void debug_dprintf( int flags, debug_level_t level, const char *fmt, ... ) CHECK_PRINTF_FORMAT(3,4);

/** The conditional expression that controls whether output is
    actually printed.

    Usage: if (DEBUG_LEVEL(DEBUG_VERBOSE)) function_call();
 */
#define DEBUG_LEVEL(level) (debug_level >= (level))

/** Short for "Print Line".  Same as debug_printf(), except the progname and
    a '\n' are automatically printed.

    @param level This message is intended for output at debug 'level' or above
    @param fmt Refer to printf() documentation
    @param ... Refer to printf() documentation
    @return nothing
*/
void debug_println (debug_level_t level, const char *fmt, ...) CHECK_PRINTF_FORMAT(2,3);

/** Print an error an exit.  Same as debug_println(), except the progname
    will immediately called exit(error) and never return.

    @param error After printing the message, call exit(error)
    @param level This message is intended for output at debug 'level' or above
    @param fmt Refer to printf() documentation
    @param ... Refer to printf() documentation
    @return This function never returns, because it calls exit(error)
*/
void debug_error   (int error, debug_level_t level, const char *fmt, ...) CHECK_PRINTF_FORMAT(3,4);

//@}

void debug_cache_disable(void);
void debug_cache_enable(void);
void debug_cache_start_caching(void);
void debug_cache_stop_caching(void);
void debug_cache_flush(void);
void debug_cache_set_size(size_t size);

/**
 * The level of strictness in turning warnings into fatal errors.
 */
enum strict_level {
	DAG_STRICT_0 = 0,	// No warnings are errors
	DAG_STRICT_1 = 1,	// Most severe warnings are errors
	DAG_STRICT_2 = 2,	// Medium-severity warnings are errors
	DAG_STRICT_3 = 3	// Almost all warnings are errors
};

typedef enum strict_level strict_level_t;

/** Determine whether the strictness setting turns a warning into a fatal
	error.
	@param strictness: The strictness level of the warning.
	@param quit_if_error: Whether to exit immediately if the warning is
		treated as an error
	@return true iff the warning is treated as an error
*/
bool check_warning_strictness( strict_level_t strictness,
			bool quit_if_error = true );

/** Convert a timestamp into a string, formatted in the same way
    as HTCondor dprintf strings.
	@param timestamp:  The timestamp to convert.
	@param tstr:  The resulting string.
*/
void time_to_str( time_t timestamp, std::string &tstr );

/** Convert a timestamp into a string, formatted in the same way
    as HTCondor dprintf strings.
	@param tm:  The tm to convert.
	@param tstr:  The resulting string.
*/
void time_to_str( const struct tm *tm, std::string &tstr );

#endif /* ifndef DAGMAN_DEBUG_H */

