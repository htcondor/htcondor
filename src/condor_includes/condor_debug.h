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


#ifndef CONDOR_DEBUG_H
#define CONDOR_DEBUG_H

#include "condor_system.h"
#include "condor_header_features.h"

/*
**	Definitions for category and flags to pass to dprintf
**  Note: this is a little confusing, since the flags specify both
**  a debug category, and dprintf options (i.e., D_NOHEADER).  The debug
**  category flags use the lower order 8 bits while the option flag(s)
**  use the higher order bit(s).  This allows a maximum of 256 categories
**  although limitations in some of the underlying data structures currently
**  allow for only 32 categories (easily bumped to 64 categories).
*/

// The debug categories enum
enum {
   D_CATEGORY_BASE = 0,
   D_ALWAYS = 0,
   D_ERROR,
   D_STATUS,
   D_ZKM,
   D_JOB,
   D_MACHINE,
   D_CONFIG,
   D_PROTOCOL,
   D_PRIV,
   D_DAEMONCORE,
   D_GENERIC_VERBOSE, // (1<<D_GENERIC_VERBOSE) == D_FULLDEBUG
   D_SECURITY,
   D_COMMAND,
   D_MATCH,
   D_NETWORK,
   D_KEYBOARD,
   D_PROCFAMILY,
   D_IDLE,
   D_THREADS,
   D_ACCOUNTANT,
   D_SYSCALLS,
   D_CKPT,
   D_HOSTNAME,
   D_PERF_TRACE,
   D_LOAD,
   D_PROC,
   D_NFS,
   D_AUDIT, // messages for the audit log
   D_TEST,  // messages with this category are parsed by various tests.
   D_STATS,
   D_MATERIALIZE,
   D_BUG,   // messages that indicate the daemon is going down.

   // NOTE: can't go beyond 31 categories so long as DebugOutputChoice is just an unsigned int.

   // this must be last
   D_CATEGORY_COUNT
};
#define D_CATEGORY_MASK (0x1F)
#define D_CATEGORY_RESERVED_MASK (0xFF)

#define D_VERBOSE_MASK  (3<<8)
#define D_TERSE         (0<<8)
#define D_VERBOSE       (1<<8)
#define D_DIAGNOSTIC    (2<<8)
#define D_NEVER         (3<<8)

#define D_FULLDEBUG     (1<<10) // when or'd with a D_category, it means that category, or (D_ALWAYS|D_VERBOSE)
#define D_EXPR          (1<<11) // set by condor_submit, used by ??
#define D_FAILURE       (1<<12) // nearly always mixed with D_ALWAYS, ignored for now.


// format-modifying flags to change the appearance of the dprintf line
#define D_BACKTRACE     (1<<24) // print stack backtrace
#define D_IDENT         (1<<25) // 
#define D_SUB_SECOND    (1<<26) // print sub-second timestamp
#define D_TIMESTAMP     (1<<27) // print unix timestamp rather than human-readable time.
#define D_PID           (1<<28)
#define D_FDS           (1<<29)
#define D_CAT           (1<<30)
#define D_NOHEADER      (1<<31)
//#define D_ALL           (~(0) & (~(D_NOHEADER)))

// first re-definition pass.  add a separate set of flags for Verbose mode
// for each category. 
//
#define IsDebugLevel(cat)    ((AnyDebugBasicListener & (1<<((cat) & D_CATEGORY_MASK))) != 0)
#define IsDebugCategory(cat) ((AnyDebugBasicListener & (1<<((cat) &D_CATEGORY_MASK))) != 0)
#define IsDebugVerbose(cat)  ((AnyDebugVerboseListener & (1<<((cat) &D_CATEGORY_MASK))) != 0)
#define IsFulldebug(cat)     ((AnyDebugBasicListener & D_FULLDEBUG) != 0 || IsDebugVerbose(cat))
#define IsDebugCatAndVerbosity(flags) (((flags) & (D_VERBOSE_MASK | D_FULLDEBUG)) ? IsDebugVerbose(flags) : IsDebugLevel(flags))

// in the future, we will change the debug system to use a table rather than 
// a bit mask.  possibly this..
//#define IsDebugLevel(cat)    (DebugCategories[(cat) & D_CATEGORY_MASK)] > 0)
//   or this
//#define IsDebugLevel(cat)    (DebugLevels & (3<<((cat)*2))) != 0)


#include <string>
#include <map>

#if _MSC_VER >= 1400 /* VC++ 2005 version */
#define PREFAST_NORETURN __declspec(noreturn)
# ifdef _PREFAST_
// inform MSVC_ANALYZE of some true thing that it's having trouble figuring out for itself.
#  define PREFAST_ASSUME(x) __analysis_assume(!!(x))
# else
#  define PREFAST_ASSUME(x)
# endif
#else
#define PREFAST_NORETURN
#define PREFAST_ASSUME(x)
#endif

typedef unsigned int DebugOutputChoice;

extern unsigned int DebugHeaderOptions;	// for D_FID, D_PID, D_NOHEADER & D_
extern DebugOutputChoice AnyDebugBasicListener;   /* Bits to look for in dprintf */
extern DebugOutputChoice AnyDebugVerboseListener; /* verbose bits for dprintf */
extern int DebugShouldLockToAppend; /* Should we lock the file before each write? */

/* DebugId is a function that may be registered to be called to insert text
 * into the header of a line that is about to be logged to the debug log file.
 * It should treat its arguments similarly to how sprintf_realloc() does.
 * It should also set its return value similarly to sprintf_realloc().
 */
extern int (*DebugId)(char **buf,int *bufpos,int *buflen);

typedef unsigned long long DPF_IDENT;
void dprintf ( int flags, const char *fmt, ... ) CHECK_PRINTF_FORMAT(2,3);
void dprintf ( int flags, DPF_IDENT ident, const char *fmt, ... ) CHECK_PRINTF_FORMAT(3,4);
// parse config files and use them to fill out the array of dprintf_output_settings
// one for each output log file. returns the number of entries needed in p_info, (may be larger than c_info!)
// if p_info is NULL, then dprintf_set_outputs is called with the dprintf_output_settings array.  if != NULL, then
// the array is returned, calling dprintf_set_outputs is left to the caller.
//
// NOTE!!! as of May-2012, some of the dprintf globals are still set as side effects in this function
//         so you should always call it unless you are doing output purely to STDERR and want defaults
//         for all config knobs.
int dprintf_config( 
	const char *subsys,  // in: subsystem name to use for param lookups
	struct dprintf_output_settings *p_info = NULL, // in,out: if != NULL results of config parsing returned here
	int c_info = 0); // in: number of entries in p_info array on input.                  

int dprintf_config_tool(const char* subsys, const char * flags, const char * logfile = nullptr);
int dprintf_config_tool_on_error(const char * flags);

// parse a string of the form "NNN Unit" where NNN is an integer, and Unit is b, Kb, Mb, Gb, or Tb (size units) or s, m, h, d, or w (time units)
bool dprintf_parse_log_size(const char * input, long long  & value, bool & is_time);

// call when you want to insure that dprintfs are thread safe on Linux regardless of
// wether daemon core threads are enabled. thread safety cannot be disabled once enabled
// note that this is always implicitly called on Windows
void dprintf_make_thread_safe();

// parse strflags and cat_and_flags and merge them into the in,out args
// for backward compatibility, the D_ALWAYS bit will always be set in basic
// and bits passed in via the in,out args will be preserved unless explicitly cleared via D_FLAG:0 syntax.
void _condor_parse_merge_debug_flags(
	const char *strflags, // in: if not NULL, parse to get flags
	int         flags,    // in: set header flags, D_FULLDEBUG prior to parsing strflags
	unsigned int & HeaderOpts, // in,out: formatting options, D_PID, etc
	DebugOutputChoice & basic, // in,out: basic output choice
	DebugOutputChoice & verbose); // in,out: verbose output choice, expect this to get folded into basic someday.

bool dprintf_to_term_check();

void _condor_dprintf_va ( int flags, DPF_IDENT ident, const char* fmt, va_list args );
int _condor_open_lock_file(const char *filename,int flags, mode_t perm);
void PREFAST_NORETURN _EXCEPT_ ( const char *fmt, ... ) CHECK_PRINTF_FORMAT(1,2) GCC_NORETURN;
void Suicide(void) GCC_NORETURN;
void set_debug_flags( const char *strflags, int cat_and_flags );
void PREFAST_NORETURN _condor_dprintf_exit( int error_code, const char* msg ) GCC_NORETURN;
void _condor_fd_panic( int line, const char *file );
void _condor_set_debug_flags( const char *strflags, int cat_and_flags );
int  _condor_dprintf_is_initialized();
void _condor_save_dprintf_line_va( int flags, const char* fmt, va_list args );
void _condor_save_dprintf_line( int flags, const char* fmt, ... );
void _condor_dprintf_saved_lines( void );
void condor_except_should_dump_core(int flag);

int  dprintf_config_ContinueOnFailure( int fContinue );

/* Set whether daemon log rotation is allowed.
 * Sets DebugRotateLog.
 * Returns previous value of DebugRotateLog.
 */
bool dprintf_allow_log_rotation(bool allow_rotate);

/* must call this before clone(CLONE_VM|CLONE_VFORK) */
void dprintf_before_shared_mem_clone( void );

/* must call this after clone(CLONE_VM|CLONE_VFORK) returns */
void dprintf_after_shared_mem_clone( void );

/* A simple function for writing to the primary daemon log in an
 * async-safe manner (e.g. from a signal handler).
 * See safe_async_simple_fwrite_fd() for argument usage.
 */
#ifdef _WIN64
void dprintf_async_safe(char const *msg, ULONG_PTR *args, unsigned int num_args);
#else
void dprintf_async_safe( char const *msg, unsigned long *args, unsigned int num_args );
#endif

void dprintf_dump_stack(void);

/* If outputs haven't been configured yet, stop buffering dprintf()
 * output until they are configured.
 */
void dprintf_pause_buffering();

time_t dprintf_last_modification(void);
void dprintf_touch_log(void);
/* write dprintf contribution to the daemon header */
void dprintf_print_daemon_header(void);

/* reset statistics about delays acquiring the debug file lock */
void dprintf_reset_lock_delay(void);

/* return fraction of time spent waiting for debug file lock since
   start of program or last call to dprintf_reset_lock_delay */
double dprintf_get_lock_delay(void);

/* get a count of dprintf messages written (for statistics)
*/
int dprintf_getCount(void);

/* flush the buffered output that is created when TOOL_DEBUG_ON_ERROR is set
 */
int dprintf_WriteOnErrorBuffer(FILE * out, int fClearBuffer);

/* flush the buffered output that is created when TOOL_DEBUG_ON_ERROR is set
 * to this file on exit (during global class destruction), if code passed to 
 * dprintf_SetExitCode is 0, the OnErrorBuffer is discarded, otherwise it is 
 * written to out
 */
FILE * dprintf_OnExitDumpOnErrorBuffer(FILE * out);
int dprintf_SetExitCode(int code);

/* wrapper for fclose() that soaks up EINTRs up to maxRetries number of times.
 */
int fclose_wrapper( FILE *stream, int maxRetries );

/* Windows-specific function to provide strerror() like functionality for
 * Win32 and Winsock error codes. Returns an error string from a static
 * buffer (no need to free it), just like strerror.
 * errCode should come from GetLastError() or WSAGetLastError().
 * If errCode is 0 (default), then GetLastErrorString will invoke
 * GetLastError itself.
 */
#ifdef WIN32
const char* GetLastErrorString(DWORD errCode=0);
#endif

/*
**	Definition of exception macro
*/
#define EXCEPT \
	_EXCEPT_Line = __LINE__, \
	_EXCEPT_File = __FILE__,				\
	_EXCEPT_Errno = errno,					\
	_EXCEPT_

/*
**	Important external variables in libc
*/
#if !( defined(LINUX) || defined(Darwin) || defined(CONDOR_FREEBSD) )
extern DLL_IMPORT_MAGIC int		errno;
extern DLL_IMPORT_MAGIC int		sys_nerr;
#if _MSC_VER < 1400 /* VC++ 2005 version */
extern DLL_IMPORT_MAGIC char	*sys_errlist[];
#endif
#endif

extern int	_EXCEPT_Line;			/* Line number of the exception    */
extern const char	*_EXCEPT_File;		/* File name of the exception      */
extern int	_EXCEPT_Errno;			/* errno from most recent system call */
extern int (*_EXCEPT_Cleanup)(int,int,const char*);	/* Function to call to clean up (or NULL) */
extern void (*_EXCEPT_Reporter)(const char * msg, int line, const char * file); /* called instead of dprintf if non-NULL */
extern PREFAST_NORETURN void _EXCEPT_(const char*, ...) CHECK_PRINTF_FORMAT(1,2) GCC_NORETURN;

class dprintf_on_function_exit {
public:
	dprintf_on_function_exit(bool on_entry, int _flags, const char * fmt, ...);
	~dprintf_on_function_exit();
	std::string msg;
	int flags;
	bool print_on_exit;
};

// get (and reset) dprintf runtime statistics (if they are enabled)
bool _condor_dprintf_runtime (
	double & disabled_runtime,
	long & disabled_count,
	double & enabled_runtime,
	long & enabled_count,
	bool clear);

/* must call this upon entering child of fork() if child calls dprintf */
void dprintf_init_fork_child( bool cloned = false );

/* call this when done with dprintf in child of fork()
 * This is not necessary if child is just going to exit.  It just
 * ensures that nothing gets inherited by exec().
 */
void dprintf_wrapup_fork_child( bool cloned = false );

bool debug_open_fds(std::map<int,bool> &open_fds);

// fetch a monotonic timer intended for measuring the time spent
// doing various things.  this timer can NOT be counted on to
// be a normal timestamp! use double condor_gettimestamp_double() for that..
// The seconds value might be epoch time
// or it might be uptime depending on which system clock is used.
extern double _condor_debug_get_time_double();

class _condor_runtime
{
public:
	_condor_runtime() : begin(0) { begin = _condor_debug_get_time_double(); }; // save result here
	double elapsed_runtime() const { return _condor_debug_get_time_double() - begin; }
	double tick(double & last) { double now = _condor_debug_get_time_double(); double diff = now - last; last = now; return diff; }
	double reset() { return tick(begin); } // resets begin to now and returns the difference between now and former begin.
	double begin;
};

// use this class to automatically add time runtime between constructor and destructor
// into the given variable.
template <typename T>
class _condor_auto_accum_runtime : public _condor_runtime
{
public:
	_condor_auto_accum_runtime(T & store) : runtime(store) { }; // remember where to save result
	~_condor_auto_accum_runtime() { runtime += elapsed_runtime(); };
	T & runtime;
};

// print hex bytes from data into buf, up to a maximum of datalen bytes
// caller must supply the buffer and must insure that it is at least datalen*3+1
// if compact=true, then the buffer must be at least datalen*2+1
// this is intended to provide a way to add small hex dumps to dprintf logging
extern const char * debug_hex_dump(char * buf, const char * data, int datalen, bool compact = false);

#ifndef CONDOR_ASSERT
#define CONDOR_ASSERT(cond) \
	if( !(cond) ) { EXCEPT("Assertion ERROR on (%s)",#cond); }
#endif /* CONDOR_ASSERT */

#ifndef ASSERT
#	define ASSERT(cond) CONDOR_ASSERT(cond)
#endif /* ASSERT */

#ifndef TRACE
#define TRACE \
	fprintf( stderr, "TRACE at line %d in file \"%s\"\n",  __LINE__, __FILE__ );
#endif /* TRACE */

#ifndef PRAGMA_REMIND
# ifdef _MSC_VER // for Microsoft C, prefix file and line to the the message
#  define PRAGMA_QUOTE(x)   #x
#  define PRAGMA_QQUOTE(y)  PRAGMA_QUOTE(y)
#  define PRAGMA_REMIND(str) __pragma(message(__FILE__ "(" PRAGMA_QQUOTE(__LINE__) ") : " str))
#  define DEPRECATE_MSC __declspec(deprecated)
#  define DEPRECATE_GCC
# elif defined __GNUC__ // gcc emits file and line prefix automatically.
#  if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
#   define PRAGMA_QUOTE(x)  _Pragma(#x)
#   define PRAGMA_REMIND(str) PRAGMA_QUOTE(message(str))
#  else
#   define PRAGMA_REMIND(str)
#  endif
#  define DEPRECATE_MSC
#  define DEPRECATE_GCC __attribute__ ((deprecated))
# else 
#  define PRAGMA_REMIND(str)
#  define DEPRECATE_MSC
#  define DEPRECATE_GCC
# endif
#endif // REMIND

// disabled REMIND when building non-debug
#if defined NDEBUG && ! defined ENABLE_PRAGMA_REMIND
#  undef PRAGMA_REMIND
#  define PRAGMA_REMIND(str)
#endif

#if defined _MSC_VER && defined _DEBUG // WIN32
# ifdef _X86_
#  define DEBUG_BREAK_INTO_DEBUGGER _asm {int 3}
# else
#  define DEBUG_BREAK_INTO_DEBUGGER DebugBreak()
# endif
# define DEBUG_WAIT_FOR_DEBUGGER(var,def) { static int var=def; while (var) Sleep(1000); }
#else
# define DEBUG_BREAK_INTO_DEBUGGER ((void)0)
# define DEBUG_WAIT_FOR_DEBUGGER(var,def) ((void)0)
#endif

#define dprintf_set_tool_debug(name, flags) dprintf_config_tool(name, flags)
#define dprintf_set_tool_debug_log(name, flags, filename) dprintf_config_tool(name, flags, filename)

#endif /* CONDOR_DEBUG_H */

/* 
 * On Win32, define assert() to really do an Condor ASSERT(), which does EXCEPT.
 * NOTE THIS MUST BE PLACED AFTER THE #endif TO CONDOR_DEBUG_H because
 * this redefinition needs to be done more than once (because the WinNT header
 * files could redefine assert more than once... sigh).
*/
#ifdef WIN32
#	ifdef assert
#		undef assert
#	endif
#	ifdef ASSERT
#		undef ASSERT
#	endif
#	define ASSERT(cond) CONDOR_ASSERT(cond)
#	define assert(cond) CONDOR_ASSERT(cond)
#endif	/* of ifdef WIN32 */
