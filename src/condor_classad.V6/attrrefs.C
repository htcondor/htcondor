#include "condor_common.h"
#include "caseSensitivity.h"
#include "classad.h"

static char *_FileName_ = __FILE__;

enum { EVAL_OK , EVAL_UNDEF , EVAL_ERROR };

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
setParentScope( ClassAd *parent ) 
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


void AttributeReference::
_evaluate (EvalState &state, Value &val)
{
	ExprTree	*tree, *dummy;
	ClassAd		*curAd;

	// find the expression and the evalstate
	curAd = state.curAd;
	switch( findExpr( state, tree, dummy, false ) ) {
		case EVAL_ERROR:
			val.setErrorValue();
			state.curAd = curAd;
			return;

		case EVAL_UNDEF:
			val.setUndefinedValue();
			state.curAd = curAd;
			return;

		case EVAL_OK:
			tree->evaluate( state , val );
			state.curAd = curAd;
			return;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
}


void AttributeReference::
_evaluate (EvalState &state, Value &val, ExprTree *&sig )
{
	ExprTree	*tree;
	ClassAd		*curAd;

	// find the expression and the evalstate
	curAd = state.curAd;
	switch( findExpr( state , tree , sig , true ) ) {
		case EVAL_ERROR:
			val.setErrorValue();
			state.curAd = curAd;
			return;

		case EVAL_UNDEF:
			val.setUndefinedValue();
			state.curAd = curAd;
			return;

		case EVAL_OK:
			tree->evaluate( state , val );
			state.curAd = curAd;
			return;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
}


bool AttributeReference::
_flatten( EvalState &state, Value &val, ExprTree*&ntree, OpKind*)
{
	if( absolute ) {
		ntree = copy();
		return( ntree != NULL );
	}

	evaluate( state, val );

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
	ClassAd 	*current=NULL, *parent=NULL;
	ExprTree	*tmp, *sigExpr = NULL;
	Value		val;
	bool		seen;
	int			rval = EVAL_OK;

	sig = NULL;
	state.superChase.clear( );

	// establish starting point for search
	if( expr == NULL ) {
		// "attr" and ".attr"
		current = absolute ? state.rootAd : state.curAd;
	} else {
		// "expr.attr"
		if( wantSig ) {
			expr->evaluate( state, val, sigExpr );
		} else {
			expr->evaluate( state, val );
		}
		if( val.isUndefinedValue( ) ) {
			if( wantSig ) delete sigExpr;
			return( EVAL_UNDEF );
		}
		if( !val.isClassAdValue( current ) ) {
			if( wantSig ) delete sigExpr;
			return( EVAL_ERROR );
		}
	}

	tree = current->lookup( attributeStr );

	while( !tree ) {
		// mark this scope as visited while chasing scopes
		if( state.superChase.insert( current, true ) < 0 ) {
			rval = EVAL_ERROR;
			break;
		}

		// determine parent scope
		if( ( tmp = current->lookup( "super" ) ) ) {
			// explicit "super" attribute specified
			state.curAd = current;
			tmp->evaluate( state, val );
			if( val.isUndefinedValue( ) ) {
				rval = EVAL_UNDEF;
				break;
			} else if( !val.isClassAdValue( parent ) ) {
				rval = EVAL_ERROR;
				break;
			}
		} else {
			// no explicit "super" attribute; get natural lexical parent
			parent = current->parentScope;
		}

		if( parent == NULL ) {
			// must have reached root scope
			rval = EVAL_UNDEF;
			break;
		} else {
			// continue searching from parent scope
			current = parent;
		}
		
		// have we already visited this scope?
		if( state.superChase.lookup( current, seen ) == 0 && seen ) {
			rval = EVAL_UNDEF;
			break;
		} 

		// if the attr ref is "super", just return the parent scope
		if( CLASSAD_RESERVED_STRCMP( attributeStr, "super" ) == 0 ) {
			tree = current;
			rval = EVAL_OK;
			break;
		}
			
		// continue search from this scope
		tree = current->lookup( attributeStr );
	}

	state.superChase.clear( );

	if( wantSig ) {
		if( rval != EVAL_OK ) {
			delete sigExpr;
			sig = NULL;
		} else {
			sig = new AttributeReference( sigExpr, attributeStr, absolute );
		}
	}

	state.curAd = current;
	return( rval );
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
