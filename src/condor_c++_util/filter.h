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
  Purpose:
  Several of the condor programs accept command line arguments which
  determine a subset of jobs in the queue which are of interest.
  Examples are condor_q, condor_rm, and condor_prio.  Here we implement
  a class intended to be used as a filter when constructing process
  lists from the job queue.  A filter consists of a list of
  cluster/process pairs or cluster identifiers and user names.  A
  cluster/process pair specifies that a specific process, a cluster
  without a process specifies every process in the cluster, and a user
  name specifies every process belonging to the user.  Also an empty
  list signifies that every process is of interest.

  These classes are intended to be used with the ProcObj class, and in
  particular result in a filtering function which can be passed to the
  ProcObj member function create_list().

  Example use:
  If one has an open job queue with handle "Q", then to build a ProcObj
  list containing all the processes in cluster 17, the specific
  processes 19.0 and 19.3, and all processes belonging to user "foo",
  the following code can be used.

  Filter	*PFilter = new Filter();
  List<ProcObj> proc_list;

  PFilter->AppendId( "17" );
  PFilter->AppendId( "19.0" );
  PFilter->AppendId( "19.3" );
  PFilter->AppendUser( "foo" );
  PFilter->Install();
  proc_list = ProcObj::create_list( Q, ProcFilter::Func );

*/


#ifndef FILTER_H
#define FILTER_H

#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "list.h"

class FilterObj;

class ProcFilter {
public:
	ProcFilter();
	~ProcFilter();
	BOOLEAN Append( const char * );
	BOOLEAN AppendUser( const char * );
	BOOLEAN AppendId( const char * );
	void Display();
	void Install();
	static Func( PROC * );
private:
	List<FilterObj> *list;
};

class FilterObj {
public:
	virtual ~FilterObj() {;}
	virtual BOOLEAN Wanted( int, int ) = 0;
	virtual BOOLEAN Wanted( const char * ) = 0;
	virtual void display() = 0;
};

class IdFilter : public FilterObj {
public:
	IdFilter( const char * );
	BOOLEAN Wanted( int, int );
	BOOLEAN Wanted( const char * );
	void display();
private:
	int		cluster;
	int		proc;
};

class UserFilter : public FilterObj {
public:
	UserFilter( const char * );
	~UserFilter();
	BOOLEAN Wanted( int, int );
	BOOLEAN Wanted( const char * );
	void display();
private:
	char *name;
};


#endif
