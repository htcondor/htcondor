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
#ifndef __CONDOR_Q_H__
#define __CONDOR_Q_H__

#include "generic_query.h"

// This is for the getFilterAndProcess function
typedef bool    (*process_function)(ClassAd *);

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

	// add constraints
	int add (CondorQIntCategories, int);
	int add (CondorQStrCategories, char *);
	int add (CondorQFltCategories, float);
	int addAND (char *);  // custom
	int addOR (char *);  // custom

	// fetch the job ads from the schedd corresponding to the given classad
	// which pass the criterion specified by the constraints; default is
	// from the local schedd
	int fetchQueue (ClassAdList &, ClassAd * = 0);
	int fetchQueueFromHost (ClassAdList &, char * = 0);
	int fetchQueueFromHostAndProcess ( char *, process_function process_func);

  private:
	GenericQuery query;
	
	// helper functions
	int getAndFilterAds( ClassAd &, ClassAdList & );
	int getFilterAndProcessAds( ClassAd &, process_function );
};

int JobSort(ClassAd *job1, ClassAd *job2, void *data);

const char encode_status( int status );

#endif
