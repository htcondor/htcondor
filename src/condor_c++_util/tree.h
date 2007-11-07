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


#ifndef _CONDOR_TREE_H
#define _CONDOR_TREE_H

#include "condor_debug.h"

template<class T> class Tree {
public:

	// construct a Tree with the given data for the root node, e.g.
	//
	//     Tree<int>* tree = new Tree<int>(5);
	//
	Tree(T data) : m_data(data), m_parent(NULL), m_child(NULL), m_sibling(NULL) { }

	// delete this tree (frees memory from all children)
	~Tree<T>() {
		if (m_sibling) delete m_sibling;
		if (m_child) delete m_child;
	}

	// get the data out of this tree's root node, e.g.
	//
	//     int x = tree.get_data() + 4;
	//
	T get_data() { return m_data; }

	// add a child node to this tree's root node (NULL return on error)
	//
	//     Tree<int>* child_node = tree.add_child(2);
	//
	Tree<T>* add_child(T data) {
		Tree<T>* child =  new Tree<T>(data, this, m_child);
		if (child != NULL) {
			m_child = child;
		}
		return child;
	}

	// get this node's parent; NULL means this is the root
	//
	//     Tree<int>* parent = child.get_parent();
	//
	Tree<T>* get_parent() { return m_parent; }

	// get a child (in this implementation the youngest), which can
	// be used to get at the rest of the children via its sibling()
	// method, e.g.
	//
	//     Tree<T>* youngest_child = tree.get_child();
	//     Tree<T>* older_sister = youngest_child.get_sibling();
	Tree<T>* get_child() { return m_child; }

	// get the next sibling in a list of siblings (see comment
	// for get_child)
	//
	Tree<T>* get_sibling() { return m_sibling; }

	// detach the node from the tree, with all children
	// inherited by our parent (note that this does not work
	// if called on the tree root, since there is nowhere to
	// reparent the children)
	//
	void remove() {

		// doesn't work for tree root
		//
		ASSERT(m_parent != NULL);

		if (m_child != NULL) {

			// if we have children, they need to be reparented to
			// our parent
			Tree<T>* child = m_child;
			while (child->m_sibling != NULL) {
				child->m_parent = m_parent;
				child = child->m_sibling;
			}
			child->m_parent = m_parent;

			// now place merge the children list into our siblings
			//
			child->m_sibling = m_sibling;
			m_sibling = m_child;
		}

		// now find our "previous" node in the tree; this could be
		// either our parent or a sibling; we need to make this node
		// point to our sibling list, since we're leaving the tree
		//
		Tree<T>** prev_ptr = &m_parent->m_child;
		while(*prev_ptr != this) {
			prev_ptr = &(*prev_ptr)->m_sibling;
		}
		*prev_ptr = m_sibling;

		// sever all ties
		//
		m_child = NULL;
		m_sibling = NULL;
		m_parent = NULL;
	}
		
private:
	// construct tree node with the given data and a sibling
	Tree(T data, Tree<T>* parent, Tree<T>* sibling) :
		m_data(data), m_parent(parent), m_child(NULL), m_sibling(sibling) { }

	// data members
	T m_data;
	Tree<T>* m_parent;
	Tree<T>* m_child;
	Tree<T>* m_sibling;
};

#endif
