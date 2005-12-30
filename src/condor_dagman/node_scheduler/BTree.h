/*
 *	This file is licensed under the terms of the Apache License Version 2.0 
 *	http://www.apache.org/licenses. This notice must appear in modified or not 
 *	redistributions of this file.
 *
 *	Redistributions of this Software, with or without modification, must
 *	reproduce the Apache License in: (1) the Software, or (2) the Documentation
 *	or some other similar material which is provided with the Software (if any).
 *
 *	Copyright 2005 Argonne National Laboratory. All rights reserved.
 */

#ifndef GM_B_TREE_H
#define GM_B_TREE_H

const int t=2;	// any node has at least t and at most 2t children, except for the root


/*
 *	FILE CONTENT:
 *	Declaration of a BTree class
 *	Each node of the tree stores:
 *		a number nkeys
 *			this number can be at most 2t-1, 
 *			the nkeys value of the root must be at least 1, the nkeys value of any other node must be at least t-1
 *		nkeys key-value pairs pair[0].key,...,pair[nkey-1].key and pair[0].val,...,pair[nkey-1].val
 *		a pointer p to the parent of the node;
 *			the parent must have the node among its children, if the node does not have a parent then p=NULL
 *		1+nkeys pointers child[0],...,child[nkeys] to children nodes;
 *			each child must have its parent pointer set to the node;
 *			if any pointer to a child node is NULL then all are NULL and the node is a leaf
 *		
 *	                                               ------
 *	                                               |  c |
 *	                                               ---|--
 *	                                                ^ |
 *	                                                | v
 *	------------------------------------------------|-----------------------------------------------------------------
 *	| nkeys                                         p                                                                |
 *	|          <= key[0] <=          <= key[1] <=          <= key[2] <=          ... <= key[nkeys-1] <=              |
 *	| child[0]              child[1]              child[2]              child[3] ...                    child[nkeys] |
 *	---|---------------------|---------------------|---------------------|-------------------------------|-----------
 *	   | ^                   | ^                   | ^                   | ^                             | ^  
 *	   v |                   v |                   v |                   v |                             v |  
 *	  ---|--                ---|--                ---|--                ---|--                          ---|--
 *	  |  p |                |  p |                |  p |                |  p |                          |  p |
 *	  ------                ------                ------                ------                          ------
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/6/05	--	coding and testing finished
 */

struct Btree_Pair
{
	unsigned long key;
	unsigned long val;
};

struct Btree_Node
{
	Btree_Node* p;				// parent node
	int nkeys;					// number of key-val pairs currently stored in the node
	Btree_Pair pair[2*t];		// one extra key-val pair to conveniently handle an overgrown node
	Btree_Node* child[2*t+1];	// one extra, same reason
};


class BTree
{

private:

	Btree_Node* root;

	void deleteRecursive(Btree_Node* pNode);

	void detachSibling(Btree_Node* pNode, Btree_Node** ppSibling, Btree_Pair* pPairUp);
	void attachSibling(Btree_Node* pNode, Btree_Node* pRightSibling);
	Btree_Node* leftSibling(Btree_Node* pNode);
	Btree_Node* rightSibling(Btree_Node* pNode);
	void keyTransferRight(Btree_Node* pLeft, Btree_Node* pRight);
	void keyTransferLeft(Btree_Node* pLeft, Btree_Node* pRight);

	void searchKey(unsigned long iKey, Btree_Node** ppNode, int* pIndex) const;
	void searchMax(Btree_Node** ppNode, int* pIdx) const;
	void searchMin(Btree_Node** ppNode, int* pIdx) const;

	void removeAtLeaf( Btree_Node* pNode, int idx );

	void printInd(Btree_Node* pNode, int iTab) const;

public:

	BTree(void);
	~BTree(void);

	bool isEmpty(void) const;

	void insert(unsigned long key, unsigned long val);

	bool exists(unsigned long key) const;

	unsigned long findVal(unsigned long key) const;
	bool remove(unsigned long key);		// the very pair that findVal finds

	unsigned long findMaxKey(void) const;
	unsigned long findValForMaxKey(void) const;
	void removeMax(void);	// the very pair that the two above return

	unsigned long findMinKey(void) const;
	unsigned long findValForMinKey(void) const;
	void removeMin(void);	// the very pair that the two above return

	void print() const;
};


typedef BTree *  BTreePtr;


void Btree_test(void);

#endif
