/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_classad.h"
#include "externrefs.h"

bool _GetExternalReferences(ExprTree*,MatchClassAd*,EvalState&,References&);


bool
GetExternalReferences( ExprTree *expr, ClassAd *ad, References& refs )
{
	MatchClassAd 	mad( NULL, ad );
	EvalState		state;
	bool 			rval;

	state.curAd = ad;
	state.rootAd = &mad; 
	refs.clear( );
	rval = _GetExternalReferences( expr, &mad, state, refs );
	mad.RemoveRightAd( );
	return( rval );
}


bool
_GetExternalReferences( ExprTree *expr, MatchClassAd *mad, EvalState &state,
	References& refs )
{
	switch( expr->GetKind( ) ) {
		case LITERAL_NODE:
				// no external references here
			return( true );

		case ATTRREF_NODE: {
			const ClassAd 	*start = mad->GetRightAd( );
			ExprTree 		*tree, *result;
			string			attr;
			Value			val;
			bool			abs;

			((AttributeReference*)expr)->GetComponents( tree, attr, abs );
				// establish starting point for attribute search
			if( tree==NULL ) {
				start = abs ? state.rootAd : state.curAd;
			} else {
				if( !tree->Evaluate( state, val ) ) return( false );

					// if the tree evals to undefined, the external references
					// are in the tree part
				if( val.IsUndefinedValue( ) ) {
					return( _GetExternalReferences( tree, mad, state, refs ) );
				}
					// otherwise, if the tree didn't evaluate to a classad,
					// we have a problem
				if( !val.IsClassAdValue( start ) ) return( false );
			}
				// lookup for attribute
			switch( start->LookupInScope( attr, result, state ) ) {
				case EVAL_ERROR:
						// some error
					return( false );

				case EVAL_UNDEF:
						// attr is external
					refs.insert( attr );
					return( true );

				case EVAL_OK:
						// attr is internal; find external refs in result
					return(_GetExternalReferences( result, mad, state, refs ));

				default:	
						// enh??
					return( false );
			}
		}
			
		case OP_NODE: {
				// recurse on subtrees
			OpKind		op;
			ExprTree	*t1, *t2, *t3;
			((Operation*)expr)->GetComponents( op, t1, t2, t3 );
			if( t1 && !_GetExternalReferences( t1, mad, state, refs ) ) {
				return( false );
			}
			if( t2 && !_GetExternalReferences( t2, mad, state, refs ) ) {
				return( false );
			}
			if( t3 && !_GetExternalReferences( t3, mad, state, refs ) ) {
				return( false );
			}
			return( true );
		}


		case FN_CALL_NODE: {
				// recurse on subtrees
			string						fnName;
			vector<ExprTree*>			args;
			vector<ExprTree*>::iterator	itr;

			((FunctionCall*)expr)->GetComponents( fnName, args );
			for( itr = args.begin( ); itr != args.end( ); itr++ ) {
				if( !_GetExternalReferences( *itr, mad, state, refs ) );
			}
			return( true );
		}


		case CLASSAD_NODE: {
				// recurse on subtrees
			vector< pair<string, ExprTree*> >			attrs;
			vector< pair<string, ExprTree*> >::iterator	itr;

			((ClassAd*)expr)->GetComponents( attrs );
			for( itr = attrs.begin( ); itr != attrs.end( ); itr++ ) {
				if( !_GetExternalReferences( itr->second, mad, state, refs ) );
			}
			return( true );
		}


		case EXPR_LIST_NODE: {
				// recurse on subtrees
			vector<ExprTree*>			exprs;
			vector<ExprTree*>::iterator	itr;

			((ExprList*)expr)->GetComponents( exprs );
			for( itr = exprs.begin( ); itr != exprs.end( ); itr++ ) {
				if( !_GetExternalReferences( *itr, mad, state, refs ) );
			}
			return( true );
		}


		default:
			return false;
	}
}
