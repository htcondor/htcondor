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


#ifndef _TRACKER_HELPER_LIST_H
#define _TRACKER_HELPER_LIST_H

#include "condor_common.h"
#include "condor_debug.h"

// forward declaractions
//
struct procInfo;
class ProcFamily;

// the template parameter T in the TrackerHelperList class below
// must implement the following interface, which supports a boolean
// matching operation against a procInfo
//
class ProcInfoMatcher {
public:
	virtual ~ProcInfoMatcher() { }
	virtual bool test(procInfo*) = 0;
};

template <class T> class TrackerHelperList {

public:

	TrackerHelperList() : m_head(NULL) { }

	void add_mapping(T tag, ProcFamily* family)
	{
		ListNode* node = new ListNode;
		ASSERT(node != NULL);
		node->tag = tag;
		node->family = family;
		node->next = m_head;
		m_head = node;
	}

	bool remove_mapping(ProcFamily* family, T* tag_ptr = NULL)
	{
		ListNode** prev_ptr = &m_head;
		ListNode* curr = m_head;
		while (curr != NULL) {
			if (curr->family == family) {
				if (tag_ptr != NULL) {
					*tag_ptr = curr->tag;
				}
				*prev_ptr = curr->next;
				delete curr;
				return true;
			}
			prev_ptr = &curr->next;
			curr = curr->next;
		}
		return false;
	}

	bool remove_head(T* tag_ptr = nullptr)
	{
		return m_head ? remove_mapping(m_head->family, tag_ptr) : false;
	}

	ProcFamily* find_family(procInfo* pi)
	{
		ListNode* node = m_head;
		while (node != NULL) {
			ProcInfoMatcher* matcher = static_cast<ProcInfoMatcher*>(&node->tag);
			if (matcher->test(pi)) {
				return node->family;
			}
			node = node->next;
		};
		return NULL;
	}

	void clear() {
		ListNode* node = m_head;
		while (node != nullptr) {
			ListNode *prev = node;
			node = node->next;
			delete prev;
		}
		node = nullptr;
	}

private:

	struct ListNode {

		// data associated with this list node
		//
		T tag;

		// the family associated with this list node
		//
		ProcFamily* family;

		// the next node in the list
		//
		ListNode* next;
	};

	ListNode* m_head;
};

#endif

