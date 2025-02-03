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
#include "classad/view.h"
#include "classad/collection.h"
#include "classad/collectionBase.h"

using std::string;
using std::vector;
using std::pair;


// ----------- <implementation of ViewMember class> -----------------

namespace classad {

ViewMember::
ViewMember( )
{
	rank.SetUndefinedValue( );
}

ViewMember::ViewMember(const ViewMember &other)
{
	key = other.key;
	rank.CopyFrom( other.rank );
	return;
}

ViewMember::
~ViewMember( )
{
	return;
}


void ViewMember::
SetRankValue( const Value &rankValue )
{
	rank.CopyFrom( rankValue );
}


void ViewMember::
SetKey( const string &adKey )
{
	key = adKey;
}


void ViewMember::
GetKey( string &adKey ) const 
{
	adKey = key;
}


void ViewMember::
GetRankValue( Value &rankValue ) const
{
	rankValue.CopyFrom( rank );
}


ViewMember ViewMember::
operator=( const ViewMember &vm )
{
	key = vm.key;
	rank.CopyFrom( vm.rank );
	return( *this );
}

bool operator<(const ViewMember &vm1, const ViewMember &vm2)
{
	bool                lessThan;
	bool                isEqual;
	Value				val1, val2;
	Value::ValueType	vt1, vt2;

	vm1.GetRankValue( val1 );
	vm2.GetRankValue( val2 );

	vt1 = val1.GetType( );
	vt2 = val2.GetType( );

	// if the values are of the same scalar type, or if they are both
	// numbers, use the builtin < comparison operator
	if( ( vt1==vt2 && val1.IsClassAdValue() && !val2.IsListValue() ) ||
			( vt1==Value::INTEGER_VALUE && vt2==Value::REAL_VALUE )			||
			( vt1 == Value::REAL_VALUE && vt2 == Value::INTEGER_VALUE ) ) {
		Value	lessThanResult, equalResult;
		Operation::Operate( Operation::LESS_THAN_OP, val1, val2, lessThanResult );
		Operation::Operate( Operation::EQUAL_OP, val1, val2, equalResult);
		lessThan = lessThanResult.IsBooleanValue( lessThan ) && lessThan;
		isEqual  = equalResult.IsBooleanValue( isEqual) && isEqual;
	} else {
		// otherwise, rank them by their type numbers
		lessThan = (vt1 < vt2);
		isEqual  = false;
	}

	// If they are equal, rank them by key.  This is important,
	// because otherwise two members with the same rank are considered
	// to be equal, but they may have different keys, and when we try
	// to erase a single member from the viewMembers, it will erase
	// everything with the same rank, whether or not they have the
	// same key.
	if (isEqual) {
		lessThan = (vm1.key < vm2.key);
	}
	
	return lessThan;
}

// ---------------- </implementation of ViewMember class> ------------------


// ----------------- <implementation of View class> -------------------

View::
View( View *parentView )
{
	vector<ExprTree*>	vec;

	ClassAd	*ad = evalEnviron.GetLeftAd( );
	parent = parentView;
	ExprTree* pExp;
	ad->InsertAttr( ATTR_REQUIREMENTS, true );
	ad->Insert( ATTR_RANK, (pExp=Literal::MakeUndefined()) );
	ad->Insert( ATTR_PARTITION_EXPRS, (pExp=ExprList::MakeExprList( vec )) );
	if( parentView ) {
		ad->InsertAttr( "ParentViewName", parentView->GetViewName( ) );
	}
}


View::
~View( )
{
	SubordinateViews::iterator	xi;
	PartitionedViews::iterator	mi;

		// recursively delete subordinate child views ...
	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		delete (*xi);
	}

		// ... and partitioned child views
	for( mi = partitionedViews.begin( ); mi != partitionedViews.end( ); mi++ ) {
		delete mi->second;
	}
}


bool View::
SetViewName( const ViewName &name )
{
	ClassAd	*ad;
	viewName = name;
	if( !(ad=evalEnviron.GetLeftAd( )) || !ad->InsertAttr("ViewName", name) ) {
		CondorErrno = ERR_FAILED_SET_VIEW_NAME;
		return( false );
	}
	return( true );
}


bool View::
SetViewInfo( ClassAdCollection *coll, ClassAd *ad )
{
	ExprTree	*rankExpr=NULL, *constraintExpr=NULL, *tmp=NULL;
	ExprList	*partitionExprs=NULL;

	if( !( rankExpr = ad->Remove( ATTR_RANK ) ) ) {
		rankExpr = Literal::MakeUndefined();
	}

	if( !( constraintExpr = ad->Remove( ATTR_REQUIREMENTS ) ) ) {
		constraintExpr = Literal::MakeBool( true );
	}
	
	if( ( ( tmp = ad->Remove( ATTR_PARTITION_EXPRS ) ) &&
			tmp->GetKind( ) != ExprTree::EXPR_LIST_NODE ) || !tmp ) {
		vector<ExprTree*>	vec;
		if( tmp ) delete tmp;
		partitionExprs = ExprList::MakeExprList( vec );
	} else {
		partitionExprs = (ExprList*) tmp;
	}

	// Preserve the view name and parent view name.
	ClassAd  *eval;
	string view_name, parent_view_name;

	eval = evalEnviron.GetLeftAd();
	eval->EvaluateAttrString("ViewName", view_name);
	eval->EvaluateAttrString("ParentViewName", parent_view_name);

	ad->InsertAttr("ViewName", view_name);
	ad->InsertAttr("ParentViewName", parent_view_name);

	if( !evalEnviron.ReplaceLeftAd( ad ) ) {
		CondorErrMsg+="; could not replace view info; failed to set view info";
		delete constraintExpr;
		delete rankExpr;
		delete partitionExprs;
		return( false );
	}

	if( constraintExpr && !SetConstraintExpr( coll, constraintExpr ) ) {
		CondorErrMsg += "; failed to set view info";
		delete constraintExpr;
		delete rankExpr;
		delete partitionExprs;
		return( false );
	}

	if( !SetRankExpr( coll, rankExpr ) ) {
		CondorErrMsg += "; failed to set view info";
		delete rankExpr;
		delete partitionExprs;
		return( false );
	}

	if( !SetPartitionExprs( coll, partitionExprs ) ) {
		CondorErrMsg += "; failed to set view info";
		delete partitionExprs;
		return( false );
	}

	return( true );
}


ClassAd *View::
GetViewInfo( )
{
	ClassAd				*newAd, *ad = evalEnviron.GetLeftAd( );
	vector<ExprTree*> 	viewNames;
	Literal				*lit;
	ExprTree* pExpr=0;

	if( !ad ) {
		CLASSAD_EXCEPT( "internal error: view has no view info!" );
	}
	
	if( !( newAd = (ClassAd*) ad->Copy( ) ) ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		if( newAd ) delete newAd;
		return( NULL );
	}
		// insert number of members
	newAd->InsertAttr( "NumMembers", (int) viewMembers.size( ) );

	
		// insert names of subordinate views
	viewNames.clear( );
	SubordinateViews::iterator	si;
	for( si=subordinateViews.begin(); si!=subordinateViews.end(); si++ ) {
		if( !( lit = Literal::MakeString( (*si)->GetViewName( ) ) ) ) {
			delete newAd;
			return( NULL );
		}
		viewNames.push_back( lit );
	}
	newAd->Insert(ATTR_SUBORDINATE_VIEWS,(pExpr=ExprList::MakeExprList(viewNames)));
	

		// insert names of partitioned views
	viewNames.clear( );
	PartitionedViews::iterator	pi;
	for( pi = partitionedViews.begin( ); pi != partitionedViews.end( ); pi++ ) {
		if( !( lit = Literal::MakeString( pi->second->GetViewName() ) ) ) {
			delete newAd;
			return( NULL );
		}
		viewNames.push_back( lit );
	}
	newAd->Insert(ATTR_PARTITIONED_VIEWS,(pExpr=ExprList::MakeExprList(viewNames)));


	return( newAd );
}


bool View::
SetConstraintExpr( ClassAdCollection *coll, const string &expr )
{
		// parse the expression and insert it into ad in left context
	ExprTree* constraint = coll->parser.ParseExpression(expr);
	if( constraint == nullptr ) {
		CondorErrMsg += "; failed to set constraint on view";
		return( false );
	}
	return( SetConstraintExpr( coll, constraint ) );
}


bool View::
SetConstraintExpr( ClassAdCollection *coll, ExprTree *constraint )
{
	ClassAd					*ad;
	ViewMembers::iterator	vmi;
	bool					match;
	string					key;

		// insert expression into ad in left context
	if( !( ad=evalEnviron.GetLeftAd() ) ||
		!ad->Insert( ATTR_REQUIREMENTS, constraint ) ) {
		CondorErrMsg += "; failed to set constraint on view";
		return( false );
	}

		// check if all members still belong to the view
	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
		vmi->GetKey( key );
		if( ( ad = coll->GetClassAd( key ) ) == NULL ) {
			CLASSAD_EXCEPT( "internal error: classad in view but not in collection" );
		}
		evalEnviron.ReplaceRightAd( ad );
		match = evalEnviron.EvaluateAttrBool("RightMatchesLeft",match) && match;
		evalEnviron.RemoveRightAd( );
			// if classad doesn't match constraint remove from view
		if( !match ) {
			ClassAdDeleted( coll, key, ad );
		}
	}

	return( true );
}

bool View::
SetRankExpr( ClassAdCollection *coll, const string &expr )
{
		// parse the expression and insert it into ad in left context
	ExprTree* rank = coll->parser.ParseExpression(expr);
	if( rank == nullptr ) {
		CondorErrMsg += "; failed to set rank on view";
		return( false );
	}
	return( SetRankExpr( coll, rank ) );
}


bool View::
SetRankExpr( ClassAdCollection *coll, ExprTree *rank )
{
	ClassAd					*ad;
	ViewMember				vm;
	MemberIndex::iterator	mIdxItr;
	ViewMembers::iterator	vmi;
	string					key;
	Value					rankValue;

		// insert expression into ad in left context
	if( !( ad=evalEnviron.GetLeftAd() ) ) {
		CLASSAD_EXCEPT( "internal error:  view has no view info" );
	}
	
	if( !ad->Insert( ATTR_RANK, rank ) ) {
		CondorErrMsg += "failed to set rank on view";
		return( false );
	}

		// clear out member list
	viewMembers.clear( );

		// re-order content by new rank expression
	for( mIdxItr=memberIndex.begin(); mIdxItr!=memberIndex.end(); mIdxItr++ ) {
		key = mIdxItr->first;
		if( ( ad = coll->GetClassAd( key ) ) == NULL	||
			!evalEnviron.ReplaceRightAd( ad )			||
			!evalEnviron.EvaluateAttr( "LeftRankValue", rankValue ) ) {
				// internal error
			CLASSAD_EXCEPT( "internal error:  could not determine 'Rank' value" );
		}

			// insert into member list
		vm.SetKey( key );
		vm.SetRankValue( rankValue );
		viewMembers.insert( vm );
	}

		// now re-do the index
	memberIndex.clear( );
	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
		vmi->GetKey( key );
		memberIndex[key] = vmi;
	}

	return( true );
}


bool View::
SetPartitionExprs( ClassAdCollection *coll, const string &expr )
{
		// parse the expression and insert it into ad in left context
	ExprTree* exprList = coll->parser.ParseExpression(expr);
	if( exprList == nullptr ||
			(exprList->GetKind( ) != ExprTree::EXPR_LIST_NODE) ) {
		if( exprList ) delete exprList;
		CondorErrno = ERR_BAD_PARTITION_EXPRS;
		CondorErrMsg += "; failed to set partition expresssions";
		return( false );
	}
	
	return( SetPartitionExprs( coll, (ExprList*)exprList ) );
}


bool View::
SetPartitionExprs( ClassAdCollection *coll, ExprList *el )
{
		// insert expression list into view info
	ClassAd *ad = evalEnviron.GetLeftAd( );
	if( !el ) {
		CondorErrno = ERR_BAD_PARTITION_EXPRS;
		CondorErrMsg = "invalid 'PartitionExprs'; failed to partition";
		return( false );
	}
	if( !( ad->Insert( ATTR_PARTITION_EXPRS, el))) {
		CondorErrMsg += "failed to set partition expressions on view";
		return( false );
	}

		// re-establish partition views; first delete all partition views
	PartitionedViews::iterator  mi;
	for( mi = partitionedViews.begin( ); mi != partitionedViews.end( ); mi++ ) {
		mi->second->DeleteView( coll );
		delete mi->second;
	}
	partitionedViews.clear( );

		// if the partition expressions list is empty, we're done
	vector<ExprTree*> components;
	el->GetComponents( components );
	if( components.size( ) == 0 ) return( true );
	
		// re-partition content
	ViewMembers::iterator	vmi;
	string					key, signature;
	View					*partition;

	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
			// get signature of this ad
		vmi->GetKey( key );
		if( ( ad = coll->GetClassAd( key ) ) == NULL ) {
			CLASSAD_EXCEPT( "internal error:  classad %s in view but not in collection",
				key.c_str( ) );
		}
		signature = makePartitionSignature( ad );

			// check if we have a partition with this signature
		if( partitionedViews.find( signature ) == partitionedViews.end( ) ) {
				// no partition ... make a new one
			if( ( partition = new View( this ) ) == NULL ) {
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
				return( false );
			}
			if( !coll->RegisterView( viewName + ":" + signature, partition ) ) {
				CondorErrMsg += "; could not complete partitioning";
				return( false );
			}
			partition->SetViewName( viewName + ":" + signature );
			partitionedViews[signature] = partition;
		} else {
				// use the partition that's already there
			partition = partitionedViews[signature];
		}

			// add classad to the partition
		if( !partition->ClassAdInserted( coll, key, ad ) ) {
			CondorErrMsg += "; failed to set partition expressions";
			return( false );
		}
	}

	return( true );
}


ExprTree *View::
GetConstraintExpr( )
{
	ClassAd		*lAd;
	ExprTree	*tree;
		// get requirements expression from ad in left context
	if( !(lAd = evalEnviron.GetLeftAd( ) ) ) {
		CLASSAD_EXCEPT( "internal error:  no view info in view" );
	}
	if( !(tree = lAd->Lookup( ATTR_REQUIREMENTS ) ) ) {
		CondorErrno = ERR_NO_REQUIREMENTS_EXPR;
		CondorErrMsg = "no 'Requirements' expression in view info";
		return( (ExprTree*) NULL );
	}
	return( tree );
}


ExprTree *View::
GetRankExpr( )
{
	ClassAd		*lAd;
	ExprTree	*tree;
		// get rank expression from ad in left context
	if( !(lAd = evalEnviron.GetLeftAd( ) ) ) {
		CLASSAD_EXCEPT( "internal error:  no view info in view" );
	}
	if( !(tree = lAd->Lookup( ATTR_RANK ) ) ) {
		CondorErrno = ERR_NO_RANK_EXPR;
		CondorErrMsg = "no 'Rank' expression in view info";
		return( (ExprTree*) NULL );
	}
	return( tree );
}



bool View::
IsMember( const string &key )
{
	return( memberIndex.find( key ) != memberIndex.end( ) );
}


bool View::
FindPartition( ClassAd *rep, ViewName &partition )
{
	PartitionedViews::iterator	itr;
	string sig = makePartitionSignature( rep );

	if( sig.empty( ) || sig == "ERROR" || 
			( itr = partitionedViews.find( sig ) ) == partitionedViews.end( ) ){
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "no partition matching representative found";
		return( false );
	}
	partition = itr->second->GetViewName( );
	return( true );
}


bool View::
InsertSubordinateView( ClassAdCollection *coll, ClassAd *viewInfo )
{
	View					*newView = new View( this );
	ViewMembers::iterator	vmi;
	string					key;
	ClassAd					*ad;
	string					name;;

	if( !newView ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( false );
	}

	if( viewInfo ) {
		viewInfo->EvaluateAttrString( ATTR_VIEW_NAME, name );
		newView->evalEnviron.ReplaceLeftAd( viewInfo );
	} 

	newView->SetViewName( name );
	if( !coll->RegisterView( name, newView ) ) {
		CondorErrMsg += "; failed to insert new view";
		delete newView;
		return( false );
	}
	subordinateViews.push_front( newView );

		// insert current view content into new view
	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
		vmi->GetKey( key );
		if( ( ad = coll->GetClassAd( key ) ) == NULL ) {
			CLASSAD_EXCEPT( "internal error:  classad %s in view but not in collection",
				key.c_str( ) );
		}
		if( !newView->ClassAdInserted( coll, key, ad ) ) {
			CondorErrMsg += "; failed to insert content into new view";
			return( false );
		}
	}

	return( true );
}


bool View::
InsertPartitionedView( ClassAdCollection *coll, ClassAd *viewInfo, 
	ClassAd *rep )
{
	string	signature;
	View	*partition;
	string	tmp;

	signature = makePartitionSignature( rep );
	delete rep;

		// must have a partitioning
	if( signature.empty( ) ) {
		delete viewInfo;
		CondorErrno = ERR_BAD_PARTITION_EXPRS;
		CondorErrMsg = "missing or bad partition expressions; cannot add "
			"partition";
		return( false );
	}

		// check if a partition with the given signature already exists
	if( partitionedViews.find( signature ) != partitionedViews.end( ) ) {
			// partition already exists
		delete viewInfo;
		CondorErrno = ERR_PARTITION_EXISTS;
		CondorErrMsg = "partition " + signature + " already exists";
		return( false );
	}
		// create new partition
	if( ( partition = new View( this ) ) == NULL ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( false );
	}

	if( viewInfo && viewInfo->EvaluateAttrString( ATTR_VIEW_NAME, tmp ) ) {
		partition->SetViewName( tmp );
		if( !coll->RegisterView( tmp, partition ) ) {
			delete viewInfo;
			delete partition;
			CondorErrMsg += "; failed to add partition " + tmp;
			return( false );
		}
	} else {
		partition->SetViewName( viewName + ":" + signature );
		if( !coll->RegisterView( viewName + ":" + signature, partition ) ) {
			delete viewInfo;
			delete partition;
			CondorErrMsg += "; failed to add partition " + tmp;
			return( false );
		}
	}

		// enter into hash_map
	partitionedViews[signature] = partition;

		// setup view info of partition
	if( viewInfo ) {
		partition->evalEnviron.ReplaceLeftAd( viewInfo );
	} 

		// NOTE:  since this is a partition which hasn't been created 
		// previously, none of the classads in this view need be inserted
		// into the new partition
	return( true );
}


bool View::
DeleteChildView( ClassAdCollection *coll, const ViewName &vName )
{
	if( !DeleteSubordinateView( coll, vName ) && 
			!DeletePartitionedView( coll, vName ) ) {
		return( false );
	}
	CondorErrno = ERR_OK;
	CondorErrMsg = "";
	return( true );
}


bool View::
DeleteSubordinateView( ClassAdCollection *coll, const ViewName &vName )
{
	SubordinateViews::iterator	xi;

	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		if( (*xi)->GetViewName( ) == vName ) {
			(*xi)->DeleteView( coll );
			delete (*xi);
			subordinateViews.erase( xi );
			return( true );
		}
	}
	CondorErrno = ERR_NO_SUCH_VIEW;
	CondorErrMsg = "no child view named " + vName + " in view";
	return( false );
}


bool View::
DeletePartitionedView( ClassAdCollection *coll, const ViewName &vName )
{
	PartitionedViews::iterator 	mi;

	for( mi = partitionedViews.begin( ); mi != partitionedViews.end( ); mi++ ) {
		if( mi->second->GetViewName( ) == vName ) {
				// check if the partition is non-empty ...
			if( mi->second->Size( ) != 0 ) {
					// remove all child views and reset view info
				View						*view = mi->second;
				SubordinateViews::iterator	svi;
				PartitionedViews::iterator	pvi;

					// delete subordinate views
				for( svi = view->subordinateViews.begin( ); 
						svi != view->subordinateViews.end( ); svi++ ) {
					(*svi)->DeleteView( coll );
					delete (*svi);
				}
				view->subordinateViews.clear( );

					// delete partitioned views
				for( pvi = view->partitionedViews.begin( ); 
						pvi != view->partitionedViews.end( ); pvi++ ) {
					pvi->second->DeleteView( coll );
					delete pvi->second;
				}
				view->partitionedViews.clear( );

					// reset name of view
				coll->UnregisterView( vName );
				coll->RegisterView( viewName + ":" + mi->first, view );

					// reset other view info
				vector<ExprTree*> vec;
				ExprTree * pExpr=0;
				ClassAd *ad = new ClassAd( );
				if( !ad ) {
					CondorErrno = ERR_MEM_ALLOC_FAILED;
					CondorErrMsg = "";
					return( false );
				}
				if( !ad->InsertAttr( ATTR_REQUIREMENTS, true )	||
						!ad->InsertAttr( ATTR_RANK, 0 )				||
						!ad->Insert(ATTR_PARTITION_EXPRS,
							(pExpr=ExprList::MakeExprList( vec )) )	||
						!view->SetViewInfo( coll, ad ) ) {
					CondorErrMsg += "; failed to delete partition view " +
						vName;
					return( false );
				}
				return( true );
			}

				// empty partition ... can just delete it
			mi->second->DeleteView( coll );
			delete mi->second;
			partitionedViews.erase( mi );
			return( true );
		}
	}
	CondorErrno = ERR_NO_SUCH_VIEW;
	CondorErrMsg = "no partition child view named " + vName + " in view";
	return( false );
}


void View::
DeleteView( ClassAdCollection *coll )
{
	SubordinateViews::iterator	si;
	PartitionedViews::iterator	pi;

	coll->UnregisterView( viewName );

	for( si = subordinateViews.begin( ); si != subordinateViews.end( ); si++ ) {
		(*si)->DeleteView( coll );
		delete (*si);
	}

	for( pi = partitionedViews.begin( ); pi != partitionedViews.end( ); pi++ ) {
		pi->second->DeleteView( coll );
		delete pi->second;
	}
}



bool View::
DeletePartitionedView( ClassAdCollection *coll, ClassAd *rep )
{
	string signature = makePartitionSignature( rep );

	if( signature.empty( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "no partition corresponds to representative";
		return( false );
	}

	ViewName vName = viewName + ":" + signature;
	return( DeletePartitionedView( coll, vName ) );
}


// The convention is that each view verifies its constraint before accepting
// an ad.  However, the view assumes that in the case of view partitions, the
// parent view will have identified the correct child partition to use.
bool View::
ClassAdInserted( ClassAdCollection *coll, const string &key, 
	ClassAd *ad )
{
	PartitionedViews::iterator	partition;
	string						signature;
	ViewMember					vm;
	bool 						match;
	Value						rankValue;
	View						*childView;

		// check if the ad satisfies the view's constraint; if the constraint 
		// was not satisfied, the ad can be ignored
	evalEnviron.ReplaceRightAd( ad );
	match = evalEnviron.EvaluateAttrBool("RightMatchesLeft",match) && match;
	if( !match ) {
		evalEnviron.RemoveRightAd( );
		return( true );
	}

		// obtain the rank value of the ad
	if( !evalEnviron.EvaluateAttr( "LeftRankValue", rankValue ) ) {
		CondorErrMsg += "; could not get 'Rank' value; failed to insert "
			"classad " + key + "in view " + viewName;
		return( false );
	}
	evalEnviron.RemoveRightAd( );

		// insert into every subordinate child view
	SubordinateViews::iterator	xi;
	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		if( !(*xi)->ClassAdInserted( coll, key, ad ) ) {
			return( false );
		}
	}

		// find partition to insert into
	signature = makePartitionSignature( ad );
	if( !signature.empty( ) ) {
		partition = partitionedViews.find( signature );
		if( partition == partitionedViews.end( ) ) {
				// no appropriate partition --- create one and insert
			if( ( childView = new View( this ) ) == 0 ) {
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
				return( false );
			}

			if( !coll->RegisterView( viewName + ":" + signature, childView ) ) {
				if( childView ) delete childView;
				CondorErrMsg += "; failed to create view; failed to insert "
					"classad " + key + "in view";
				return( false );
			}
			childView->SetViewName( viewName + ":" + signature );
			partitionedViews[signature] = childView;
		} else {
			childView = partitionedViews[signature];
		}

			// update the partition
		if( !childView->ClassAdInserted( coll, key, ad ) ) {
			return( false );
		}
	}

		// insert ad into list of view members and update index
	vm.SetKey( key );
	vm.SetRankValue( rankValue );
	memberIndex[key] = viewMembers.insert(vm);

	return( true );
}



void View::
ClassAdPreModify( ClassAdCollection *coll, ClassAd *ad )
{
	SubordinateViews::iterator	xi;
	PartitionedViews::iterator	mi;
	
		// stash signature of ad
	oldAdSignature = makePartitionSignature( ad );

		// perform premodify on all subordinate children ...
	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		(*xi)->ClassAdPreModify( coll, ad );
	}

		// ... and partition children
	for( mi = partitionedViews.begin( ); mi != partitionedViews.end( ); mi++ ) {
		mi->second->ClassAdPreModify( coll, ad );
	}

}


bool View::
ClassAdModified( ClassAdCollection *coll, const string &key, 
	ClassAd *mad )
{
	bool	match, wasMember, sameRank, rval = true;
	Value	rankValue, oldAdRank, equal;
	MemberIndex::iterator	itr = memberIndex.find( key );

		// check if classad is currently a member of this view
	if( itr == memberIndex.end( ) ) {
		wasMember = false;
	} else {
		wasMember = true;
		((ViewMember) *(itr->second)).GetRankValue( oldAdRank );
	}

		// evaluate constraint and get new rank value
	evalEnviron.ReplaceRightAd( mad );
	match = evalEnviron.EvaluateAttrBool("RightMatchesLeft",match) && match;
	if( !evalEnviron.EvaluateAttr( "LeftRankValue", rankValue ) ) {
		rankValue.SetUndefinedValue( );
	}
	evalEnviron.RemoveRightAd( );

	if( wasMember && match ) {
		string 	sig;
			// was and still is a member; check if rank has changed
		Operation::Operate( Operation::IS_OP, rankValue, oldAdRank, equal );
		if( !equal.IsBooleanValue( sameRank ) || !sameRank ) {
				// rank changed ... need to re-order
			ViewMember	vm;

				// remove old view member ...
			vm.SetRankValue( oldAdRank );
			vm.SetKey(key);
			viewMembers.erase( vm );
				// re-insert with new rank value and update member index
			vm.SetRankValue( rankValue );
			memberIndex[key] = viewMembers.insert( vm );
		}

			// check if the signature has changed
		sig = makePartitionSignature( mad );
		if( sig != oldAdSignature ) {
			PartitionedViews::iterator	mi;
			View						*newPartition;
				// yes ... remove from old partition and insert into new
			if( !oldAdSignature.empty( ) ) {
				mi = partitionedViews.find( oldAdSignature );
				if( mi == partitionedViews.end( ) ) {
						// partition of ad not found; some internal error
					CLASSAD_EXCEPT( "internal error:  partition of classad with "
						"signature %s not found", oldAdSignature.c_str( ) );
				}
					// delete from old partition
				mi->second->ClassAdDeleted( coll, key, mad );
			}

				// is there a partition with the new signature?
			if( !sig.empty( ) ) {
				mi = partitionedViews.find( sig );
				if( mi != partitionedViews.end( ) ) {
						// yes ... insert into this partition
					if( !mi->second->ClassAdInserted(coll, key, mad) ) {
						CondorErrMsg+="; failed to relocate ad on modification";
						return( false );
					}
				} else {
						// no ... create a new partition
					if( ( newPartition = new View( this ) ) == 0 ) {
						oldAdSignature.erase( oldAdSignature.begin( ), 
							oldAdSignature.end( ) );
						CondorErrno = ERR_MEM_ALLOC_FAILED;
						CondorErrMsg = "";
						return( false );
					} 
					if( !coll->RegisterView( viewName+":"+sig, newPartition ) ){
						delete newPartition;
						CondorErrMsg += "; failed to create new partition for "
							" modified ad";
						return( false );
					}
					newPartition->SetViewName( viewName + ":" + sig );
					if( !newPartition->ClassAdInserted(coll, key, mad) ) {
						CondorErrMsg+="; failed to relocate ad on modification";
						return( false );
					}
					partitionedViews[sig] = newPartition;
				}
			}
		}

			// send modification notification to all subordinate children
		SubordinateViews::iterator xi;
		for( xi=subordinateViews.begin( ); xi!=subordinateViews.end( ); xi++ ){
			if( !(*xi)->ClassAdModified( coll, key, mad ) ) {
				return( false );
			}
		}
	} else if( !wasMember && match ) {
			// wasn't a member, but now is --- insert the ad
		rval = ClassAdInserted( coll, key, mad );
	} else if( wasMember && !match ) {
			// was a member, but now isnt --- delete the ad
		ClassAdDeleted( coll, key, mad );
		rval = true;
	} else {
		// wasn't a member and still isn't --- nothing to do
	}

	oldAdSignature.erase( oldAdSignature.begin( ), oldAdSignature.end( ) );
	if( !rval ) {
		CondorErrMsg += "; failed to modify ad";
	}
	return( rval );
}


void View::
ClassAdDeleted( ClassAdCollection *coll, const string &key, 
	ClassAd *ad )
{
	ViewMembers::iterator	vmi;

		// check if the classad is in the view
	if( memberIndex.find( key ) == memberIndex.end( ) ) {
			// not in view; nothing to do
		return;
	}

		// delete from member index and member list
	vmi = memberIndex[key];
	memberIndex.erase( key );
	viewMembers.erase( vmi );

		// delete from every subordinate child view
	SubordinateViews::iterator	xi;
	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		(*xi)->ClassAdDeleted( coll, key, ad );
	}

		// delete from partition view
	string signature = makePartitionSignature( ad );
	if( !signature.empty( ) ) {
		PartitionedViews::iterator mi = partitionedViews.find( signature );
		if( mi == partitionedViews.end( ) ) {
				// an internal error
			CLASSAD_EXCEPT( "classad %s doesn't have a partition", signature.c_str( ) );
		}

			// delete from the partition
		mi->second->ClassAdDeleted( coll, key, ad );
	}
}


string View::
makePartitionSignature( ClassAd *ad )
{
	ClassAdUnParser		unparser;
	std::string				signature;
    Value   			value;
	ClassAd				*oad, *info;
	const ExprList		*el = NULL;

		// stash the old ad from the environment and insert the new ad in
	oad = evalEnviron.RemoveRightAd( );
	evalEnviron.ReplaceRightAd( ad );
	if( !( info = evalEnviron.GetLeftAd( ) ) ) {
		CLASSAD_EXCEPT( "internal error:  view doesn't have view info" );
	}
	vector<ExprTree*> exprs;
	if( !info->EvaluateAttr( ATTR_PARTITION_EXPRS, value ) ||
			!value.IsListValue( el ) ) {
		evalEnviron.RemoveRightAd( );
		return( string( "" ) );
	}
	el->GetComponents( exprs );
	if( exprs.size( ) == 0 ) {
		evalEnviron.RemoveRightAd( );
		return( string( "" ) );
	}

		// go through the expression list and form a value vector
	signature = "<|";
	for (int i = 0; i < el->size(); i++) {
		el->GetValueAt(i, value);
		unparser.Unparse( signature, value );
		signature += "|";
    }
	signature += ">";

		// put the old ad back in the environmnet
	evalEnviron.RemoveRightAd( );
	evalEnviron.ReplaceRightAd( oad );

		// return the computed signature
    return( signature );
}


bool View::
Display( FILE *file )
{
	ViewMembers::iterator	vmi;
	ClassAdUnParser			unparser;
	Value					val;
	ClassAd					*viewInfo;
	string					buf;

		// display view info
	if( !( viewInfo = GetViewInfo( ) ) ) return( false );
	unparser.Unparse( buf, viewInfo );
	fprintf( file, "%s\n", buf.c_str( ) );
	delete viewInfo;

	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
		vmi->GetKey( buf );
		vmi->GetRankValue( val );
		buf += ": ";
		unparser.Unparse( buf, val );
		fprintf( file, "%s\n", buf.c_str() );
	}

	return( true );
}

void View::
GetSubordinateViewNames( vector<string>& views )
{
	SubordinateViews::iterator	itr;

	views.clear( );
	for( itr=subordinateViews.begin( ); itr!=subordinateViews.end( ); itr++ ) {
		views.push_back( (*itr)->GetViewName( ) );
	}
}

void View::
GetPartitionedViewNames( vector<string>& views )
{
	PartitionedViews::iterator	itr;

	views.clear( );
	for( itr=partitionedViews.begin( ); itr!=partitionedViews.end( ); itr++ ) {
		views.push_back( itr->second->GetViewName( ) );
	}
}


bool ViewMemberLT::
operator()( const ViewMember &vm1, const ViewMember &vm2 ) const
{
	bool                lessThan;

	lessThan = vm1 < vm2;

	return lessThan;
}

}
