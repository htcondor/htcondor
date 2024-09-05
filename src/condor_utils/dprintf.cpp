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


 


/************************************************************************
**
**	Generic logging function.  Prints the message on DebugFP with a date if
**	any bits in "flags" are set in DebugFlags.  If locking is desired,
**	DebugLock should contain the name of the lock file.  If log length
**	management is desired, MaxLog should contain the maximum length of the
**	log in bytes.  (The log will be copied to "DebugFile.old", so MaxLog
**	should be half of the space you are willing to devote.  If both log 
**	length management and locking are desired, the lock file should not be
**	the same as the log file.  Along with the date, other identifying
**	information can be logged with the message by supplying the function
**	(*DebugId)() which takes DebugFP as an argument.
**
************************************************************************/
#include "condor_common.h"
#include "condor_sys_types.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "exit.h"
#include "condor_uid.h"
#include "basename.h"
#include "file_lock.h"
#if HAVE_BACKTRACE
#include <execinfo.h>
#elif defined WIN32
# if NTDDI_VERSION > NTDDI_WINXP
#   include "winnt.h" // for CaptureStackBackTrace
#   include <DbgHelp.h> // for symbol lookup
#   define HAVE_BACKTRACE
    static bool backtrace_have_symbols = false;
# endif
#endif
#include "condor_threads.h"
#include "log_rotate.h"
#include "dprintf_internal.h"
#include "utc_time.h"

#if defined(HAVE__FTIME)
# include <sys/timeb.h>
#endif
#if defined(HAVE_GETTIMEOFDAY)
# include <sys/time.h>
#endif
#if defined(HAVE_CLOCK_GETTTIME)
# include <time.h>
#endif

// call when you want to insure that dprintfs are thread safe on Linux regardless of
// wether daemon core threads are enabled. thread safety cannot be disabled once enabled
#ifdef WIN32
static bool _dprintf_expect_threads = true;
#else
static bool _dprintf_expect_threads = false;
#endif
void dprintf_make_thread_safe() {
	_dprintf_expect_threads = true;
}

// define this to have D_TIMESTAMP|D_SUB_SECOND be microseconds rather than milliseconds
// this is useful mostly when trying to put log entries from multiple daemons on the same
// machine in order
//#define D_SUB_SECOND_IS_MICROSECONDS

extern const char * const _condor_DebugCategoryNames[D_CATEGORY_COUNT];

static FILE *debug_lock_it(struct DebugFileInfo* it, const char *mode, int force_lock, bool dont_panic);
static void debug_unlock_it(struct DebugFileInfo* it);
static FILE *open_debug_file(struct DebugFileInfo* it, const char flags[], bool dont_panic);
static void debug_close_file(struct DebugFileInfo* it);
static void debug_close_all_files(void);
static void debug_open_lock(void);
static void debug_close_lock(void);
static FILE *preserve_log_file(struct DebugFileInfo* it, bool dont_panic, time_t tt);

FILE *open_debug_file( int debug_level, const char flags[] );

void _condor_set_debug_flags( const char *strflags, int cat_and_flags );
void _condor_save_dprintf_line_va( int flags, const char* fmt, va_list args );
void _condor_save_dprintf_line( int flags, const char* fmt, ... );
void _condor_dprintf_saved_lines( void );
struct saved_dprintf {
	int level;
	char* line;
	struct saved_dprintf* next;
};
static struct saved_dprintf* saved_list = NULL;
static struct saved_dprintf* saved_list_tail = NULL;

char * DebugLogDir = NULL; // set to the value of the $(LOG) macro

extern	DLL_IMPORT_MAGIC int		errno;
extern unsigned int DebugHeaderOptions;	// for D_FID, D_PID, D_NOHEADER & D_
extern DebugOutputChoice AnyDebugBasicListener; /* Bits to look for in dprintf */
extern DebugOutputChoice AnyDebugBasicListener; /* verbose bits for dprintf */
//extern  int		DebugFlags;

/*
   This is a global flag that tells us if we've successfully ran
   dprintf_config() or otherwise setup dprintf() to print where we
   want it to go.  We use it here so that if we call dprintf() before
   dprintf_config(), we save the messages into a special list and
   dump them all out once dprintf is configured.
*/
extern int _condor_dprintf_works;

int DebugShouldLockToAppend = 0;
static int DebugIsLocked = 0;

static int DebugLockDelay = 0; /* seconds spent waiting for lock */
static time_t DebugLockDelayPeriodStarted = 0;

/*
 * We have the ability to hold log files open between writes. This has
 * caused some grief with respect to log rotation on Linux because two
 * processes have open FDs to the same file and may attempt to rotate
 * in succession.
 */
std::vector<DebugFileInfo> * DebugLogs = NULL;
/*
 * This is last modification time of the main debug file as returned
 * by stat() before the current process has written anything to the
 * file. It is set in dprintf_config, which sets it to -errno if that
 * stat() fails.
 * DaemonCore uses this as an approximation of when the daemon
 * was last alive.
 */
time_t	DebugLastMod = 0;

/*
 * If LOGS_USE_TIMESTAMP is enabled, we will print out Unix timestamps
 * instead of the standard date format in all the log messages
 */
char *	DebugTimeFormat = NULL;

/*
 * if TOOL_DEBUG_ON_ERROR is set, then every dprintf writes to this buffer
 * in addition to anything defined in TOOL_DEBUG.  The contents of this buffer
 * can be flushed and written by calling dprintf_WriteOnErrorBuffer
 */
static std::string DebugOnErrorBuffer;
void * dprintf_get_onerror_data() { return (void*)&DebugOnErrorBuffer; }
int dprintf_WriteOnErrorBuffer(FILE * out, int fClearBuffer) {
	int cch = 0;
	if (out) {
		if ( ! DebugOnErrorBuffer.empty()) {
			cch = (int)fwrite(DebugOnErrorBuffer.c_str(), 1, DebugOnErrorBuffer.size(), out);
		}
	}
	if (fClearBuffer) {
		DebugOnErrorBuffer.clear();
	}
	return cch;
}

static class dpf_on_error_trigger {
	FILE * file;
	int    code; // write if code is non-zero
public:
	dpf_on_error_trigger() : file(NULL), code(1) {}
	~dpf_on_error_trigger() {
		if (code && file && ! DebugOnErrorBuffer.empty()) {
			fprintf(file, "\n---------------- TOOL_DEBUG_ON_ERROR output -----------------\n");
			dprintf_WriteOnErrorBuffer(file, true);
			fprintf(file, "---------------- TOOL_DEBUG_ON_ERROR ends -------------------\n");
		}
	}
	FILE * WriteOnErrorExit(FILE * out) { FILE * tmp = file; file = out; return tmp; }
	int ExitCode(int n) { return code = n; }
} _dprintf_on_error_trigger;
FILE * dprintf_OnExitDumpOnErrorBuffer(FILE * out) { return _dprintf_on_error_trigger.WriteOnErrorExit(out); }
int dprintf_SetExitCode(int code) { return _dprintf_on_error_trigger.ExitCode(code); }


/*
 * When true, don't exit even if we fail to open the debug output file.
 * Added so that on Win32 the kbdd (which is running as a user) won't quit 
 * if it does't have access to the directory where log files live.
 *
 */
int      DebugContinueOnOpenFailure = 0;

/*
** These arrays must be D_NUMLEVELS+1 in size since we can have a
** debug file for each level plus an additional catch-all debug file
** at index 0.
*/
char	*DebugLock = NULL;
int		DebugLockIsMutex = -1;

int		(*DebugId)(char **buf,int *bufpos,int *buflen);

int		LockFd = -1;

bool	log_keep_open = false;

bool 	should_block_signals = false;

static bool DebugRotateLog = true;

static	int DprintfBroken = 0;
static	int DebugUnlockBroken = 0;
#if !defined(WIN32) && defined(HAVE_PTHREADS)
#include <pthread.h>
static pthread_mutex_t _condor_dprintf_critsec = 
#if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
						PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
						PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#endif
#endif
#ifdef WIN32
static CRITICAL_SECTION	*_condor_dprintf_critsec = NULL;
static int lock_or_mutex_file(int fd, LOCK_TYPE type, int do_block);
extern int vprintf_length(const char *format, va_list args);
static HANDLE debug_win32_mutex = NULL;
#endif
static int dprintf_count = 0;
/*
** Note: setting this to true will avoid blocking signal handlers from running
** while we are printing log messages.  It's probably a good idea to block
** them as they may get into trouble with manipulating the lock on the
** log file.  However blocking them will cause many implementations of
** dbx to hang.
*/
int InDBX = 0;

#define DPRINTF_ERR_MAX 255

#define FCLOSE_RETRY_MAX 10

// fetch a monotonic timer intended for measuring the time spent
// doing various things.  this timer can NOT be counted on to
// be a normal timestamp.  The seconds value might be epoch time
// or it might be uptime depending on which system clock is used.
double _condor_debug_get_time_double()
{
#ifdef UNIX
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	return (double)tm.tv_sec + (tm.tv_nsec * 1.0e-9);
#elif defined(WIN32)
	LARGE_INTEGER li;
	static bool initialized = false;
	static double secs_per_tick = 0;
	if ( ! initialized) {
		QueryPerformanceFrequency(&li);
		secs_per_tick = 1.0 / li.QuadPart;
		initialized = true;
	}
	QueryPerformanceCounter(&li);
	return li.QuadPart * secs_per_tick;
#endif
}

// print hex bytes from data into buf, up to a maximum of datalen bytes
// caller must supply the buffer and must insure that it is at least datalen*3+1
// this is intended to provide a way to add small hex dumps to dprintf logging
static char hex_digit(unsigned char n) { return n + ((n < 10) ? '0' : ('a' - 10)); }
const char * debug_hex_dump(char * buf, const char * data, int datalen, bool compact)
{
	if (!buf) return "";
	const unsigned char * d = (const unsigned char *)data;
	char * p = buf;
	char * endp = buf;
	while (datalen-- > 0) {
		unsigned char ch = *d++;
		*p++ = hex_digit((ch >> 4) & 0xF);
		*p++ = hex_digit(ch & 0xF);
		endp = p;
		if (!compact) {
			*p++ = ' ';
		}
	}
	*endp = 0;
	return buf;
}

DebugFileInfo::~DebugFileInfo()
{
	if(outputTarget == FILE_OUT && debugFP)
	{
		fclose(debugFP);
		debugFP = NULL;
	}
}

DebugFileInfo::DebugFileInfo(const dprintf_output_settings& p)
	: outputTarget(STD_OUT), choice(p.choice), verbose(p.VerboseCats), headerOpts(p.HeaderOpts)
	, debugFP(NULL), dprintfFunc(_dprintf_global_func), userData(0), logPath(p.logPath)
	, maxLog(p.logMax), logZero(0), maxLogNum(p.maxLogNum)
	, want_truncate(p.want_truncate), accepts_all(p.accepts_all)
	, rotate_by_time(p.rotate_by_time), dont_panic(p.optional_file)
{}

bool DebugFileInfo::MatchesCatAndFlags(int cat_and_flags) const
{
	// the the message matches the verbose mask then it must match the basic choice mask also
	// so if it matches, we are done.
	unsigned int cat_mask = (1<<(cat_and_flags&D_CATEGORY_MASK));
	if (this->verbose & cat_mask)
		return true;
	// if the message is tagged as D_EXCEPT or D_ERROR_ALSO message it is
	// always in the D_ERROR:1 category as well is its normal category
	if ((cat_and_flags & D_ERROR_MASK) && (this->choice & (1<<D_ERROR)))
		return true;
	// if the message has the verbose bit set and it has not matched yet, then no match
	if (cat_and_flags & (D_VERBOSE_MASK | D_FULLDEBUG))
		return false;
	// if the message is D_ALWAYS (which is 0) and no verbose flags (because if the test above)
	// then it matches if this output has accepts_all
	if (((cat_and_flags & D_CATEGORY_MASK) == D_ALWAYS) && this->accepts_all)
		return true;
	return (this->choice & cat_mask) != 0;
}

static char *formatTimeHeader(struct tm *tm) {
	static char timebuf[80];
	static int firstTime = 1;

	if (firstTime) {
		firstTime = 0;
		if (!DebugTimeFormat) {
			DebugTimeFormat = strdup("%m/%d/%y %H:%M:%S");
		}
	}
	strftime(timebuf, 80, DebugTimeFormat, tm);
	return timebuf;
}

const char* _format_global_header(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info)
{
	time_t clock_now = info.tv.tv_sec;

	static char *buf = NULL;
	static int buflen = 0;
	int bufpos = 0;
	int rc = 0;
	int sprintf_errno = 0;

	int my_pid;
	int my_tid;

	hdr_flags |= (cat_and_flags & ~D_CATEGORY_RESERVED_MASK); // pick up flags passed directly to dprintf call
	unsigned char cat = (unsigned char)(cat_and_flags & D_CATEGORY_MASK);

		/* Print the message with the time and a nice identifier */
	if( ! (hdr_flags & D_NOHEADER)) {
		if (hdr_flags & D_TIMESTAMP) {
				// Casting clock_now to int to get rid of compile
				// warning.  Probably format should be %ld, and
				// we should cast to long int, but I'm afraid of
				// changing the output format.  wenger 2009-02-24.
			if (hdr_flags & D_SUB_SECOND) {
				#ifdef D_SUB_SECOND_IS_MICROSECONDS
				rc = sprintf_realloc( &buf, &bufpos, &buflen, "%d.%06d ", (int)clock_now, (int)info.tv.tv_usec );
				#else
				int seconds = (int)clock_now;
				int micros = info.tv.tv_usec + 500;
				if( micros >= 1'000'000 ) { micros = 0; seconds += 1; }
				rc = sprintf_realloc( &buf, &bufpos, &buflen, "%d.%03d ", seconds, micros / 1000 );
				#endif
			} else {
				rc = sprintf_realloc( &buf, &bufpos, &buflen, "%lld ", (long long)clock_now );
			}
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		} else {
			if (hdr_flags & D_SUB_SECOND) {
				#ifdef D_SUB_SECOND_IS_MICROSECONDS
				rc = sprintf_realloc( &buf, &bufpos, &buflen, "%s.%06d ", formatTimeHeader(info.tm), (int)info.tv.tv_usec );
				#else
				struct tm * then = info.tm;
				int micros = info.tv.tv_usec + 500;
				if( micros >= 1'000'000 ) {
					micros = 0;
					time_t seconds = clock_now + 1;
					then = localtime(& seconds);
				}
				rc = sprintf_realloc( &buf, &bufpos, &buflen, "%s.%03d ", formatTimeHeader(then), micros / 1000 );
				#endif
			} else {
				rc = sprintf_realloc( &buf, &bufpos, &buflen, "%s ", formatTimeHeader(info.tm));
			}
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

		if (hdr_flags & D_FDS) {
			// Print the lowest available fd, as a guess if we're leaking fds.
			rc = sprintf_realloc(&buf, &bufpos, &buflen, "(fd:%d) ", safe_open_last_fd);
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

		if (hdr_flags & D_PID) {
#ifdef WIN32
			my_pid = (int) GetCurrentProcessId();
#else
			my_pid = (int) getpid();
#endif
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(pid:%d) ", my_pid );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

			/* include tid if we are configured to use a thread pool */
		my_tid = CondorThreads_gettid();
		if ( my_tid > 0 ) {
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(tid:%d) ", my_tid );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

		if (hdr_flags & D_IDENT) {
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(cid:%llu) ", info.ident );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

		if (hdr_flags & D_BACKTRACE) {
			rc = sprintf_realloc( &buf, &bufpos, &buflen, "(bt:%04x:%d) ", info.backtrace_id, info.num_backtrace );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}

		if ((hdr_flags & D_CAT) && /*cat > 0 &&*/ cat < D_CATEGORY_COUNT) {
			char verbosity[10] = "";
			int sprintf_error = 0;
			if (cat_and_flags & (D_VERBOSE_MASK | D_FULLDEBUG)) {
				int verb = 1 + ((cat_and_flags & D_VERBOSE_MASK) >> 8);
				if (cat_and_flags & D_FULLDEBUG) verb = 2;
				sprintf_error = snprintf(verbosity, sizeof(verbosity), ":%d", verb);
				if(sprintf_error < 0)
				{
					_condor_dprintf_exit(sprintf_error, "Error writing to debug header\n");	
				}
			}
			// If the D_ERROR_ALSO or D_EXCEPT flag was or'd to the category, then this is both a category message (i.e. D_AUDIT)
			// and should also show up in the primary log as a D_ERROR category message. we print this as |D_FAILURE for
			// most categories, but just as D_ERROR for D_ALWAYS and D_ERROR categories.
			const char * orfail = "";
			if (cat_and_flags & D_ERROR_MASK) {
				if (cat > D_ERROR) { orfail = "|D_FAILURE"; }
				else { cat = D_ERROR; }
			}
			// For now, print D_STATUS messages as D_ALWAYS.. TODO: TJ remove this someday!
			if (cat == D_STATUS) { cat = D_ALWAYS; }
			rc = sprintf_realloc(&buf, &bufpos, &buflen, "(%s%s%s) ", _condor_DebugCategoryNames[cat], verbosity, orfail);
			if (rc < 0) sprintf_errno = errno;
		}

		if( DebugId ) {
			rc = (*DebugId)( &buf, &bufpos, &buflen );
			if( rc < 0 ) {
				sprintf_errno = errno;
			}
		}
	}
	else
	{
		return NULL;
	}

	if( sprintf_errno != 0 ) {
		_condor_dprintf_exit(sprintf_errno, "Error writing to debug header\n");
	}

	return buf;
}

void
_dprintf_global_func(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo)
{
	int start_pos = 0;
	int bufpos = 0;
	int rc = 0;
	static char* buffer = NULL;
	static int buflen = 0;
	hdr_flags |= dbgInfo->headerOpts;
	const char* header = _format_global_header(cat_and_flags, hdr_flags, info);
	if(header)
	{
		rc = sprintf_realloc(&buffer, &bufpos, &buflen, "%s", header);
		if (rc < 0) {
			_condor_dprintf_exit(errno, "Error writing to debug header\n");	
		}
	}
	rc = sprintf_realloc(&buffer, &bufpos, &buflen, "%s", message);
	if (rc < 0) {
		_condor_dprintf_exit(errno, "Error writing to debug message\n");	
	}

	if ((hdr_flags & D_BACKTRACE) && info.num_backtrace && info.backtrace) {
	#ifdef HAVE_BACKTRACE
		static unsigned int trace_mask[0x10000/32] = {0};
		int ix_mask = info.backtrace_id / 32;
		int mask_bit = 1<<(info.backtrace_id%32);
		if ( ! (trace_mask[ix_mask] & mask_bit)) {
			trace_mask[ix_mask] |= mask_bit;
			rc = sprintf_realloc(&buffer, &bufpos, &buflen, "\tBacktrace bt:%04x:%d is\n", info.backtrace_id, info.num_backtrace);
			#ifdef WIN32
			if (true) {
				#ifdef _DBGHELP_
				if ( ! backtrace_have_symbols) {
					static bool tried_sym_init = false;
					if ( ! tried_sym_init) {
						tried_sym_init = true;
						backtrace_have_symbols = SymInitialize(GetCurrentProcess(),NULL,TRUE);
					}
				}
				#endif
				char szModule[MAX_PATH], szFileAndLine[MAX_PATH];
				for (int ix = 0; ix < info.num_backtrace; ++ix) {
					void * pfn = info.backtrace[ix];
					#ifdef _DBGHELP_
					if (backtrace_have_symbols) {
						DWORD displace32 = 0;
						IMAGEHLP_LINE64 li; li.SizeOfStruct = sizeof(li);
						if (SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)(ULONG_PTR)pfn, &displace32, &li)) {
							// we want to print only the part of the file path that is inside the HTCondor source tree
							PCHAR pszFile = li.FileName;
							for (PCHAR p = pszFile; *p; ++p) {
								if (*p == '/' || *p == '\\') {
									pszFile = p+1;
									if ((p[1] == 's') && (p[2] == 'r') && (p[3]=='c') && (p[4]=='\\' || p[4] == '/')){
										pszFile = p+5; break;
									}
								}
							}
							sprintf_s(szFileAndLine, COUNTOF(szFileAndLine), "%s(%d) ", pszFile, li.LineNumber);
						} else {
							szFileAndLine[0] = 0;
						}
						DWORD64 displacement = 0;
						SYMBOL_INFO * psym = (SYMBOL_INFO*)szModule;
						psym->SizeOfStruct = sizeof(SYMBOL_INFO);
						psym->MaxNameLen = 1 + (sizeof(szModule) - sizeof(SYMBOL_INFO)) / sizeof(psym->Name[0]);
						if (SymFromAddr(GetCurrentProcess(), (DWORD64)(ULONG_PTR)pfn, &displacement, psym)) {
							int off = (int)((ULONG64)pfn - (ULONG64)psym->Address);
							sprintf_realloc(&buffer, &bufpos, &buflen, "\t%p %s%s+%d\n", pfn, szFileAndLine, psym->Name, off);
							continue;
						}
					}
					#endif // _DGBHELP_
					MEMORY_BASIC_INFORMATION mbi;
					SIZE_T cb = VirtualQuery (pfn, &mbi, sizeof(mbi));
					int off = (int)((const char*)pfn - (const char*)mbi.AllocationBase);
					if (cb == sizeof(mbi) && mbi.AllocationBase != nullptr) {
						if (GetModuleFileNameA ((HMODULE)mbi.AllocationBase, szModule, COUNTOF(szModule))) {
							// print only the part after the last path separator.
							char * pname = filename_from_path(szModule);
							sprintf_realloc(&buffer, &bufpos, &buflen, "\t%p %s+%d\n", pfn, pname, off);
						} else {
							sprintf_realloc(&buffer, &bufpos, &buflen, "\t%p %p+%d\n", pfn, (void*)mbi.AllocationBase, off);
						}
					} else {
						sprintf_realloc(&buffer, &bufpos, &buflen, "\t%p\n", pfn);
					}
				}
			}
			#else // ! WIN32
			char ** syms =  backtrace_symbols(info.backtrace, info.num_backtrace);
			if (syms) {
				for (int jj = 0; jj < info.num_backtrace; ++jj) {
					const char * sym = syms[jj];
					// strip off path, keep only filename
					//sym = filename_from_path(sym);
					rc = sprintf_realloc(&buffer, &bufpos, &buflen, "\t%s\n", sym);
					if (rc < 0) break;
				}
				free(syms);
			}
			#endif
			else {
				buffer[bufpos-1] = ' ';
				for (int jj = 0; jj < info.num_backtrace; ++jj) {
					const char * fmt = (jj+1 == info.num_backtrace) ? "%p\n" : "%p, ";
					sprintf_realloc(&buffer, &bufpos, &buflen, fmt, info.backtrace[jj]);
				}
			}
		}
	#endif // HAVE_BACKTRACE
	}

	if ( ! dbgInfo->debugFP && dbgInfo->dont_panic) {
		// TODO: buffer until the file opens?
		return;
	}
		// We attempt to write the log record with one call to
		// write(), because then O_APPEND will ensure (on
		// compliant file systems) that writes from different
		// processes are not interleaved.  Since we are blocking
		// signals, we should not need to loop here on EINTR,
		// but we do anyway in case one of the exotic signals
		// that we are not blocking interrupts us.
	while( start_pos<bufpos ) {
		rc = write( fileno(dbgInfo->debugFP),
					buffer+start_pos,
					bufpos-start_pos );
		if( rc > 0 ) {
			start_pos += rc;
		}
		else if( errno != EINTR ) {
			_condor_dprintf_exit(errno, "Error writing debug log\n");
		}
	}
}

/* _condor_dfprintf_va
 * This function is used internally by the dprintf system wherever
 * it wants to write directly to the open debug log.
 *
 * Since this function is called from a code path that is already
 * not reentrant, this function itself does not bother to try
 * to be reentrant.  Specifically, it uses static buffers.  If we
 * ever do reenter this function it is assumed that the program
 * will abort before returning to the prior call to this function.
 *
 * The caller of this function should not assume that args can be
 * used again on systems where va_copy is required.  If the caller
 * wishes to use args again, the caller should therefore pass in
 * a copy of the args.
 */

static void
_condor_dfprintf_va( int cat_and_flags, int hdr_flags, DebugHeaderInfo &info, DebugFileInfo *dbgInfo, const char* fmt, va_list args )
{
		// static buffer to avoid frequent memory allocation
	static char *buf = NULL;
	static int buflen = 0;
	int bufpos = 0;
	int rc = 0;

	rc = vsprintf_realloc( &buf, &bufpos, &buflen, fmt, args );
	if (rc < 0) {
		_condor_dprintf_exit(errno, "Error writing to debug buffer\n");	
	}

	dbgInfo->dprintfFunc(cat_and_flags, hdr_flags, info, buf, dbgInfo);
}

// forward ref to the function that helps use remove dprintf calls from the call stack
#ifdef HAVE_BACKTRACE
static bool is_dprintf_function_addr(const void * pfn);
#endif

/* _condor_dprintf_getbacktrace
 * fill in backtrace in the DebugHeaderInfo structure, paying attention to dprintf flags
 * and returning modified dprintf flags if requested.
 */
static int _condor_dprintf_getbacktrace(DebugHeaderInfo &info, unsigned int hdr_flags, unsigned int * phdr_flags_out = NULL)
{
	info.backtrace = NULL;
	info.backtrace_id = 0;
	info.num_backtrace = 0;
#ifdef HAVE_BACKTRACE
	if (hdr_flags & D_BACKTRACE) {
		static void * tracebuf[50];
		info.backtrace = tracebuf;
		#ifdef WIN32
		int num_trace = CaptureStackBackTrace(0, COUNTOF(tracebuf), tracebuf, NULL);
		#else
		int num_trace = backtrace(tracebuf, COUNTOF(tracebuf));
		#endif
		// skip over the dprintf calls themselves, beginning the backtrace in the caller of dprintf.
		int skip = 0;
		for (skip = 0; skip < num_trace; ++skip) {
			if ( ! is_dprintf_function_addr(tracebuf[skip]))
				break;
		}
		//if (skip > 0) { --skip; } // keep at least the first dprintf call.
		info.num_backtrace = num_trace - skip;
		info.backtrace = &tracebuf[skip];
		if (info.num_backtrace <= 0) {
			hdr_flags &= ~D_BACKTRACE;
			info.num_backtrace = 0;
		} else {
			unsigned short * ps = (unsigned short *)info.backtrace;
			unsigned int id = 0;
			for (int ii = 0; ii < info.num_backtrace * (int)(sizeof(void*)/sizeof(short)); ++ii) {
				id += ps[ii];
			}
			info.backtrace_id = (id & 0xFFFF) ^ (id >> 16);
		}
	}
#else
	hdr_flags &= ~D_BACKTRACE;
#endif
	if (phdr_flags_out) *phdr_flags_out = hdr_flags;
	return info.num_backtrace;
}

/* _condor_dprintf_gettime
 * fill in current time in the DebugHeaderInfo structure, paying attention to dprintf flags
 * and returning modified dprintf flags if requested.
 */
static void _condor_dprintf_gettime(DebugHeaderInfo &info, unsigned int hdr_flags)
{
	if (hdr_flags & D_SUB_SECOND) {
		condor_gettimestamp(info.tv);
	} else {
		info.tv.tv_sec = time(NULL);
		info.tv.tv_usec = 0;
	}
	if ( ! (hdr_flags & D_TIMESTAMP)) {
		// On windows, timeval::tv_sec is a long, not a time_t
		time_t now = info.tv.tv_sec;
		info.tm = localtime(&now);
	}
}

/* _condor_dfprintf
 * This function is used internally by the dprintf system wherever
 * it wants to write directly to the open debug log.
 */
static void
_condor_dfprintf( struct DebugFileInfo* it, const char* fmt, ... )
{
	DebugHeaderInfo info;
    va_list args;
	unsigned int hdr_flags = DebugHeaderOptions;

	memset((void*)&info,0,sizeof(info)); // just to stop Purify UMR errors
	_condor_dprintf_gettime(info, hdr_flags);
	if (hdr_flags & D_BACKTRACE) _condor_dprintf_getbacktrace(info, hdr_flags, &hdr_flags);

    va_start( args, fmt );
	_condor_dfprintf_va(D_ALWAYS, hdr_flags, info,it,fmt,args);
    va_end( args );
}

int dprintf_getCount(void)
{
    return dprintf_count;
}

/*
** Print a nice log message, but only if "flags" are included in the
** current debugging flags.
*/
/* VARARGS1 */

// prototype
struct tm *localtime();

//#define ENABLE_DPRINTF_PROFILING 1
#ifdef ENABLE_DPRINTF_PROFILING

class _dprintf_va_runtime : public _condor_runtime
{
public:
	bool is_enabled;
	_dprintf_va_runtime() : is_enabled(false) { };
	~_dprintf_va_runtime() {
		if (is_enabled) {
			enabled_runtime += elapsed_runtime();
			enabled_count += 1;
		} else {
			disabled_runtime += elapsed_runtime();
			disabled_count += 1;
		}
	};
	static double disabled_runtime;
	static double enabled_runtime;
	static long disabled_count;
	static long enabled_count;
	static void clear() {
		enabled_runtime = disabled_runtime = 0;
		enabled_count = disabled_count = 0;
	}
};

double _dprintf_va_runtime::enabled_runtime = 0;
double _dprintf_va_runtime::disabled_runtime = 0;
long _dprintf_va_runtime::enabled_count = 0;
long _dprintf_va_runtime::disabled_count = 0;

#endif // ENABLE_DPRINTF_PROFILING

bool _condor_dprintf_runtime (
	double & disabled_runtime,
	long & disabled_count,
	double & enabled_runtime,
	long & enabled_count,
	bool clear)
{
#ifdef ENABLE_DPRINTF_PROFILING
	disabled_runtime = _dprintf_va_runtime::disabled_runtime;
	disabled_count = _dprintf_va_runtime::disabled_count;
	enabled_runtime = _dprintf_va_runtime::enabled_runtime;
	enabled_count = _dprintf_va_runtime::enabled_count;
	if (clear) { _dprintf_va_runtime::clear(); }
	return true;
#else
	enabled_runtime = disabled_runtime = 0;
	enabled_count = disabled_count = 0;
	if (clear) {}
	return false;
#endif
}


void
_condor_dprintf_va( int cat_and_flags, DPF_IDENT ident, const char* fmt, va_list args )
{
	static char* message_buffer = NULL;
	static int buflen = 0;
	int bufpos = 0;
	DebugHeaderInfo info;
	//struct tm *tm=0;
	//time_t clock_now;
#if !defined(WIN32)
	sigset_t	mask, omask;
#endif
	int saved_errno;
	priv_state	priv;
	std::vector<DebugFileInfo>::iterator it;

#ifdef ENABLE_DPRINTF_PROFILING
	_dprintf_va_runtime art;
#endif

		/* DebugFP should be static initialized to stderr,
	 	   but stderr is not a constant on all systems. */

		/* If we hit some fatal error in dprintf, this flag is set.
		   If dprintf is broken and someone (like _EXCEPT_Cleanup)
		   trys to dprintf, we just return to avoid infinite loops. */
	if( DprintfBroken ) return;

		/* 
		   See if dprintf_config() has been called.  if not, save the
		   message into a list so we can dump them out all at once
		   when we've got a working log file.  we need to do this
		   before we check the debug cat_and_flags since they won't be
		   initialized until we call dprintf_config().
		*/
	if( ! _condor_dprintf_works ) {
		_condor_save_dprintf_line_va( cat_and_flags, fmt, args );
		return; 
	} 

		/* See if this is one of the messages we are logging */
	if ( ! IsDebugCatAndVerbosity(cat_and_flags) && ! (cat_and_flags & D_ERROR_MASK))
		return;

	// if this dprintf is enabled, switch runtime accumulation into the enabled counters
#ifdef ENABLE_DPRINTF_PROFILING
	art.is_enabled = true;
#endif

#if !defined(WIN32) /* signals don't exist in WIN32 */

	/* Block any signal handlers which might try to print something */
	/* Note: do this BEFORE grabbing the _condor_dprintf_critsec mutex */

	// Do we really need this?  Not sure.  Cowardly conditionalizing
	// this with a knob  Profiling shows the sigprocmask is more
	// expensive that you might think, especially with D_FULLDEBUG
	// or code that dprintfs a lot..

	if (should_block_signals) {
		sigfillset( &mask );
		sigdelset( &mask, SIGABRT );
		sigdelset( &mask, SIGBUS );
		sigdelset( &mask, SIGFPE );
		sigdelset( &mask, SIGILL );
		sigdelset( &mask, SIGSEGV );
		sigdelset( &mask, SIGTRAP );
		sigprocmask( SIG_BLOCK, &mask, &omask );
	}
#endif

	/* We want dprintf to be thread safe.  For now, we achieve this
	 * with fairly coarse-grained mutex. On Unix, signals that may result
	 * in a call to dprintf() had better be blocked by now, or deadlock may 
	 * occur.
	 */
#ifdef WIN32
	if ( _condor_dprintf_critsec == NULL ) {
		_condor_dprintf_critsec = 
			(CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
		ASSERT( _condor_dprintf_critsec );
		MSC_SUPPRESS_WARNING(28125) // suppress warning: InitCritSec should be called inside a try/except block.
		InitializeCriticalSection(_condor_dprintf_critsec);
	}
	EnterCriticalSection(_condor_dprintf_critsec);
#elif defined(HAVE_PTHREADS)
	/* On Win32 we always grab a mutex because we are always running
	 * with mutiple threads.  But on Unix, lets bother w/ mutexes if and only
	 * if we are running w/ threads.
	 */
	if ( _dprintf_expect_threads || CondorThreads_pool_size() ) {  /* will == 0 if no threads running */
		pthread_mutex_lock(&_condor_dprintf_critsec);
	}
#endif

	saved_errno = errno;


	/* log files owned by condor system acct */

		/* If we're in PRIV_USER_FINAL, there's a good chance we won't
		   be able to write to the log file.  We can't rely on Condor
		   code to refrain from calling dprintf() after switching to
		   PRIV_USER_FINAL.  So, we check here and simply don't try to
		   log anything when we're in PRIV_USER_FINAL, to avoid
		   exit(DPRINTF_ERROR). */
	if (get_priv() == PRIV_USER_FINAL) {
		/* Ensure to undo the signal blocking code for unix and
			leave the critical section for windows. */
		goto cleanup;
	}

	{
		static int in_nonreentrant_part = 0;
		if( in_nonreentrant_part ) {
			/* Some of the following code messes with global state and
			 * does not expect to be called recursively.  Note that if
			 * we do get called recursively, the locking that happens
			 * before this point is expected to work correctly (avoid
			 * self-deadlock).
			 */
			goto cleanup;
		}
		in_nonreentrant_part = 1;

			/* avoid priv macros so we can bypass priv logging */
		priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

			/* Grab the time info only once, instead of inside the for
			   loop.  -Derek 9/14 */
		memset((void*)&info,0,sizeof(info)); // just to stop Purify UMR errors
		info.ident = ident;
		unsigned int hdr_flags = DebugHeaderOptions | (cat_and_flags & D_BACKTRACE);
		_condor_dprintf_gettime(info, hdr_flags);
		if (hdr_flags & D_BACKTRACE) _condor_dprintf_getbacktrace(info, hdr_flags, &hdr_flags);
	
		#ifdef va_copy
		va_list copyargs;
		va_copy(copyargs, args);
		int rc = vsprintf_realloc(&message_buffer, &bufpos, &buflen, fmt, copyargs);
		va_end(copyargs);
		#else
		int rc = vsprintf_realloc(&message_buffer, &bufpos, &buflen, fmt, args);
		#endif
		if (rc < 0) {
			_condor_dprintf_exit(errno, "Error writing to debug buffer\n");	
		}

			/* print debug message to catch-all debug file plus files */
			/* registered for other debug levels */
		if(!DebugLogs->size())
		{
			DebugFileInfo backup;
			backup.outputTarget = STD_ERR;
			backup.debugFP = stderr;
			backup.dprintfFunc = _dprintf_global_func;
			backup.dprintfFunc(cat_and_flags, hdr_flags, info, message_buffer, &backup);
			backup.debugFP = NULL; // don't allow destructor to free stderr
		}

		int ixOutput = 0;

		//PRAGMA_REMIND("TJ: fix this to distinguish between verbose:2 and verbose:3")
		for(it = DebugLogs->begin(); it < DebugLogs->end(); it++, ++ixOutput)
		{
			if ( ! it->MatchesCatAndFlags(cat_and_flags))
				continue;

			/* Open and lock the log file */
			bool   funlock_it = false;
			switch ((*it).outputTarget) {
				case STD_ERR: it->debugFP = stderr; break;
				case STD_OUT: it->debugFP = stdout; break;
				case SYSLOG: break;
				default:
				case FILE_OUT:
					debug_lock_it(&(*it), NULL, 0, it->dont_panic);
					funlock_it = it->debugFP != nullptr; // can be null only when dont_panic is true.
					break;
				case OUTPUT_DEBUG_STR: // use for >BUFFER and on windows for logging to the Windows debug stream
					break;
			}
			
			it->dprintfFunc(cat_and_flags, hdr_flags, info, message_buffer, &(*it));
			if (funlock_it) {
				debug_unlock_it(&(*it));
			}
		}

			/* restore privileges */
		_set_priv(priv, __FILE__, __LINE__, 0);

        dprintf_count += 1;

		in_nonreentrant_part = 0;
	}

	cleanup:

	errno = saved_errno;

	/* Release mutex.  Note: we MUST do this before we renable signals */
#ifdef WIN32
	LeaveCriticalSection(_condor_dprintf_critsec);
#elif defined(HAVE_PTHREADS)
	if ( _dprintf_expect_threads || CondorThreads_pool_size() ) {  /* will == 0 if no threads running */
		pthread_mutex_unlock(&_condor_dprintf_critsec);
	}
#endif

#if !defined(WIN32) // signals don't exist in WIN32
		/* Let them signal handlers go!! */
	if (should_block_signals) {
		std::ignore = sigprocmask( SIG_SETMASK, &omask, 0 );
	}	
#endif
}

int
_condor_open_lock_file(const char *filename,int flags, mode_t perm)
{
	int	retry = 0;
	int save_errno = 0;
	priv_state	priv;
	int lock_fd;

	if( !filename ) {
		return -1;
	}

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);
	lock_fd = safe_open_wrapper_follow(filename,flags,perm);
	if( lock_fd < 0 ) {
		save_errno = errno;
		if( save_errno == ENOENT ) {
				/* 
				   No directory: Try to create the directory
				   itself, first as condor, then as root.  If
				   we created it as root, we need to try to
				   chown() it to condor.
				*/ 
			std::string dirpath = condor_dirname( filename );
			errno = 0;
			if( mkdir(dirpath.c_str(), 0777) < 0 ) {
				if( errno == EACCES ) {
						/* Try as root */ 
					_set_priv(PRIV_ROOT, __FILE__, __LINE__, 0);
					if( mkdir(dirpath.c_str(), 0777) < 0 ) {
						/* We failed, we're screwed */
						fprintf( stderr, "Can't create lock directory \"%s\", "
								 "errno: %d (%s)\n", dirpath.c_str(), errno, 
								 strerror(errno) );
					} else {
						/* It worked as root, so chown() the
						   new directory and set a flag so we
						   retry the safe_open_wrapper(). */
#ifndef WIN32
						if (chown( dirpath.c_str(), get_condor_uid(),
								   get_condor_gid() )) {
							fprintf( stderr, "Failed to chown(%s) to %d.%d: %s\n",
									 dirpath.c_str(), get_condor_uid(),
									 get_condor_gid(), strerror(errno) );
						}
#endif
						retry = 1;
					}
					_set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);
				} else {
						/* Some other error than access, give up */ 
					fprintf( stderr, "Can't create lock directory: \"%s\""
							 "errno: %d (%s)\n", dirpath.c_str(), errno, 
							 strerror(errno) );							
				}
			} else {
					/* We succeeded in creating the directory,
					   try the safe_open_wrapper() again */
				retry = 1;
			}
		}
		if( retry ) {
			lock_fd = safe_open_wrapper_follow(filename,flags,perm);
			if( lock_fd < 0 ) {
				save_errno = errno;
			}
		}
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	if( lock_fd < 0 ) {
		errno = save_errno;
	}
	return lock_fd;
}

/* debug_open_lock
 * - assumes correct priv state (PRIV_CONDOR) has already been set
 * - aborts the program on error
 */
static void
debug_open_lock(void)
{
	int save_errno;
	char msg_buf[DPRINTF_ERR_MAX];
	struct stat fstatus;
	time_t start_time,end_time;

	if ( DebugLockIsMutex == -1 ) {
#ifdef WIN32
		// Use a mutex by default on Win32
		//DebugLockIsMutex = (int)dprintf_param_funcs->param_boolean("FILE_LOCK_VIA_MUTEX", true);
//PRAGMA_REMIND("Figure out better way of doing this without relying on param!!!!")
		DebugLockIsMutex = TRUE;
#else
		// Use file locking by default on Unix.  We should 
		// call param_boolean here, but since locking via
		// a mutex is not yet implemented on Unix, we will force it
		// to always be FALSE no matter what the config file says.
		// DebugLockIsMutex = (int)param_boolean("FILE_LOCK_VIA_MUTEX", false);
		DebugLockIsMutex = FALSE;
#endif
	}

		/* Acquire the lock */
	if( DebugLock ) {
		if ( ! DebugLockIsMutex) {
			if (LockFd > 0 ) {
					// fstat can't possibly fail, right?			
				(void) fstat(LockFd, &fstatus);
				if (fstatus.st_nlink == 0){
					close(LockFd);
					LockFd = -1;
				}	
			}
			if (LockFd < 0) {
				LockFd = _condor_open_lock_file(DebugLock,O_CREAT|O_WRONLY|_O_NOINHERIT,0660);
				if( LockFd < 0 ) {
					save_errno = errno;
					snprintf( msg_buf, sizeof(msg_buf), "Can't open \"%s\"\n", DebugLock );
					_condor_dprintf_exit( save_errno, msg_buf );
				} 
			}	
		}

		start_time = time(NULL);
		if( DebugLockDelayPeriodStarted == 0 ) {
			DebugLockDelayPeriodStarted = start_time;
		}

		errno = 0;
#ifdef WIN32
		if( lock_or_mutex_file(LockFd,WRITE_LOCK,TRUE) < 0 )
#else
		if( lock_file_plain(LockFd,WRITE_LOCK,TRUE) < 0 )
#endif
		{
			save_errno = errno;
			snprintf( msg_buf, sizeof(msg_buf), "Can't get exclusive lock on \"%s\", "
					 "LockFd: %d\n", DebugLock, LockFd );
			_condor_dprintf_exit( save_errno, msg_buf );
		}

		DebugIsLocked = 1;

			/* Update DebugLockDelay.  Ignore delays that are less than
			 * two seconds because the resolution is only 1s.
			 */
		end_time = time(NULL);
		if( end_time-start_time > 1 ) {
			DebugLockDelay += end_time-start_time;
		}
	}
}

void dprintf_reset_lock_delay(void) {
	DebugLockDelay = 0;
	DebugLockDelayPeriodStarted = 0;
}

double dprintf_get_lock_delay(void) {
	time_t now = time(NULL);
	if( now - DebugLockDelayPeriodStarted <= 0 ) {
		return 0;
	}
	return ((double)DebugLockDelay)/(now-DebugLockDelayPeriodStarted);
}

static FILE *
debug_lock_it(struct DebugFileInfo* it, const char *mode, int force_lock, bool dont_panic)
{
	long long length = 0; // this gets assigned return value from lseek()
	time_t now = 0;         // this gets assigned only when rotate_by_time is true
	time_t rotation_timestamp = 0;
	priv_state	priv;
	int save_errno;
	char msg_buf[DPRINTF_ERR_MAX];
	int locked = 0;
	FILE *debug_file_ptr = it->debugFP;

	if ( mode == NULL ) {
		mode = "aN";
	}

	errno = 0;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	if(debug_file_ptr)
	{
		//Hypothetically if we never closed the file, we
		//should have never unlocked it either.  The best
		//way to handle this will need further thought.
		if( DebugShouldLockToAppend || force_lock )
			locked = 1;
	}
	else
	{
		if( DebugShouldLockToAppend || force_lock ) {
			debug_open_lock();
			locked = 1;
		}

		//open_debug_file will set DebugFPs[debug_level] so we do
		//not have to worry about it in this function, assuming
		//there are no further errors.
		debug_file_ptr = open_debug_file(it, mode, dont_panic);

		if( debug_file_ptr == NULL ) {
			
			save_errno = errno;
			if (dont_panic) {
				_set_priv(priv, __FILE__, __LINE__, 0);
				return NULL;
			}
#ifdef WIN32
			if (DebugContinueOnOpenFailure) {
				_set_priv(priv, __FILE__, __LINE__, 0);
				return NULL;
			}
#else
			if( errno == EMFILE ) {
				_condor_fd_panic( __LINE__, __FILE__ );
			}
#endif
			snprintf( msg_buf, sizeof(msg_buf), "Could not open DebugFile \"%s\"\n", 
				it->logPath.c_str() );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
	}

	if (it->rotate_by_time) {
		now = time(NULL);
		if (it->maxLog) {
			long long now_quantized = quantizeTimestamp(now, it->maxLog);
			if ( ! it->logZero) {
				struct stat stat_buf;
				if (fstat(fileno(debug_file_ptr), &stat_buf) >= 0) {
					it->logZero = stat_buf.st_mtime;
				} else {
					it->logZero = now;
				}
			}
			long long zero_quantized = quantizeTimestamp((time_t)it->logZero, it->maxLog);
			if (now_quantized >= zero_quantized) {
				length = now_quantized - zero_quantized;
				rotation_timestamp = zero_quantized;
			} else {
				// clock went backwards what do we do now?
			}
		}
	} else {
		// when we preserve more than 1 old log file, we use rotation times
		// as file extensions, so we need to have an initialized rotation timestamp.
		rotation_timestamp = time(NULL);

	#ifdef WIN32
		length = _lseeki64(fileno(debug_file_ptr), 0, SEEK_END);
	#elif Linux
		length = lseek64(fileno(debug_file_ptr), 0, SEEK_END);
	#else
		length = lseek(fileno(debug_file_ptr), 0, SEEK_END);
	#endif
		if(length < 0 ) {
			if (dont_panic) {
				if(locked) debug_close_lock();
				debug_close_file(it);

				return NULL;
			}
			save_errno = errno;
			snprintf( msg_buf, sizeof(msg_buf), "Can't seek to end of DebugFP file\n" );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
	}

	//This is checking for a non-zero max length.  Zero is infinity.
	if( DebugRotateLog && it->maxLog && length >= it->maxLog ) {
		if( !locked ) {
			/*
			 * We only need to redo everything if there is a lock defined
			 * for the log.
			 */

			if (debug_file_ptr) {
				int result = fflush( debug_file_ptr );
				if (result < 0) {
					DebugUnlockBroken = 1;
					_condor_dprintf_exit(errno, "Can't fflush debug log file\n");
				}
			}

			/*
			 * We need to be in PRIV_CONDOR for the code in these two
			 * functions, so since we are already in that privilege mode,
			 * we do not go back to the old priv state until we call the
			 * two functions.
			 */
			if(DebugLock)
			{
				debug_close_lock();
				debug_close_file(it);
				_set_priv(priv, __FILE__, __LINE__, 0);
				return debug_lock_it(it, mode, 1, dont_panic);
			}
		}

		_condor_dfprintf(it, "MaxLog = %lld %s, length = %lld\n",
			it->maxLog, it->rotate_by_time ? "sec" : "bytes", length);
		
		debug_file_ptr = preserve_log_file(it, dont_panic, rotation_timestamp);
		if (it->rotate_by_time) {
			it->logZero = now;
		}
	}

	_set_priv(priv, __FILE__, __LINE__, 0);

	return debug_file_ptr;
}

static void 
debug_close_lock(void)
{
	int flock_errno;
	char msg_buf[DPRINTF_ERR_MAX];
	if(DebugUnlockBroken)
		return;

	if(DebugIsLocked)
	{
		errno = 0;
#ifdef WIN32
		if ( lock_or_mutex_file(LockFd,UN_LOCK,TRUE) < 0 )
#else
		if( lock_file_plain(LockFd,UN_LOCK,TRUE) < 0 )
#endif
		{
			flock_errno = errno;
			snprintf( msg_buf, sizeof(msg_buf), "Can't release exclusive lock on \"%s\", LockFd=%d\n", 
				DebugLock, LockFd );
			DebugUnlockBroken = 1;
			_condor_dprintf_exit( flock_errno, msg_buf );
		}
		DebugIsLocked = 0;
	}
}

static void 
debug_close_file(struct DebugFileInfo* it)
{
	FILE *debug_file_ptr = (*it).debugFP;

	if( debug_file_ptr ) {
		if (debug_file_ptr) {
			int close_result = fclose_wrapper( debug_file_ptr, FCLOSE_RETRY_MAX );
			if (close_result < 0) {
				DebugUnlockBroken = 1;
				_condor_dprintf_exit(errno, "Can't fclose debug log file\n");
			}
			(*it).debugFP = NULL;
		}
	}
}

static void 
debug_close_all_files()
{
	if ( ! DebugLogs) return;

	std::vector<DebugFileInfo>::iterator it;
	for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
	{
		if (it->outputTarget != FILE_OUT)
			continue;

		FILE *debug_file_ptr = (*it).debugFP;
		if(!debug_file_ptr)
			continue;
		int close_result = fclose_wrapper( debug_file_ptr, FCLOSE_RETRY_MAX );
		if (close_result < 0) {
			DebugUnlockBroken = 1;
			_condor_dprintf_exit(errno, "Can't fclose debug log file\n");
		}
		(*it).debugFP = NULL;
	}
}

static void
debug_unlock_it(struct DebugFileInfo* it)
{
	priv_state priv;
	int result = 0;

	FILE *debug_file_ptr = (*it).debugFP;

	if(log_keep_open)
		return;

	if( DebugUnlockBroken ) {
		return;
	}

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	if (debug_file_ptr) {
		result = fflush( debug_file_ptr );
		if (result < 0) {
				DebugUnlockBroken = 1;
				_condor_dprintf_exit(errno, "Can't fflush debug log file\n");
		}

		debug_close_lock();
		debug_close_file(it);
	}

	_set_priv(priv, __FILE__, __LINE__, 0);
}


/*
** Copy the log file to a backup, then truncate the current one.
*/
static FILE *
preserve_log_file(struct DebugFileInfo* it, bool dont_panic, time_t now)
{
	char		old[MAXPATHLEN + 4];
	priv_state	priv;
	int			still_in_old_file = FALSE;
	int			failed_to_rotate = FALSE;
	int			save_errno;
	const char *timestamp;
	int			result;
	int			file_there = 0;
	FILE		*debug_file_ptr = (*it).debugFP;
	std::string		filePath = (*it).logPath;
	char msg_buf[DPRINTF_ERR_MAX + MAXPATHLEN + 4];


	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);
	(void)setBaseName(filePath.c_str());
	timestamp = createRotateFilename(NULL, it->maxLogNum, now);
	(void)snprintf( old, sizeof(old), "%s.%s", filePath.c_str() , timestamp);
	_condor_dfprintf( it, "Saving log file to \"%s\"\n", old );
	(void)fflush( debug_file_ptr );

	fclose_wrapper( debug_file_ptr, FCLOSE_RETRY_MAX );
	debug_file_ptr = NULL;
	(*it).debugFP = debug_file_ptr;

	result = rotateTimestamp(timestamp, it->maxLogNum, now);

#if defined(WIN32)
	if (result != 0) { // MoveFileEx and Copy failed
		failed_to_rotate = TRUE;
		debug_file_ptr = open_debug_file(it, "wN", dont_panic);
		if ( debug_file_ptr ==  NULL ) {
			still_in_old_file = TRUE;
		}
	}
#else

	errno = 0;
	if (result != 0) {
		failed_to_rotate = TRUE;
		save_errno = result;
		if( save_errno == ENOENT && !DebugLock ) {
				/* This can happen if we are not using debug file locking,
				   and two processes try to rotate this log file at the
				   same time.  The other process must have already done
				   the rename but not created the new log file yet.
				*/
		}
		else {
			snprintf( msg_buf, sizeof(msg_buf), "Can't rename(%s,%s)\n", filePath.c_str(), old );
			_condor_dprintf_exit( save_errno, msg_buf );
		}
	}

	/* double check the result of the rename
	   If we are not using locking, then it is possible for two processes
	   to rotate at the same time, in which case the following check
	   should be skipped, because it is expected that a new file may
	   have already been created by now. */

	if( DebugLock && DebugShouldLockToAppend ) {
		errno = 0;
		struct stat statbuf;
		if (stat (filePath.c_str(), &statbuf) >= 0)
		{
			file_there = 1;
			snprintf( msg_buf, sizeof(msg_buf), "rename(%s) succeeded but file still exists!\n", 
					 filePath.c_str() );
			/* We should not exit here - file did rotate but something else created it newly. We
			 therefore won't grow without bounds, we "just" lost control over creating the file.
			 We should happily continue anyway and just put a log message into the system telling
			 about this incident.
			 */
		}
	}

#endif

	if (debug_file_ptr == NULL) {
		debug_file_ptr = open_debug_file(it, "aN", dont_panic);
	}

	if( debug_file_ptr == NULL ) {
		debug_file_ptr = stderr;

		save_errno = errno;
		snprintf( msg_buf, sizeof(msg_buf), "Can't open file for debug level %d\n",
				 it->choice ); 
		_condor_dprintf_exit( save_errno, msg_buf );
	}

	if ( !still_in_old_file ) {
		_condor_dfprintf (it, "Now in new log file %s\n", it->logPath.c_str());
	}

	// We may have a message left over from the succeeded rename after which the file
	// may have been recreated by another process. Tell user about it.
	if (file_there > 0) {
		_condor_dfprintf(it, "WARNING: %s", msg_buf);
	}

	if ( failed_to_rotate ) {
	#ifdef WIN32
		const char * reason_hint = "Perhaps another process is keeping log files open?";
	#else
		const char * reason_hint = "Likely cause is that another Condor process rotated the file at the same time.";
	#endif
		_condor_dfprintf(it,"WARNING: Failed to rotate old log into file %s!\n       %s\n",old,reason_hint);
	}
	
	_set_priv(priv, __FILE__, __LINE__, 0);
	cleanUpOldLogFiles(it->maxLogNum);

	return debug_file_ptr;
}


#if !defined(WIN32)
/*
** Can't open log or lock file becuase we are out of fd's.  Try to let
** somebody know what happened.
*/
void
_condor_fd_panic( int line, const char* file )
{
	int i;
	char msg_buf[DPRINTF_ERR_MAX * 2];
	char panic_msg[DPRINTF_ERR_MAX];
	int save_errno;
	std::vector<DebugFileInfo>::iterator it;
	std::string filePath;
	bool fileExists = false;
	FILE* debug_file_ptr=0;

	_set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	snprintf( panic_msg, sizeof(panic_msg),
			 "**** PANIC -- OUT OF FILE DESCRIPTORS at line %d in %s",
			 line, file );

		/* Just to be extra paranoid, let's nuke a bunch of fds. */
	for ( i=0 ; i<50 ; i++ ) {
		(void)close( i );
	}

	it = DebugLogs->begin();
	if (it != DebugLogs->end())
	{
		filePath = (*it).logPath;
		fileExists = true;
	}
	if( fileExists ) {
		debug_file_ptr = safe_fopen_wrapper_follow(filePath.c_str(), "a", 0644);
	}

	if( !debug_file_ptr ) {
		save_errno = errno;
		snprintf( msg_buf, sizeof(msg_buf), "Can't open \"%s\"\n%s\n", filePath.c_str(), panic_msg );
		_condor_dprintf_exit( save_errno, msg_buf );
	}
		/* Seek to the end */
#if Linux
	lseek64(fileno(debug_file_ptr), 0, SEEK_END);
#else
	lseek(fileno(debug_file_ptr), 0, SEEK_END);
#endif
	fprintf( debug_file_ptr, "%s\n", panic_msg );
	(void)fflush( debug_file_ptr );

	_condor_dprintf_exit( 0, panic_msg );
}
#endif
	

#ifdef NOTDEF
void tzset(){}
extern char	**environ;

char *
_getenv( name )
char	*name;
{
	char	**envp;
	char	*p1, *p2;

	for( envp = environ; *envp; envp++ ) {
		for( p1 = name, p2 = *envp; *p1 && *p2 && *p1 == *p2; p1++, p2++ )
			;
		if( *p1 == '\0' ) {
			return p2;
		}
	}
	return (char *)0;
}

char *
getenv( name )
char	*name;
{
	return _getenv(name);
}

sigset(){}
#endif

static FILE *
open_debug_file(struct DebugFileInfo* it, const char flags[], bool dont_panic)
{
	FILE		*fp;
	priv_state	priv;
	int save_errno;
	const std::string & filePath = (*it).logPath;

	priv = _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 0);

	/* Note: The log file shouldn't need to be group writeable anymore,
	   since PRIV_CONDOR changes euid now. */

	errno = 0;
	if( (fp=safe_fopen_wrapper_follow(filePath.c_str(),flags,0644)) == NULL ) {
		save_errno = errno;
#if !defined(WIN32)
		if( errno == EMFILE ) { // too many files
			_condor_fd_panic( __LINE__, __FILE__ );
		}
#endif
		// unless we are told not to panic about bad log files (used for optional second logs)
		// write a failure message to stderr and (if we are aborting) also create a file with the same text
		if ( ! dont_panic) {
			std::string msg_buf;
			formatstr(msg_buf, "Can't open \"%s\"\n", filePath.c_str());

			// since debugFP isn't set to anything, we can borrow it to write to stderr
			it->debugFP = stderr;
			_condor_dfprintf(it, msg_buf.c_str());

			if ( ! DebugContinueOnOpenFailure) {
				_condor_dprintf_exit( save_errno, msg_buf.c_str() );
			}
		}

		// fp is guaranteed to be NULL here.
		it->debugFP = nullptr;
	}

	_set_priv(priv, __FILE__, __LINE__, 0);
	it->debugFP = fp;

	return fp;
}

bool debug_check_it(struct DebugFileInfo& it, bool fTruncate, bool dont_panic)
{
	FILE *debug_file_fp;

	if( fTruncate ) {
		debug_file_fp = debug_lock_it(&it, "wN", 0, dont_panic);
	} else {
		debug_file_fp = debug_lock_it(&it, "aN", 0, dont_panic);
	}

	if (debug_file_fp) (void)debug_unlock_it(&it);
	return (debug_file_fp != NULL);
}

/* dprintf() hit some fatal error and is going to exit. */
void
_condor_dprintf_exit( int error_code, const char* msg )
{
	FILE* fail_fp;
	char buf[DPRINTF_ERR_MAX];
	char header[DPRINTF_ERR_MAX];
	char tail[DPRINTF_ERR_MAX];
	int wrote_warning = FALSE;
	struct tm *tm;
	time_t clock_now;

		/* We might land here with DprintfBroken true if our call to
		   dprintf_unlock() down below hits an error.  Since the
		   "error" that it hit might simply be that there was no lock,
		   we don't want to overwrite the original dprintf error
		   message with a new one, so skip most of the following if
		   DprintfBroken is already true.
		*/
	if( !DprintfBroken ) {
		(void)time( &clock_now );

		if (DebugHeaderOptions & D_TIMESTAMP) {
				// Casting clock_now to int to get rid of compile warning.
				// Probably format should be %ld, and we should cast to long
				// int, but I'm afraid of changing the output format.
				// wenger 2009-02-24.
			snprintf( header, sizeof(header), "%lld ", (long long)clock_now );
		} else {
			tm = localtime( &clock_now );
			snprintf( header, sizeof(header), "%d/%d %02d:%02d:%02d ",
					  tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, 
					  tm->tm_min, tm->tm_sec );
		}
		snprintf( header, sizeof(header), "dprintf() had a fatal error in pid %d\n", (int)getpid() );
		tail[0] = '\0';
		if( error_code ) {
			snprintf( tail, sizeof(tail), " errno: %d (%s)", error_code,
					 strerror(error_code) );
		}
#ifndef WIN32			
		snprintf( buf, sizeof(buf), " euid: %d, ruid: %d", (int)geteuid(),
				 (int)getuid() );
		strcat( tail, buf );
#endif

		if( DebugLogDir ) {
			snprintf( buf, sizeof(buf), "%s/dprintf_failure.%s",
					  DebugLogDir, get_mySubSystemName() );
			fail_fp = safe_fopen_wrapper_follow( buf, "wN",0644 );
			if( fail_fp ) {
				fprintf( fail_fp, "%s%s%s\n", header, msg, tail );
				fclose_wrapper( fail_fp, FCLOSE_RETRY_MAX );
				wrote_warning = TRUE;
			} 
		}
		if( ! wrote_warning ) {
			fprintf( stderr, "%s%s%s\n", header, msg, tail );
		}
			/* First, set a flag so we know not to try to keep using
			   dprintf during the rest of this */
		DprintfBroken = 1;

			/* Don't forget to unlock the log file, if possible! */
		debug_close_lock();
		debug_close_all_files();
	}

		/* If _EXCEPT_Cleanup is set for cleaning up during EXCEPT(),
		   we call that here, as well. */
	if( _EXCEPT_Cleanup ) {
		(*_EXCEPT_Cleanup)( __LINE__, errno, "dprintf hit fatal errors" );
	}

		/* Actually exit now */
	fflush (stderr);

	exit(DPRINTF_ERROR); 
}


/*
  We want these all to have _condor in front of them for inside the
  user job, but the rest of the Condor code just calls the regular
  versions.  So, we'll just call the "safe" version so we can share
  the code in both places. -Derek Wright 9/29/99
*/
void
set_debug_flags( const char *strflags, int cat_and_flags )
{
	_condor_set_debug_flags( strflags, cat_and_flags );
}


time_t
dprintf_last_modification()
{
	return DebugLastMod;
}

void
dprintf_touch_log()
{
	std::vector<DebugFileInfo>::iterator it;
	if ( _condor_dprintf_works ) {
		if (!DebugLogs->empty()) {
			it = DebugLogs->begin();
#ifdef WIN32
			utime( it->logPath.c_str(), NULL );
#else
		/* The following updates the ctime without touching 
			the mtime of the file.  This way, we can differentiate
			a "heartbeat" touch from a append touch
		*/
			(void) chmod( it->logPath.c_str(), 0644);
#endif
		}
	}
}

bool dprintf_retry_errno( int value )
{
#ifdef WIN32
	return false;
#else
	return value == EINTR;
#endif
}

/* This function calls fclose(), soaking up EINTRs up to maxRetries times.
   The motivation for this function is Gnats PR 937 (DAGMan crashes if
   straced).  Psilord investigated this and found that, because LIGO
   had their dagman.out files on NFS, stracing DAGMan could interrupt
   an fclose() on the dagman.out file.  So hopefully this will fix the
   problem...   wenger 2008-07-01.
 */
int
fclose_wrapper( FILE *stream, int maxRetries )
{

	int		result = 0;

	int		retryCount = 0;
	bool	done = FALSE;

	ASSERT( maxRetries >= 0 );
	while ( !done ) {
		if ( ( result = fclose( stream ) ) != 0 ) {
			if ( dprintf_retry_errno( errno ) && retryCount < maxRetries ) {
				retryCount++;
			} else {
				fprintf( stderr, "fclose_wrapper() failed after %d retries; "
							"errno: %d (%s)\n",
							retryCount, errno, strerror( errno ) );
				done = true;
			}
		} else {
			done = true;
		}
	}

	return result;
}

void
_condor_save_dprintf_line( int flags, const char* fmt, ... )
{
	va_list ap;
	va_start(ap, fmt);
	_condor_save_dprintf_line_va( flags, fmt, ap );
	va_end(ap);
}

void
_condor_save_dprintf_line_va( int flags, const char* fmt, va_list args )
{
	char* buf;
	struct saved_dprintf* new_node;
	int len;

		/* figure out how much space we need to store the string */
	len = vprintf_length( fmt, args )+1; /* add 1 for the null terminator */
	if( len <= 0 ) { 
		return;
	}
		/* make a buffer to hold it and print it there */
	buf = (char *)malloc( sizeof(char) * (len + 1) );
	if( ! buf ) {
		EXCEPT( "Out of memory!" );
	}
	vsnprintf( buf, len, fmt, args );

		/* finally, make a new node in our list and save the line */
	new_node = (struct saved_dprintf *)malloc( sizeof(struct saved_dprintf) );
	ASSERT( new_node != NULL );
	if( saved_list == NULL ) {
		saved_list = new_node;
	} else {
		saved_list_tail->next = new_node;
	}
	saved_list_tail = new_node;
	new_node->next = NULL;
	new_node->level = flags;
	new_node->line = buf;
}


void
_condor_dprintf_saved_lines( void )
{
	struct saved_dprintf* node;
	struct saved_dprintf* next;

	if( ! saved_list ) {
		return;
	}

	if( ! _condor_dprintf_works ) {
		// if this function was called, but there's no place to put
		// the saved dprintf messages, there's nothing we can do at the
		// moment so just return.  The messages are still saved.
		return;
	}

	node = saved_list;
	while( node ) {
			/* 
			   print the line.  since we've already got the complete
			   string, including all the original args, we won't have
			   any optional args or a va_list.  we're just printing a
			   string literal.  however, we want to do it with a %s so
			   that the underlying vfprintf() code doesn't try to
			   interpret any format strings that might still exist in
			   the string literal, since that'd screw us up.  we
			   definitely want to use the real dprintf() code so we
			   get the locking, potentially different log files, all
			   that stuff handled for us automatically.
			*/
		dprintf( node->level, "%s", node->line );

			/* save the next node so we don't loose it */
		next = node->next;

			/* make sure we don't leak anything */
		free( node->line );
		free( node );

		node = next;
	}

		/* now that we deallocated everything, clear out our pointer
		   to the list so it's not dangling. */
	saved_list = NULL;
}

const char * _condor_print_dprintf_info(DebugFileInfo & info, std::string & out)
{
	extern const char * const _condor_DebugCategoryNames[D_CATEGORY_COUNT];
	const unsigned int all_category_bits = ((unsigned int)1 << (D_CATEGORY_COUNT-1)) | (((unsigned int)1 << (D_CATEGORY_COUNT-1))-1);

	DebugOutputChoice base = info.choice;
	DebugOutputChoice verb = info.verbose;
	const unsigned int D_ALL_HDR_FLAGS = D_PID | D_FDS | D_CAT;
	bool has_all_hdr_opts = (info.headerOpts & D_ALL_HDR_FLAGS) == D_ALL_HDR_FLAGS;

	const char * sep = "";
	if (base && (base == verb)) {
		out += sep; sep = " ";
		out += "D_FULLDEBUG";
		verb = 0;
	}
	if (base == all_category_bits) {
		out += sep; sep = " ";
		out += has_all_hdr_opts ? "D_ALL" : "D_ANY";
		base = 0;
	}

	for (int cat = D_ALWAYS; cat < D_CATEGORY_COUNT; ++cat) {
		if (cat == D_GENERIC_VERBOSE) continue; // this is accounted for above..
		DebugOutputChoice mask = 1 << cat;
		if (mask & (base | verb)) {
			out += sep; sep = " ";
			out += _condor_DebugCategoryNames[cat];
			if (mask & verb) {
				out += ":2";
			}
		}
	}
	return out.c_str();
}

void dprintf_print_daemon_header(void)
{
	std::string d_log;
	if (DebugLogs->size() > 0) {
		_condor_print_dprintf_info((*DebugLogs)[0], d_log);
		dprintf(D_ALWAYS, "Daemon Log is logging: %s\n", d_log.c_str());
	}
	size_t ix = DebugLogs->size();
	if (ix > 1 && (*DebugLogs)[ix-1].accepts_all) {
		d_log.clear();
		_condor_print_dprintf_info((*DebugLogs)[ix-1], d_log);
		dprintf(D_ALWAYS, " +logging: %s to %s\n", d_log.c_str(), (*DebugLogs)[ix-1].logPath.c_str());
	}
}

// a simple function to write strings & ints to a file without allocating any memory.
// This function is for use in situations (such as during an abort) when we want to log
// some things to the file but can't safely malloc. if the msg string contains a %
// then this code assumes that it will be followed by a number indicating an index into
// the args array which will be printed as an integer at that point in the output.
// optionally, there can be an s, x or X between the % and the integer. s indicates
// that args[n] is a null-terminated pointer to a string.  x and X print args[n] as
// hex. X prints leading zeros and x does not.
static int
#ifdef _WIN64
safe_async_simple_fwrite_fd(int fd, char const *msg, ULONG_PTR *args, unsigned int num_args)
#else
safe_async_simple_fwrite_fd(int fd,char const *msg,unsigned long *args,unsigned int num_args)
#endif
{
	unsigned int arg_index;
	unsigned int digit,arg;
	char intbuf[50];
	char *intbuf_pos;

	int r = 0;
	for(;*msg;msg++) {
		if( *msg != '%' ) {
			r = write(fd,msg,1);
		}
		else {
			bool is_hex = msg[1] == 'x'; if (is_hex) ++msg; // hex without leading zeros
			bool is_HEX = msg[1] == 'X'; if (is_HEX) ++msg; // hex with leading zeros
			bool is_str = msg[1] == 's'; if (is_str) ++msg;
				// format is % followed by index of argument in args array
			arg_index = *(++msg)-'0';
			if( arg_index >= num_args || !*msg ) {
				r = write(fd," INVALID! ",10);
				break;
			}
			if (is_str) {
				char * psz = (char*)(args[arg_index]);
				unsigned int cb = 0; while (psz[cb]) ++cb;
				r = write(fd,psz,cb);
			} else {
				arg = args[arg_index];
				intbuf_pos=intbuf;
				if (is_hex || is_HEX) {
					for (int ii = 0; ii < (int)sizeof(arg)*2; ++ii) {
						digit = arg % 16;
						*(intbuf_pos++) = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');
						arg /= 16;
						if (!arg && is_hex) break;
					};
				} else {
					do {
						digit = arg % 10;
						*(intbuf_pos++) = digit + '0';
						arg /= 10;  // integer division, shifts base-10 digits right
					} while( arg ); // terminate when no more non-zero digits
				}

					// intbuf now contains the base-10 digits of arg
					// in order of least to most significant
				while( intbuf_pos-- > intbuf ) {
					r = write(fd,intbuf_pos,1);
				}
			}
		}
	}
	return r;
}

/* In case we want to write to the log in a signal handler (particularly
 * for a segfault), we need to be as simple as possible. Calling
 * malloc() could be fatal, since the heap may be trashed. Therefore,
 * we dispense with some of the formalities.
 */
static int
safe_async_log_open()
{
	int fd;

	if (DprintfBroken || !_condor_dprintf_works || DebugLogs->empty()) {
			// Note that although this would appear to enable
			// backtrace printing to stderr before dprintf is
			// configured, the backtrace sighandler is only installed
			// when dprintf is configured, so we won't even get here
			// in that case.  Therefore, most command-line tools need
			// -debug to enable the backtrace.
		fd = 2;
	}
	else {
		bool create_log = true;
#if !defined(WIN32)
			// set_priv() is unsafe, because it may call into
			// the password cache, which may call unsafe functions
			// such as getpwuid() or initgroups() or malloc().
		uid_t orig_euid = geteuid();
		gid_t orig_egid = getegid();
		priv_state orig_priv_state = get_priv_state();
		bool did_seteuid = false;
		if( orig_priv_state != PRIV_CONDOR ) {
			uid_t condor_uid = 0;
			gid_t condor_gid = 0;
			if( get_condor_uid_if_inited(condor_uid,condor_gid) ) {
				// The if statements are to quiet a compiler warning.
				if(setegid(condor_gid)) {}
				if(seteuid(condor_uid)) {}
				did_seteuid = true;
			}
			else if( orig_euid != getuid() || orig_egid != getgid() ) {
				// To keep things simple, we do not bother trying to
				// find out the correct condor uid if it is not
				// already known.  Just use our real user id, which is
				// probably either the same as our effective id
				// (no-op) or root.

				// The if statements are to quiet a compiler warning.
				if(setegid(getgid())) {}
				if(seteuid(getuid())) {}
				did_seteuid = true;
					// Do not open with O_CREAT in this case, so
					// we don't leave behind a file owned by root,
					// which could cause the daemon to fail to
					// restart.  This means we will fail to log
					// the backtrace if we get here and the log
					// file does not already exist.
				create_log = false;
			}
		}
#endif

		fd = safe_open_wrapper_follow(DebugLogs->begin()->logPath.c_str(),O_APPEND|O_WRONLY|(create_log ? O_CREAT : 0),0644);

#if !defined(WIN32)
		if( did_seteuid ) {
			// The if statements are to quiet a compiler warning.
			if(setegid(orig_egid)) {}
			if(seteuid(orig_euid)) {}
		}
#endif

		if( fd==-1 ) {
			fd=2;
		}
	}
	return fd;
}

static void
safe_async_log_close(int fd)
{
	if ( fd != 2 ) {
		close( fd );
	}
}

/* This function allows code outside of the dprintf() system to write
 * to the primary daemon log from a signal handler. It avoids any calls
 * that are not async-safe.
 * See safe_async_simple_fwrite_fd() for argument usage.
 */
#ifdef _WIN64
void dprintf_async_safe(char const *msg, ULONG_PTR *args, unsigned int num_args)
#else
void dprintf_async_safe(char const *msg,unsigned long *args,unsigned int num_args)
#endif
{
	// Use the async-safe logging operations.
	int fd = safe_async_log_open();

	safe_async_simple_fwrite_fd( fd, msg, args, num_args );

	safe_async_log_close( fd );
}


#ifdef WIN32
static int 
lock_or_mutex_file(int fd, LOCK_TYPE type, int do_block)
{
	int result = -1;

		// If we're trying to lock NUL, just return success early
	if (strcasecmp(DebugLock, "NUL") == 0) {
		return 0;
	}

	if ( ! DebugLockIsMutex) {
			// use a filesystem lock
		return lock_file_plain(fd,type,do_block);
	}
	
		// If we made it here, we want to use a kernel mutex.
		//
		// We use a kernel mutex by default to fix a major shortcoming
		// with using Win32 file locking: file locking on Win32 is
		// non-deterministic.  Thus, we have observed processes
		// starving to get the lock.  The Win32 mutex object,
		// on the other hand, is FIFO --- thus starvation is avoided.


		// first, open a handle to the mutex if we haven't already
	if ( debug_win32_mutex == NULL && DebugLock ) {
		char mutex_name[MAX_PATH];

		// start the mutex name with Global\ so that it works properly on systems running Terminal Services
		strcpy(mutex_name, "Global\\");
		size_t ix = strlen(mutex_name);

		// Create the mutex name based upon the lock file
		// specified in the config file.
		const char * ptr = DebugLock;

		// Note: Win32 will not allow backslashes in the name,
		// so get convert them to / as we copy. Also
		// The mutex name is case-sensitive, but the NTFS filesystem
		// is not.  So to avoid user confusion, we lowercase it
		while (*ptr) {
			char ch = *ptr++;
			if (ch == '\\') ch = '/';
			else if (isupper(ch)) ch = _tolower(ch);
			mutex_name[ix++] = ch;
			if (ix+1 >= COUNTOF(mutex_name))
				break;
		}
		mutex_name[ix] = 0;
			// Call CreateMutex - this will create the mutex if it does
			// not exist, or just open it if it already does.  Note that
			// the handle to the mutex is automatically closed by the
			// operating system when the process exits, and the mutex
			// object is automatically destroyed when there are no more
			// handles... go win32 kernel!  Thus, although we are not
			// explicitly closing any handles, nothing is being leaked.
			// Note: someday, to make BoundsChecker happy, we should
			// add a dprintf subsystem shutdown routine to nicely
			// deallocate this stuff instead of relying on the OS.
		debug_win32_mutex = CreateMutex(0,FALSE,mutex_name);
	}

		// now, if we have mutex, grab it or release it as needed
	if ( debug_win32_mutex ) {
		if ( type == UN_LOCK ) {
				// release mutex
			ReleaseMutex(debug_win32_mutex);
			result = 0;	// 0 means success
		} else {
				// grab mutex
				// block 10 secs if do_block is false, else block forever
			result = WaitForSingleObject(debug_win32_mutex, 
				do_block ? INFINITE : 10 * 1000);	// time in milliseconds
				// consider WAIT_ABANDONED as success so we do not EXCEPT
			if ( result==WAIT_OBJECT_0 || result==WAIT_ABANDONED ) {
				result = 0;
			} else {
				result = -1;
			}
		}

	}

	return result;
}

#ifdef HAVE_BACKTRACE

// a WIN32 implementation of the linux function backtrace_symbols_fd.
// this function  needs DbgHelp.dll to show symbols, but can do module name and offset without it.
// WinXP sp3 or later is needed to collect the backtrace.
static void backtrace_symbols_fd(void* trace[], int cFrames, int fd)
{
	ULONG_PTR args[3];
	char szModule[MAX_PATH];
	for (int ix = 0; ix < cFrames; ++ix) {
#if 0 //def _WIN64
		PRAGMA_REMIND("write win64 backtrace printing.")
		args[0] = (ULONG_PTR)trace[ix];
		//void* imageBase;
		//RtlPcToFileHeader(trace[ix], &imageBase);
		void* imageBase = 0;
		RtlPcToFileHeader(trace[ix], &imageBase);
		args[1] = (ULONG_PTR)imageBase;
		args[2] = args[0] - (ULONG_PTR)imageBase;
		safe_async_simple_fwrite_fd(fd,"  %X0 %x1 + %2\n",args,3);
#else // 32bit -- tj/2017 - maybe works for x64 also?
		args[0] = (ULONG_PTR)trace[ix];
	#ifdef _DBGHELP_
		if (backtrace_have_symbols) {
			DWORD64 displacement = 0;
			SYMBOL_INFO * psym = (SYMBOL_INFO*)szModule;
			psym->SizeOfStruct = sizeof(SYMBOL_INFO);
			psym->MaxNameLen = 1 + (sizeof(szModule) - sizeof(SYMBOL_INFO)) / sizeof(psym->Name[0]);
			if (SymFromAddr(GetCurrentProcess(), (DWORD64)(ULONG_PTR)trace[ix], &displacement, psym)) {
				args[1] = (ULONG_PTR)psym->Name;
				args[2] = args[0] - (ULONG_PTR)psym->Address;
				safe_async_simple_fwrite_fd(fd,"  %X0 %s1 + %2\n",args,3);
				continue;
			}
		}
	#endif // _DGBHELP_
		MEMORY_BASIC_INFORMATION mbi;
		SIZE_T cb = VirtualQuery (trace[ix], &mbi, sizeof(mbi));
		if (cb == sizeof(mbi) && mbi.AllocationBase != nullptr) {
			if (GetModuleFileNameA ((HMODULE)mbi.AllocationBase, szModule, COUNTOF(szModule))) {
				args[1] = (ULONG_PTR)szModule;
				// print only the part after the last path separator.
				args[1] = (ULONG_PTR)filename_from_path(szModule);
				args[2] = args[0] - (ULONG_PTR)mbi.AllocationBase;
				safe_async_simple_fwrite_fd(fd,"  %X0 %s1 + %2\n",args,3);
			} else {
				args[1] = (ULONG_PTR)mbi.AllocationBase;
				args[2] = args[0] - (ULONG_PTR)mbi.AllocationBase;
				safe_async_simple_fwrite_fd(fd,"  %X0 0x%x1 + %2\n",args,3);
			}
		} else {
			safe_async_simple_fwrite_fd(fd,"  %X0\n",args,1);
		}
#endif // X86_64
	}
}

void
dprintf_dump_stack(void) {
	int fd;
	ULONG_PTR args[3];
	void* trace[50];
	int cFrames = CaptureStackBackTrace(0, COUNTOF(trace), trace, NULL);

	// We're probably in a signal handler, so use the async-safe logging
	// operations.
	fd = safe_async_log_open();

		// sprintf() and other convenient string-handling functions
		// are not officially async-signal safe, so use a crude replacement
	args[0] = (ULONG_PTR)GetCurrentProcessId();
	args[1] = (ULONG_PTR)time(NULL);
	args[2] = (ULONG_PTR)cFrames;
	safe_async_simple_fwrite_fd(fd,"Stack dump for process %0 at timestamp %1 (%2 frames)\n",args,3);

	backtrace_symbols_fd(trace,cFrames,fd);

	safe_async_log_close( fd );
}
#else // !HAVE_BACKTRACE
void
dprintf_dump_stack(void) {
	// this version of Windows does not support backtrace.
}
#endif // HAVE_BACKTRACE

#else // !WIN32
// LockFd and DebugRotateLog are values that a clone child will modify,
// but we must ensure the values in the parent process are preserved
// after the child exec()s or exits.
static int ParentLockFd = -1;
static bool ParentDebugRotateLog = true;

void
dprintf_before_shared_mem_clone() {
	ParentLockFd = LockFd;
	ParentDebugRotateLog = DebugRotateLog;
}

void
dprintf_after_shared_mem_clone() {
	LockFd = ParentLockFd;
	DebugRotateLog = ParentDebugRotateLog;
}

void
dprintf_init_fork_child( bool cloned ) {
	if( LockFd >= 0 ) {
		close( LockFd );
		LockFd = -1;
	}
	// Avoid opening or closing files while in a cloned child process,
	// since we're sharing the parent's memory but have our own copy of
	// of the file descriptors. The LockFd is an exception, for which
	// we have extra handling code.
	// For forked child processes, don't do log rotation and reopen the
	// log file on every dprintf(). That avoids a race between parent
	// and child that can result in the parent writing to a rotated log
	// file.
	DebugRotateLog = false;
	if ( !cloned ) {
		log_keep_open = false;
		std::vector<DebugFileInfo>::iterator it;
		for ( it = DebugLogs->begin(); it < DebugLogs->end(); it++ ) {
			if ( it->outputTarget == FILE_OUT ) {
				debug_unlock_it(&(*it));
			}
		}
	}
}

void
dprintf_wrapup_fork_child( bool /* cloned */ ) {
		/* Child pledges not to call dprintf any more, so it is
		   safe to close the lock file.  If parent closes all
		   fds anyway, then this is redundant.
		*/
	if( LockFd >= 0 ) {
		close( LockFd );
		LockFd = -1;
	}
	// We don't need to restore the original values of DebugRotateLog or
	// log_keep_open here. In a forked child, the memory isn't shared
	// with the parent. For a cloned child, log_keep_open wasn't changed
	// and the parent will restore DebugRotateLog immediately after the
	// child exec()s or exits.
}

#if HAVE_BACKTRACE

void
dprintf_dump_stack(void) {
	int fd;
	void *trace[50];
	int trace_size;
	unsigned long args[3];

	// We're probably in a signal handler, so use the async-safe logging
	// operations.
	fd = safe_async_log_open();

	trace_size = backtrace(trace,50);

		// sprintf() and other convenient string-handling functions
		// are not officially async-signal safe, so use a crude replacement
	args[0] = (unsigned long)getpid();
	args[1] = (unsigned long)time(NULL);
	args[2] = (unsigned long)trace_size;
	safe_async_simple_fwrite_fd(fd,"Stack dump for process %0 at timestamp %1 (%2 frames)\n",args,3);

	backtrace_symbols_fd(trace,trace_size,fd);

	safe_async_log_close(fd);
}

#else // ! HAVE_BACKTRACE

void
dprintf_dump_stack(void) {
		// this platform does not support backtrace()
}
#endif

#endif

// Allow explicit disabling of log rotation
bool dprintf_allow_log_rotation(bool allow_rotate)
{
	bool prev_val = DebugRotateLog;
	DebugRotateLog = allow_rotate;
	return prev_val;
}

// If outputs haven't been configured yet, stop buffering dprintf()
// output until they are configured.
void dprintf_pause_buffering()
{
	_condor_dprintf_works = 1;
	if ( DebugLogs == NULL ) {
		DebugLogs = new std::vector<DebugFileInfo>;
	}
}

/* open any logs in the given directory as whatever priv is currently set
*  caller is responsible for init_user_ids() and set_user_priv() if user_priv is desired
*  or set_condor_priv() is condor_priv is desired
*/
int dprintf_open_logs_in_directory(const char * dir, bool fTruncate /*=false*/)
{
	if ( ! DebugLogs) return 0;
	int opened = 0;
	auto_free_ptr realdir(realpath(dir, nullptr));
	if (!realdir) {
		dprintf(D_ERROR, "Failed to find real pathname of %s\n", dir);
		return 0;
	}
	const char* flags = fTruncate ? "wN" : "aN";
	for (auto & it : *DebugLogs) {
		// TODO: do a better job with directory matching here?
		if (it.outputTarget == FILE_OUT && ! it.debugFP && starts_with(it.logPath, realdir.ptr())) {
			it.debugFP = safe_fopen_wrapper_follow(it.logPath.c_str(), flags, 0644);
			if (it.debugFP) {
				// TODO: flush buffered messages into the newly opened log
				++opened;
			} else {
				dprintf(D_ALWAYS, "Failed to open log %s\n", it.logPath.c_str());
			}
		}
	}
	return opened;
}

/* close any dprintf logs in the given directory
*  a non-permanent close is basically a flush unless the log needs to be opened as user priv
*  if close is permanent, 
*/
int dprintf_close_logs_in_directory(const char * dir, bool permanent /*=true*/)
{
	if ( ! DebugLogs) return 0;
	int closed = 0;
	auto_free_ptr realdir(realpath(dir, nullptr));
	dprintf(D_FULLDEBUG, "closing logs in %s real=%s\n", dir, realdir.ptr());
	for (auto & it : *DebugLogs) {
		// TODO: do a better job with directory matching here?
		if (it.outputTarget == FILE_OUT && it.debugFP && starts_with(it.logPath, realdir.ptr())) {
			if (permanent) {
				dprintf(D_ALWAYS, "Closing/Ending log %s\n", it.logPath.c_str());
			} else {
				dprintf(D_FULLDEBUG, "Flushing/Closing log %s\n", it.logPath.c_str());
			}
			fflush(it.debugFP);
			if (permanent) {
				// not using debug_close_file because we don't want to abort on failure here...
				// debug_close_file(&it);
				fclose_wrapper(it.debugFP, FCLOSE_RETRY_MAX);
				it.debugFP = nullptr;
				it.outputTarget = OUTPUT_DEBUG_STR; // to prevent attempts to reopen
				it.dprintfFunc = _dprintf_to_nowhere;
			}
			++closed;
		}
	}
	return closed;
}

bool debug_open_fds(std::map<int,bool> &open_fds)
{
	bool found = false;
	std::vector<DebugFileInfo>::iterator it;

	for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
	{
		if(!it->debugFP)
			continue;
		open_fds.insert(std::pair<int,bool>(fileno(it->debugFP),true));
		found = true;
	}

	return found;
}


void _dprintf_to_buffer(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo)
{
	void * pvUser = dbgInfo->userData;
	if (pvUser) {
		std::string * pstr = (std::string *)pvUser;
		const char* header = _format_global_header(cat_and_flags, hdr_flags, info);
		if (header) { pstr->append(header); }
		pstr->append(message);
	}
}

// for use when an dprintf output is muted during shutdown or error
void _dprintf_to_nowhere(int /*cat_and_flags*/, int /*hdr_flags*/, DebugHeaderInfo & /*info*/, const char* /*message*/, DebugFileInfo* /*dbgInfo*/)
{
}

#ifdef WIN32
void dprintf_to_outdbgstr(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo)
{
	_format_global_header(cat_and_flags,hdr_flags,info);
	OutputDebugStringA(message);
}
#endif

dprintf_on_function_exit::dprintf_on_function_exit(bool on_entry, int _flags, const char * fmt, ...)
	: msg("\n"), flags(_flags), print_on_exit(true)
{
	va_list args;
	va_start(args, fmt);
	vformatstr(msg, fmt, args);
	va_end(args);
	if (on_entry) dprintf(flags,      "entering %s", msg.c_str());
}
dprintf_on_function_exit::~dprintf_on_function_exit()
{
	if (print_on_exit) dprintf(flags, "leaving  %s", msg.c_str());
}

// get pointers to the two dprintf entry points, because we can't refer to their addresses any other way.
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*DPRINTF_C_FN)(int, const char *, ...);
static const DPRINTF_C_FN dprintf_c_addr = &dprintf;
#ifdef __cplusplus
}
#endif
typedef void (*DPRINTF_CPP_FN)(int, const char *, ...);
static const DPRINTF_CPP_FN dprintf_cpp_addr = &dprintf;

/*
 * this should be the last function in the dprintf module so that it can see all of the statics
 */
#ifdef HAVE_BACKTRACE
static bool is_dprintf_function_addr(const void * pfn)
{
	static const struct {
		const char * pfn;
		unsigned long length;
	} afns[] = {
		{ (const char*)&_condor_dprintf_getbacktrace, 0x200 },
		{ (const char*)&_condor_dprintf_va, 0x800 },
		{ (const char*)&_condor_dfprintf, 0x100 },
		{ (const char*)dprintf_c_addr, 0x100 },
		{ (const char*)dprintf_cpp_addr, 0x100 },
	};

	#define PTRDIFF(p1,p2) (unsigned long)((const char*)p1 - (const char *)p2)

	for (int ii = 0; ii < (int)COUNTOF(afns); ++ii) {
		if (pfn >= afns[ii].pfn && PTRDIFF(pfn, afns[ii].pfn) < afns[ii].length) {
			return true;
		}
	}
	return false;
}
#endif

