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
 ***************************************************************/

#ifndef __dprintf_internal_h_
#define __dprintf_internal_h_

// This #define doesn't actually do anything. This value needs to be
// defined before any system header files are included in the source file
// to have any effect.
#include <string>
#include <map>
#if _MSC_VER && (_MSC_VER < 1600)
typedef _Longlong int64_t;
#else
#include <stdint.h>
#endif
#include <ctime>
#if defined(WIN32)
# include <winsock2.h>
#else
# include <sys/time.h>
#endif

struct DebugFileInfo;
typedef struct DebugHeaderInfo {
   struct timeval tv;
   struct tm * tm;
   DPF_IDENT  ident; // caller supplied identity, used by D_AUDIT
   int        backtrace_id;  // per-process unique identifier for this backtrace
   int        num_backtrace; // number of valid entries in the backtrace array
   void **    backtrace;     // if non null, pointer to an array of void* pointers containing the backtrace (set when D_BACKTRACE is specified)
}  DebugHeaderInfo;

typedef void (*DprintfFuncPtr)(int, int, DebugHeaderInfo &, const char*, DebugFileInfo*);

enum DebugOutput
{
	FILE_OUT,
	STD_OUT,
	STD_ERR,
	OUTPUT_DEBUG_STR,
	SYSLOG
};

/* future
class DebugOutputChoice
{
public:
	unsigned int flags; // one or more of D_xxx flags (but NOT category) values
	unsigned char level[D_CATEGORY_COUNT]; // indexed by D_CATEGORY enum
	DebugOutputChoice(unsigned int val);
	DebugOutputChoice::DebugOutputChoice(unsigned int val)
	{
		flags = val & ~D_CATEGORY_RESERVED_MASK;
		memset(level, 0, sizeof(level));
		unsigned int catflags = val & D_CATEGORY_MASK;
		for (int ix = 0; catflags && ix < sizeof(level); ++ix, catflags/=2)
			level[ix] += (catflags&1);
	}
};
*/
struct dprintf_output_settings;

struct DebugFileInfo
{
	DebugOutput outputTarget;
	DebugOutputChoice choice;
	DebugOutputChoice verbose;
	unsigned int headerOpts;

	FILE *debugFP;
	DprintfFuncPtr dprintfFunc;
	void *userData;
	std::string logPath;

	long long maxLog;
	long long logZero;
	int maxLogNum;
	bool want_truncate;
	bool accepts_all;
	bool rotate_by_time; // when true, logMax is a time interval for rotation
	bool dont_panic;

	DebugFileInfo()
		: outputTarget(FILE_OUT), choice(0), verbose(0), headerOpts(0)
		, debugFP(nullptr), dprintfFunc(nullptr), userData(nullptr)
		, maxLog(0), logZero(0), maxLogNum(0)
		, want_truncate(false), accepts_all(false), rotate_by_time(false), dont_panic(false)
		{}
	DebugFileInfo(const DebugFileInfo &dfi)
		: outputTarget(dfi.outputTarget), choice(dfi.choice), verbose(dfi.verbose), headerOpts(dfi.headerOpts)
		, debugFP(nullptr), dprintfFunc(dfi.dprintfFunc), userData(dfi.userData), logPath(dfi.logPath)
		, maxLog(dfi.maxLog), logZero(dfi.logZero), maxLogNum(dfi.maxLogNum)
		, want_truncate(dfi.want_truncate), accepts_all(dfi.accepts_all), rotate_by_time(dfi.rotate_by_time), dont_panic(dfi.dont_panic)
		{}
	DebugFileInfo(const dprintf_output_settings&);
	~DebugFileInfo();
	bool MatchesCatAndFlags(int cat_and_flags) const;
};

struct dprintf_output_settings
{
	DebugOutputChoice choice;
	std::string logPath;
	long long logMax;  // max size/duration of a single log file
	int maxLogNum;
	bool optional_file;
	bool want_truncate;
	bool accepts_all;
	bool rotate_by_time; // when true, logMax is a time interval for rotation
	unsigned int HeaderOpts;
	unsigned int VerboseCats; // temporary, should get folded into choice

	dprintf_output_settings()
		: choice(0), logMax(0), maxLogNum(0)
		, optional_file(false), want_truncate(false), accepts_all(false), rotate_by_time(false)
		, HeaderOpts(0), VerboseCats(0) {}
};

void dprintf_set_outputs(const struct dprintf_output_settings *p_info, int c_info);

void * dprintf_get_onerror_data();

const char* _format_global_header(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info);
//Global dprint functions meant as fallbacks.
void _dprintf_global_func(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo);
void _dprintf_to_buffer(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo);
void _dprintf_to_nowhere(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo);

#ifdef WIN32
//Output to dbg string
void dprintf_to_outdbgstr(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo);
#endif

#endif

