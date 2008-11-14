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
 *	Implementation of the Trie class
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/7/05	--	coding and testing finished
 */


/*
 *	Method:
 *	Trie::Trie(void)
 *
 *	Purpose:
 *	Constructor that creates an empty trie. An empty trie contains
 *	a string 'a' with identifier -1, meaning that the string
 *	does not actually exist. By assumption any identifier of any
 *	string stored in the trie must be non-negative.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
Trie::Trie(void)
{
	root = new Trie_Node;
	if( NULL==root )
		throw "Trie::Trie, root is NULL";

	root->letter = 'a';
	root->id = -1; // means that the string does not actually exist
	root->child = NULL;
	root->sibling = NULL;
}


/*
 *	Method:
 *	Trie::~Trie(void)
 *
 *	Purpose:
 *	Destructor of the trie. Recursively frees all memory starting from
 *	the root of the trie.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
Trie::~Trie(void)
{
	removeRecursive(root);
	root = NULL;
}


/*
 *	Method:
 *	void Trie::removeRecursive( Trie_Node *node )
 *
 *	Purpose:
 *	Recursive removal, to be used by the destructor.
 *
 *	Input:
 *	as described
 *
 *	Output:
 *	none
 */
void Trie::removeRecursive( Trie_Node *node )
{
	Trie_Node *temp;

	while( NULL != node ) {
		removeRecursive( node->child );
		temp = node;
		node = node->sibling;
		delete temp;
	};
}


/*
 *	Method:
 *	void Trie::add( const char *string, int id )
 *
 *	Purpose:
 *	Adds a given string to the trie, and assigns the string the given id.
 *	The id must be non-negative, it is not allowed to add a string that
 *	already exists in the trie.
 *
 *	Input:
 *	string
 *	identifier to be assigned
 *
 *	Output:
 *	none
 */
void Trie::add( const char *string, int id )
{
	Trie_Node *node;
	int i;

	// we do not accept empty strings
	if( NULL==string || '\0'==string[0] )
		throw "Trie::add, null or empty string";

	// check if id is negative
	if( id<0 )
		throw "Trie::add, negative id";

	//descend down the trie
	i=0;
	node = root;
	while( true ) { 

		// just in case someone forgets the ending zero
		if( i>10000 )
			throw "Trie::add, lack of zero?";

		// check if the letter is on the list (none is nil)
		Trie_Node *temp = node;
		bool found = false;
		while( true ) {
			if( temp->letter == string[i] ) {
				found = true;
				break;
			};
			if( NULL != temp->sibling )
				temp = temp->sibling;
			else
				break;
		};

		if( found ) {

			// if this is the last letter then add the string by assigning the given ID
			if( '\0' == string[i+1] ) {

				// check if we have already added such string
				if( -1 != temp->id ) {
					throw "Trie::add, string already exists\n";
				}
				else {
					temp->id = id;
					break;
				};
			}
			//so it is not the last letter, and so there are other letters to look for
			else {

				// descend if possible and take the next letter
				if( NULL != temp->child ) {
					node = temp->child;
					i++;
				}
				else {
					// not possible to descend, so append a chain of remaining letters of the string

					while( '\0' != string[i+1] ) {
						i++;
						temp->child = new Trie_Node;
						temp = temp->child;
						temp->child = NULL;
						temp->id = -1;
						temp->letter = string[i];
						temp->sibling = NULL;
					};
					temp->id = id;
					break;
				};
                
			};

		}
		else {
			// the letter was not found on the list
			// so temp points to the last Trie_Node on the list

			// so add a new letter on the list
			temp->sibling = new Trie_Node;
			temp = temp->sibling;
			temp->child = NULL;
			temp->id = -1;
			temp->letter = string[i];
			temp->sibling = NULL;
			i++;

			// so append a chain of remaining letters of the string
			while( '\0' != string[i] ) {
				temp->child = new Trie_Node;
				temp = temp->child;
				temp->child = NULL;
				temp->id = -1;
				temp->letter = string[i];
				temp->sibling = NULL;
				i++;
			};
			temp->id = id;
			break;
		};

	};
}


/*
 *	Method:
 *	int Trie::find( const char *string ) const
 *
 *	Purpose:
 *	Finds the identifier of the given string in the trie.
 *	If not found, returns -1 (recall that any id is non-negative).
 *
 *	Input:
 *	string
 *
 *	Output:
 *	id of the string
 *	-1,	if string does not exist in the trie
 */
int Trie::find( const char *string ) const
{
	Trie_Node *node;
	int i;

	// we do not accept empty strings
	if( NULL==string || '\0'==string[0] )
		throw "Trie::find, null or empty string";

	//descend down the trie
	i=0;
	node = root;
	while( true ) { 

		// just in case someone forgets the ending zero
		if( i>10000 )
			throw "Trie::find, lack of zero?";

		// check if the letter is on the list (none is nil)
		Trie_Node *temp = node;
		bool found = false;
		while( true ) {
			if( temp->letter == string[i] ) {
				found = true;
				break;
			};
			if( NULL != temp->sibling )
				temp = temp->sibling;
			else
				break;
		};

		if( !found )
			return -1;
		else {
			// letter has been found

			// if this is the last letter, then check if ID is assigned
			if( '\0' == string[i+1] ) {
				return temp->id;
			}
			else {
				// so not last letter, if there is not way to descend, then return not found
				if( NULL == temp->child ) 
					return -1;
				else {
					// so we can descend, and we do 
					node = temp->child;
					i++;
				};
			};
		};
	};
	return -1;
}


/*
 *	Method:
 *	void Trie::print(void) const
 *
 *	Purpose:
 *	Prints the content of the trie.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Trie::print(void) const
{
	print( root, 0 );
}


/*
 *	Method:
 *	void Trie::print(Trie_Node * node, int indent) const
 *
 *	Purpose:
 *	Prints the content of the trie starting at the given node
 *	and with given indention.
 *
 *	Input:
 *	node
 *	indention
 *
 *	Output:
 *	none
 */
void Trie::print(Trie_Node * node, int indent) const
{
	if( NULL!=node ) {
		while( NULL != node ) {
			for(int i=0; i<indent; i++ )
				printf(" ");
			printf("l: %c,  id: %d\n", node->letter, node->id );
			print( node->child, indent+4);
			node = node->sibling;
		};
	};
}


/*
 *	Method:
 *	void Trie_test(void)
 *
 *	Purpose:
 *	Tests the operation of the trie.
 *
 *	Input:
 *	none
 *
 *	Output:
 *	none
 */
void Trie_test(void)
{
	Trie t;

	t.add("ab",0);
	t.print();
	printf("===============\n");
	t.add("a",2);
	t.print();
	printf("===============\n");
	t.add("abcde",3);
	t.print();
	printf("===============\n");
	t.add("z",4);
	t.print();
	printf("===============\n");
	t.add("ze",5);
	t.print();
	printf("===============\n");
	t.add("abd",6);
	t.print();
	printf("===============\n");
	t.add("abcf",7);
	t.print();
	printf("===============\n");
	if( -1!=t.find("c") )
		throw "Trie_test, 1";
	if( -1!=t.find("abe") )
		throw "Trie_test, 2";
	if( -1!=t.find("abcdef") )
		throw "Trie_test, 3";
	if( 2!=t.find("a") )
		throw "Trie_test, 4";
	if( 7!=t.find("abcf") )
		throw "Trie_test, 5";
	printf("must be 5 error messages -- ok\n");
	try {
		t.add(NULL,1);
	} catch( char* msg ) {
		alert(msg);
	};
	try {
		t.add("",1);
	} catch( char* msg ) {
		alert(msg);
	};
	try {
		t.add("aa",-1);
	} catch( char* msg ) {
		alert(msg);
	};
	try {
		t.find(NULL);
	} catch( char* msg ) {
		alert(msg);
	};
	try {
		t.find("");
	} catch( char* msg ) {
		alert(msg);
	};
}

