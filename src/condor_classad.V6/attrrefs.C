#include "condor_common.h"
#include "caseSensitivity.h"
#include "classad.h"

static char *_FileName_ = __FILE__;

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
	attributeStr = strdup( attrname );
	expr = tree;
	absolute = absolut;
}

AttributeReference::
~AttributeReference()
{
	if( attributeStr ) free( attributeStr );
	if( expr ) delete expr;
}


ExprTree *AttributeReference::
_copy( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY ) {
		AttributeReference *newTree = new AttributeReference ();
		if (newTree == 0) return NULL;

		if (attributeStr) newTree->attributeStr = strdup (attributeStr);
		newTree->attributeName = attributeName;

		if( expr && ( newTree->expr=expr->copy(EXPR_DEEP_COPY) ) == NULL ) {
			free( newTree->attributeStr );
			delete newTree;
			return NULL;
		}

		newTree->nodeKind = nodeKind;
		newTree->absolute = absolute;

		return newTree;
	} else {
		if( expr ) expr->copy( EXPR_REF_COUNT );
		return this;
	}

	// will not reach here
	return 0;	
}

void AttributeReference::
_setParentScope( ClassAd *parent ) 
{
	if( expr ) expr->setParentScope( parent );
}


bool AttributeReference::
toSink (Sink &s)
{
	if( ( absolute 	&& !s.sendToSink( (void*)".", 1 ) )	 ||
		( expr && ( !expr->toSink( s ) || !s.sendToSink( (void*) ".", 1 ) ) ) ||
		( attributeStr && 
			!s.sendToSink((void*)attributeStr,strlen(attributeStr))))
				return false;
		
	return true;
}


bool AttributeReference::
_evaluate (EvalState &state, Value &val)
{
	ExprTree	*tree, *dummy;
	ClassAd		*curAd;
	bool		rval;

	// find the expression and the evalstate
	curAd = state.curAd;
	switch( findExpr( state, tree, dummy, false ) ) {
		case EVAL_FAIL:
			return false;

		case EVAL_ERROR:
			val.setErrorValue();
			state.curAd = curAd;
			return true;

		case EVAL_UNDEF:
			val.setUndefinedValue();
			state.curAd = curAd;
			return true;

		case EVAL_OK:
			rval = tree->evaluate( state , val );
			state.curAd = curAd;
			return rval;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}


bool AttributeReference::
_evaluate (EvalState &state, Value &val, ExprTree *&sig )
{
	ExprTree	*tree;
	ClassAd		*curAd;
	bool		rval;

	// find the expression and the evalstate
	curAd = state.curAd;
	switch( findExpr( state , tree , sig , true ) ) {
		case EVAL_FAIL:
			return false;

		case EVAL_ERROR:
			val.setErrorValue();
			state.curAd = curAd;
			return true;

		case EVAL_UNDEF:
			val.setUndefinedValue();
			state.curAd = curAd;
			return true;

		case EVAL_OK:
			rval = tree->evaluate( state , val );
			state.curAd = curAd;
			return rval;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}


bool AttributeReference::
_flatten( EvalState &state, Value &val, ExprTree*&ntree, OpKind*)
{
	if( absolute ) {
		ntree = copy();
		return( ntree != NULL );
	}

	if( !evaluate( state, val ) ) {
		return false;
	}

	if( val.isClassAdValue() || val.isListValue() || val.isUndefinedValue() ) {
		ntree = copy();
		return( ntree != NULL );
	} else {
		ntree = NULL;
	}
	return true;
}


int AttributeReference::
findExpr( EvalState &state, ExprTree *&tree, ExprTree *&sig, bool wantSig )
{
	ClassAd 	*current=NULL;
	ExprTree	*sigXpr = NULL;
	Value		val;
	bool		rval;

	sig = NULL;

	// establish starting point for search
	if( expr == NULL ) {
		// "attr" and ".attr"
		current = absolute ? state.rootAd : state.curAd;
	} else {
		// "expr.attr"
		rval=wantSig?expr->evaluate(state,val,sigXpr):expr->evaluate(state,val);
		if( !rval ) {
			return( EVAL_FAIL );
		}

		if( val.isUndefinedValue( ) ) {
			sig = sigXpr;
			return( EVAL_UNDEF );
		}
		if( !val.isClassAdValue( current ) ) {
			if( wantSig ) {
				if(!(sig=new AttributeReference(sigXpr,attributeStr,absolute))){
					return( EVAL_FAIL );
				}
			}
			return( EVAL_ERROR );
		}
	}

	if( wantSig ) {
		sig = new AttributeReference( sigXpr, attributeStr, absolute );
	}

	// lookup with scope; this may side-affect state
	return( current->lookupInScope( attributeStr, tree, state ) );
}


void AttributeReference::
setReference (ExprTree *tree, char *attrStr, bool absolut)
{
	if (expr) delete expr;
	if (attributeStr) free (attributeStr);

	expr 		= tree;
	absolute 	= absolut;
	attributeStr= attrStr ? strdup(attrStr) : 0;
}
