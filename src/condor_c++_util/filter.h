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
