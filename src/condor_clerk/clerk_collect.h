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

#ifndef _CLERK_COLLECT_H_
#define _CLERK_COLLECT_H_

// call this to reconfig the collection code
void collect_config();

// call this to register the collectors own ad
int collect_self(ClassAd * ad);

// the act of collection. 
int collect(int operation, AdTypes adtype, ClassAd * ad, ClassAd * adPvt);
// possible values for operation argument of collect
#define COLLECT_OP_REPLACE   0
#define COLLECT_OP_MERGE     1
#define COLLECT_OP_EXPIRE    2 // expire, but do not delete expired ads
#define COLLECT_OP_DELETE    3

// delete or exprire ads that match the delete_if constraint
int collect_delete(int operation, AdTypes adtype, ConstraintHolder & delete_if);

template<typename T> class CollectionIterFromCondorList : public CollectionIterator<T*>
{
public:
	List<T> * lst;
	CollectionIterFromCondorList(List<T>* l) : lst(l) {}
	virtual ~CollectionIterFromCondorList() {};
	virtual bool IsEmpty() const { return lst->IsEmpty(); }
	virtual void Rewind() { lst->Rewind(); }
	virtual T* Next () { return lst->Next(); }
	virtual T* Current() const { return lst->Current(); }
	virtual bool AtEnd() const { return lst->AtEnd(); }
};

class CollectionIterFromClassadList : public CollectionIterator<ClassAd*>
{
public:
	ClassAdList * lst;
	ClassAd * cur;
	CollectionIterFromClassadList(ClassAdList* l) : lst(l), cur(NULL) {}
	virtual ~CollectionIterFromClassadList() {};
	virtual bool IsEmpty() const { return lst->Length() <= 0; }
	virtual void Rewind() { lst->Rewind(); }
	virtual ClassAd* Next () { cur = lst->Next(); return cur; }
	virtual ClassAd* Current() const { return cur; }
	virtual bool AtEnd() const { return cur != NULL; }
};

CollectionIterator<ClassAd*>* collect_get_iter(AdTypes whichAds);
void collect_free_iter(CollectionIterator<ClassAd*> * it);

struct CollectionCounters {
	int submitterNumAds;
	int submitterRunningJobs;
	int submitterIdleJobs;
	int startdNumAds;

	int machinesTotal;
	int machinesUnclaimed;
	int machinesClaimed;
	int machinesOwner;

	int jobsByUniverse[CONDOR_UNIVERSE_MAX];

	void reset() { memset(this, 0, sizeof(*this)); }
};

void calc_collection_totals(struct CollectionCounters & counts);

#endif
