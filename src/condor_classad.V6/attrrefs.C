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


AttributeReference::
~AttributeReference()
{
	if( attributeStr ) free( attributeStr );
	if( expr ) delete expr;
}


ExprTree *AttributeReference::
copy (CopyMode)
{
	AttributeReference *newTree = new AttributeReference ();
	if (newTree == 0) return NULL;

	if (attributeStr) newTree->attributeStr = strdup (attributeStr);
	newTree->attributeName = attributeName;

	if( expr && ( newTree->expr = expr->copy() ) == NULL )	{
		free( newTree->attributeStr );
		delete newTree;
		return NULL;
	}

	newTree->nodeKind = nodeKind;
	newTree->absolute = absolute;

	return newTree;
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
_evaluate (EvalState &state, EvalValue &val)
{
	ExprTree	*tree;
	EvalState	nextState;

	// find the expression and the evalstate
	switch( findExpr( state , expr , tree , nextState ) ) {
		case EVAL_ERROR:
			val.setErrorValue();
			return;

		case EVAL_UNDEF:
			val.setUndefinedValue();
			return;

		case EVAL_OK:
			tree->evaluate( nextState , val );
			return;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
}


int AttributeReference::
findExpr( EvalState curState, ExprTree *expr, ExprTree *&tree, 
			EvalState &nextState )
{
	EvalState	parentState;

	if( curState.curAd == NULL ) {
		tree = NULL;
		return EVAL_UNDEF;
	}

	if( expr == NULL && !absolute ) {	
		// case 1:  attr
		if( CLASSAD_RESERVED_STRCMP( attributeStr , "super" ) == 0 ) {
			// 1.0:  The implicit "super" attribute
			if( curState.curAd == curState.rootAd ) {
				// the root ad has no "super"
				tree = NULL;
				return EVAL_UNDEF;
			}
			tree = curState.curAd->parentScope;
			nextState.curAd = curState.curAd->parentScope;
			nextState.rootAd = curState.rootAd;
			return EVAL_OK;
		}
		if( ( tree = curState.curAd->lookup( attributeStr ) ) ) {
			// 1.1:  In current scope
			nextState = curState;
			return EVAL_OK;
		} else {
			// 1.2:  In closest enclosing scope
			if( curState.curAd == curState.rootAd ) {
				// Assume curAd is the root ad; so undefined
				tree = NULL;
				return EVAL_UNDEF;
			}
			parentState.curAd  = curState.curAd->parentScope;
			parentState.rootAd = curState.rootAd;
			return findExpr( parentState , expr , tree , nextState );
		}
	} else
	if( expr == NULL && absolute ) {
		// case 2:  .attr
		if( CLASSAD_RESERVED_STRCMP( attributeStr , "super" ) == 0 ) {
			// 2.0:  The implicit "super" attribute (doesn't exist for root)
			tree = NULL;
			return EVAL_UNDEF;
		}
		if( ( curState.rootAd != NULL ) && 
			( tree = curState.rootAd->lookup( attributeStr ) ) ) {
			// 2.1:  In root scope
			nextState.curAd  = curState.rootAd;
			nextState.rootAd = curState.rootAd;
			return EVAL_OK;
		} else {
			// 2.2:  Not in root scope; (undefined)
			tree = NULL;
			return EVAL_UNDEF;
		}
	} else {
		// case 3:  expr.attr
		EvalValue	adv;
		EvalState	intermState;
		ClassAd		*ad;

		expr->evaluate( curState , adv );
		if( adv.isClassAdValue( ad ) ) {
			// 3.1:  Evaluated expression is a classad
			intermState.curAd  = ad;
			intermState.rootAd = curState.rootAd;
			return findExpr( intermState , NULL , tree , nextState );
		} else {
			// 3.2:  Evaluated expression is not a classad; (error)
			tree = NULL;
			return EVAL_ERROR;
		}
	}
}


void AttributeReference::
setParentScope( ClassAd *ad )
{
	if( expr ) expr->setParentScope( ad );
}

void AttributeReference::
setReference (ExprTree *tree, char *attrStr, bool absolut)
{
	if (expr) delete expr;
	if (attributeStr) free (attributeStr);

	expr 		= tree;
	absolute 	= absolut;
	attributeStr= attrStr ? strdup(attrStr) : NULL;
}
