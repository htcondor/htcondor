/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __COMMON_H__
#define __COMMON_H__

#if defined( WANT_NAMESPACES ) && defined(__cplusplus)
#define BEGIN_NAMESPACE( x ) namespace x {
#define END_NAMESPACE }
#else
#define BEGIN_NAMESPACE( x )
#define END_NAMESPACE
#endif

#ifdef CLASSAD_DISTRIBUTION
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* to get definition for strptime on Linux */
#endif

#ifndef __EXTENSIONS__
#define __EXTENSIONS__ /* to get gmtime_r and localtime_r on Solaris */
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199506L /* To get asctime_r */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "debug.h"
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#ifndef WORD_BIT
#define WORD_BIT 32
#endif

#else /* CLASSAD_DISTRIBUTION isn't defined */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199506L /* To get asctime_r */
#endif

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "classad_features.h"
#endif

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

#if defined( CLASSAD_DISTRIBUTION )
static const char ATTR_REQUIREMENTS			[]	= "Requirements";
static const char ATTR_RANK					[]	= "Rank";
#endif

#if defined(__cplusplus)
using namespace std;
#include <string>
struct CaseIgnLTStr {
    bool operator( )( const string &s1, const string &s2 ) const {
       return( strcasecmp( s1.c_str( ), s2.c_str( ) ) < 0 );
	}
};

struct CaseIgnEqStr {
	bool operator( )( const string &s1, const string &s2 ) const {
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
	size_t operator()( const string &s ) const {
		size_t h = 0;
		for( int i = s.size()-1; i >= 0; i-- ) {
			h = 5*h + s[i];
		}
		return( h );
	}
};

struct StringCaseIgnHash {
	size_t operator()( const string &s ) const {
		size_t h = 0;
		for( int i = s.size()-1; i >= 0; i-- ) {
			h = 5*h + tolower(s[i]);
		}
		return( h );
	}
};
#endif


END_NAMESPACE // classad

char* strnewp( const char* );

extern int 		CondorErrno;

#ifdef __cplusplus
#include <string>
extern string 	CondorErrMsg;
static const string NULL_XACTION = "";
#endif

#if defined(CLASSAD_DISTRIBUTION)
#include "classadErrno.h"
#else
#include "condor_errno.h"
#endif

#endif//__COMMON_H__
