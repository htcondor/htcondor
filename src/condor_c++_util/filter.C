/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

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

#if defined(OSF1)
#pragma define_template List<FilterObj>
#pragma define_template Item<FilterObj>
#endif


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

	if( p=strchr(str,'.') ) {
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
