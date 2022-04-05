/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "compat_classad_list.h"
#include "compat_classad_util.h"

#include <vector>
#include <algorithm>
#include <random>

static size_t
ptr_hash_fn(ClassAd* const &index)
{
	intptr_t i = (intptr_t)index;

	return (size_t) i;
}

ClassAdListDoesNotDeleteAds::ClassAdListDoesNotDeleteAds():
	htable(ptr_hash_fn)
{
		// Why do we allocate list_head separately on the heap rather
		// than just having it be a member of this class?  Because
		// there are cases where arrays of ClassAdLists are qsorted,
		// which does a shallow copy.  If list_head were a member of
		// this class, pointers to the old list_head from within the
		// linked list would no longer be valid, because they would
		// point to the address of list_head in the old copy rather
		// than the new copy of the list.
	list_head = new ClassAdListItem;
	list_head->ad = NULL;
	list_head->next = list_head; // beginning of list
	list_head->prev = list_head; // end of list
	list_cur = list_head;
}

ClassAdListDoesNotDeleteAds::~ClassAdListDoesNotDeleteAds()
{
	Clear();
	delete list_head;
	list_head = NULL;
}

ClassAdList::~ClassAdList()
{
	Clear();
}

void ClassAdListDoesNotDeleteAds::Clear()
{
	for(list_cur=list_head->next;
		list_cur!=list_head;
		list_cur=list_head->next)
	{
		list_head->next = list_cur->next;
		delete list_cur;
	}
	list_head->next = list_head;
	list_head->prev = list_head;
	list_cur = list_head;
}

void ClassAdList::Clear()
{
	for(list_cur=list_head->next;
		list_cur!=list_head;
		list_cur=list_cur->next)
	{
		delete list_cur->ad;
		list_cur->ad = NULL;
	}
	ClassAdListDoesNotDeleteAds::Clear();
}

ClassAd* ClassAdListDoesNotDeleteAds::Next()
{
	ASSERT( list_cur );
	list_cur = list_cur->next;
	return list_cur->ad;
}

int ClassAdList::Delete(ClassAd* cad)
{
	int ret = Remove( cad );
	if ( ret == TRUE ) {
		delete cad;
	}
	return ret;
}

int ClassAdListDoesNotDeleteAds::Remove(ClassAd* cad)
{
	ClassAdListItem *item = NULL;
	if( htable.lookup(cad,item) == 0 ) {
		htable.remove(cad);
		ASSERT( item );
		item->prev->next = item->next;
		item->next->prev = item->prev;
		if( list_cur == item ) {
			list_cur = item->prev;
		}
		delete item;
		return TRUE;
	}
	return FALSE;
}

void ClassAdListDoesNotDeleteAds::Insert(ClassAd* cad)
{
	ClassAdListItem *item = new ClassAdListItem;
	item->ad = cad;

	if( htable.insert(cad,item) == -1 ) {
		delete item;
		return; // already in list
	}
		// append to end of list
	item->next = list_head;
	item->prev = list_head->prev;
	item->prev->next = item;
	item->next->prev = item;
}

void ClassAdListDoesNotDeleteAds::Open()
{
	list_cur = list_head;
}

void ClassAdListDoesNotDeleteAds::Close()
{
}

int ClassAdListDoesNotDeleteAds::Length()
{
	return htable.getNumElements();
}

void ClassAdListDoesNotDeleteAds::Sort(SortFunctionType smallerThan, void* userInfo)
{
	ClassAdComparator isSmallerThan(userInfo, smallerThan);

		// copy into an STL vector for convenient sorting

	std::vector<ClassAdListItem *> tmp_vect;
	ClassAdListItem *item;

	for(item=list_head->next;
		item!=list_head;
		item=item->next)
	{
		tmp_vect.push_back(item);
	}

	std::sort(tmp_vect.begin(), tmp_vect.end(), isSmallerThan);

		// empty our list
	list_head->next = list_head;
	list_head->prev = list_head;

		// arrange our list in same order as tmp_vect
	std::vector<ClassAdListItem *>::iterator it;
	for(it = tmp_vect.begin();
		it != tmp_vect.end();
		it++)
	{
		item = *(it);
			// append to end of our list
		item->next = list_head;
		item->prev = list_head->prev;
		item->prev->next = item;
		item->next->prev = item;
	}
}

void
ClassAdListDoesNotDeleteAds::Shuffle()
{
		// copy into an STL vector for convenient sorting

	std::vector<ClassAdListItem *> tmp_vect;
	ClassAdListItem *item;

	for(item=list_head->next;
		item!=list_head;
		item=item->next)
	{
		tmp_vect.push_back(item);
	}

	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(tmp_vect.begin(), tmp_vect.end(), g);

		// empty our list
	list_head->next = list_head;
	list_head->prev = list_head;

		// arrange our list in same order as tmp_vect
	std::vector<ClassAdListItem *>::iterator it;
	for(it = tmp_vect.begin();
		it != tmp_vect.end();
		it++)
	{
		item = *(it);
			// append to end of our list
		item->next = list_head;
		item->prev = list_head->prev;
		item->prev->next = item;
		item->next->prev = item;
	}
}

int ClassAdListDoesNotDeleteAds::CountMatches(classad::ExprTree* constraint)
{
	ClassAd *ad = NULL;
	int matchCount  = 0;

	// Check for null constraint.
	if ( constraint == NULL ) {
		return 0;
	}

	Rewind();
	while( (ad = Next()) ) {
		if ( EvalExprBool(ad, constraint) ) {
			matchCount++;
        }
	}
	return matchCount;
}
