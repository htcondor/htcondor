/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include "common.h"
#include "exprTree.h"
#include "sink.h"

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

	state.SetScopes( parentScope );
	return( Evaluate( state, val ) );
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
		prevScope = curScope;
		curScope  = (ClassAd*)(curScope->parentScope);
	}

	rootAd = prevScope;
}

END_NAMESPACE // classad
