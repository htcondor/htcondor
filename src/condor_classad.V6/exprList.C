#include "condor_common.h"
#include "exprTree.h"

ExprList::
ExprList()
{
	nodeKind = EXPR_LIST_NODE;
}


ExprList::
~ExprList()
{
	Clear( );
}


void ExprList::
Clear ()
{
	ExprTree *tree;

	exprList.Rewind();
	while ((tree = exprList.Next())) {
		delete tree;
		exprList.DeleteCurrent ();
	}
}

	
ExprList *ExprList::
Copy( )
{
	ExprList *newList = new ExprList;
	ExprTree *newTree, *tree;

	if (newList == 0) return 0;
	newList->parentScope = parentScope;

	exprList.Rewind();
	while ((tree = exprList.Next())) {
		newTree = tree->Copy( );
		if (newTree) {
			newList->AppendExpression( newTree );
		} else {
			delete newList;
			return 0;
		}
	}
	return newList;
}


void ExprList::
_SetParentScope( ClassAd *parent )
{
	ExprTree *tree;
	exprList.Rewind( );
	while( ( tree = exprList.Next( ) ) ) {
		tree->SetParentScope( parent );
	}
}

bool ExprList::
ToSink( Sink &s )
{
	ExprTree *tree;
	bool 	wantIndent;
	int		indentLength;
	int		indentLevel;
	FormatOptions *p = s.GetFormatOptions( );

	if( p ) {
		p->GetListIndentation( wantIndent, indentLength );
		indentLevel = p->GetIndentLevel( );
	} else {
		wantIndent = false;
	}

	if( wantIndent ) {
		if( !s.SendToSink( (void*)"\n", 1 ) || !p->Indent( s ) ) return false;
	}
	if (!s.SendToSink ((void*)"{", 1)) return false;

	if( wantIndent ) {
		if( !s.SendToSink( (void*)"\n", 1 ) ) return false;
		p->SetIndentLevel( indentLevel+indentLength );
	}

	exprList.Rewind();
	while ((tree = exprList.Next())) {
		if( wantIndent && !p->Indent( s ) ) return false;
		if (!tree->ToSink(s)) return false;
		if (!exprList.AtEnd()) {
			if (!s.SendToSink ((void*)",", 1)) return false;
			if( wantIndent && !s.SendToSink( (void*)"\n", 1 ) ) return false;
		}
	}

	if( wantIndent ) {
		p->SetIndentLevel( indentLevel );
		if( !s.SendToSink( (void*)"\n", 1 ) || !p->Indent( s ) ) return false;
	}

	if (!s.SendToSink ((void*)"}", 1)) return false;

	return true;
}


int ExprList::
Number()
{
	return exprList.Number();
}


void ExprList::
Rewind( )
{
	exprList.Rewind( );
}

ExprTree *ExprList::
Next( )
{
	return( exprList.Next( ) );
}

void ExprList::
AppendExpression (ExprTree *tree)
{
	exprList.Append (tree);
}


bool ExprList::
_Evaluate (EvalState &, Value &val)
{
	val.SetListValue (this);
	return( true );
}

bool ExprList::
_Evaluate( EvalState &, Value &val, ExprTree *&sig )
{
	val.SetListValue( this );
	return( ( sig = Copy( ) ) );
}

bool ExprList::
_Flatten( EvalState &state, Value &, ExprTree *&tree, OpKind* )
{
	ExprTree	*expr, *nexpr;
	Value		tempVal;
	ExprList	*newList;

	if( ( newList = new ExprList( ) ) == NULL ) return false;

	exprList.Rewind();
	while( ( expr = exprList.Next() ) ) {
		// flatten the constituent expression
		if( !expr->Flatten( state, tempVal, nexpr ) ) {
			delete newList;
			tree = NULL;
			return false;
		}

		// if only a value was obtained, convert to an expression
		if( !nexpr ) {
			nexpr = Literal::MakeLiteral( tempVal );
			if( !nexpr ) {
				delete newList;
				return false;
			}
		}
		// add the new expression to the flattened list
		newList->AppendExpression( nexpr );
	}

	tree = newList;

	return true;
}


// Iterator implementation
ExprListIterator::
ExprListIterator( )
{
}


ExprListIterator::
ExprListIterator( ExprList* l )
{
	Initialize( l );
}


ExprListIterator::
~ExprListIterator( )
{
}


void ExprListIterator::
Initialize( const ExprList *l )
{
	// initialize eval state
	state.cache.clear( );
	state.curAd = l->parentScope;
	state.SetRootScope( );

	// initialize list iterator
	iter.Initialize( l->exprList );
}


void ExprListIterator::
ToBeforeFirst( )
{
	iter.ToBeforeFirst( );
}


void ExprListIterator::
ToAfterLast( )
{
	iter.ToAfterLast( );
}


bool ExprListIterator::
ToNth( int n ) 
{
	ToBeforeFirst( );
	while( n-- ) {
		iter.Next( );
	}
	return( iter.Next( ) != NULL );
}


ExprTree* ExprListIterator::
NextExpr( )
{
	return( iter.Next( ) );
}


ExprTree* ExprListIterator::
CurrentExpr( )
{
	return( iter.Current( ) );
}


ExprTree* ExprListIterator::
PrevExpr( )
{
	return( iter.Prev( ) );
}


bool ExprListIterator::
NextValue( Value& val, EvalState *es )
{
	ExprTree *tree = NextExpr( );

	if( !tree ) return false;
	tree->Evaluate( es?*es:state, val );
	return true;
}


bool ExprListIterator::
CurrentValue( Value& val, EvalState *es )
{
	ExprTree *tree = CurrentExpr( );

	if( !tree ) return false;
	tree->Evaluate( es?*es:state, val );
	return true;
}


bool ExprListIterator::
PrevValue( Value& val, EvalState *es )
{
	ExprTree *tree = PrevExpr( );

	if( !tree ) return false;
	tree->Evaluate( es?*es:state, val );
	return true;
}


bool ExprListIterator::
NextValue( Value& val, ExprTree*& sig, EvalState *es )
{
	ExprTree *tree = NextExpr( );

	if( !tree ) return false;
	tree->Evaluate( es?*es:state, val, sig );
	return true;
}


bool ExprListIterator::
CurrentValue( Value& val, ExprTree*& sig, EvalState *es )
{
	ExprTree *tree = CurrentExpr( );

	if( !tree ) return false;
	tree->Evaluate( es?*es:state, val, sig );
	return true;
}


bool ExprListIterator::
PrevValue( Value& val, ExprTree*& sig, EvalState *es )
{
	ExprTree *tree = PrevExpr( );

	if( !tree ) return false;
	tree->Evaluate( es?*es:state, val, sig );
	return true;
}


bool ExprListIterator::
IsBeforeFirst( )
{
	return( iter.IsBeforeFirst( ) );
}


bool ExprListIterator::
IsAfterLast( )
{
	return( iter.IsAfterLast( ) );
}

