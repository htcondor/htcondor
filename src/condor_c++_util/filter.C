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

 

/*
  The purpose and usage of this code is documented in the header file
  "filter.h".
*/

#define _POSIX_SOURCE

/* Solaris specific change ..dhaval 6/27 */
#if defined(Solaris)
#include "_condor_fix_types.h"
#endif 

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"

#include "list.h"
#include "filter.h"

static const int STAR = -1;
static int FilterError;
static List<FilterObj> *GlobalList;

ProcFilter::ProcFilter()
{
	list = new List<FilterObj>;
}

ProcFilter::~ProcFilter()
{
	FilterObj	*f;

	list->Rewind();
	while( f = list->Next() ) {
		delete f;
	}
	delete list;
}

BOOLEAN
ProcFilter::AppendUser( const char *str )
{
	list->Append( new UserFilter(str) );
	if( FilterError ) {
		return FALSE;
	} else {
		return TRUE;
	}
}

BOOLEAN
ProcFilter::AppendId( const char *str )
{
	list->Append( new IdFilter(str) );
	if( FilterError ) {
		return FALSE;
	} else {
		return TRUE;
	}
}

void
ProcFilter::Display()
{
	FilterObj	*f;

	list->Rewind();
	while( f = list->Next() ) {
		f->display();
	}
}

void
ProcFilter::Install()
{
	GlobalList = list;
}

template <class Type>
static int
get_cluster( Type *p )
{
	return p->id.cluster;
}

template <class Type>
static int
get_proc( Type *p )
{
	return p->id.proc;
}

template <class Type>
static char *
get_owner( Type *p )
{
	return p->owner;
}

/*
  Given a pointer to a PROC, return true if the is one of the proc's the
  user has asked to see.
*/
BOOLEAN
ProcFilter::Func( PROC *p )
{
	FilterObj	*f;
	int		cluster;
	int		proc;
	char	*owner;


	if( GlobalList->IsEmpty() ) {
		return TRUE;
	}

	if( p->version_num == 2 ) {
		cluster = get_cluster( (V2_PROC *)p );
		proc = get_proc( (V2_PROC *)p );
		owner = get_owner( (V2_PROC *)p );
	} else {
		cluster = get_cluster( (V3_PROC *)p );
		proc = get_proc( (V3_PROC *)p );
		owner = get_owner( (V3_PROC *)p );
	}
		
	GlobalList->Rewind();
	while( f = GlobalList->Next() ) {
		if( f->Wanted( cluster, proc ) ) {
			return TRUE;
		}
		if( f->Wanted( owner ) ) {
			return TRUE;
		}
	}
	return FALSE;
}

IdFilter::IdFilter( const char *str )
{
	char	*p;

	if( p=(char *)strchr((const char *)str,'.') ) {
		cluster = atoi( str );
		proc = atoi( ++p );
	} else {
		cluster = atoi( str );
		proc = STAR;
	}

	if( cluster < 1 || proc < STAR )
		FilterError = TRUE;
}

BOOLEAN
IdFilter::Wanted( int c, int p )
{
	if( cluster == c ) {
		if( proc == STAR || proc == p ) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOLEAN
IdFilter::Wanted( const char *name )
{
	return FALSE;
}

void
IdFilter::display()
{
	if( proc == STAR ) {
		printf( "Id Filter: %d.*\n", cluster );
	} else {
		printf( "Id Filter: %d.%d\n", cluster, proc );
	}
}

UserFilter::UserFilter( const char *n )
{
	name = new char [ strlen(n) + 1 ];
	strcpy( name, n );
}

UserFilter::~UserFilter()
{
	delete [] name;
}

BOOLEAN
UserFilter::Wanted( const char *str )
{
	if( strcmp(str,name) == 0 ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

BOOLEAN
UserFilter::Wanted( int cluster, int proc )
{
	return FALSE;
}

void
UserFilter::display()
{
	printf( "User Filter: %s\n", name );
}

BOOLEAN
ProcFilter::Append( const char *arg )
{
	const char	*p;

	for( p=arg; *p; p++ ) {
		if( !isdigit(*p) && *p != '.' ) {
			return AppendUser( arg );
		}
	}
	return AppendId( arg );
}
