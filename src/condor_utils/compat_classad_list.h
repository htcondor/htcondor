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

#ifndef COMPAT_CLASSAD_LIST_H
#define COMPAT_CLASSAD_LIST_H

#include "compat_classad.h"

#include "HashTable.h"

// The SortFunction returns a 1 if the first classad is
// "smaller than" the second classad. Do not assume
// about other return values.
typedef int (*SortFunctionType)(ClassAd*,ClassAd*,void*);

class ClassAdListItem {
public:
	ClassAd *ad;
	ClassAdListItem *prev;
	ClassAdListItem *next;
};

/* ClassAdListDoesNotDeleteAds is a list of ads.  When the list is
 * destructed, any ads that it contains are _not_ deleted.  If you
 * want them to be, use ClassAdList instead.
 *
 * Behavior that exists for compatibility with historical usage:
 *   - duplicates are detected and ignored at insertion time
 *   - removal is fast and does not involve a linear search
 */
class ClassAdListDoesNotDeleteAds
{
protected:
		/* For fast lookups (required in Insert() and Remove()),
		 * we maintain a hash table that indexes into the list
		 * via a pointer to the classad.
		 */
	HashTable<ClassAd*,ClassAdListItem*> htable;
	ClassAdListItem *list_head; // double-linked list
	ClassAdListItem *list_cur; // current position in list

		/* The following private class applies the user supplied
		 * sort function and the user supplied context to compare
		 * two ClassAds. It is made use of in the Sort() function
		 * as a parameter to the generic std::sort() algorithm.
		 */
	class ClassAdComparator
	{
	private:
		void* userInfo;
		SortFunctionType smallerThan;
	public:
		ClassAdComparator(void* uinfo, SortFunctionType sf)
		{
			userInfo = uinfo;
			smallerThan = sf;
		}
			/** Return true if a comes before b in the sorted order
			 * @param a classad to be compared
			 * @param b classad to be compared
			 * @return true if a comes before b in sorted order,
			 *  false otherwise.
			 */
		bool operator() (ClassAdListItem* a, ClassAdListItem* b)
		{
			int res = this->smallerThan(a->ad,b->ad,this->userInfo);
			if (res == 1) return true;
			return false;
		}
	};

public:
	ClassAdListDoesNotDeleteAds();
	virtual ~ClassAdListDoesNotDeleteAds();
	virtual void Clear();
	ClassAd* Next();
	void Open();
		/*This Close() function is no longer really needed*/
	void Close();
	void Rewind()           { Open(); }
	int MyLength()          { return Length(); }
		/* Removes ad from list, but does not delete it.  Removal is
		 * fast.  No linear search through the list is required.
		 */
	int Remove(ClassAd* cad);
		/* Duplicates are efficiently detected and ignored */
	void Insert(ClassAd* cad);
	int Length();
		/* Note on behaviour. The Sort function does not touch the
		 * index, nor invalidate it. index will be within defined limits
		 * but continuing an iteration from the index would be unwise.
		 */
	void Sort(SortFunctionType smallerThan, void* userInfo = NULL);

	void Shuffle();

    // Count classads satisfying constraint.  Optionally remove ads that don't.
    int CountMatches(classad::ExprTree* constraint);
};

/* ClassAdList is just like ClassAdListDoesNotDeleteAds, except it
 * deletes ads that remain in the list when the list is destructed.
 * It also defines a Delete() method for deleting specific ads.
 */
class ClassAdList: public ClassAdListDoesNotDeleteAds
{
 public:
	~ClassAdList();

		/* Removes ad from list, but does not delete it.  Removal is
		 * fast.  No linear search through the list is required.
		 */
	int Delete(ClassAd* cad);
	virtual void Clear();
};

#endif
