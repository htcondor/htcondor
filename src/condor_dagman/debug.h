/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef DAGMAN_DEBUG_H
#define DAGMAN_DEBUG_H

#include "condor_header_features.h"

BEGIN_C_DECLS   /* from condor_header_features.h */

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
  /** NEVER output, except for usage info */ DEBUG_SILENT  = 0,
  /** Very quiet, only servere errors */     DEBUG_QUIET   = 1,
  /** Normal output, errors and warnings */  DEBUG_NORMAL  = 2,
  /** Errors, and all warnings */            DEBUG_VERBOSE = 3,
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
extern char *        debug_progname;

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
void debug_printf( debug_level_t level, char *fmt, ... );
void debug_dprintf( int flags, debug_level_t level, char *fmt, ... );

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
void debug_println (debug_level_t level, char *fmt, ...);

/** Print an error an exit.  Same as debug_println(), except the progname
    will immediately called exit(error) and never return.

    @param error After printing the message, call exit(error)
    @param level This message is intended for output at debug 'level' or above
    @param fmt Refer to printf() documentation
    @param ... Refer to printf() documentation
    @return This function never returns, because it calls exit(error)
*/
void debug_error   (int error, debug_level_t level, char *fmt, ...);

//@}

END_C_DECLS /* from condor_header_features.h */

#endif /* ifndef DAGMAN_DEBUG_H */

