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


#include "classad/common.h"
#include "classad/exprTree.h"
#include "classad/sink.h"

using namespace std;

BEGIN_NAMESPACE( classad )

extern int exprHash( const ExprTree* const&, int );

static const int MAX_CLASSAD_RECURSION = 1000;

ExprTree::
ExprTree ()
{
	parentScope = NULL;
}


ExprTree::
~ExprTree()
{
}

void ExprTree::
CopyFrom(const ExprTree &tree)
{
    if (this != &tree) {
        parentScope = tree.parentScope;
        nodeKind    = tree.nodeKind;
    }
    return;
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

	depth_remaining = MAX_CLASSAD_RECURSION;
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
	ClassAd *prevScope = curAd;
    if (curAd == NULL) {
        rootAd = NULL;
    } else {
        ClassAd *curScope = (ClassAd*)(curAd->parentScope);
        
        while( curScope ) {
            if( curScope == curAd ) {	// NAC - loop detection
                return;					// NAC
            }							// NAC
            prevScope = curScope;
            curScope  = (ClassAd*)(curScope->parentScope);
        }
        
        rootAd = prevScope;
    }
    return;
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
