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


#ifndef _PROC_REFERENCE_TREE
#define _PROC_REFERENCE_TREE

#include "condor_common.h"
#include "local_client.h"
#include <set>
#include "HashTable.h"
#include "tree.h"

class ProcReferenceTree {

public:
	ProcReferenceTree(pid_t);
	ProcReferenceTree(LocalClient&);

	pid_t get_root_pid() { return m_tree->get_data()->m_root; }

	void process_created(pid_t, pid_t);
	void process_exited(pid_t);

	void subfamily_registered(pid_t, pid_t);
	void family_killed(pid_t);

	void test(ProcReferenceTree&);

	void output(FILE*);

	struct Family {
		pid_t m_root;
		pid_t m_watcher;
		std::set<pid_t> m_set;
	};

private:
	Tree<Family*>* m_tree;
	HashTable<pid_t, Tree<Family*>*> m_table;

	void delete_unwatched_families(Tree<Family*>*, pid_t);

	void free_families(Tree<Family*>*);

	void read_family(LocalClient&, Tree<Family*>*);

	void test(Tree<Family*>*, Tree<Family*>*);

	void output(FILE*, Tree<Family*>*, int);
};

#endif
