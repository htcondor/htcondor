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
**	Set up the various dprintf variables based on the configuration file.
**
************************************************************************/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_string.h" 
#include "condor_sys_types.h"
#include "dprintf_internal.h"

#if HAVE_BACKTRACE
#include "sig_install.h"
#endif

int		Termlog = 0;

extern int		DebugFlags;
extern FILE		*DebugFPs[D_NUMLEVELS+1];
extern std::vector<DebugFileInfo> *DebugLogs;
extern char		*DebugLock;
extern const char		*_condor_DebugFlagNames[];
extern int		_condor_dprintf_works;
extern time_t	DebugLastMod;
extern int		DebugUseTimestamps;
extern int      DebugContinueOnOpenFailure;
extern int		log_keep_open;

extern void		_condor_set_debug_flags( const char *strflags );
extern void		_condor_dprintf_saved_lines( void );
extern bool debug_check_it(struct DebugFileInfo& it, bool fTruncate, bool dont_panic);

param_functions *dprintf_param_funcs = NULL;

#if HAVE_EXT_GCB
void	_condor_gcb_dprintf_va( int flags, char* fmt, va_list args );
extern "C" void Generic_set_log_va(void(*app_log_va)(int level, char *fmt, va_list args));
#endif

#if HAVE_BACKTRACE
static void
sig_backtrace_handler(int signum)
{
	dprintf_dump_stack();

		// terminate for the same reason.
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(signum, &sa, NULL);
	sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

	raise(signum);
}

static void
install_backtrace_handler(void)
{
	sigset_t fullset;
	sigfillset( &fullset );
	install_sig_handler_with_mask(SIGSEGV, &fullset, sig_backtrace_handler);
	install_sig_handler_with_mask(SIGABRT, &fullset, sig_backtrace_handler);
	install_sig_handler_with_mask(SIGILL, &fullset, sig_backtrace_handler);
	install_sig_handler_with_mask(SIGFPE, &fullset, sig_backtrace_handler);
	install_sig_handler_with_mask(SIGBUS, &fullset, sig_backtrace_handler);
}
#endif

int
dprintf_config_ContinueOnFailure ( int fContinue )
{
    int fOld = DebugContinueOnOpenFailure;
	DebugContinueOnOpenFailure = fContinue;
	return fOld;
}

void
dprintf_config( const char *subsys, param_functions *p_funcs )
{
	char pname[ BUFSIZ ];
	char *pval;
	static int first_time = 1;
	int want_truncate;
	int debug_level;
	int log_open_default = TRUE;
	std::vector<DebugFileInfo> *debugLogsOld = DebugLogs;
	bool debug_zero = false;	//This indicates whether debug level zero has been initialized.
	

	DebugLogs = new std::vector<DebugFileInfo>();
	if(!dprintf_param_funcs)
		dprintf_param_funcs = new param_functions();

	/*
	 * The duplication of the param_function instance is to ensure no one else can change
	 * the data structure out from under dprintf.  It is also to prevent transfer of ownership/
	 * responsibility for the block of memory used to store the function pointers.
	 */
	if(p_funcs)
	{
		dprintf_param_funcs->set_param_func(p_funcs->get_param_func());
		dprintf_param_funcs->set_param_bool_int_func(p_funcs->get_param_bool_int_func());
		dprintf_param_funcs->set_param_wo_default_func(p_funcs->get_param_wo_default_func());
	}

	/*  
	**  We want to initialize this here so if we reconfig and the
	**	debug flags have changed, we actually use the new
	**  flags.  -Derek Wright 12/8/97 
	*/
	DebugFlags = D_ALWAYS;

	/*
	** First, add the debug flags that are shared by everyone.
	*/
	pval = dprintf_param_funcs->param("ALL_DEBUG");
	if( pval ) {
		_condor_set_debug_flags( pval );
		free( pval );
	}

	/*
	**  Then, pick up the subsys_DEBUG parameters.   Note: if we don't have
	**  anything set, we just leave it as D_ALWAYS.
	*/
	(void)sprintf(pname, "%s_DEBUG", subsys);
	pval = dprintf_param_funcs->param(pname);
	if( pval ) {
		_condor_set_debug_flags( pval );
		free( pval );
	}

#ifdef WIN32
		/* Two reasons why we need to lock the log in Windows
		 * (when a lock is configured)
		 * 1) File rotation requires exclusive access in Windows.
		 * 2) O_APPEND doesn't guarantee atomic writes in Windows
		 */
	DebugShouldLockToAppend = 1;
#else
	DebugShouldLockToAppend = dprintf_param_funcs->param_boolean_int("LOCK_DEBUG_LOG_TO_APPEND",0);
#endif

	/*
	**	If this is not going to the terminal, pick up the name
	**	of the log file, maximum log size, and the name of the
	**	lock file (if it is specified).
	*/
	if( !( Termlog) ) {
		std::vector<DebugFileInfo>::iterator it;	//iterator indicating the file we got to.
		for (debug_level = 0; debug_level <= D_NUMLEVELS; debug_level++) {
			std::string logPath;
			want_truncate = 0;
			if (debug_level == 0) {
				/*
				** the level 0 file gets all debug messages; thus, the
				** offset into DebugFlagNames is off by one, since the
				** first level-specific file goes into the other arrays at
				** index 1
				*/
				(void)sprintf(pname, "%s_LOG", subsys);
			} else {
				(void)sprintf(pname, "%s_%s_LOG", subsys,
							  _condor_DebugFlagNames[debug_level-1]+2);
			}

			// Hold a temporary copy of the old file pointer until
			// *after* the param -- param can dprintf() in some cases
			{
				// NEGOTIATOR_MATCH_LOG is necessary by default, but debug_level
				// is not 0
				char *logPathParam = NULL;
				bool file_found = false;
				if(debug_level == 0)
				{
					logPathParam = param(pname);

					// No default value found, so use $(LOG)/$(SUBSYSTEM)Log
					if(!logPathParam) {
						char *log = param("LOG");
						char *lsubsys = param("SUBSYSTEM");
						if(!log || !lsubsys) {
							EXCEPT("Unable to find LOG or SUBSYSTEM.\n");
						}
						
						sprintf(logPath, "%s%c%sLog", log, DIR_DELIM_CHAR, lsubsys);

						free(log);
						free(lsubsys);
					}
					else {
						logPath.insert(0, logPathParam);
					}
				}
				else {
					// This is looking up configuration options that I can't
					// find documentation for, so instead of coding in an
					// incorrect default value, I'm gonna use 
					// param_without_default.
					// tristan 5/29/09
					logPathParam = param_without_default(pname);
					if(logPathParam)
						logPath.insert(0, logPathParam);
				}

				if(!logPath.empty())
				{
					if(!DebugLogs->empty())
					{
						for(it = DebugLogs->begin(); it < DebugLogs->end(); it++)
						{
							if(it->logPath != logPath)
								continue;
							it->debugFlags |= debug_level;
							file_found = true;
							break;
						}
					}
					if(!file_found)
					{
						DebugFileInfo logFileInfo;
						logFileInfo.debugFlags = debug_level;
						logFileInfo.logPath = logPath;
						it = DebugLogs->insert(DebugLogs->end(), logFileInfo);
					}
				}

				if(logPathParam)
				{
					free(logPathParam);
					logPathParam = NULL;
				}
			}

			debug_zero = true;

			if( debug_level == 0 && !debug_zero ) {
				EXCEPT("No '%s' parameter specified.", pname);
			} else if ( !logPath.empty() ) {

				if (debug_level == 0 && first_time) {
					struct stat stat_buf;
					if ( stat( logPath.c_str(), &stat_buf ) >= 0 ) {
						DebugLastMod = stat_buf.st_mtime > stat_buf.st_ctime ? stat_buf.st_mtime : stat_buf.st_ctime;
					} else {
						DebugLastMod = -errno;
					}
				}

				if (debug_level == 0) {
					(void)sprintf(pname, "TRUNC_%s_LOG_ON_OPEN", subsys);
				} else {
					(void)sprintf(pname, "TRUNC_%s_%s_LOG_ON_OPEN", subsys,
								  _condor_DebugFlagNames[debug_level-1]+2);
				}
				want_truncate = param_boolean_crufty(pname, false) ? 1 : 0;

				if (debug_level == 0) {
					(void)sprintf(pname, "%s_LOCK", subsys);
					if (DebugLock) {
						free(DebugLock);
					}
					DebugLock = param(pname);
				}

				// check to see if we can open the log file.
				bool dont_panic = true;
				bool fOk = debug_check_it(*it, (first_time && want_truncate), dont_panic);
				if( ! fOk && debug_level == 0 ) {
			       #ifdef WIN32
					/*
					** If we could not open the log file, we might want to keep running anyway.
					** If we do, then set the log filename to NUL so we don't keep trying
					** (and failing) to open the file.
					*/
					if (DebugContinueOnOpenFailure) {
			
						// change the debug file to point to the NUL device.
						it->logPath.insert(0, NULL_FILE);

					} else
			       #endif
					{
					    EXCEPT("Cannot open log file '%s'", logPath.c_str());
					}
				}

				if (debug_level == 0) {
					(void)sprintf(pname, "MAX_%s_LOG", subsys);
				} else {
					(void)sprintf(pname, "MAX_%s_%s_LOG", subsys,
								  _condor_DebugFlagNames[debug_level-1]+2);
				}
                
                off_t maxlog = 0;
				pval = param(pname);
				if (pval != NULL) {
                    // because there is nothing like param_long_long() or param_off_t()
                    bool r = lex_cast(pval, maxlog);
                    if (!r || (maxlog < 0)) {
                        EXCEPT("Invalid config: %s = %s", pname, pval);
                    }
                    it->maxLog = maxlog;
					free(pval);
				} else {
					it->maxLog = 1024*1024;
				}
				
				if (debug_level == 0) {
					(void)sprintf(pname, "MAX_NUM_%s_LOG", subsys);
				} else {
					(void)sprintf(pname, "MAX_NUM_%s_%s_LOG", subsys,
								  _condor_DebugFlagNames[debug_level-1]+2);
				}

				pval = param(pname);
				if (pval != NULL) {
					it->maxLogNum = param_integer(pname, 1, 0);
					free(pval);
				} else {
					it->maxLogNum = 1;
				}
			}
		}
	} else {

#if !defined(WIN32)
		setlinebuf( stderr );
#endif

		(void)fflush( stderr );	/* Don't know why we need this, but if not here
							   the first couple dprintf don't come out right */
	}

#ifndef WIN32
	if((strcmp(subsys, "SHADOW") == 0) || (strcmp(subsys, "GRIDMANAGER") == 0))
	{
		log_open_default = FALSE;
	}
#endif

	if(!DebugLock) {
		sprintf(pname, "%s_LOG_KEEP_OPEN", subsys);
		log_keep_open = dprintf_param_funcs->param_boolean_int(pname, log_open_default);
	}

	first_time = 0;
	_condor_dprintf_works = 1;
#if HAVE_EXT_GCB
		/*
		  this method currently only lives in libGCB.a, so don't even
		  try to param() or call this function unless we're on a
		  platform where we're using the GCB external
		*/
    if ( dprintf_param_funcs->param_boolean_int("NET_REMAP_ENABLE", 0) ) {
        Generic_set_log_va(_condor_gcb_dprintf_va);
    }
#endif

		/*
		  If LOGS_USE_TIMESTAMP is enabled, we will print out Unix timestamps
		  instead of the standard date format in all the log messages
		*/
	DebugUseTimestamps = dprintf_param_funcs->param_boolean_int( "LOGS_USE_TIMESTAMP", FALSE );

#if HAVE_BACKTRACE
	install_backtrace_handler();
#endif

	if(debugLogsOld)
	{
		delete debugLogsOld;
	}

	_condor_dprintf_saved_lines();
}


#if HAVE_EXT_GCB
void
_condor_gcb_dprintf_va( int flags, char* fmt, va_list args )
{
	char* new_fmt;
	int len;

	len = strlen(fmt);
	new_fmt = (char*) malloc( (len + 6) * sizeof(char) );
	if( ! new_fmt ) {
		EXCEPT( "_condor_gcb_dprintf_va() out of memory!" );
	}
	snprintf( new_fmt, len + 6, "GCB: %s", fmt );
	_condor_dprintf_va( flags, new_fmt, args );
	free( new_fmt );
}
#endif /* HAVE_EXT_GCB */
