/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __CONDOR_Q_H__
#define __CONDOR_Q_H__

#include "condor_common.h"
#include "generic_query.h"
#include "CondorError.h"

#define MAXOWNERLEN 20

// This is for the getFilterAndProcess function
typedef bool    (*process_function)(ClassAd *);

/* a list of all types of direct DB query defined here */
enum CondorQQueryType
{
	AVG_TIME_IN_QUEUE
};

enum
{
	Q_NO_SCHEDD_IP_ADDR = 20,
	Q_SCHEDD_COMMUNICATION_ERROR
};

enum CondorQIntCategories
{
	CQ_CLUSTER_ID,
	CQ_PROC_ID,
	CQ_STATUS,
	CQ_UNIVERSE,

	CQ_INT_THRESHOLD
};

enum CondorQStrCategories
{
	CQ_OWNER,

	CQ_STR_THRESHOLD
};

enum CondorQFltCategories
{
	CQ_FLT_THRESHOLD
};


class CondorQ
{
  public:
	// ctor/dtor
	CondorQ ();
	// CondorQ (const CondorQ &);
	~CondorQ ();

	// initialize defaults, etc.
	bool init();  

	// add constraints
	int add (CondorQIntCategories, int);
	int add (CondorQStrCategories, char *);
	int add (CondorQFltCategories, float);
	int addAND (char *);  // custom
	int addOR (char *);  // custom

	// fetch the job ads from the schedd corresponding to the given classad
	// which pass the criterion specified by the constraints; default is
	// from the local schedd
	int fetchQueue (ClassAdList &, ClassAd * = 0, CondorError* errstack = 0);
	int fetchQueueFromHost (ClassAdList &, char * = 0, CondorError* errstack = 0);
	int fetchQueueFromHostAndProcess ( char *, process_function process_func, CondorError* errstack = 0);
	
		// fetch the job ads from database 	
	int fetchQueueFromDB (ClassAdList &, char * = 0, CondorError* errstack = 0);
	int fetchQueueFromDBAndProcess ( char *, process_function process_func, CondorError* errstack = 0);

		// return the results from a DB query directly to user
	void rawDBQuery(char *, CondorQQueryType);

  private:
	GenericQuery query;
	
	// default timeout when talking the schedd (via ConnectQ())
	int connect_timeout;
	
	int cluster;
	int proc;
	char owner[MAXOWNERLEN];
	
	// helper functions
	int getAndFilterAds( ClassAd &, ClassAdList & );
	int getFilterAndProcessAds( ClassAd &, process_function );
};

int JobSort(ClassAd *job1, ClassAd *job2, void *data);

const char encode_status( int status );

#endif
