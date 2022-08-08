/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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
 * Programmers:
 * Ziliang Guo (ziliang@cs.wisc.edu)
 * John Knoeller
 *
 ***************************************************************/
#include "condor_common.h"
#include "condor_sys_types.h"
#include "condor_debug.h"
#include "dprintf_internal.h"
#if !defined(WIN32)
#include "dprintf_syslog.h"
#endif

#include <sys/stat.h>
#include <vector>

extern std::vector<DebugFileInfo> *DebugLogs;
extern time_t	DebugLastMod;
extern int		DebugContinueOnOpenFailure;
extern int		_condor_dprintf_works;

extern bool		debug_check_it(struct DebugFileInfo& it, bool fTruncate, bool dont_panic);
extern void		_condor_dprintf_saved_lines( void );

void dprintf_set_outputs(const struct dprintf_output_settings *p_info, int c_info)
{
	static int first_time = 1;

	std::vector<DebugFileInfo> *debugLogsOld = DebugLogs;
	DebugLogs = new std::vector<DebugFileInfo>();

	/*
	**  We want to initialize this here so if we reconfig and the
	**	debug flags have changed, we actually use the new
	**  flags.  -Derek Wright 12/8/97
	*/
	AnyDebugBasicListener = (1<<D_ALWAYS) | (1<<D_ERROR) | (1<<D_STATUS);
	AnyDebugVerboseListener = 0;
	DebugHeaderOptions = 0;

	/*
	**	If this is not going to the terminal, pick up the name
	**	of the log file, maximum log size, and the name of the
	**	lock file (if it is specified).
	*/
	std::vector<DebugFileInfo>::iterator it;	//iterator indicating the file we got to.
	for (int ii = 0; ii < c_info; ++ii)
	{
		std::string logPath = p_info[ii].logPath;

		if(!logPath.empty())
		{
			// merge flags if we see the same log file name more than once.
			// we don't really expect this to happen, but things get weird of
			// it does happen and we don't check for it.
			//
			for(it = DebugLogs->begin(); it != DebugLogs->end(); ++it)
			{
				if(it->logPath != logPath)
					continue;
				it->choice |= p_info[ii].choice;
				break;
			}

			if(it == DebugLogs->end()) // We did not find the logPath in our DebugLogs
			{
				it = DebugLogs->insert(DebugLogs->end(),p_info[ii]);

				if(logPath == "1>")
				{
					it->outputTarget = STD_OUT;
					it->debugFP = stdout;
					it->dprintfFunc = _dprintf_global_func;
				}
				else if(logPath == "2>")
				{
					it->outputTarget = STD_ERR;
					it->debugFP = stderr;
					it->dprintfFunc = _dprintf_global_func;
				}
#ifdef WIN32
				else if(logPath == "OUTDBGSTR")
				{
					it->outputTarget = OUTPUT_DEBUG_STR;
					it->dprintfFunc = dprintf_to_outdbgstr;
				}
#else
				else if (logPath == "SYSLOG")
				{
					// Intention is to eventually user-selected
					it->dprintfFunc = DprintfSyslog::Log;
					it->outputTarget = SYSLOG;
					it->userData = static_cast<void*>(DprintfSyslogFactory::NewLog(LOG_DAEMON));
				}
#endif
				else if (logPath == ">BUFFER")
				{
					it->outputTarget = OUTPUT_DEBUG_STR;
					it->dprintfFunc = _dprintf_to_buffer;
					it->userData = dprintf_get_onerror_data();
				}
				else
				{
					it->outputTarget = FILE_OUT;
					it->dprintfFunc = _dprintf_global_func;
				}
				/*
				This seems like a catch all default that we did not want.
				else
				{
					it->outputTarget = ((ii == 0) && Termlog) ? STD_OUT : FILE_OUT;
					it->dprintfFunc = _dprintf_global_func;
				}
				*/
				it->logPath = logPath;
			}

			if (ii == 0) {
				if(first_time && it->outputTarget == FILE_OUT) {
					struct stat stat_buf;
					if ( stat( logPath.c_str(), &stat_buf ) >= 0 ) {
						DebugLastMod = stat_buf.st_mtime > stat_buf.st_ctime ? stat_buf.st_mtime : stat_buf.st_ctime;
					} else {
						DebugLastMod = -errno;
					}
				}
				//PRAGMA_REMIND("TJ: fix this when choice includes verbose.")
				AnyDebugBasicListener = p_info[ii].choice;
				AnyDebugVerboseListener = p_info[ii].VerboseCats;
				DebugHeaderOptions = p_info[ii].HeaderOpts;
			} else {
				AnyDebugBasicListener |= p_info[ii].choice;
				AnyDebugVerboseListener |= p_info[ii].VerboseCats;
			}

			// check to see if we can open the log file.
			if(it->outputTarget == FILE_OUT)
			{
				bool dont_panic = true;
				bool fOk = debug_check_it(*it, (first_time && it->want_truncate), dont_panic);
				if( ! fOk && ii == 0 )
				{
#ifdef WIN32
					/*
					** If we could not open the log file, we might want to keep running anyway.
					** If we do, then set the log filename to NUL so we don't keep trying
					** (and failing) to open the file.
					*/
					if (DebugContinueOnOpenFailure) 
					{
						// change the debug file to point to the NUL device.
						it->logPath.insert(0, NULL_FILE);
					} else
#endif
					{
						EXCEPT("Cannot open log file '%s'", logPath.c_str());
					}
				}
			}
		}
	}

	if ( ! p_info || ! c_info || p_info[0].logPath == "2>" || p_info[0].logPath == "CON:" || p_info[0].logPath == "\\dev\\tty")
	{
#if !defined(WIN32)
		setlinebuf( stderr );
#endif

		(void)fflush( stderr );	/* Don't know why we need this, but if not here
							   the first couple dprintf don't come out right */
	}

	first_time = 0;
	_condor_dprintf_works = 1;

	if(debugLogsOld)
	{
		
		for (it = debugLogsOld->begin(); it != debugLogsOld->end(); it++)
		{
			if ((it->outputTarget == SYSLOG) && (it->userData))
			{
#if !defined(WIN32)
				delete static_cast<DprintfSyslog*>(it->userData);
#endif
			}
		}
		delete debugLogsOld;
	}

	_condor_dprintf_saved_lines();
}

bool dprintf_to_term_check()
{
	if(DebugLogs && !DebugLogs->empty())
	{
		if((*DebugLogs)[0].outputTarget == STD_ERR)
			return true;
	}

	return false;
}

