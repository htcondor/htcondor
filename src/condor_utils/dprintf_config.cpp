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
#include "condor_sys_types.h"
#include "condor_config.h"
#include "dprintf_internal.h"

#include "subsystem_info.h"

#include <string>
using std::string;

extern char		*DebugLock;
extern const char* const _condor_DebugCategoryNames[D_CATEGORY_COUNT];
extern int		DebugContinueOnOpenFailure;
extern bool		log_keep_open;
extern char*	DebugTimeFormat;
extern int		DebugLockIsMutex;
extern char*	DebugLogDir;
extern bool 	should_block_signals;

extern void		_condor_set_debug_flags( const char *strflags, int cat_and_flags );


int
dprintf_config_ContinueOnFailure ( int fContinue )
{
	int fOld = DebugContinueOnOpenFailure;
	DebugContinueOnOpenFailure = fContinue;
	return fOld;
}

// configure tool_on_error output from cat_and_flags, or if cat_and_flags is 0
// configure it from the TOOL_ON_ERROR_DEBUG parameter.
int
dprintf_config_tool_on_error(const char * flags)
{
	dprintf_output_settings tool_output[1];
	int cOutputs = 0;

	char * pval = nullptr; 
	if (flags) {
		pval = expand_param(flags);
	}
	if ( ! pval) {
		pval = param("TOOL_DEBUG_ON_ERROR");
	}
	{ // this brace is to avoid re-indenting the code below
		if (pval) {
			tool_output[cOutputs].logPath = ">BUFFER";
			tool_output[cOutputs].HeaderOpts = 0;
			tool_output[cOutputs].choice |= (1<<D_ALWAYS) | (1<<D_ERROR) | (1<<D_STATUS);
			tool_output[cOutputs].VerboseCats = 0;
			tool_output[cOutputs].accepts_all = true;
			_condor_parse_merge_debug_flags( pval, 0,
				tool_output[cOutputs].HeaderOpts,
				tool_output[cOutputs].choice,
				tool_output[cOutputs].VerboseCats);
			++cOutputs;
			free(pval);
		}
	}

	if (cOutputs > 0) {
		dprintf_set_outputs(tool_output, cOutputs);
	}
	return cOutputs;
}

int
dprintf_config_tool(const char* subsys, const char * flags, const char * logfile /*=NULL*/)
{
	char *pval = NULL;
	unsigned int HeaderOpts = 0;
	DebugOutputChoice verbose = 0;

	dprintf_output_settings tool_output[2];
	tool_output[0].choice = (1<<D_ALWAYS) | (1<<D_ERROR) | (1<<D_STATUS);
	tool_output[0].accepts_all = true;
	
	/*
	** First, add the debug flags that are shared by everyone.
	*/
	pval = param("ALL_DEBUG");//dprintf_param_funcs->param("ALL_DEBUG");
	if( pval ) {
		_condor_parse_merge_debug_flags( pval, 0, HeaderOpts, tool_output[0].choice, verbose);
		free( pval );
	}

	/*
	**  Then, add flags set by the subsys_DEBUG parameters
	*/
	if (flags) {
		pval = expand_param(flags);
	} else {
		std::string pname;
		formatstr(pname, "%s_DEBUG", subsys);
		pval = param(pname.c_str());
		if ( ! pval) {
			pval = param("DEFAULT_DEBUG");
		}
	}
	if (pval) {
		_condor_parse_merge_debug_flags(pval, 0, HeaderOpts, tool_output[0].choice, verbose);
		free(pval);
	}

	/*
	If LOGS_USE_TIMESTAMP is enabled, we will print out Unix timestamps
	instead of the standard date format in all the log messages
	*/
	bool UseTimestamps = param_boolean( "LOGS_USE_TIMESTAMP", false );
	if (UseTimestamps) HeaderOpts |= D_TIMESTAMP;
	char * time_format = param( "DEBUG_TIME_FORMAT" );
	if (time_format) {
		if(DebugTimeFormat)
			free(DebugTimeFormat);
		DebugTimeFormat = time_format;
		// Skip enclosing quotes
		if (*time_format == '"') {
			DebugTimeFormat = strdup(&time_format[1]);
			free(time_format);
			char * p = DebugTimeFormat;
			while (*p++) {
				if (*p == '"') *p = '\0';
			}
		}
	}

	tool_output[0].logPath = (logfile&&logfile[0]) ? logfile : "2>";
	tool_output[0].HeaderOpts = HeaderOpts;
	tool_output[0].VerboseCats = verbose;
	int cOutputs = 1;

	dprintf_set_outputs(tool_output, cOutputs);

	return 0;
}

bool dprintf_parse_log_size (
	const char * input,
	long long  & value,
	bool       & is_time)
{
	value = 0;
	const char * p = input;
	while(isspace(*p)) ++p;
		// all whitespace is failure
	if ( ! *p) return false;

		// fetch the number, and get a pointer to the first char after
	char *pend;
	value = strtoll(p, &pend, 10);

		// no number at this position is failure
	if (pend == p) return false;
	p = pend;

		// skip whitespace after number
	while (isspace(*p)) ++p;

		// check for unit after the number
	if (*p) {
		int ch = *p++;
		int ch2 = *p & ~0x20; if (ch2) ++p; // convert to uppercase if lowercase, symbols don't matter here.
		int ch3 = *p & ~0x20; if (ch3) ++p;
		while (isalpha(*p)) ++p;
		switch (toupper(ch)) {
			case 'S': is_time = true; break;
			case 'H': is_time = true; value *= 60*60; break;
			case 'D': is_time = true; value *= 60*60*24; break;
			case 'W': is_time = true; value *= 60*60*24*7; break;

			case 'B': is_time = false; break;
			case 'K': is_time = false; value *= 1024; break;
			case 'G': is_time = false; value *= 1024*1024*1024; break;
			case 'T': is_time = false; value *= (long long)1024*1024*1024*1024; break;

			// M can be Mb or min
			case 'M': {
				if (ch2) {
					if (ch2 == 'B') is_time = false;
					else if (ch2 == 'I') is_time = (ch3 != 'B');
					else return false;
				} else {
					if (ch == 'm') is_time = true;
				}
				if (is_time) value *= 60;
				else value *= 1024*1024;
			}
			break;

			// not a valid unit is failure
			// default: return false;
		}

		// skip whitespace after unit
		while (isspace(*p)) ++p;
	}

	// return success if we are at the end of the string.
	return !*p;
}

// make a log name from daemon name by Capitalizing each word
// and removing _.  so SHARED_PORT becomes SharedPort
static void make_log_name_from_daemon_name( std::string &str )
{
	if (str.length() == 0) return;
	bool upper = true;
	unsigned int j=0;
	for (unsigned int i=0; i < str.length(); ++i) {
		char ch = str[i];
		if (isspace(ch) || ch == '_') {
			upper = true;
		} else {
			if (ch >= 'a' && ch <= 'z' && upper) {
				ch = _toupper(ch);
			} else if (ch >= 'A' && ch <= 'Z' && ! upper) {
				ch = _tolower(ch);
			}
			str[j++] = ch;
			upper = false;
		}
	}
	str[j] = 0;
}


int
dprintf_config( const char *subsys, struct dprintf_output_settings *p_info /* = NULL*/, int c_info /*= 0*/, const char * log2arg /*=null*/)
{
	char pname[ BUFSIZ ];
	char *pval = NULL;
	//static int first_time = 1;
	bool log_open_default = true;
	long long def_max_log = 1024*1024*10;  // default to 10 Mb
	bool def_max_log_by_time = false;

	/*
	**  We want to initialize this here so if we reconfig and the
	**	debug flags have changed, we actually use the new
	**  flags.  -Derek Wright 12/8/97 
	*/
	unsigned int HeaderOpts = 0;
	DebugOutputChoice verbose = 0;
	//PRAGMA_REMIND("TJ: move verbose into choice")

	std::vector<struct dprintf_output_settings> DebugParams(1);
	DebugParams[0].choice = (1<<D_ALWAYS) | (1<<D_ERROR) | (1<<D_STATUS);
	DebugParams[0].accepts_all = true;

	/*
	** First, add the debug flags that are shared by everyone.
	*/
	pval = param("ALL_DEBUG");//dprintf_param_funcs->param("ALL_DEBUG");
	if( pval ) {
		_condor_parse_merge_debug_flags( pval, 0, HeaderOpts, DebugParams[0].choice, verbose);
		free( pval );
	}

	/*
	** Get the default value for a log file size
	*/
	pval = param("MAX_DEFAULT_LOG");
	if (pval) {
		long long maxlog = 0;
		bool unit_is_time = false;
		bool r = dprintf_parse_log_size(pval, maxlog, unit_is_time);
		if (!r || (maxlog < 0)) {
			std::string m;
			formatstr(m, "Invalid config %s = %s: %s must be an integer literal >= 0 and may be followed by a units value\n", pname, pval, pname);
			_condor_dprintf_exit(EINVAL, m.c_str());
		}
		if (unit_is_time) {
			// TJ: 8.1.5 the time rotation code currently doesn't work for logs that have multiple writers...
			_condor_dprintf_exit(EINVAL, "Invalid config. MAX_DEFAULT_LOG must be a size, not a time in this version of HTCondor.\n");
		}
		def_max_log = maxlog;
		def_max_log_by_time = unit_is_time;
		free(pval);
	}

	/*
	**  Then, add flags set by the subsys_DEBUG parameters
	*/
	(void)snprintf(pname, sizeof(pname), "%s_DEBUG", subsys);
	pval = param(pname);//dprintf_param_funcs->param(pname);
	if ( ! pval) {
		pval = param("DEFAULT_DEBUG");//dprintf_param_funcs->param("DEFAULT_DEBUG");
	}
	if( pval ) {
		_condor_parse_merge_debug_flags( pval, 0, HeaderOpts, DebugParams[0].choice, verbose);
		free( pval );
	}

	if(DebugLogDir)
		free(DebugLogDir);
	DebugLogDir = param( "LOG" );//dprintf_param_funcs->param( "LOG" );

	//PRAGMA_REMIND("TJ: dprintf_config should not set globals if p_info != NULL")
#ifdef WIN32
		/* Two reasons why we need to lock the log in Windows
		 * (when a lock is configured)
		 * 1) File rotation requires exclusive access in Windows.
		 * 2) O_APPEND doesn't guarantee atomic writes in Windows
		 */
	DebugShouldLockToAppend = 1;
	DebugLockIsMutex = (int)param_boolean("FILE_LOCK_VIA_MUTEX", true);//dprintf_param_funcs->param_boolean("FILE_LOCK_VIA_MUTEX", true);
#else
	DebugShouldLockToAppend = (int)param_boolean("LOCK_DEBUG_LOG_TO_APPEND",false);
	DebugLockIsMutex = FALSE;
#endif

	(void)snprintf(pname, sizeof(pname), "%s_LOCK", subsys);
	if (DebugLock) {
		free(DebugLock);
	}
	DebugLock = param(pname);//dprintf_param_funcs->param(pname);

#ifndef WIN32
	if((strcmp(subsys, "SHADOW") == 0) || (strcmp(subsys, "GRIDMANAGER") == 0))
	{
		log_open_default = false;
	}
#endif

	if(!DebugLock) {
		(void)snprintf(pname, sizeof(pname), "%s_LOG_KEEP_OPEN", subsys);
		log_keep_open = param_boolean(pname, log_open_default);//dprintf_param_funcs->param_boolean(pname, log_open_default);
	}

	should_block_signals = param_boolean("DPRINTF_BLOCK_SIGNALS", true);
	/*
	If LOGS_USE_TIMESTAMP is enabled, we will print out Unix timestamps
	instead of the standard date format in all the log messages
	*/
	bool UseTimestamps = param_boolean( "LOGS_USE_TIMESTAMP", false );//dprintf_param_funcs->param_boolean( "LOGS_USE_TIMESTAMP", false );
	if (UseTimestamps) HeaderOpts |= D_TIMESTAMP;
	char * time_format = param( "DEBUG_TIME_FORMAT" );//dprintf_param_funcs->param( "DEBUG_TIME_FORMAT" );
	if (time_format) {
		if(DebugTimeFormat)
			free(DebugTimeFormat);
		DebugTimeFormat = time_format;
		// Skip enclosing quotes
		if (*time_format == '"') {
			DebugTimeFormat = strdup(&time_format[1]);
			free(time_format);
			char * p = DebugTimeFormat;
			while (*p++) {
				if (*p == '"') *p = '\0';
			}
		}
	}

	/*
	 * Allow the configuration to override all logs to syslog.
	 */
	bool to_syslog = param_boolean("LOG_TO_SYSLOG", false);

	/*
	**	pick up the name of the log file, maximum log size, and the name of the
	**	lock file (if it is specified).
	*/
	for (int debug_level = 0; debug_level < (int)COUNTOF(_condor_DebugCategoryNames); ++debug_level) {

		std::string logPath;
		std::string subsys_and_level = subsys;
		int param_index = 0;

		if (debug_level != D_ALWAYS) {
			/*
			** the level 0 file gets all debug messages; thus, the
			** offset into DebugFlagNames is off by one, since the
			** first level-specific file goes into the other arrays at
			** index 1
			*/
			subsys_and_level += _condor_DebugCategoryNames[debug_level]+1;
			param_index = (int)DebugParams.size();
		}

		(void)snprintf(pname, sizeof(pname), "%s_LOG", subsys_and_level.c_str());

		char *logPathParam = NULL;
		if(debug_level == D_ALWAYS)
		{

			logPathParam = param(pname);

			// See ticket #5849: daemons started with a local name should
			// not by default share logs with other daemons of the same
			// subsystem.  If this daemon has a local name, then, we check
			// <LOCALNAME>.<SUBSYS>_LOG explicitly, to make sure we don't
			// "inherit" the value of <SUBSYS>_LOG.  Likewise, if the daemon
			// has a local name but no log path, we default to <Localname>Log
			// instead of <SUBSYS>Log (below).
			const char * localName = get_mySubSystem()->getLocalName();
			if( localName != NULL ) {
				std::string lln( localName );
				lln += "."; lln += pname;
				if( logPathParam ) { free( logPathParam ); }
				logPathParam = param( lln.c_str() );
			}

			if (to_syslog) {
				logPath = "SYSLOG";
			} else if (logPathParam) {
				logPath = logPathParam;
			} else {
				// No default value found, so use $(LOG)/$(SUBSYSTEM)Log or $(LOG)/$(LOCALNAME)Log
				std::string ln;
				if (localName) {
					ln = localName;
				} else {
					char * lsubsys = param("SUBSYSTEM");
					if (lsubsys) { ln = lsubsys; free(lsubsys); } else { ln = subsys; }
				}
				make_log_name_from_daemon_name( ln );
				formatstr(logPath, "%s%c%sLog", DebugLogDir, DIR_DELIM_CHAR, ln.c_str() );
			}

			DebugParams[0].accepts_all = true;
			DebugParams[0].want_truncate = false;
			DebugParams[0].rotate_by_time = false;
			DebugParams[0].logPath = logPath;
			DebugParams[0].logMax = def_max_log;
			DebugParams[0].rotate_by_time = def_max_log_by_time;
			DebugParams[0].maxLogNum = 1;
			DebugParams[0].HeaderOpts = HeaderOpts;
			DebugParams[0].VerboseCats = verbose;
		}
		else
		{
			// This is looking up configuration options that I can't
			// find documentation for, so instead of coding in an
			// incorrect default value, I'm gonna use
			// param_without_default.
			// tristan 5/29/09
			logPathParam = param(pname);
			if(logPathParam && to_syslog) {
				logPath = "SYSLOG";
			} else if(logPathParam) {
				logPath = logPathParam;
			}

			if(!DebugParams.empty())
			{
				for (int jj = 0; jj < (int)DebugParams.size(); ++jj)
				{
					if (DebugParams[jj].logPath == logPath)
					{
						DebugParams[jj].choice |= 1<<debug_level;
						param_index = jj;
						break;
					}
				}
			}

			if (param_index >= (int)DebugParams.size())
			{
				struct dprintf_output_settings info;
				info.accepts_all = false;
				info.want_truncate = false;
				info.rotate_by_time = false;
				info.choice = 1<<debug_level;
				//PRAGMA_REMIND("tj: remove this hack when you can config header of secondary log files.")
				if (debug_level == D_AUDIT) info.HeaderOpts |= D_IDENT;
				info.logPath = logPath;
				info.logMax = def_max_log;
				info.rotate_by_time = def_max_log_by_time;
				info.maxLogNum = 1;

				DebugParams.push_back(info);
				param_index = (int)DebugParams.size() -1;
			}
		}

		if(logPathParam)
		{
			free(logPathParam);
			logPathParam = NULL;
		}

		(void)snprintf(pname, sizeof(pname), "TRUNC_%s_LOG_ON_OPEN", subsys_and_level.c_str());
		DebugParams[param_index].want_truncate = param_boolean(pname, DebugParams[param_index].want_truncate) ? 1 : 0;//dprintf_param_funcs->param_boolean(pname, DebugParams[param_index].want_truncate) ? 1 : 0;

		//PRAGMA_REMIND("TJ: move initialization of DebugLock")
		if (debug_level == D_ALWAYS) {
			(void)snprintf(pname, sizeof(pname), "%s_LOCK", subsys);
			if (DebugLock) {
				free(DebugLock);
			}
			DebugLock = param(pname);
		}

		(void)snprintf(pname, sizeof(pname), "MAX_%s_LOG", subsys_and_level.c_str());
		pval = param(pname);
		if (pval != NULL) {
			long long maxlog = 0;
			bool unit_is_time = false;
			bool r = dprintf_parse_log_size(pval, maxlog, unit_is_time);
			if (!r || (maxlog < 0)) {
				std::string m;
				formatstr(m, "Invalid config %s = %s: %s must be an integer literal >= 0 and may be followed by a units value\n", pname, pval, pname);
				_condor_dprintf_exit(EINVAL, m.c_str());
			}
			DebugParams[param_index].logMax = maxlog;
			DebugParams[param_index].rotate_by_time = unit_is_time;
			free(pval);
		}

		(void)snprintf(pname, sizeof(pname), "MAX_NUM_%s_LOG", subsys_and_level.c_str());
		pval = param(pname);
		if (pval != NULL) {
			DebugParams[param_index].maxLogNum = param_integer(pname, 1, 0);
			free(pval);
		}
	}

	// if there is a log2arg add it as an additional log file at the end of the DebugParams array
	// the format of log2arg  is  [<flags>,]<logfile>
	// the entire log2arg value will be macro-expanded before it is used, so that a valid value can be
	// -a2 '$(LOG)/$(LOCALNAME).extra.log`
	// or 
	// -a2 '$(DEFAULT_DEBUG),$(STARTER_DEBUG),D_JOB,D_MACHINE,D_ZKM:2,/var/lib/condor/execute/slot1/dir_$(PID)/.starter.log'
	// where <flags> is optional, if present it will define what _DEBUG flags to use for that log file
	// 
	if (log2arg) {
		struct dprintf_output_settings info = DebugParams[0];
		auto_free_ptr arg(expand_param(log2arg));
		if (arg) {
			// if there are commas, the log filename is after the last one
			char * logarg = strrchr(arg.ptr(),',');
			if (logarg) {
				*logarg = 0; // turn the comma to a null
				logarg++; // use the part after the comma as the log filename
				// set flags from the passed-in debug flags
				info.HeaderOpts = HeaderOpts;
				info.choice = (1<<D_ALWAYS) | (1<<D_ERROR) | (1<<D_STATUS);
				info.VerboseCats = 0;
				_condor_parse_merge_debug_flags(arg.ptr(), 0, info.HeaderOpts, info.choice, info.VerboseCats);
			} else {
				logarg = arg.ptr();
			}
			info.logPath = logarg;
			trim(info.logPath);
			info.optional_file = true; // for the condor_starter, the directory of this file may not exist at first
			DebugParams.push_back(info);
		}
	}

	// if a p_info array was supplied, return the parsed params, but don't operate
	// on them
	// if it was not supplied, then use the params to configure dprintf outputs.
	//
	if (p_info)
	{
		for (int ii = 0; ii < c_info && ii < (int)DebugParams.size(); ++ii)
		{
			p_info[ii].accepts_all   = DebugParams[ii].accepts_all;
			p_info[ii].want_truncate = DebugParams[ii].want_truncate;
			p_info[ii].rotate_by_time = DebugParams[ii].rotate_by_time;
			p_info[ii].choice        = DebugParams[ii].choice;
			p_info[ii].logPath       = DebugParams[ii].logPath;
			p_info[ii].logMax        = DebugParams[ii].logMax;
			p_info[ii].maxLogNum     = DebugParams[ii].maxLogNum;
			p_info[ii].HeaderOpts    = DebugParams[ii].HeaderOpts;
			p_info[ii].VerboseCats   = DebugParams[ii].VerboseCats;
		}
		// return the NEEDED size of the p_info array, even if it is bigger than c_info
		return (int)DebugParams.size();
	}
	else
	{
		dprintf_set_outputs(&DebugParams[0], (int)DebugParams.size());
	}
	return 0;
}

