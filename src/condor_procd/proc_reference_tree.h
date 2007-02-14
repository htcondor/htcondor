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
