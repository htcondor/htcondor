#include "condor_common.h"
#include "classad.h"

static char *_FileName_ = __FILE__;

BEGIN_NAMESPACE( classad )

AttributeReference::
AttributeReference()
{
	nodeKind = ATTRREF_NODE;
	expr = NULL;
	attributeStr = NULL;
	absolute = false;
}

// a private ctor for use in significant expr identification
AttributeReference::
AttributeReference( ExprTree *tree, char *attrname, bool absolut )
{
	nodeKind = ATTRREF_NODE;
	attributeStr = strnewp( attrname );
	expr = tree;
	absolute = absolut;
}

AttributeReference::
~AttributeReference()
{
	if( attributeStr ) delete []( attributeStr );
	if( expr ) delete expr;
}


AttributeReference *AttributeReference::
Copy( )
{
	AttributeReference *newTree = new AttributeReference ();
	if (newTree == 0) return NULL;

	if (attributeStr) newTree->attributeStr = strnewp( attributeStr );
	newTree->attributeName = attributeName;

	if( expr && ( newTree->expr=expr->Copy( ) ) == NULL ) {
		delete []( newTree->attributeStr );
		delete newTree;
		return NULL;
	}

	newTree->nodeKind = nodeKind;
	newTree->parentScope = parentScope;
	newTree->absolute = absolute;

	return newTree;
}

void AttributeReference::
_SetParentScope( ClassAd *parent ) 
{
	if( expr ) expr->SetParentScope( parent );
}


bool AttributeReference::
ToSink (Sink &s)
{
	if( ( absolute 	&& !s.SendToSink( (void*)".", 1 ) )	 || ( expr 
			&& ( !expr->ToSink( s ) || !s.SendToSink( (void*) ".", 1 ) ) ) ||
		( attributeStr && 
			!s.SendToSink((void*)attributeStr,strlen(attributeStr))))
				return false;
		
	return true;
}


bool AttributeReference::
_Evaluate (EvalState &state, Value &val)
{
	ExprTree	*tree, *dummy;
	ClassAd		*curAd;
	bool		rval;

	// find the expression and the evalstate
	curAd = state.curAd;
	switch( FindExpr( state, tree, dummy, false ) ) {
		case EVAL_FAIL:
			return false;

		case EVAL_ERROR:
		case PROP_ERROR:
			val.SetErrorValue();
			state.curAd = curAd;
			return true;

		case EVAL_UNDEF:
		case PROP_UNDEF:
			val.SetUndefinedValue();
			state.curAd = curAd;
			return true;

		case EVAL_OK:
			rval = tree->Evaluate( state , val );
			state.curAd = curAd;
			return rval;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}


bool AttributeReference::
_Evaluate (EvalState &state, Value &val, ExprTree *&sig )
{
	ExprTree	*tree, *exprSig;
	ClassAd		*curAd;
	bool		rval;

	curAd = state.curAd;
	exprSig = NULL;
	rval 	= true;

	switch( FindExpr( state , tree , exprSig , true ) ) {
		case EVAL_FAIL:
			rval = false;
			break;

		case EVAL_ERROR:
		case PROP_ERROR:
			val.SetErrorValue( );
			break;

		case EVAL_UNDEF:
		case PROP_UNDEF:
			val.SetUndefinedValue( );
			break;

		case EVAL_OK:
			rval = tree->Evaluate( state, val );
			break;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	if(!rval || !(sig=new AttributeReference(exprSig,attributeStr,absolute))){
		delete exprSig;
		sig = NULL;
		return( false );
	}
	state.curAd = curAd;
	return rval;
}


bool AttributeReference::
_Flatten( EvalState &state, Value &val, ExprTree*&ntree, OpKind*)
{
	if( absolute ) {
		ntree = Copy();
		return( ntree != NULL );
	}

	if( !Evaluate( state, val ) ) {
		return false;
	}

	if( val.IsClassAdValue() || val.IsListValue() || val.IsUndefinedValue() ) {
		ntree = Copy();
		return( ntree != NULL );
	} else {
		ntree = NULL;
	}
	return true;
}


int AttributeReference::
FindExpr( EvalState &state, ExprTree *&tree, ExprTree *&sig, bool wantSig )
{
	ClassAd 	*current=NULL;
	Value		val;
	bool		rval;

	sig = NULL;

	// establish starting point for search
	if( expr == NULL ) {
		// "attr" and ".attr"
		current = absolute ? state.rootAd : state.curAd;
	} else {
		// "expr.attr"
		rval=wantSig?expr->Evaluate(state,val,sig):expr->Evaluate(state,val);
		if( !rval ) {
			return( EVAL_FAIL );
		}

		if( val.IsUndefinedValue( ) ) {
			return( PROP_UNDEF );
		} else if( val.IsErrorValue( ) ) {
			return( PROP_ERROR );
		}

		if( !val.IsClassAdValue( current ) ) {
			return( EVAL_ERROR );
		}
	}

	// lookup with scope; this may side-affect state
	return( current->LookupInScope( attributeStr, tree, state ) );
}


void AttributeReference::
SetReference (ExprTree *tree, char *attrStr, bool absolut)
{
	if (expr) delete expr;
	if (attributeStr) delete [] (attributeStr);

	expr 		= tree;
	absolute 	= absolut;
	attributeStr= attrStr ? strnewp(attrStr) : 0;
}

END_NAMESPACE // classad
