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

#include "headers.h"

/*
 *	FILE CONTENT:
 *	Implementation of the BTree class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/6/05	--	coding and testing finished
 */


/*
 *	Method:
 *	BTree::BTree(void)
 *
 *	Purpose: 
 *	Initialize BTree to empty.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
BTree::BTree(void)
{
	root = NULL;
}


/*
 *	Method:
 *	BTree::~BTree(void)
 *
 *	Purpose: 
 *	Deletes BTree by freeing all the memory associated with the BTree nodes.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
BTree::~BTree(void)
{
	deleteRecursive(root);
	root = NULL;
}


/*
 *	Method:
 *	bool BTree::isEmpty(void) const
 *
 *	Purpose: 
 *	Checks if BTree is empty.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	true,	if BTree is empty
 *	false,	if not empty
 */
bool BTree::isEmpty(void) const
{
	return NULL == root;
}


/*
 *	Method:
 *	void BTree::deleteRecursive(Btree_Node* pNode)
 *
 *	Purpose: 
 *	A recursive method for freeing memory associated with nodes of BTree.
 *
 *	Input:
 *	node,	node whose memory will be freed, along with all descendants
 *
 *	Output:
 *	none
 */
void BTree::deleteRecursive(Btree_Node* pNode)
{
	if( NULL==pNode )
		return;

	for( int i=0; i<=pNode->nkeys; i++ )
		deleteRecursive( (pNode->child)[i] );
	delete pNode;
	return;
}


/*
 *	Method:
 *	void BTree::detachSibling(Btree_Node* pNode, Btree_Node** ppSibling, Btree_Pair* pPairUp)
 *
 *	Purpose:
 *	This method takes a node pNode that has 2t+1 children, creates a new node *ppSibling; moves t right children
 *	to the new node; and updates the pointers so that the node *ppSibling has the same parent as the original node.
 *
 *	BEFORE
 *	======
 *	                             ------
 *	                             |  c |
 *	                             ---|--
 *	                              ^ |
 *	                              | v
 *	------------------------------|--------------------------------------------------------
 *	| 2t                          p                                                       |
 *	|       k[0]     ......          k[t-1]      k[t]        k[t+1]  ....  k[2t-1]        |
 *	|  c[0]        .......    c[t-1]        c[t]      c[t+1]        .......        c[2t]  |
 *	---|----------------------|-------------|---------|----------------------------|-------
 *	   | ^                    | ^           | ^       | ^                          | ^  
 *	   v |                    v |           v |       v |                          v |  
 *	  ---|--                 ---|--        ---|--    ---|--                       ---|--
 *	  |  p |    .........    |  p |        |  p |    |  p |      ............     |  p |
 *	  ------                 ------        ------    ------                       ------
 *	
 *			
 *	AFTER
 *	=====
 *	                             ------                                                      NEW SIBLING NODE
 *	 UPDATE                      |  c | <-----------------------------------------\                 |
 *	 NUMBER OF KEYS              ---|--                       UPDATE              |      UPDATE     |
 *	  |                           ^ |                         NUMBER OF KEYS      |  /---PARENT     |
 *	  |                           | v                              |              | /               |
 *	------------------------------|----------------             ------------------|---------------------
 *	| t                           p               |             | t-1             p                    |
 *	|       k[0]     ......          k[t-1]       |             |         k[t+1]  ....  k[2t-1]        |
 *	|  c[0]        .......    c[t-1]        c[t]  |             |  c[t+1]        .......        c[2t]  |
 *	---|----------------------|-------------|------             ---|----------------------------|-------
 *	   | ^                    | ^           | ^                    | ^                          | ^  
 *	   v |                    v |           v |                    v |                          v |  
 *	  ---|--                 ---|--        ---|--                 ---|--                       ---|--
 *	  |  p |    .........    |  p |        |  p |                 |  p |      ............     |  p |  --- UPDATE
 *	  ------                 ------        ------                 ------                       ------      PARENTS
 *	                                                                                                       IF NOT NULL
 *	
 *	New sibling node is created, its parent pointer is set to the parent of the node, if children pointers are
 *	not NULL, then the parent pointers of the children of the sibling need to be relinked to point to the sibling, 
 *	and not to the node (as was in the case BEFORE above)
 *
 *	Input:
 *	pNode,		given node
 *	ppSibling,	detached node
 *	pKeyUp,		middle key of the given node
 *
 *	Output:
 *	none
 */
void BTree::detachSibling(Btree_Node* pNode, Btree_Node** ppSibling, Btree_Pair* pPairUp)
{
	if(NULL==pNode) throw "BTree::detachSibling, pNode is NULL";
	if(NULL==ppSibling) throw "BTree::detachSibling, ppSIBLING is NULL";
	if(NULL==pPairUp) throw "BTree::detachSibling, pPairUp is NULL";

	int i;

	// check if the node is indeed full
	if( 2*t != pNode->nkeys ) {
		*ppSibling = NULL;
		return;
	};

	*pPairUp = (pNode->pair)[t];

	*ppSibling = new Btree_Node;
	if( NULL==*ppSibling )
		throw "BTree::detachSibling, *ppSibling is NULL";

	(*ppSibling)->p = pNode->p;

	(*ppSibling)->nkeys = t-1;
	pNode->nkeys = t;

	for( i=t+1; i<=2*t-1; i++ ) {
		((*ppSibling)->pair)[i-t-1] = (pNode->pair)[i];
		((*ppSibling)->child)[i-t-1] = (pNode->child)[i];
	};
	((*ppSibling)->child)[t-1] = (pNode->child)[2*t];

	for( i=0; i<=t-1; i++ ) {
		if( ((*ppSibling)->child)[i] ) {
			((*ppSibling)->child)[i]->p = *ppSibling;
		};
	};

}


/*
 *	Method:
 *	void BTree::attachSibling(Btree_Node* pNode, Btree_Node* pRightSibling)
 *
 *	Purpose: 
 *	The method takes a node and its right sibling, and merges the right sibling into the node, along with
 *	the key from the parent, parent pointers are updated accordingly.
 *	
 *	
 *	BEFORE
 *	======
 *	                            --------------------------------------------------------
 *	                              ...           k"[i]               k"[i+1] ...
 *	                            ... c"[i]                   c"[i+1]         c"[i+2] ...
 *	                            ----|-----------------------|---------------------------
 *	                              ^ |                       |  ^
 *	                              | v                       v  |
 *	------------------------------|----------             -----|----------------------------------
 *	| a                           p         |             | b  p                                 |
 *	|       k[0]     ......    k[a-1]       |             |         k'[0]  ....   k'[b-1]        |
 *	|  c[0]        .......            c[a]  |             |  c'[0]        .......         c'[b]  |
 *	---|------------------------------|------             ---|----------------------------|-------
 *	   | ^                            | ^                    | ^                          | ^  
 *	   v |                            v |                    v |                          v |  
 *	  ---|--                         ---|--                 ---|--                       ---|--
 *	  |  p |    .........            |  p |                 |  p |      ............     |  p |
 *	  ------                         ------                 ------                       ------
 *	
 *			
 *	AFTER
 *	=====
 *	                            ----------------------------------------
 *	                              ...           k"[i+1]    ...
 *	                            ... c"[i]                   c"[i+2] ...
 *	                            ----|-----------------------------------
 *	                              ^ |
 *	                              | v
 *	------------------------------|--------------------------------------------------------
 *	| a+b                         p                                                       |
 *	|       k[0]     ......          k[a-1]      k"[i]        k'[0]  ....  k'[b-1]        |
 *	|  c[0]        .......    c[a-1]        c[a]       c'[0]        .......        c'[b]  |
 *	---|----------------------|-------------|----------|----------------------------|-------
 *	   | ^                    | ^           | ^        | ^                          | ^  
 *	   v |                    v |           v |        v |                          v |  
 *	  ---|--                 ---|--        ---|--     ---|--                       ---|--
 *	  |  p |    .........    |  p |        |  p |     |  p |      ............     |  p |
 *	  ------                 ------        ------     ------                       ------
 *	
 *	                                                  \----------------------------------/
 *													     UPDATE PARENT POINTERS, IF ANY
 *
 *	Input:
 *	pNode,			given node
 *	pRightSibling,	its right sibling
 *
 *	Output:
 *	none
 */
void BTree::attachSibling(Btree_Node* pNode, Btree_Node* pRightSibling)
{
	if(NULL==pNode) throw "BTree::attachSibling, pNode is NULL";
	if(NULL==pRightSibling) throw "BTree::attachSibling, pRightSibling is NULL";
	if( pNode->nkeys + pRightSibling->nkeys > 2*t-2 ) throw "BTree::attachSibling, too many keys"; // too many keys

	Btree_Node* pParent;
	int i,j,h;

	// find the child c"[i], among the children of the parent of pNode, that is equal to pNode
	pParent = pNode->p;
	if( NULL == pParent ) { printf("cannot happen, because pNode has a sibling\n"); return; }
	for( i=0; i<pParent->nkeys; i++ ) // we do not need to check the right most child
		if( (pParent->child)[i] == pNode ) break;
	if( i == pParent->nkeys ) { printf("cannot happen, because pNode has a sibling on the right\n"); return; }

	// copy the key k"[i] into the pNode
	j = pNode->nkeys;
	(pNode->pair)[j] = (pParent->pair)[i];
	j++;

	// copy the keys and the children pointers of pRightSibling into the pNode
	for( h=0; h<pRightSibling->nkeys; h++,j++) {
		(pNode->pair)[j] = (pRightSibling->pair)[h];
		(pNode->child)[j] = (pRightSibling->child)[h];
	};
	(pNode->child)[j] = (pRightSibling->child)[h];

	// update the parent pointers of the copied children, so that they now point to pNode (except for NULL chuldren)
	for( h=0; h<=pRightSibling->nkeys; h++) {
		if( NULL != (pRightSibling->child)[h] )
			((pRightSibling->child)[h])->p = pNode;
	};

	//remove k"[i] and c"[i+1] from the parent by shifting pointers, and update the nkeys field
	for( h=i+1; h<pParent->nkeys; h++ ) {
		(pParent->pair)[h-1] = (pParent->pair)[h];
		(pParent->child)[h] = (pParent->child)[h+1];
	};
	(pParent->nkeys)--;

	//update the number of keys in pNode (as now keys from pSibling have been copied and one from the parent)
	(pNode->nkeys) += (pRightSibling->nkeys) + 1;

	//release memory occupied by pSibling
	delete pRightSibling;
}


/*
 *	Method:
 *	Btree_Node* BTree::leftSibling(Btree_Node* pNode)
 *
 *	Purpose: 
 *	Returns the left sibling of a node pNode, or NULL when such sibling does not exist.
 *
 *	Input:
 *	pNode,			given node
 *
 *	Output:
 *	its left sibling
 *	NULL,	if no left sibling
 */
Btree_Node* BTree::leftSibling(Btree_Node* pNode)
{
	if(NULL==pNode) throw "BTree::leftSibling, pNode is NULL";

	Btree_Node* pParent;
	int i;

	// if pNode has the root, then it does not have any siblings
	pParent = pNode->p;
	if( NULL == pParent )
		return NULL;

	// find a child c[i], among the children of the parent pParent, that is equal to pNode
	for( i=0; i<=pParent->nkeys; i++ ) {
		if( (pParent->child)[i] == pNode ) break;
	}
	if( i == pParent->nkeys +1 ) {printf("this cannot happen becasue pNode is a child of pParent\n"); return NULL; };

	// if pNode is the left-most child of pParent, then it does not have a left sibling
	if( i==0 )
		return NULL;
	else
		return (pParent->child)[i-1];
}


/*
 *	Method:
 *	Btree_Node* BTree::rightSibling(Btree_Node* pNode)
 *
 *	Purpose: 
 *	Returns the right sibling of a node pNode, or NULL when such sibling does not exist.
 *
 *	Input:
 *	pNode,			given node
 *
 *	Output:
 *	its right sibling
 *	NULL,	if no right sibling
 */
Btree_Node* BTree::rightSibling(Btree_Node* pNode)
{
	if(NULL==pNode) throw "BTree::rightSibling, pNode is NULL";

	Btree_Node* pParent;
	int i;

	// if pNode has the root, then it does not have any siblings
	pParent = pNode->p;
	if( NULL == pParent )
		return NULL;

	// find a child c[i], among the children of the parent pParent, that is equal to pNode
	for( i=0; i<=pParent->nkeys; i++ ) {
		if( (pParent->child)[i] == pNode ) break;
	}
	if( i == pParent->nkeys +1 ) {printf("this cannot happen becasue pNode is a child of pParent\n"); return NULL; };

	// if pNode is the right-most child of pParent, then it does not have a right sibling
	if( i == pParent->nkeys )
		return NULL;
	else
		return (pParent->child)[i+1];
}


/*
 *	Method:
 *	void BTree::insert(unsigned long key, unsigned long val)
 *
 *	Purpose: 
 *	Inserts a given key-value pair into BTree (duplicates are allowed).
 *
 *	Input:
 *	iKey,	key to be inserted
 *	iVal,	value associated with the key
 *
 *	Output:
 *	none
 */
void BTree::insert(unsigned long key, unsigned long val)
{
	Btree_Node* pLeaf;
	Btree_Node* pNode;
	Btree_Node* pSibling;
	Btree_Node* pParent;
	Btree_Pair pairUp;

	// if the tree is empty, then make the root contain a single key
	//
	//   ------------------
	//   | 1    NULL      |
	//   |      key       |
	//   | NULL      NULL |
	//   ------------------
	//
	if( NULL == root ) {
		root = new Btree_Node;
		if( NULL==root )
			throw "BTree::insert, root is NULL";
		root->p = NULL;
		root->nkeys = 1;
		(root->pair)[0].key = key;
		(root->pair)[0].val = val;
		(root->child)[0] = NULL;
		(root->child)[1] = NULL;
		return;
	};

	// descend down the tree to the leaf node where the key can be inserted
    pLeaf = root;
	int i;
	while( NULL != (pLeaf->child)[0] ) {
		// pLeaf is not a leaf
		// so find the index i of the first key that is strictly greater than key, 
		// or let i=nkeys when all are smaller or equal
		for( i=0; i<(pLeaf->nkeys); i++ )
			if( key < (pLeaf->pair)[i].key ) break;

		// now we know that
		//		when 0<=i<nkeys then key[0] <= ...  <= key[i-1] <= key < key[i]
		// and 
		//		when i=nkeys then key[0] <= ...  <= key[nkeys-1] <= key
		//
		// so we can insert the key into the subtree child[i] without violating the key order property of B-tree

		// we descend to this child
		pLeaf = (pLeaf->child)[i];
	};
	// we have found a leaf node pLeaf, such that inserting key there will not violate the key order property of B-tree

	// insert key to the leaf so that the keys are sorted
	i = (pLeaf->nkeys)-1;
	while( i>=0 && (pLeaf->pair)[i].key > key ) {
		(pLeaf->pair)[i+1] = (pLeaf->pair)[i];
		i--;
	};
	(pLeaf->pair)[i+1].key = key;
	(pLeaf->pair)[i+1].val = val;
	(pLeaf->nkeys)++;
	// the node is a leaf, so all child pointers are NULL
	(pLeaf->child)[ pLeaf->nkeys ] = NULL;

	pNode = pLeaf;
	while( 2*t == pNode->nkeys ) {
		// we have an overgrown node that stores 2t+1 pointers, so we transfer some of its keys to a sibling node

		detachSibling(pNode,&pSibling,&pairUp);

		// if pNode was the root, then create a new root, and link pNode and pSibling to it
		if( NULL == pNode->p ) {
			root = new Btree_Node;
			if( NULL==root )
				throw "BTree::insert, root is NULL 2";
			root->p = NULL;
			root->nkeys = 1;
			(root->pair)[0] = pairUp;
			(root->child)[0] = pNode;
			(root->child)[1] = pSibling;
			pNode->p = root;
			pSibling->p = root;
			return;
		};

		// so pNode was not the root, hence the parent of pNode must have pNode as one of its children
		//
		//           ...           k[i]         ...
		//        ...   c[i]=pNode      c[i+1]     ...
		//
		// we insert pSibling and keyUp next to that pointer
		//
		//           ...            k'[i]=keyUp            k'[i+1]=k[i]         ...
		//        ...   c[i]=pNode                pSibling               c'[i+2]=c[i+1]     ...
		pParent = pNode->p;
		i = pParent->nkeys;
		while( i>=0 && (pParent->child)[i] != pNode ) {
			(pParent->child)[i+1] = (pParent->child)[i];
			(pParent->pair)[i] = (pParent->pair)[i-1];
			i--;
		};
		if( i == -1 ) printf("cannot happen\n");
		(pParent->child)[i+1] = pSibling;
		(pParent->pair)[i] = pairUp;
		(pParent->nkeys) ++;

		// go up
		pNode = pNode->p;
	};
}


/*
 *	Method:
 *	void BTree::keyTransferRight(Btree_Node* pLeft, Btree_Node* pRight)
 *
 *	Purpose: 
 *	This procedure transfers the right-most key kl_2 and the child cl_3 of the left node to the right node,
 *	and updates parent key.
 *	
 *	BEFORE
 *	======
 *	                     ----------------
 *	                            k        
 *	                       left   right  
 *						 ---|------|-----
 *	                        /       \
 *	                       |         |
 *	                       v         v
 *	   ----------------------       ----------------
 *	    kl_1      kl_2      |       |       kr_0
 *	         cl_2      cl_3 |       |  cr_0
 *	   ----------------------       ----------------
 *	
 *	
 *	AFTER
 *	=====
 *	                     ----------------
 *	                           kl_2        
 *	                       left   right  
 *						 ---|------|-----
 *	                        /       \
 *	                       |         |
 *	                       v         v
 *	           --------------       ----------------
 *	              kl_1      |       |      k      kr_0
 *	                   cl_2 |       | cl_3   cr_0
 *	           --------------       ----------------
 *	
 *	This operation preserves the B-tree properties when the left child has at least t keys, 
 *	and the right child has at most 2t-2 keys -- because one key is being transferred.
 *	
 *	Input:
 *	pLeft,		left node
 *	pRight,		right node
 *
 *	Output:
 *	none
 */
void BTree::keyTransferRight(Btree_Node* pLeft, Btree_Node* pRight)
{
	if(NULL==pLeft) throw "BTree::keyTransferRight, pLeft is NULL";
	if(NULL==pRight) throw "BTree::keyTransferRight, pRight is NULL";
	if( pLeft->nkeys <= t-1 ) throw "BTree::keyTransferRight, not enough keys";	// must be enough keys left
	if( pRight->nkeys >= 2*t-1 ) throw "BTree::keyTransferRight, too many keys";	// must be free space for the incoming key

	int i,j;
	Btree_Node* pParent;

	// find the left child inside the parent's child table (pLeft cannot be the right most pointer)
	pParent = pLeft->p;
	for(i=0; i<pParent->nkeys; i++ ) {
		if( (pParent->child)[i] == pLeft ) break;
	};
	if( i==pParent->nkeys ) printf("not possible");

	// make space in the right node, by shifting keys and children one to the right
	j = pRight->nkeys;
	(pRight->child)[j+1] = (pRight->child)[j];
	for(j--;j>=0;j--) {
		(pRight->pair)[j+1] = (pRight->pair)[j];
		(pRight->child)[j+1] = (pRight->child)[j];
	};

	// place in the right
	(pRight->pair)[0] = (pParent->pair)[i];
	(pRight->child)[0] = (pLeft->child)[pLeft->nkeys];
	(pRight->nkeys)++;

	// place in the parent
	(pParent->pair)[i] = (pLeft->pair)[pLeft->nkeys-1];

	// adjust the size of the left
	(pLeft->nkeys)--;
}


/*
 *	Method:
 *	void BTree::keyTransferLeft(Btree_Node* pLeft, Btree_Node* pRight)
 *
 *	Purpose: 
 *	This is a symmetric procedure that transfers the left-most key and child of the right node to the left node, and updates parent key.
 *
 *	BEFORE
 *	======
 *	
 *	                     ----------------
 *	                            k        
 *	                       left   right  
 *						 ---|------|-----
 *	                        /       \
 *	                       |         |
 *	                       v         v
 *	   ----------------------       ----------------
 *	              kl_2      |       |       kr_0      kr_1
 *	                   cl_3 |       |  cr_0      cr_1
 *	   ----------------------       ----------------
 *	
 *	
 *	AFTER
 *	=====
 *	                     ----------------
 *	                           kr_0
 *	                       left   right  
 *						 ---|------|-----
 *	                        /       \
 *	                       |         |
 *	                       v         v
 *	           --------------       ----------------
 *	       kl_2      k      |       |      kr_1
 *	            cl_3   cr_0 |       | cr_1 
 *	           --------------       ----------------
 *	
 *	This operation preserves the B-tree properties when the right child has at least t keys, 
 *	and the left child has at most 2t-2 keys -- because one key is being transferred.
 *	
 *	Input:
 *	pLeft,		left node
 *	pRight,		right node
 *
 *	Output:
 *	none
 */
void BTree::keyTransferLeft(Btree_Node* pLeft, Btree_Node* pRight)
{
	if(NULL==pLeft) throw "BTree::keyTransferLeft, pLeft is NULL";
	if(NULL==pRight) throw "BTree::keyTransferLeft, pRight is NULL";
	if( pRight->nkeys <= t-1 ) throw "BTree::keyTransferLeft, not enough keys";	// must be enough keys right
	if( pLeft->nkeys >= 2*t-1 ) throw "BTree::keyTransferLeft, too many keys";	// must be free space for the incoming key

	int i,j;
	Btree_Node* pParent;

	// find the left child inside the parent's child table (pLeft cannot be the right most pointer)
	pParent = pLeft->p;
	for(i=0; i<pParent->nkeys; i++ ) {
		if( (pParent->child)[i] == pLeft ) break;
	};
	if( i==i<pParent->nkeys ) printf("not possible");

	// place in the left and increase its nkeys
	(pLeft->pair)[pLeft->nkeys] = (pParent->pair)[i];
	(pLeft->child)[pLeft->nkeys+1] = (pRight->child)[0];
	(pLeft->nkeys)++;

	// place in the parent
	(pParent->pair)[i] = (pRight->pair)[0];

	// move the right by one to the left
	for(j=0;j<pRight->nkeys;j++) {
		(pRight->pair)[j] = (pRight->pair)[j+1];
		(pRight->child)[j] = (pRight->child)[j+1];
	};
	(pRight->child)[j] = (pRight->child)[j+1];
	(pRight->nkeys)--;
}


/*
 *	Method:
 *	void BTree::searchKey(unsigned long key, Btree_Node** ppNode, int* pIndex) const
 *
 *	Purpose: 
 *	Searches for a given key.
 *	If the key exists in the BTree, then the procedure finds a node *ppNode that stores
 *	the key and the index *pIndex of the first occurrence of this key within the node.
 *	If the key does not exist, then the procedure returns NULL for *ppNode.
 *
 *	Input:
 *	key,		key to be searched
 *	ppNode,		node where the key occurs, if any
 *	pIndex,		index of the key within the node, if any
 *
 *	Output:
 *	none
 */
void BTree::searchKey(unsigned long key, Btree_Node** ppNode, int* pIndex) const
{
	if(NULL==ppNode) throw "BTree::searchKey, ppNode is NULL";
	if(NULL==pIndex) throw "BTree::searchKey, pIndex is NULL";

	Btree_Node* pNode;
	int i;

	pNode = root;
	while( pNode ) {

		// ckeck if the node pNode stores a key that is equal to iKey
		for(i=0; i<pNode->nkeys; i++) {
			if( (pNode->pair)[i].key == key ) {
				*ppNode = pNode;
				*pIndex = i;
				return;
			};
		};

		// now we know that no key stored at the node pNode is equal to iKey,
		// so each key is either > or < thay iKey (this strict inequality is crucial for correctness)
		// therefore, iKey is either strictly smaller than key[0], or between key[i] and key[i+1] or strictly
		// greater than key[nkeys-1]. this is what we will find out next
		i=0;
		while( i<pNode->nkeys && (pNode->pair)[i].key < key ) i++;

		// if iKey < key[0] then, by the property of B-tree, all children child[1],... have keys >=key[0], and
		// so the keys stored there are strictly greater than iKey. Hence if iKey is in the B-tree, then it
		// must be in child[0]. Similar analysis that uses transitivity holds for the other 2 cases.
		// Therefore, we can descend.

		pNode = (pNode->child)[i];
	};

	// iKey cannot possibly be in the tree
	*ppNode = NULL;
}


/*
 *	Method:
 *	bool BTree::exists(unsigned long key) const
 *
 *	Purpose: 
 *	Checks if the given key exists in the BTree.
 *
 *	Input:
 *	key,		key to be searched
 *
 *	Output:
 *	true,	if the key exists
 *	false,	otherwise
 */
bool BTree::exists(unsigned long key) const
{
	Btree_Node* pNode=NULL;
	int idx;
	searchKey(key,&pNode,&idx);
	return NULL!=pNode;
}


/*
 *	Method:
 *	unsigned long BTree::findVal(unsigned long key) const
 *
 *	Purpose: 
 *	Searches for a value associated with the given key.
 *	Throws an exception if the key does not exist.
 *
 *	Input:
 *	key,		key to be searched
 *
 *	Output:
 *	val associated with the key
 */
unsigned long BTree::findVal(unsigned long key) const
{
	Btree_Node* pNode=NULL;
	int Index;

	searchKey(key,&pNode,&Index);
	if( NULL==pNode )
		throw "BTree::findVal, key does not exist";

	return (pNode->pair)[Index].val;
}


/*
 *	Method:
 *	void BTree::removeAtLeaf( Btree_Node* pNode, int i )
 *
 *	Purpose: 
 *	Removes a key-value pair indexed by i located at a leaf node pNode.
 *
 *	Input:
 *	pNode,		leaf node
 *	i,			index of the pair within the node
 *
 *	Output:
 *	none
 */
void BTree::removeAtLeaf( Btree_Node* pNode, int i )
{
	Btree_Node* pLeftSibling;
	Btree_Node* pRightSibling;
	int h;

	// for safety
	if( NULL==pNode )
		throw "removeAtLeaf, pNode is NULL";

	// check if leaf
	if( NULL != (pNode->child)[0] )
		throw "removeAtLeaf, not a leaf";

	// check if deleting an existing key
	if( i<0 || i>=pNode->nkeys )
		throw "removeAtLeaf, i out of bound";

	// now we know that pNode is a leaf, and we must delete a key with index i

	// delete key k[i] from a leaf node pNode (shift and adjust nkeys)
	for( h=i+1; h<pNode->nkeys; h++ )
		(pNode->pair)[h-1] = (pNode->pair)[h];
	(pNode->nkeys) -- ;

	// if pNode is the root and it carries no keys any more, then set the tree to empty
	if( NULL==(pNode->p) && 0==(pNode->nkeys) ) {
		delete root;
		root = NULL;
		return;
	};

	// repeat while not root and has insufficient number of keys
	while( (pNode->nkeys) < t-1 && NULL!=pNode->p ) {
		// if the node has a sibling on the left, 
		pLeftSibling = leftSibling(pNode);
		if( NULL!=pLeftSibling ) {
			//and the sibling has at least t keys, then transfer one from the sibling, and stop propagation
			if( pLeftSibling->nkeys>=t ) {
				keyTransferRight(pLeftSibling,pNode);
				return;
			}
			else {
				// the sibling has t-1 keys, and the pNode has t-2, so merge the two nodes
				//(note that we are attaching pNode to its left sibling, not vice versa
				//we could attach the left sibling to pNode, but then we will have to 
				//write a second function Btree_attachSibling
				attachSibling(pLeftSibling,pNode);

				// after the attachemnt, the parent may have less than t-1 keys
				// set pNode to the parnet (pNode pointer is not longer valid, as it was deleted 
				// by the attachSibling procedure invoked above)
				pNode = pLeftSibling->p;

				// if pNode is the root with no keys, then set pLeftSibling as the root
				if( NULL==pNode->p && 0==pNode->nkeys ) {
					root = pLeftSibling;
					root->p = NULL;
					delete pNode;
					return;
				};
			};
		}
		else {
			// since pNode does not have a left sibling, and is not the root, then it must have a right sibling
			pRightSibling = rightSibling(pNode);
			if( NULL==pRightSibling ) {printf("not possible, must have right sibling\n"); return;}

			// if the sibling has at least t keys, then transfer one from the sibling
			if( pRightSibling->nkeys >= t ) {
				keyTransferLeft(pNode,pRightSibling);
				return;
			};

			//so the right sibling has t-1 keys and the node has t-2, so merge them
			attachSibling(pNode,pRightSibling);

			pNode = pNode->p;

			// if pNode is the root with no keys, then set its only child to be the root
			if( NULL==pNode->p && 0==pNode->nkeys ) {
				root = (pNode->child)[0];
				root->p = NULL;
				delete pNode;
				return;
			};
		};

	};
}


/*
 *	Method:
 *	bool BTree::remove(unsigned long key)
 *
 *	Purpose: 
 *	Deletes from the tree a node with the given key, if any
 *
 *	Input:
 *	key,		the key to be removed
 *
 *	Output:
 *	true,		removal succeeded (key existed)
 *	fales,		key did not exist
 */
bool BTree::remove(unsigned long key)
{
	Btree_Node* pLeaf;
	int idx;

	// find the node to be deleted
	Btree_Node* pNode=NULL;
	searchKey(key,&pNode,&idx);

	if( NULL == pNode )
		// the tree does not carry a key iKey 
		return false;

	//we do a trick so as to ensure that we delete from a leaf:
	//if we are not deleting from a leaf, then we find the successor
	//of iKey, this successor is guaranteed to be located at a leaf,
	//then we copy the successor the place where iKey is, 
	//this copying preserves the properties of B-tree,
	//and it deletes iKey (by overwriting iKey),
	//but creates a duplicate of the successor,
	//so we will need to delete the original successor (located at a leaf, as desired)
	if( NULL != (pNode->child)[0] ) {
		// pNode is not a leaf
		// go to the child on the right of the key
		// and then descend along first child
		pLeaf = (pNode->child)[idx+1];
		while( NULL != (pLeaf->child)[0] )
			pLeaf = (pLeaf->child)[0];
		(pNode->pair)[idx] = (pLeaf->pair)[0];

		// change which key is to be deleted
		pNode = pLeaf;
		idx = 0;
	};

	// now we know that pNode is a leaf, and we must delete a key with index i
	removeAtLeaf(pNode,idx);

	return true;
}


/*
 *	Method:
 *	void BTree::searchMax(Btree_Node** ppNode, int* pIdx) const
 *
 *	Purpose: 
 *	Finds the node and an index that stores the rightmost 
 *	(also the largest) key in the BTree.
 *	If tree is empty, throws an exception.
 *
 *	Input:
 *	pointers to return values;
 *
 *	Output:
 *	none
 */
void BTree::searchMax(Btree_Node** ppNode, int* pIdx) const
{
	if( NULL==ppNode )
		throw "BTree::searchMax, ppNode is NULL";
	if( NULL==pIdx)
		throw "BTree::searchMax, pIdx is NULL";
	if( NULL==root )
		throw "BTree::searchMax, BTree is empty";

	// descend to the rightmost key
	Btree_Node* node;
	node = root;
	while( NULL != (node->child)[node->nkeys] )
		node = (node->child)[node->nkeys];

	*ppNode = node;
	*pIdx = (node->nkeys) - 1;
}


/*
 *	Method:
 *	void BTree::searchMin(Btree_Node** ppNode, int* pIdx) const
 *
 *	Purpose: 
 *	Finds the node and an index that stores the leftmost 
 *	(also the smallest) key in the BTree.
 *	If tree is empty, throws an exception.
 *
 *	Input:
 *	pointers to return values;
 *
 *	Output:
 *	none
 */
void BTree::searchMin(Btree_Node** ppNode, int* pIdx) const
{
	if( NULL==ppNode )
		throw "BTree::searchMax, ppNode is NULL";
	if( NULL==pIdx)
		throw "BTree::searchMax, pIdx is NULL";
	if( NULL==root )
		throw "BTree::searchMax, BTree is empty";

	// descend to the leftmost key
	Btree_Node* node;
	node = root;
	while( NULL != (node->child)[0] )
		node = (node->child)[0];

	*ppNode = node;
	*pIdx = 0;
}


/*
 *	Method:
 *	unsigned long BTree::findMaxKey(void) const
 *
 *	Purpose: 
 *	Returns the largest key in the BTree.
 *	Throws an exception when tree is empty.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	largest key
 */
unsigned long BTree::findMaxKey(void) const
{
	Btree_Node* node=NULL;
	int idx;
	searchMax(&node,&idx);
	return (node->pair)[idx].key;
}


/*
 *	Method:
 *	unsigned long BTree::findMinKey(void) const
 *
 *	Purpose: 
 *	Returns the smallest key in the BTree.
 *	Throws an exception when tree is empty.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	smallest key
 */
unsigned long BTree::findMinKey(void) const
{
	Btree_Node* node=NULL;
	int idx;
	searchMin(&node,&idx);
	return (node->pair)[idx].key;
}


/*
 *	Method:
 *	unsigned long BTree::findValForMaxKey(void) const
 *
 *	Purpose: 
 *	Returns the value associated with a largest key 
 *	in the BTree.
 *	Throws an exception when tree is empty.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	largest key
 */
unsigned long BTree::findValForMaxKey(void) const
{
	Btree_Node* node=NULL;
	int idx;
	searchMax(&node,&idx);
	return (node->pair)[idx].val;
}


/*
 *	Method:
 *	unsigned long BTree::findValForMinKey(void) const
 *
 *	Purpose: 
 *	Returns the value associated with a smallest key 
 *	in the BTree.
 *	Throws an exception when tree is empty.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	smallest key
 */
unsigned long BTree::findValForMinKey(void) const
{
	Btree_Node* node=NULL;
	int idx;
	searchMin(&node,&idx);
	return (node->pair)[idx].val;
}


/*
 *	Method:
 *	void BTree::removeMax(void)
 *
 *	Purpose: 
 *	Removes a pair associated witht the largest key.
 *	This is the very pair that findValForMaxKey and 
 *	findMaxKey pertain to.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void BTree::removeMax(void)
{
	Btree_Node* node=NULL;
	int idx;
	searchMax(&node,&idx);
	removeAtLeaf(node,idx);
}


/*
 *	Method:
 *	void BTree::removeMin(void)
 *
 *	Purpose: 
 *	Removes a pair associated witht the smallest key.
 *	This is the very pair that findValForMinKey and 
 *	findMinKey pertain to.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void BTree::removeMin(void)
{
	Btree_Node* node=NULL;
	int idx;
	searchMin(&node,&idx);
	removeAtLeaf(node,idx);
}


/*
 *	Method:
 *	void BTree::printInd(Btree_Node* pNode, int iTab) const
 *
 *	Purpose: 
 *	Prints a node with an indention.
 *
 *	Input:
 *	pNode,	node
 *	iTab,	amount of indention
 *
 *	Output:
 *	none
 */
void BTree::printInd(Btree_Node* pNode, int iTab) const
{
	if( NULL!=pNode ) {
		for(int j=pNode->nkeys; j>0; j-- ) {
			printInd( (pNode->child)[j],iTab+3);
			for(int i=0;i<iTab;i++) printf(" ");
			printf("key %d, val %d \n",(pNode->pair)[j-1].key, (pNode->pair)[j-1].val );
		};
		printInd( (pNode->child)[0],iTab+3);
	};
}


/*
 *	Method:
 *	void BTree::print(void) const
 *
 *	Purpose: 
 *	Prints BTree.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void BTree::print(void) const
{
	printf("-----------------------\n");
	if( NULL == root )
		printf("empty\n");
	else
		printInd(root,0);
}


/*
 *	Method:
 *	void Btree_test(void)
 *
 *	Purpose: 
 *	Tests the implementation of the BTree.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Btree_test(void)
{
	BTree * bt;

	bt = new BTree();
	if( NULL==bt )
		throw "Btree_test, 1";

	bt->print();
	if( false == bt->isEmpty() )
		throw "Btree_test, 2";
	bt->insert(10,1);
	if( true == bt->isEmpty() )
		throw "Btree_test, 3";
	bt->insert(10,2);
	bt->insert(11,3);
	bt->insert(7,4);
	bt->insert(9,5);
	bt->insert(2,6);
	bt->insert(5,7);
	bt->print();
	bt->insert(0,8);
	bt->print();
	bt->insert(1,9);
	bt->print();
	bt->insert(3,10);
	bt->print();
	bt->insert(100,11);
	bt->print();
	bt->insert(50,12);
	bt->print();
	bt->insert(30,13);
	bt->print();
	bt->insert(6,14);
	bt->insert(6,15);
	bt->print();
	bt->insert(8,16);
	bt->insert(8,17);
	bt->print();
	bt->insert(4,18);
	bt->insert(5,19);
	bt->insert(5,20);

	bt->insert(20,21);
	bt->insert(15,22);
	bt->print();
	bt->insert(17,23);
	bt->print();

	if( bt->exists(120) )
		throw "Btree_test, 4";
	if( !bt->exists(20) )
		throw "Btree_test, 5";

	if( 16 != bt->findVal(8) )  // there are two such keys
		throw "Btree_test, 6";
	bt->remove(8);
	bt->print();

	bt->insert(100,12);
	bt->print();
	if( 100!=bt->findMaxKey() )
		throw "Btree_test, 7";
	if( 12!=bt->findValForMaxKey() )
		throw "Btree_test, 8";
	bt->removeMax();
	bt->insert(0,9);
	bt->print();
	if( 0!=bt->findMinKey() )
		throw "Btree_test, 9";
	if( 8!=bt->findValForMinKey() )
		throw "Btree_test, 10";
	bt->removeMin();
	bt->print();

	printf(">>>>> %d\n", bt->findMaxKey() );
	printf(">>>>> %d\n", bt->findMinKey() );
	printf(">>>>> %d\n", bt->findVal(15) );
	delete bt;
}
