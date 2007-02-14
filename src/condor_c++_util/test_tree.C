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
#include "tree.h"

#include <iostream>

using namespace std;

int fill_array(Tree<int>* tree, int array[][2], int size, int index)
{
	int num_inserted = 0;

	// insert current node's data
	assert(index + num_inserted < size);
	array[index + num_inserted][0] = tree->get_data();
	Tree<int>* parent = tree->get_parent();
	array[index + num_inserted][1] = parent ? parent->get_data() : -1;
	num_inserted += 1;
	
	// recurse on chlidren
	Tree<int>* child = tree->get_child();
	while (child != NULL) {
		num_inserted += fill_array(child, array, size, index + num_inserted);
		child = child->get_sibling();
	}

	return num_inserted;
}

void print(Tree<int>* t)
{
	int array[100][2];
	memset(array, 0, 200);
	fill_array(t, array, 100, 0);

	for (int i = 0; array[i][0]; i++) {
		printf("%d -> %d\n", array[i][1], array[i][0]);
	}
}

int
main()
{
	Tree<int>* tmp;

	// just a root node
	Tree<int>* t1 = new Tree<int>(1);
	cout << "test 1:" << endl;
	print(t1);
	cout << endl;
	
	// root node with three children
	Tree<int>* t12;
	Tree<int>* t13;
	Tree<int>* t14;
	t12 = t1->add_child(12);
	assert(t12 != NULL);
	t13 = t1->add_child(13);
	assert(t13 != NULL);
	t14 = t1->add_child(14);
	assert(t14 != NULL);
	cout << "test 2:" << endl;
	print(t1);
	cout << endl;

	// give two children each to "13" and "14"
	tmp = t13->add_child(20);
	assert(tmp != NULL);
	tmp = t13->add_child(21);
	assert(tmp != NULL);
	tmp = t14->add_child(22);
	assert(tmp != NULL);
	tmp = t14->add_child(23);
	assert(tmp != NULL);
	cout << "test 3:" << endl;
	print(t1);
	cout << endl;

	// detach 13
	t13->remove();
	cout << "test 4 (detached):" << endl;
	print(t13);
	cout << endl;
	cout << "test 4 (original):" << endl;
	print(t1);
	cout << endl;

	// detach 14
	t14->remove();
	cout << "test 5 (detached):" << endl;
	print(t14);
	cout << endl;
	cout << "test 5 (original):" << endl;
	print(t1);
	cout << endl;

	delete t1;

	return EXIT_SUCCESS;
}
