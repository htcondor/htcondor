/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __COMMON_H__
#define __COMMON_H__

#if defined( WANT_NAMESPACES ) && defined(__cplusplus)
#define BEGIN_NAMESPACE( x ) namespace x {
#define END_NAMESPACE }
#else
#define BEGIN_NAMESPACE( x )
#define END_NAMESPACE
#endif

#include "classad_features.h"

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
static const char ATTR_RANK					[]	= "Rank";
static const char ATTR_RANK_HINTS			[] 	= "RankHints";
static const char ATTR_REPLACE				[] 	= "Replace";
static const char ATTR_REQUIREMENTS			[]	= "Requirements";
static const char ATTR_SUBORDINATE_VIEWS	[]	= "SubordinateViews";
static const char ATTR_UPDATES				[] 	= "Updates";
static const char ATTR_WANT_LIST			[]	= "WantList";
static const char ATTR_WANT_PRELUDE			[]	= "WantPrelude";
static const char ATTR_WANT_RESULTS			[]	= "WantResults";
static const char ATTR_WANT_POSTLUDE		[]	= "WantPostlude";
static const char ATTR_VIEW_INFO			[]	= "ViewInfo";
static const char ATTR_VIEW_NAME			[]	= "ViewName";
static const char ATTR_XACTION_NAME			[]	= "XactionName";

#if defined(__cplusplus)
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

#include "classadErrno.h"

#endif//__COMMON_H__
