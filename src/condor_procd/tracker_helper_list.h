/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef _TRACKER_HELPER_LIST_H
#define _TRACKER_HELPER_LIST_H

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

