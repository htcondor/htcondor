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


 
/* 
   Shared functions that are used both for the real dprintf(), used by
   our daemons, and the mirco-version used by the Condor code we link
   with the user job in the libcondorsyscall.a or libcondorckpt.a. 
*/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_uid.h"



/* 
   This should default to 0 so we only get dprintf() messages if we
   actually request them somewhere, either in dprintf_config(), or the
   equivalent inside the user job.
*/
unsigned int      DebugHeaderOptions = 0;
DebugOutputChoice AnyDebugBasicListener = 0;
DebugOutputChoice AnyDebugVerboseListener = 0;


/*
   This is a global flag that tells us if we've successfully ran
   dprintf_config() or otherwise setup dprintf() to print where we
   want it to go.  This is used by EXCEPT() to know if it can safely
   call dprintf(), or if it should just use printf(), instead.
*/
int		_condor_dprintf_works = 0;

extern const char * const _condor_DebugCategoryNames[D_CATEGORY_COUNT];
const char * const _condor_DebugCategoryNames[D_CATEGORY_COUNT] = {
	"D_ALWAYS", "D_ERROR", "D_STATUS", "D_ZKM",
	"D_JOB", "D_MACHINE", "D_CONFIG", "D_PROTOCOL",
	"D_PRIV", "D_DAEMONCORE", "D_FULLDEBUG", "D_SECURITY",
	"D_COMMAND", "D_MATCH", "D_NETWORK", "D_KEYBOARD",
	"D_PROCFAMILY", "D_IDLE", "D_THREADS", "D_ACCOUNTANT",
	"D_SYSCALLS", "D_CRON", "D_HOSTNAME", "D_PERF_TRACE", // D_CRON formerly D_CKPT
	"D_LOAD", "D_25", "D_26", "D_AUDIT", "D_TEST",      // D_25 formerly D_PROC (HAD)  D_26 formerly D_NFS
	"D_STATS", "D_MATERIALIZE", "D_31",
};
// these are flags rather than categories
// "D_EXPR", "D_FULLDEBUG", "D_PID", "D_FDS", "D_CAT", "D_NOHEADER",

/*
   The real dprintf(), called by both the user job and all the daemons
   and tools.  To actually log the message, we call
   _condor_dprintf_va(), which is implemented differently for the user
   job and everything else.  If dprintf() has been configured (with
   dprintf_config() or it's equivalent in the user job), this will
   show up where we want it.  If not, we'll just drop the message in
   the bit bucket.  Someday, when we clean up dprintf() more
   effectively, we'll want to call _condor_sprintf_va() (defined
   below) if dprintf hasn't been configured so we'll still see
   the messages going to stderr, instead of being dropped.
*/
void
dprintf(int flags, DPF_IDENT ident, const char* fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, ident, fmt, args );
    va_end( args );
}

void
dprintf(int flags, const char* fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, (DPF_IDENT)0, fmt, args );
    va_end( args );
}

/*
 * See the comments in CMakeLists.txt about the purpose of this function.
 * This fixes issues with dynamically loading libcondor_utils when libc's
 * dprintf resolves first.
 */
extern "C" {
void __wrap_dprintf(int flags, const char * fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, (DPF_IDENT)0, fmt, args );
    va_end( args );
}
}

/* parse debug flags, but don't set any of the debug global variables.
 *
 * Note: we don't default basic output D_ALWAYS here, it's up to the caller to do so if needed.
 *       we also don't clear basic & verbose here.  once again its up to the caller to initialize
 *
 * If this were C++ code, we could use StringList instead of strtok().
 * We don't use strtok_r() because it's not available on Windows.
 */
void
_condor_parse_merge_debug_flags(
	const char *strflags,
	int         flags,          // in: inital flags and/or D_FULLDEBUG
	unsigned int & HeaderOpts,  // in,out: formatting options modified by flags & strflags (not initialized!)
	DebugOutputChoice & basic,  // in,out: basic verbosity categories modified flags & strflags (not initialized!)
	DebugOutputChoice & verbose)// in,out: verbose verbosity categoris modified by flags & strflags (not initialized!)
{
	//unsigned int flag_verbosity, bit=0, hdr;
	const unsigned int all_category_bits = ((unsigned int)1 << (D_CATEGORY_COUNT-1)) | (((unsigned int)1 << (D_CATEGORY_COUNT-1))-1);

	// this flag is set when strflags or cat_and_flags has D_FULLDEBUG
	// 
	bool fulldebug = (flags & D_FULLDEBUG) != 0;

	// this flag is set when D_FLAG:n syntax is used, 
	// when true, D_FULLDEBUG is treated strictly as a category and 
	// not as a verbosity modifier of other flags.
	// TJ 2022 - disabled secondary meaning of D_FULLDEBUG as a global verbosity modifier
	bool individual_verbosity = true;

	HeaderOpts |= (flags & ~(D_CATEGORY_RESERVED_MASK | D_FULLDEBUG | D_VERBOSE_MASK));

	if (strflags) {
		char * tmp = strdup( strflags );
		if ( tmp == NULL ) {
			return;
		}

		char * flag = strtok( tmp, "|, " );

		while ( flag != NULL ) {
			// in case the flag is NOT followed by ":<int>", default the verbosity number to 1
			// unless there is a - prefix on the flag, then default verbosity to 0
			// thus "-D_COMMAND" and "D_COMMAND:0" mean the same thing. while
			// "-D_COMMAND:2" means the same thing as "D_COMMAND:2".
			unsigned int flag_verbosity = 1;
			if( *flag == '-' ) {
				flag += 1;
				flag_verbosity = 0;
			} else if (*flag == '+') {
				flag += 1;
				flag_verbosity = 1;
			}

			char * colon = strchr(flag, ':');
			if (colon) {
				colon[0] = 0; // null terminate at the ':' so we can use strcasecmp on the flag name.
				individual_verbosity = true;
				if (colon[1] >= '0' && colon[1] <= '9') {
					flag_verbosity = (int)(colon[1] - '0');
				}
			}

			unsigned int bit = 0, hdr = 0;
			if( strcasecmp(flag, "D_ALL") == 0 ) {
				hdr = D_PID | D_FDS | D_CAT;
				bit = all_category_bits;
			} else if( strcasecmp(flag, "D_ANY") == 0 ) {
				bit = all_category_bits;
			} else if( strcasecmp(flag, "D_PID") == 0 ) {
				hdr = D_PID;
			} else if( strcasecmp(flag, "D_FDS") == 0 ) {
				hdr = D_FDS;
			} else if( strcasecmp(flag, "D_IDENT") == 0 ) {
				hdr = D_IDENT;
			} else if( (strcasecmp(flag, "D_LEVEL") == 0) || (strcasecmp(flag, "D_CATEGORY") == 0) || (strcasecmp(flag, "D_CAT") == 0) ) {
				hdr = D_CAT;
			} else if( strcasecmp(flag, "D_SUB_SECOND") == 0 ) {
				hdr = D_SUB_SECOND;
			} else if( strcasecmp(flag, "D_TIMESTAMP") == 0 ) {
				hdr = D_TIMESTAMP;
			} else if( strcasecmp(flag, "D_BACKTRACE") == 0 ) {
				hdr = D_BACKTRACE;
			} else if( strcasecmp(flag, "D_FULLDEBUG") == 0 ) {
				fulldebug = (flag_verbosity > 0);
				flag_verbosity *= 2; // so D_FULLDEBUG:1 ends up as D_ALWAYS:2
				bit = (1<<D_ALWAYS);
			} else if( strcasecmp(flag, "D_FAILURE") == 0 ) {
				// D_FAILURE is a special case because old code used it with 2 meanings, sometimes
				// to indicate messages that should be D_ERROR.  but also in some code to indicate
				// messages that should show up in a FAILURE (exception) log. if D_FAILURE were a category rather than a flag, then
				// when OR'd with D_ALWAYS, it would equal D_FAILURE and thus NOT output in D_ALWAYS logs.
				// So treat we split it to D_ERROR_ALSO and D_EXCEPTfrom the dprintf side, and as a synonym for the D_ERROR
				// category from the condor_config side.
				hdr = D_EXCEPT;
				bit = (1<<D_ERROR);
			} else {
				for(unsigned int i = 0; i < COUNTOF(_condor_DebugCategoryNames); i++ ) {
					if( strcasecmp(flag, _condor_DebugCategoryNames[i]) == 0 ) {
						bit = (1 << i);
						break;
					}
				}
			}

			if (flag_verbosity) {
				HeaderOpts |= hdr;
				basic |= bit;
				if (flag_verbosity > 1)
					verbose |= bit;
			} else {
				HeaderOpts &= ~hdr;
				verbose &= ~bit;
			}

			flag = strtok( NULL, "|, " );
		}

		free( tmp );
	}

	if ( ! individual_verbosity) {
		verbose |= (fulldebug) ? basic : 0;
	} else if (verbose & (1<<D_ALWAYS)) {
		// special case so that D_ALWAYS:2 means the same thing as D_FULLDEBUG all by itself.
		basic |= (1<<D_GENERIC_VERBOSE);
	}
}

bool parse_debug_cat_and_verbosity(const char * strFlags, int & cat_and_verb, unsigned int * hdr_flags)
{
	if ( ! strFlags || ! strFlags[0])
		return false;

	cat_and_verb = 0;
	unsigned int hdr = 0;
	DebugOutputChoice basic=0, verbose=0;
	_condor_parse_merge_debug_flags(strFlags, 0, hdr, basic, verbose);
	// bits that are set in the verbose mask are always set in basic also
	// so we only need to look at the basic mask to know if any bits were set
	// we will return the category code of the *lowest* set bit
	if (basic) {
		for (unsigned int cat = D_ALWAYS; cat < D_CATEGORY_COUNT; ++cat) {
			if (basic & (1<<cat)) {
				if (hdr_flags) *hdr_flags = hdr;
				cat_and_verb = cat;
				if (verbose & (1<<cat)) cat_and_verb |= D_VERBOSE;
				return true;
			}
		}
	}

	return false;
}


// convert old style flags to DebugOutputChoice
//
void
_condor_set_debug_flags_ex(
	const char *strflags,
	int cat_and_flags,
	unsigned int & header,
	DebugOutputChoice & choice,
	DebugOutputChoice & verbose)
{
	// special case. if a single category to be passed in cat_and_flags
	// in practice, this category is always D_ALWAYS which is set above anyway.
	choice |= 1<<(cat_and_flags & D_CATEGORY_MASK);
	//PRAGMA_REMIND("TJ: fix this to handle more than just basic & verbose levels")
	if (cat_and_flags & (D_FULLDEBUG | D_VERBOSE_MASK)) { verbose |= choice; }

	// parse and merge strflags and cat_and_flags into header & choice
	_condor_parse_merge_debug_flags(strflags, (cat_and_flags & ~D_CATEGORY_RESERVED_MASK), header, choice, verbose);
}

// set global debug global flags and header options
//
void
_condor_set_debug_flags( const char *strflags, int cat_and_flags )
{
	// set default values for flags and header options before we parse the passed in args
	unsigned int      header = 0;
	DebugOutputChoice choice = (1<<D_ALWAYS) | (1<<D_ERROR) | (1<<D_STATUS);
	DebugOutputChoice verbose = 0;
	_condor_set_debug_flags_ex(strflags, cat_and_flags, header, choice, verbose);
	DebugHeaderOptions = header;
	AnyDebugBasicListener = choice;
	AnyDebugVerboseListener = verbose;
}


#if 0
/*
   Until we know the difference between D_ALWAYS and D_ERROR, we don't
   really want to do this stuff below, since there are lots of
   D_ALWAYS messages we really don't want to see in the tools.  For
   now, all we really care about is the dprintf() from EXCEPT(), which
   we handle in except.c, anyway.
*/

/* 
   This method is called by dprintf() if we haven't already configured
   dprintf() to tell it where and what to log.  It this prints all
   debug messages we care about to the given fp, usually stderr.  In
   addition, there's no date/time header printed in this case (it's
   equivalent to always having D_NOHEADER defined), to avoid clutter. 
   Derek Wright <wright@cs.wisc.edu>
*/
void
_condor_sprintf_va( int flags, FILE* fp, char* fmt, va_list args )
{
		/* 
		   For now, only log D_ALWAYS if we're dumping to stderr.
		   Once we have ToolCore and can easily set the debug flags for
		   all command-line tools, and *everything* is just using
		   dprintf() again, we should compare against DebugFlags.
		   Derek Wright <wright@cs.wisc.edu>
		*/
    if( ! (flags & D_ALWAYS) ) {
        return;
    }
	vfprintf( fp, fmt, args );
}
#endif /* 0 */

