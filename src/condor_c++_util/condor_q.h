#ifndef __CONDOR_Q_H__
#define __CONDOR_Q_H__

#include "generic_query.h"

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
	int add (char *);  // custom

	// fetch the job ads from the schedd corresponding to the given classad
	// which pass the criterion specified by the constraints; default is
	// from the local schedd
	int fetchQueue (ClassAdList &, ClassAd * = 0);
	int fetchQueueFromHost (ClassAdList &, char * = 0);

  private:
	GenericQuery query;
	
	// helper functions
	int getAndFilterAds (ClassAd &, ClassAdList &);
};

int JobSort(ClassAd *job1, ClassAd *job2, void *data);

#endif
