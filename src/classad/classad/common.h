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



#ifndef __CLASSAD_COMMON_H__
#define __CLASSAD_COMMON_H__

#include <numeric>
#ifndef WIN32
#include <strings.h>
#endif

#ifdef WIN32
	// These must be defined before any of the
	// other headers are pulled in.
#define _STLP_NEW_PLATFORM_SDK
#define _STLP_NO_OWN_IOSTREAMS 1

// Disable warnings about calling posix functions like open()
// instead of _open()
#define _CRT_NONSTDC_NO_WARNINGS

// Disable warnings about possible loss of data, since "we know what
// we are doing" and fixing them correctly would require too much 
// time from one of us. (Maybe this should be a student exercise.)
#pragma warning( disable : 4244 )

#endif /* WIN32 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#ifndef WIN32
	#include <unistd.h>
	#define DLL_IMPORT_MAGIC  /* a no-op on Unix */
#endif
#include <errno.h>
#include <ctype.h>




#ifdef WIN32

	// special definitions we need for Windows
#ifndef DLL_IMPORT_MAGIC
#define DLL_IMPORT_MAGIC __declspec(dllimport)
#endif

// must be defined before windows.h
#define NOMINMAX
struct IUnknown;

#include <windows.h>
#include <float.h>
#include <io.h>
#define fsync _commit
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

// anotations that help the MSVC code analyzer
#define PREFAST_NORETURN __declspec(noreturn)
#define SAL_Ret_notnull _Ret_notnull_
#define SAL_assume(expr) __analysis_assume(expr);

#define snprintf _snprintf

	// Disable warnings about multiple template instantiations
	// (done for gcc)
#pragma warning( disable : 4660 )  
	// Disable warnings about forcing bools
#pragma warning( disable : 4800 )  
	// Disable warnings about truncated debug identifiers
#pragma warning( disable : 4786 )
	// Disable warnings about str* needing to be _str*
#pragma warning( disable : 4996 )

#else
#define PREFAST_NORETURN
#define SAL_Ret_notnull
#define SAL_assume(expr)
#endif // WIN32

// add methods for binary persist/de-persist (i.e. Pickle, Make and Scan)
//
#define TJ_PICKLE
//#define TJ_PICKLE_VALUES

#include "classad/debug.h"

#ifdef __cplusplus
#include <string>
#endif
#include <cstring>
#include <string.h>

namespace classad {

extern const char * const ATTR_AD;
extern const char * const ATTR_CONTEXT;
extern const char * const ATTR_DEEP_MODS;
extern const char * const ATTR_DELETE_AD;
extern const char * const ATTR_DELETES;
extern const char * const ATTR_KEY;
extern const char * const ATTR_NEW_AD;
extern const char * const ATTR_OP_TYPE;
extern const char * const ATTR_PARENT_VIEW_NAME;
extern const char * const ATTR_PARTITION_EXPRS;
extern const char * const ATTR_PARTITIONED_VIEWS;
extern const char * const ATTR_PROJECT_THROUGH;
extern const char * const ATTR_RANK_HINTS;
extern const char * const ATTR_REPLACE;
extern const char * const ATTR_SUBORDINATE_VIEWS;
extern const char * const ATTR_UPDATES;
extern const char * const ATTR_WANT_LIST;
extern const char * const ATTR_WANT_PRELUDE;
extern const char * const ATTR_WANT_RESULTS;
extern const char * const ATTR_WANT_POSTLUDE;
extern const char * const ATTR_VIEW_INFO;
extern const char * const ATTR_VIEW_NAME;
extern const char * const ATTR_XACTION_NAME;

#define ATTR_REQUIREMENTS  "Requirements"
#define ATTR_RANK  "Rank"

struct CaseIgnLTStr {
	typedef void is_transparent; // magic to enable transparent comparators

	bool operator()(const std::string &s1, const std::string &s2 ) const {
		return( strcasecmp(s1.c_str(), s2.c_str()) < 0 );
	}

	bool operator()(const char *s1, const char *s2 ) const {
		return( strcasecmp(s1,s2) < 0 );
	}

	bool operator()(const char *s1, const std::string &s2 ) const {
		return( strcasecmp(s1,s2.c_str()) < 0 );
	}

	bool operator()(const std::string &s1, const char *s2 ) const {
		return( strcasecmp(s1.c_str(),s2) < 0 );
	}
};

struct CaseIgnSizeLTStr {
	typedef void is_transparent; // magic to enable transparent comparators

	bool operator( )( const std::string &s1, const std::string &s2 ) const {
		size_t s1len = s1.length();
		size_t s2len = s2.length();
		if (s1len == s2len) {
			return( strcasecmp( s1.c_str( ), s2.c_str( ) ) < 0 );
		} else {
			return s1len < s2len;
		}
	}

	bool operator()(const char *s1, const char *s2) const {
		size_t s1len = strlen(s1);
		size_t s2len = strlen(s2);
		if (s1len == s2len) {
			return( strcasecmp( s1, s2) < 0 );
		} else {
			return s1len < s2len;
		}
	}

	bool operator()(const char *s1, const std::string &s2) const {
		size_t s1len = strlen(s1);
		size_t s2len = s2.length();
		if (s1len == s2len) {
			return( strcasecmp( s1, s2.c_str()) < 0 );
		} else {
			return s1len < s2len;
		}
	}

	bool operator()(const std::string &s1, const char *s2) const {
		size_t s1len = s1.length();
		size_t s2len = strlen(s2);
		if (s1len == s2len) {
			return( strcasecmp( s1.c_str(), s2) < 0 );
		} else {
			return s1len < s2len;
		}
	}
};

struct CaseIgnEqStr {
	typedef void is_transparent; // magic to enable transparent comparators

	bool operator()(const std::string &s1, const std::string &s2 ) const {
		return( strcasecmp(s1.c_str(), s2.c_str()) == 0 );
	}

	bool operator()(const char *s1, const char *s2) const {
		return( strcasecmp(s1, s2) == 0 );
	}
	bool operator()(const char *s1, const std::string &s2) const {
		return( strcasecmp(s1, s2.c_str()) == 0 );
	}
	bool operator()(const std::string &s1, const char *s2) const {
		return( strcasecmp(s1.c_str(), s2) == 0 );
	}
};

class ExprTree;
struct ExprHash {
	inline size_t operator()( const ExprTree *const &x ) const {
		return( (size_t)x );
	}
};

struct ClassadAttrNameHash
{
	typedef void is_transparent; // magic to enable transparent comparators

	size_t operator()( const std::string &s ) const {
		unsigned char const *ch = (unsigned const char*)s.c_str();
		size_t h = 0;
		for( ; *ch; ch++ ) {
			h = 5*h + (*ch | 0x20);
		}
		return h;
	}

	constexpr size_t operator()(const char *s) const {
		size_t h = 0;
		for( ; *s; s++ ) {
			h = 5*h + (((unsigned char )*s) | 0x20);
		}
		return h;
	}

};
extern std::string       CondorErrMsg;

extern int 		CondorErrno;


} // classad

#include "classad/classadErrno.h"

#endif//__CLASSAD_COMMON_H__
