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
#include "classad/classad.h"

namespace classad {

// a private ctor for use in significant expr identification
AttributeReference::
AttributeReference( ExprTree *tree, const std::string &attrname, bool absolut )
{
	parentScope = NULL;
	attributeStr = attrname;
	expr = tree;
	absolute = absolut;
}

AttributeReference::
AttributeReference(const AttributeReference &ref) : ExprTree()
{
    CopyFrom(ref);
    return;
}

AttributeReference::
~AttributeReference()
{
	if( expr ) delete expr;
}

AttributeReference &AttributeReference::
operator=(const AttributeReference &ref)
{
    if (this != &ref) {
        CopyFrom(ref);
    }
    return *this;
}


ExprTree *AttributeReference::
Copy( ) const
{
	AttributeReference *newTree = new AttributeReference ();
	if (newTree == 0) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return NULL;
	}

    if (!newTree->CopyFrom(*this)) {
        delete newTree;
        newTree = NULL;
    }

	return newTree;
}

bool AttributeReference::
CopyFrom(const AttributeReference &ref)
{
    bool success;

    success = true;

	parentScope = ref.parentScope;
	attributeStr = ref.attributeStr;
	if( ref.expr && ( expr=ref.expr->Copy( ) ) == NULL ) {
        success = false;
	} else {
        ExprTree::CopyFrom(ref);
        absolute = ref.absolute;
    }
    return success;
}

bool AttributeReference::SetComponents( ExprTree *tree, const std::string &attr, bool abs )
{
	if (tree != expr) {
		if (expr) delete expr;
		expr = tree;
	}
	attributeStr = attr;
	absolute = abs;
	return true;
}


bool AttributeReference::
SameAs(const ExprTree *tree) const
{
    bool is_same;

    const ExprTree * pSelfTree= tree->self();
    
    if (this == pSelfTree) {
        is_same = true;
    } else if (pSelfTree->GetKind() != ATTRREF_NODE) {
        is_same = false;
    } else {
        const AttributeReference *other_ref = (const AttributeReference *) pSelfTree;
        
        if (   absolute     != other_ref->absolute
            || attributeStr != other_ref->attributeStr) {
            is_same = false;
        } else if (    (expr == NULL && other_ref->expr == NULL)
                    || (expr == other_ref->expr)
                    || (expr != NULL && other_ref->expr != NULL && expr->SameAs(other_ref->expr))) {
            // Will this check result in infinite recursion? How do I stop it? 
            is_same = true;
        } else {
            is_same = false;
        }
    }

    return is_same;
}

bool 
operator==(const AttributeReference &ref1, const AttributeReference &ref2)
{
    return ref1.SameAs(&ref2);
}

void AttributeReference::
_SetParentScope( const ClassAd *parent ) 
{
	parentScope = parent;
	if( expr ) expr->SetParentScope( parent );
}


void AttributeReference::
GetComponents( ExprTree *&tree, std::string &attr, bool &abs ) const
{
	tree = expr;
	attr = attributeStr;
	abs = absolute;
}


bool AttributeReference::
_Evaluate (EvalState &state, Value &val) const
{
	ExprTree	*tree; 
	ExprTree	*dummy;
	const ClassAd *curAd;
	bool		rval;

	// find the expression and the evalstate
	curAd = state.curAd;
	switch( FindExpr( state, tree, dummy, false ) ) {
		case EVAL_FAIL:
			return false;

		case EVAL_ERROR:
			val.SetErrorValue();
			state.curAd = curAd;
			return true;

		case EVAL_UNDEF:
			val.SetUndefinedValue();
			state.curAd = curAd;
			return true;

		case EVAL_OK:
		{
			if( state.depth_remaining <= 0 ) {
				val.SetErrorValue();
				state.curAd = curAd;
				return false;
			}

			// Circular-reference detection. If we are already evaluating
			// this same target expression further up the stack, then the
			// attribute refers (directly or transitively) to itself. Treat
			// that as undefined rather than recursing into it again -- a
			// cycle reachable through more than one operand would otherwise
			// blow up exponentially before depth_remaining is exhausted.
			for( const ExprTree *active : state.eval_stack ) {
				if( active == tree ) {
					val.SetUndefinedValue();
					state.curAd = curAd;
					return true;
				}
			}

			state.eval_stack.push_back( tree );
			state.depth_remaining--;

			rval = tree->Evaluate( state, val );

			state.depth_remaining++;
			state.eval_stack.pop_back();

			state.curAd = curAd;

			return rval;
		}
		default:  CLASSAD_EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}


bool AttributeReference::
_Evaluate (EvalState &state, Value &val, ExprTree *&sig ) const
{
	ExprTree	*tree;
	ExprTree	*exprSig;
	const ClassAd *curAd;
	bool		rval;

	curAd = state.curAd;
	exprSig = NULL;
	rval 	= true;

	switch( FindExpr( state , tree , exprSig , true ) ) {
		case EVAL_FAIL:
			rval = false;
			break;

		case EVAL_ERROR:
			val.SetErrorValue( );
			break;

		case EVAL_UNDEF:
			val.SetUndefinedValue( );
			break;

		case EVAL_OK:
		{
			if( state.depth_remaining <= 0 ) {
				val.SetErrorValue();
				state.curAd = curAd;
				return false;
			}
			state.depth_remaining--;

			rval = tree->Evaluate( state, val );

			state.depth_remaining++;

			break;
		}
		default:  CLASSAD_EXCEPT( "ClassAd:  Should not reach here" );
	}
	if(!rval || !(sig=new AttributeReference(exprSig,attributeStr,absolute))){
		if( rval ) {
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
		}
		delete exprSig;
		sig = NULL;
		return( false );
	}
	state.curAd = curAd;
	return rval;
}


bool AttributeReference::
_Flatten( EvalState &state, Value &val, ExprTree*&ntree, int*) const
{
	ExprTree	*tree;
	ExprTree	*dummy;
	const ClassAd *curAd;
	bool		rval;

	ntree = NULL; // Just to be safe...  wenger 2003-12-11.

		// find the expression and the evalstate
	curAd = state.curAd;
	switch( FindExpr( state, tree, dummy, false ) ) {
		case EVAL_FAIL:
			return false;

		case EVAL_ERROR:
			val.SetErrorValue();
			state.curAd = curAd;
			return true;

		case EVAL_UNDEF:
			if( expr && state.flattenAndInline ) {
				ExprTree *expr_ntree = NULL;
				Value expr_val;
				if( state.depth_remaining <= 0 ) {
					val.SetErrorValue();
					state.curAd = curAd;
					return false;
				}
				state.depth_remaining--;

				rval = expr->Flatten(state,expr_val,expr_ntree);

				state.depth_remaining++;

				if( rval && expr_ntree ) {
					ntree = MakeAttributeReference(expr_ntree,attributeStr);
					if( ntree ) {
						state.curAd = curAd;
						return true;
					}
				}
				delete expr_ntree;
			}
			if( !(ntree = Copy( ) ) ) {
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
				state.curAd = curAd;
				return( false );
			}
			state.curAd = curAd;
			return true;

		case EVAL_OK:
		{
			// Don't flatten or inline a classad that's referred to
			// by an attribute.
			if ( tree->GetKind() == CLASSAD_NODE ) {
				ntree = Copy( );
				val.SetUndefinedValue( );
				return true;
			}

			if( state.depth_remaining <= 0 ) {
				val.SetErrorValue();
				state.curAd = curAd;
				return false;
			}
			state.depth_remaining--;

			rval = tree->Flatten( state, val, ntree );

			state.depth_remaining++;

			// don't inline if it didn't flatten to a value, and clear cache
			// do inline if FlattenAndInline was called
			if( ntree ) {
				if( state.flattenAndInline ) {	// NAC
					return true;   				// NAC
				}								// NAC
				delete ntree;
				ntree = Copy( );
				val.SetUndefinedValue( );
			}

			state.curAd = curAd;
			return rval;
		}
		default:  CLASSAD_EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}

/*static*/
int AttributeReference::Deref(const AttributeReference & ref, EvalState & state, ExprTree*& tree)
{
	ExprTree * sig = nullptr;
	return ref.FindExpr(state, tree, sig, false);
}

int AttributeReference::
FindExpr(EvalState &state, ExprTree *&tree, ExprTree *&sig, bool wantSig) const
{
	const ClassAd *current=NULL;
	const ExprList *adList = NULL;
	bool	rval;

	sig = NULL;

	// establish starting point for search
	if( expr == NULL ) {
		// "attr" and ".attr"
		current = absolute ? state.rootAd : state.curAd;
		if( absolute && ( current == NULL ) ) {	// NAC - circularity so no root
			return EVAL_FAIL;					// NAC
		}										// NAC
	} else {
		Value	val;

		// "expr.attr" -- expr can itself be an "expr.attr", so a long
		// left-nested chain (a.b.c.d...) recurses here through expr->Evaluate.
		// _Evaluate only decrements depth_remaining when evaluating the
		// *resolved* target, which happens as this descent unwinds -- too late
		// to keep the descent from overflowing the stack. Bound it explicitly.
		if( state.depth_remaining <= 0 ) {
			return EVAL_ERROR;
		}
		state.depth_remaining--;
		rval=wantSig?expr->Evaluate(state,val,sig):expr->Evaluate(state,val);
		state.depth_remaining++;
		if( !rval ) {
			return( EVAL_FAIL );
		}

		if( val.IsUndefinedValue( ) ) {
			return( EVAL_UNDEF );
		} else if( val.IsErrorValue( ) ) {
			return( EVAL_ERROR );
		}
		
		if( !val.IsClassAdValue( current ) && !val.IsListValue( adList ) ) {
			return( EVAL_ERROR );
		}

		if( val.IsListValue( ) ) {
			std::vector< ExprTree *> eVector;
				// eVector owns the ExprTrees pushed into it until they are
				// handed off to the ExprList (and deletion cache) below. If we
				// bail out early, we must free them ourselves or they leak.
			auto freeVector = [&eVector]() {
				for( ExprTree *e : eVector ) { delete e; }
			};
				// iterate through exprList and apply attribute reference
				// to each exprTree
			for (const auto currExpr: *adList) { 
				if( currExpr == NULL ) {
					freeVector();
					return( EVAL_FAIL );
				} else {
						// Build a temporary "element.attr" reference over the
						// list element. We *borrow* currExpr rather than deep-
						// copying it (the old code copied every element, which
						// is O(element size) per element and turns nested list
						// projections into exponential work). attrRef.expr is
						// released below before attrRef is destroyed so the
						// borrowed element -- owned by adList -- is not deleted.
					AttributeReference attrRef( currExpr, attributeStr, false );
					val.Clear( );
						// Create new EvalState, within this scope, because
						// attrRef is only temporary, so we do not want to
						// cache the evaluated result in the outer state object.
					EvalState tstate;

					// In case we recurse back here, let's propagate our
					// recursion depth counter to prevent infinite regress.
					tstate.depth_remaining = state.depth_remaining;

					// Propagate the cumulative work budget so this fresh
					// EvalState can't reset it and escape the bound; carry
					// what's left back out after each element is evaluated.
					tstate.eval_steps_remaining = state.eval_steps_remaining;

					// Propagate the in-flight evaluation stack so circular
					// references are still detected across list projection.
					// Otherwise a self-referential ad (e.g. a list that
					// contains a reference to its own attribute) hides the
					// cycle behind this fresh EvalState and re-evaluates until
					// the work budget is exhausted instead of erroring out.
					tstate.eval_stack = state.eval_stack;

					tstate.SetScopes(state.curAd);
					rval = wantSig ? attrRef.Evaluate( tstate, val, sig )
						: attrRef.Evaluate( tstate, val );
					// Carry the consumed budget back so it accumulates across
					// list elements instead of resetting per iteration.
					state.eval_steps_remaining = tstate.eval_steps_remaining;
					// Release the borrowed element so ~AttributeReference does
					// not delete it (it belongs to adList).
					attrRef.expr = nullptr;
					if( !rval ) {
						freeVector();
						return( EVAL_FAIL );
					}

					ExprTree * item = nullptr;
					ClassAd *evaledAd = nullptr;
					ExprList *evaledList = nullptr;
					if( val.IsClassAdValue( evaledAd ) ) {
						item = evaledAd;
						if ( ! tstate.TakeFromDeletionCache(evaledAd)) { item = evaledAd->Copy(); }
					}
					else if( val.IsListValue( evaledList ) ) {
						item = evaledList;
						if (!tstate.TakeFromDeletionCache(evaledList)) { item = evaledList->Copy(); }
					} else {
						item = Literal::MakeLiteral(val);
						if (!item) {
							freeVector();
							return( EVAL_FAIL );
						}
					}
					eVector.push_back(item);
				}
			}
			tree = ExprList::MakeExprList( eVector );
			state.AddToDeletionCache(tree);
		#if 1
			// TODO: tj make a magic value for empty classad root scope?
			tree->SetParentScope(nullptr);
		#else
			ClassAd *newRoot = new ClassAd( );
			((ExprList*)tree)->SetParentScope( newRoot );
			state.AddToDeletionCache(newRoot);
		#endif
			return EVAL_OK;
		}
	}
		// lookup with scope; this may side-affect state		

		/* ClassAd::alternateScope is intended for transitioning Condor from
		 * old to new ClassAds. It allows unscoped attribute references
		 * in expressions that can't be found in the local scope to be
		 * looked for in an alternate scope. In Condor, the alternate
		 * scope is the Target ad in matchmaking.
		 * Expect alternateScope to be removed from a future release.
		 */
	if (!current) { return EVAL_UNDEF; }
	int rc = current->LookupInScope( attributeStr, tree, state );
	if ( !expr && !absolute && rc == EVAL_UNDEF && current->alternateScope ) {
		rc = current->alternateScope->LookupInScope( attributeStr, tree, state );
	}
	return rc;
}


AttributeReference *AttributeReference::
MakeAttributeReference(ExprTree *tree, const std::string &attrStr, bool absolut)
{
	return( new AttributeReference( tree, attrStr, absolut ) );
}

} // classad
