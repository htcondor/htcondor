/***************************************************************
 *
 * Copyright (C) 1990-2023, Condor Team, Computer Sciences Department,
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


#ifndef DAG_PRIORITY_Q_H
#define DAG_PRIORITY_Q_H

#include "node.h"

#include <vector>
#include <algorithm>


struct NodeSorter {
	// Our heap is a max heap, sorted in ascending order left to right
	bool operator() (const Node *a, const Node *b) {
		if (a->GetEffectivePrio() < b->GetEffectivePrio()) {
			return true;
		}

		if (a->GetEffectivePrio() > b->GetEffectivePrio()) {
			return false;
		}

		if (a->GetSubPrio() < b->GetSubPrio()) {
			return true;
		}
		return false;
	}
};

class DagPriorityQ {
	public:
		int size() const {return (int) _q.size();}
		void prepend(Node *j) {
			generation++;
			j->setSubPrio(generation);
			_q.push_back(j);
			std::push_heap(_q.begin(), _q.end(), NodeSorter{});
		}
		void append(Node *j) {
			generation++;
			j->setSubPrio(-generation);
			_q.push_back(j);
			std::push_heap(_q.begin(), _q.end(), NodeSorter{});
		}

		bool empty() const { return _q.empty();}

		Node *pop() {
			std::pop_heap(_q.begin(), _q.end(), NodeSorter{});
			Node *j = _q.back();
			_q.pop_back();
			return j;
		}	

		// must call make_heap after this
		template<typename T> size_t erase_if(T filter) {
			// filter = [](Job* node) -> bool { nodes to erase from queue }
			return std::erase_if(_q, filter);
		}

		void make_heap() {
			std::make_heap(_q.begin(), _q.end(), NodeSorter{});
		}

		// If you mutate the heap, you must call make_heap afterwards
		std::vector<Node *>::iterator begin() { return _q.begin();}
		std::vector<Node *>::iterator end()   { return _q.end();}
	private:
		int generation = 0;
		std::vector<Node *> _q;	
};

#endif /* #ifndef DAG_PRIORITY_Q_H */
