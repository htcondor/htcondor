/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#include "common.h"
#include "exprTree.h"
#include "sink.h"

using namespace std;

BEGIN_NAMESPACE( classad )

extern int exprHash( const ExprTree* const&, int );

ExprTree::
ExprTree ()
{
	parentScope = NULL;
}


ExprTree::
~ExprTree()
{
}

ExprTree& ExprTree::operator=(const ExprTree &tree)
{
    if (this != &tree) {
        parentScope = tree.parentScope;
        nodeKind    = tree.nodeKind;
    }
    return *this;
}

bool ExprTree::
Evaluate (EvalState &state, Value &val) const
{
	return( _Evaluate( state, val ) );
}

bool ExprTree::
Evaluate( EvalState &state, Value &val, ExprTree *&sig ) const
{
	return( _Evaluate( state, val, sig ) );
}


void ExprTree::
SetParentScope( const ClassAd* scope ) 
{
	parentScope = scope;
	_SetParentScope( scope );
}


bool ExprTree::
Evaluate( Value& val ) const
{
	EvalState 	state;

	if (parentScope == NULL) {
		val.SetErrorValue();
		return false;
	} else {
		state.SetScopes( parentScope );
		return( Evaluate( state, val ) );
	}
}


bool ExprTree::
Evaluate( Value& val, ExprTree*& sig ) const
{
	EvalState 	state;

	state.SetScopes( parentScope );
	return( Evaluate( state, val, sig  ) );
}


bool ExprTree::
Flatten( Value& val, ExprTree *&tree ) const
{
	EvalState state;

	state.SetScopes( parentScope );
	return( Flatten( state, val, tree ) );
}


bool ExprTree::
Flatten( EvalState &state, Value &val, ExprTree *&tree, int* op) const
{
	return( _Flatten( state, val, tree, op ) );
}


void ExprTree::
Puke( ) const
{
	PrettyPrint	unp;
	string		buffer;

	unp.Unparse( buffer, (ExprTree*)this );
	printf( "%s\n", buffer.c_str( ) );
}


int 
exprHash( const ExprTree* const& expr, int numBkts ) 
{
	unsigned char *ptr = (unsigned char*) &expr;
	int	result = 0;
	
	for( int i = 0 ; (unsigned)i < sizeof( expr ) ; i++ ) result += ptr[i];
	return( result % numBkts );
}



EvalState::
EvalState( )
{
	rootAd = NULL;
	curAd  = NULL;

	flattenAndInline = false;	// NAC
}

EvalState::
~EvalState( )
{
	/*
	classad_hash_map< const ExprTree*, Value, ExprHash >::iterator i;

	for (i = cache_to_delete.begin(); i != cache_to_delete.end(); i++) {
		const ExprTree *tree = i->first;
		fprintf(stderr, "**** Deleting tree: %x\n", tree);
		delete tree;
	}

	*/	
	return;
}

void EvalState::
SetScopes( const ClassAd *curScope )
{
	curAd = (ClassAd*)curScope;
	SetRootScope( );
}


void EvalState::
SetRootScope( )
{
	ClassAd *prevScope = curAd, *curScope = (ClassAd*)(curAd->parentScope);

	while( curScope ) {
		if( curScope == curAd ) {	// NAC - loop detection
			return;					// NAC
		}							// NAC
		prevScope = curScope;
		curScope  = (ClassAd*)(curScope->parentScope);
	}

	rootAd = prevScope;
}

ostream& operator<<(ostream &stream, const ExprTree &expr)
{
	PrettyPrint unparser;
	string      string_representation;

	unparser.Unparse(string_representation, &expr);
	stream << string_representation;
	
	return stream;
}

bool operator==(const ExprTree &tree1, const ExprTree &tree2)
{
    return tree1.SameAs(&tree2);
}

END_NAMESPACE // classad
