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

#ifndef GM_TRIE_H
#define GM_TRIE_H

/*
 *	FILE CONTENT:
 *	Declaration of a trie data structure class.
 *	A trie stores strings and their identifiers. Each string must be 
 *	non-empty, and each identifier must be non-negative.
 *	We can find the identifier of any string stored in the trie,
 *	or report that there is no such string.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/3/05	--	coding and testing finished
 *	9/16/05	--	added "const" in add and find argument list
 */


struct Trie_Node
{
	char		letter;
	int			id;
	Trie_Node	*child;
	Trie_Node	*sibling;
};


class Trie
{

private:

	Trie_Node*			root;	// root node of the trie, NULL if empty

	void removeRecursive(Trie_Node *node );
	void print(Trie_Node * node, int indent) const;

public:

	Trie(void);
	~Trie(void);
	void add( const char *string, int id );
	int find( const char *string ) const;
	void print(void) const;
};

void Trie_test(void);

#endif
