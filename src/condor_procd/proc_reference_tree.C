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

#include "condor_common.h"
#include "../condor_procapi/procapi.h"
#include "proc_reference_tree.h"
#include <map>

using namespace std;

ProcReferenceTree::ProcReferenceTree(pid_t pid) :
	m_table(31, pidHashFunc)
{
	Family* root_family = new Family;
	root_family->m_root = pid;
	root_family->m_watcher = 1;
	root_family->m_set.insert(pid);
	m_tree = new Tree<Family*>(root_family);

	int ret = m_table.insert(pid, m_tree);
	ASSERT(ret != -1);
}

ProcReferenceTree::ProcReferenceTree(LocalClient& client) :
	m_table(31, pidHashFunc)
{
	Family* root_family = new Family;
	m_tree = new Tree<Family*>(root_family);

	read_family(client, m_tree);

	HashTable<pid_t, Tree<Family*>*> nodes_by_root(11, pidHashFunc);
	int ret = nodes_by_root.insert(root_family->m_root, m_tree);
	ASSERT(ret != -1);

	while (true) {

		pid_t parent_pid;
		client.read_data(&parent_pid, sizeof(pid_t));
		printf("READ: %u\n", parent_pid);
		
		if (parent_pid == 0) {
			break;
		}

		Tree<Family*>* parent_node;
		ret = nodes_by_root.lookup(parent_pid, parent_node);
		ASSERT(ret != -1);

		Family* family = new Family;
		Tree<Family*>* tree_node = parent_node->add_child(family);

		read_family(client, tree_node);

		ret = nodes_by_root.insert(family->m_root, tree_node);
		ASSERT(ret != -1);
	}

	client.end_connection();
}

void
ProcReferenceTree::process_created(pid_t parent, pid_t child)
{
	Tree<Family*>* tree_node;
	int ret = m_table.lookup(parent, tree_node);
	ASSERT(ret != -1);

	tree_node->get_data()->m_set.insert(child);

	ret = m_table.insert(child, tree_node);
	ASSERT(ret != -1);
}

void
ProcReferenceTree::process_exited(pid_t pid)
{
	Tree<Family*>* tree_node;
	int ret = m_table.lookup(pid, tree_node);
	ASSERT(ret != -1);

	tree_node->get_data()->m_set.erase(pid);

	ret = m_table.remove(pid);
	ASSERT(ret != -1);

	delete_unwatched_families(m_tree, pid);
}

void
ProcReferenceTree::subfamily_registered(pid_t pid, pid_t watcher_pid)
{
	Tree<Family*>* tree_node;
	int ret = m_table.lookup(pid, tree_node);
	ASSERT(ret != -1);

	// take it out of it current family since it will be
	// moved into the newly created subfamily
	//
	tree_node->get_data()->m_set.erase(pid);

	// create the subfamily and attach it to our tree
	//
	Family* child_family = new Family;
	child_family->m_root = pid;
	child_family->m_watcher = watcher_pid;
	child_family->m_set.insert(pid);
	Tree<Family*>* child_tree_node = tree_node->add_child(child_family);

	// update our hash table
	ret = m_table.remove(pid);
	ASSERT(ret != -1);
	ret = m_table.insert(pid, child_tree_node);
	ASSERT(ret != -1);
}

void
ProcReferenceTree::family_killed(pid_t pid)
{
	// look up the tree node that will be the root of the killing spree
	//
	Tree<Family*>* tree_node;
	int ret = m_table.lookup(pid, tree_node);
	ASSERT(ret != -1);

	// recursively blow all the family allocations away
	//
	free_families(tree_node);
}

void
ProcReferenceTree::test(ProcReferenceTree& procd_tree)
{
	printf("TESTING PROCD DUMP FOR SUBFAMILY WITH PID %u...\n",
	       m_tree->get_data()->m_root);

	// make sure the root pids match
	//
	Family* procd_family = procd_tree.m_tree->get_data();
	if (procd_family->m_root != m_tree->get_data()->m_root) {
		printf("  ERROR: procd root = %u in dump output for pid %u\n",
		       procd_family->m_root,
			   m_tree->get_data()->m_root);
		return;
	}

	// begin the recursive tests
	//
	test(m_tree, procd_tree.m_tree);
}

void
ProcReferenceTree::output(FILE* fp)
{
	output(fp, m_tree, 0);
}

void
ProcReferenceTree::delete_unwatched_families(Tree<Family*>* tree, pid_t exited_pid)
{
	// recurse on children
	//
	Tree<Family*>* child = tree->get_child();
	while (child != NULL) {

		// save the next child, since recusing may result in
		// child being removed from the tree
		//
		Tree<Family*>* next_child = child->get_sibling();
		delete_unwatched_families(child, exited_pid);
		child = next_child;
	}

	// then check the current node
	//
	if (exited_pid == tree->get_data()->m_watcher) {
		
		// move all processes for this set to the parent set
		//
		Tree<Family*>* parent = tree->get_parent();
		ASSERT(parent != NULL);
		set<pid_t>::iterator iter = tree->get_data()->m_set.begin();
		set<pid_t>::iterator iter_end = tree->get_data()->m_set.end();
		while (iter != iter_end) {

			parent->get_data()->m_set.insert(*iter);

			int ret;
			ret = m_table.remove(*iter);
			ASSERT(ret != -1);
			ret = m_table.insert(*iter, parent);
			ASSERT(ret != -1);
			
			iter++;
		}

		// detach this node from the tree (reparenting all
		// children) and delete it
		//
		tree->remove();
		delete tree->get_data();
		delete tree;
	}
}

void
ProcReferenceTree::free_families(Tree<Family*>* tree)
{
	// recurse on children (it's important to do children first
	// sice we're freeing memory here)
	//
	Tree<Family*>* child = tree->get_child();
	while (child != NULL) {
		free_families(child);
		child = child->get_sibling();
	}

	// now detach then free up the current node
	//
	tree->remove();
	delete tree->get_data();
	delete tree;
}

void
ProcReferenceTree::read_family(LocalClient& client, Tree<Family*>* tree)
{
	Family* family = tree->get_data();

	family->m_watcher = 1;

	client.read_data(&family->m_root, sizeof(pid_t));
	printf("READ: %u\n", family->m_root);

	while (true) {
		pid_t pid;
		client.read_data(&pid, sizeof(pid_t));
		printf("READ: %u\n", pid);
		if (pid == 0) {
			break;
		}
		family->m_set.insert(pid);
		int ret = m_table.insert(pid, tree);
		ASSERT(ret != -1);
	}
}

void
ProcReferenceTree::test(Tree<Family*>* real_tree, Tree<Family*>* procd_tree)
{
	printf("  TESTING SUBFAMILY WITH PID %u...\n",
	       real_tree->get_data()->m_root);

	Family* real_family = real_tree->get_data();
	Family* procd_family = procd_tree->get_data();

	// run an iterator over the procd's pids; any that are not
	// in the real family are false positives
	//
	set<pid_t> real_family_set = real_family->m_set;
	set<pid_t>::iterator iter = procd_family->m_set.begin();
	set<pid_t>::iterator iter_end = procd_family->m_set.end();
	while (iter != iter_end) {
		size_t num_erased = real_family_set.erase(*iter);
		if (num_erased == 0) {
			printf("    ERROR: false positive: procd has %u\n", *iter);
		}
		else {
			//printf("    GOOD: procd has %u\n", *iter);
		}
		iter++;
	}

	// now we've removed from the real family all the pids that the
	// procd had for this family; any pids that remain in the real
	// family now are false negatives
	//
	iter = real_family_set.begin();
	iter_end = real_family_set.end();
	while (iter != iter_end) {
		printf("    ERROR: false negative: procd does not have %u\n", *iter);
		iter++;
	}

	// now we'll build up two maps:
	//   1) all child nodes of tree by root pid
	//   2) all child nodes of procd_tree by root pid
	//
	Tree<Family*>* child;

	// 1
	//
	map<pid_t, Tree<Family*>*> real_child_map;
	child = real_tree->get_child();
	while (child != NULL) {
		real_child_map[child->get_data()->m_root] = child;
		child = child->get_sibling();
	}

	// 2
	//
	map<pid_t, Tree<Family*>*> procd_child_map;
	child = procd_tree->get_child();
	while (child != NULL) {
		procd_child_map[child->get_data()->m_root] = child;
		child = child->get_sibling();
	}
		
	// run an iterator over the procd's child families; any that are not
	// in the real set of child families are false positives
	//
	map<pid_t, Tree<Family*>*>::iterator map_iter = procd_child_map.begin();
	map<pid_t, Tree<Family*>*>::iterator map_iter_end = procd_child_map.end();
	while (map_iter != map_iter_end) {
		if (real_child_map.count((*map_iter).first) == 0) {
			printf("    ERROR: false positive: procd has child with root %u\n",
			       (*map_iter).first);
		}
		else {
			printf("    GOOD: procd has child with root %u\n",
			       (*map_iter).first);
		}
		map_iter++;
	}

	// now run an iterator over the real child families; any that are not
	// in the procd's set of child families are false negatives
	//
	map_iter = real_child_map.begin();
	map_iter_end = real_child_map.end();
	while (map_iter != map_iter_end) {
		if (procd_child_map.count((*map_iter).first) == 0) {
			printf("    ERROR: false negative: procd does not have child with root %u\n",
			       (*map_iter).first);
		}
		else {
			printf("    GOOD: procd has child with root %u\n",
			       (*map_iter).first);
		}
		map_iter++;
	}

	// iterate over the real child families one last time, this time
	// recursing on those that are also present in the procd's child
	// map
	//
	map_iter = real_child_map.begin();
	map_iter_end = real_child_map.end();
	while (map_iter != map_iter_end) {
		if (procd_child_map.count((*map_iter).first) != 0) {
			test((*map_iter).second, procd_child_map[(*map_iter).first]);
		}
		map_iter++;
	}
}

void
ProcReferenceTree::output(FILE* fp, Tree<Family*>* tree, int level)
{
	// output the current tree node first
	for (int i = 0; i < level; i++) {
		fprintf(fp, "    ");
	}
	set<pid_t>::iterator iter = tree->get_data()->m_set.begin();
	set<pid_t>::iterator iter_end = tree->get_data()->m_set.end();
	while (iter != iter_end) {
		fprintf(fp, "%d ", *iter);
		iter++;
	}
	fprintf(fp,
	        "(root: %u, watcher: %u)\n",
	        tree->get_data()->m_root,
	        tree->get_data()->m_watcher);

	// recurse on children
	Tree<Family*>* child = tree->get_child();
	while (child != NULL) {
		output(fp, child, level + 1);
		child = child->get_sibling();
	}
}
