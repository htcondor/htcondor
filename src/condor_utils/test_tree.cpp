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


#include "condor_common.h"
#include "tree.h"

#include <iostream>

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
	std::cout << "test 1:" << std::endl;
	print(t1);
	std::cout << std::endl;
	
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
	std::cout << "test 2:" << std::endl;
	print(t1);
	std::cout << std::endl;

	// give two children each to "13" and "14"
	tmp = t13->add_child(20);
	assert(tmp != NULL);
	tmp = t13->add_child(21);
	assert(tmp != NULL);
	tmp = t14->add_child(22);
	assert(tmp != NULL);
	tmp = t14->add_child(23);
	assert(tmp != NULL);
	std::cout << "test 3:" << std::endl;
	print(t1);
	std::cout << std::endl;

	// detach 13
	t13->remove();
	std::cout << "test 4 (detached):" << std::endl;
	print(t13);
	std::cout << std::endl;
	std::cout << "test 4 (original):" << std::endl;
	print(t1);
	std::cout << std::endl;

	// detach 14
	t14->remove();
	std::cout << "test 5 (detached):" << std::endl;
	print(t14);
	std::cout << std::endl;
	std::cout << "test 5 (original):" << std::endl;
	print(t1);
	std::cout << std::endl;

	delete t1;

	return EXIT_SUCCESS;
}
