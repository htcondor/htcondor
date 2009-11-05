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

#include "condor_common.h"
#include "compat_classad.h"

#include <vector>
#include <algorithm>

namespace compat_classad {

// The SortFunction returns a 1 if the first classad is
// "smaller than" the second classad. Do not assume
// about other return values.
typedef int (*SortFunctionType)(classad::ClassAd*,classad::ClassAd*,void*);

class ClassAdList
{
private:
	std::vector<ClassAd*> list;
	int index;

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
		bool operator() (classad::ClassAd* a, classad::ClassAd* b)
		{
			int res = this->smallerThan(a,b,this->userInfo);
			if (res == 1) return true;
			return false;
		}
	};

public:
	ClassAdList()           { index = 0; }
	~ClassAdList();
	ClassAd* Next();
	void Open()             { index = 0; }
		/*This Close() function is no longer really needed*/
	void Close()            { index = 0; /*Just a safety measure*/ }
	void Rewind()           { /*same as Open()*/ index = 0; }
	int MyLength()          { /*Same as Length()*/ return list.size(); }
		/* Removes ad from list and deletes it */
	int Delete(ClassAd* cad);
		/* Removes ad from list, but does not delete it */
	int Remove(ClassAd* cad);
	void Insert(ClassAd* cad);
	int Length()            { /*Same as MyLength()*/ return list.size(); }
		/* Note on behaviour. The Sort function does not touch the
		 * index, nor invalidate it. index will be within defined limits
		 * but continuing an iteration from the index would be unwise.
		 */
	void Sort(SortFunctionType smallerThan, void* userInfo = NULL);
};

} // namespace compat_classad

#endif
