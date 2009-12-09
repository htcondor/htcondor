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



#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef WIN32
#include <strings.h>
#endif

#if defined( WANT_CLASSAD_NAMESPACE ) && defined(__cplusplus)
#define BEGIN_NAMESPACE( x ) namespace x {
#define END_NAMESPACE }
#else
#define BEGIN_NAMESPACE( x )
#define END_NAMESPACE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* to get definition for strptime on Linux */
#endif

#ifndef __EXTENSIONS__
#define __EXTENSIONS__ /* to get gmtime_r and localtime_r on Solaris */
#endif

#ifdef WIN32
	// These must be defined before any of the
	// other headers are pulled in.
#define _STLP_NEW_PLATFORM_SDK
#define _STLP_NO_OWN_IOSTREAMS 1

// Disable warnings about possible loss of data, since "we know what
// we are doing" and fixing them correctly would require too much 
// time from one of us. (Maybe this should be a student exercise.)
#pragma warning( disable : 4244 )

#endif /* WIN32 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#ifndef WIN32
	#include <unistd.h>
	#define DLL_IMPORT_MAGIC  /* a no-op on Unix */
#endif
#include <errno.h>
#include <ctype.h>

#ifndef WORD_BIT
#define WORD_BIT 32
#endif



#ifdef WIN32
	// special definitions we need for Windows
#ifndef DLL_IMPORT_MAGIC
#define DLL_IMPORT_MAGIC __declspec(dllimport)
#endif
#include <windows.h>
#include <float.h>
#include <io.h>
#define fsync _commit
#ifndef open
#define open _open
#endif
#define strcasecmp _stricmp
#ifndef rint
#define rint(num) floor(num + .5)
#endif
#define isnan _isnan
	// isinf() defined in util.h



#define snprintf _snprintf

	// Disable warnings about multiple template instantiations
	// (done for gcc)
#pragma warning( disable : 4660 )  
	// Disable warnings about forcing bools
#pragma warning( disable : 4800 )  
	// Disable warnings about truncated debug identifiers
#pragma warning( disable : 4786 )
#endif // WIN32


#include "classad/debug.h"

#ifdef __cplusplus
#include <string>
#endif
#include <cstring>
#include <string.h>

BEGIN_NAMESPACE( classad )

static const char ATTR_AD					[]	= "Ad";
static const char ATTR_CONTEXT				[] 	= "Context";
static const char ATTR_DEEP_MODS			[] 	= "DeepMods";
static const char ATTR_DELETE_AD			[] 	= "DeleteAd";
static const char ATTR_DELETES				[] 	= "Deletes";
static const char ATTR_KEY					[]	= "Key";
static const char ATTR_NEW_AD				[]	= "NewAd";
static const char ATTR_OP_TYPE				[]	= "OpType";
static const char ATTR_PARENT_VIEW_NAME		[]	= "ParentViewName";
static const char ATTR_PARTITION_EXPRS 		[]  = "PartitionExprs";
static const char ATTR_PARTITIONED_VIEWS	[] 	= "PartitionedViews";
static const char ATTR_PROJECT_THROUGH		[]	= "ProjectThrough";
static const char ATTR_RANK_HINTS			[] 	= "RankHints";
static const char ATTR_REPLACE				[] 	= "Replace";
static const char ATTR_SUBORDINATE_VIEWS	[]	= "SubordinateViews";
static const char ATTR_UPDATES				[] 	= "Updates";
static const char ATTR_WANT_LIST			[]	= "WantList";
static const char ATTR_WANT_PRELUDE			[]	= "WantPrelude";
static const char ATTR_WANT_RESULTS			[]	= "WantResults";
static const char ATTR_WANT_POSTLUDE		[]	= "WantPostlude";
static const char ATTR_VIEW_INFO			[]	= "ViewInfo";
static const char ATTR_VIEW_NAME			[]	= "ViewName";
static const char ATTR_XACTION_NAME			[]	= "XactionName";

static const char ATTR_REQUIREMENTS			[]	= "Requirements";
static const char ATTR_RANK					[]	= "Rank";

#if defined(__cplusplus)
struct CaseIgnLTStr {
    bool operator( )( const std::string &s1, const std::string &s2 ) const {
       return( strcasecmp( s1.c_str( ), s2.c_str( ) ) < 0 );
	}
};

struct CaseIgnEqStr {
	bool operator( )( const std::string &s1, const std::string &s2 ) const {
		return( strcasecmp( s1.c_str( ), s2.c_str( ) ) == 0 );
	}
};

class ExprTree;
struct ExprHash {
	size_t operator()( const ExprTree *const &x ) const {
		return( (size_t)x );
	}
};

struct StringHash {
	size_t operator()( const std::string &s ) const {
		unsigned long h = 0;
		char const *ch;
		for( ch = s.c_str(); *ch; ch++ ) {
			h = 5*h + (unsigned char)*ch;
		}
		return (size_t)h;
	}
};

struct StringCaseIgnHash {
	size_t operator()( const std::string &s ) const {
		unsigned long h = 0;
		char const *ch;
		for( ch = s.c_str(); *ch; ch++ ) {
			h = 5*h + (unsigned char)tolower(*ch);
		}
		return (size_t)h;
	}
};
extern std::string       CondorErrMsg;
#endif

extern int 		CondorErrno;
static const std::string NULL_XACTION = "";

END_NAMESPACE // classad

char* strnewp( const char* );

#include "classad/classadErrno.h"

#endif//__COMMON_H__
