#include "condor_common.h"
#include "condor_classad.h"
#include "view.h"
#include "collection.h"

// ----------- <implementation of ViewMember class> -----------------

BEGIN_NAMESPACE( classad )

ViewMember::
ViewMember( )
{
	rank.SetUndefinedValue( );
}

ViewMember::
~ViewMember( )
{
}


void ViewMember::
SetRankValue( const Value &rankValue )
{
	rank.CopyFrom( (Value&) rankValue );
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
GetRankValue( Value &rankValue )
{
	rankValue.CopyFrom( rank );
}


ViewMember ViewMember::
operator=( const ViewMember &vm )
{
	key = vm.key;
	rank.CopyFrom( (Value&) vm.rank );
	return( *this );
}


// ---------------- </implementation of ViewMember class> ------------------


// ----------------- <implementation of View class> -------------------

View::
View( View *parentView )
{
	parent = parentView;
	evalEnviron.DeepInsertAttr( "adcl.ad", ATTR_REQUIREMENTS, true );
	evalEnviron.DeepInsertAttr( "adcl.ad", ATTR_RANK, 0 );
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
	viewName = name;
	return( evalEnviron.DeepInsertAttr( "adcl.ad", "ViewName", name.c_str() ) );
}


bool View::
SetViewInfo( ClassAdCollectionServer *coll, ClassAd *ad )
{
	ExprTree	*rankExpr = ad->Remove( ATTR_RANK );
	ExprTree	*constraintExpr = ad->Remove( ATTR_REQUIREMENTS );
	ExprTree	*tmp = ad->Remove( "PartitionExprs" );
	ExprList	*partitionExprs = ( tmp && tmp->GetKind( )==EXPR_LIST_NODE ) ? 
						(ExprList*) tmp : NULL;

	if( !rankExpr || !constraintExpr || !partitionExprs || 
			!evalEnviron.ReplaceLeftAd( ad ) ) {
		if( rankExpr ) delete rankExpr;
		if( constraintExpr ) delete constraintExpr;
		if( partitionExprs ) delete partitionExprs;
		return( false );
	}

	return( SetConstraintExpr( coll, constraintExpr ) && 
			SetRankExpr( coll, rankExpr ) &&
			SetPartitionExprs( coll, partitionExprs ) );
}


ClassAd *View::
GetViewInfo( )
{
	ClassAd	*newAd, *ad = evalEnviron.GetLeftAd( );
	if( !ad || !( newAd = ad->Copy( ) ) ) return( NULL );

	if( parent ) {
		newAd->InsertAttr( "ParentView", parent->GetViewName( ).c_str( ) );
	}
	newAd->InsertAttr( "NumMembers", (int) viewMembers.size( ) );
	return( newAd );
}


bool View::
SetConstraintExpr( ClassAdCollectionServer *coll, const string &expr )
{
	Source		src;
	ExprTree	*constraint;

		// parse the expression and insert it into ad in left context
	return( src.SetSource( expr.c_str( ) ) 	&& 
		src.ParseExpression( constraint ) 	&&
		SetConstraintExpr( coll, constraint ) );
}


bool View::
SetConstraintExpr( ClassAdCollectionServer *coll, ExprTree *constraint )
{
	ClassAd					*ad;
	ViewMembers::iterator	vmi;
	bool					match, rval = true;
	string				key;

		// insert expression into ad in left context
	if( !( ad=evalEnviron.GetLeftAd() ) ||
		!ad->Insert( ATTR_REQUIREMENTS, constraint ) ) {
		return( false );
	}

		// check if all members still belong to the view
	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
		vmi->GetKey( key );
		if( ( ad = coll->GetClassAd( key ) ) == NULL 	||
			!evalEnviron.ReplaceRightAd( ad )			||
			!evalEnviron.EvaluateAttrBool( "RightMatchesLeft", match ) ) {
				// some error; try and finish anyway
			rval = false;
			evalEnviron.RemoveRightAd( );
			continue;
		}
		evalEnviron.RemoveRightAd( );
			// if classad doesn't match constraint remove from view
		if( !match ) {
			rval = rval && ClassAdDeleted( coll, key, ad );
		}
	}

	return( rval );
}

bool View::
SetRankExpr( ClassAdCollectionServer *coll, const string &expr )
{
	Source		src;
	ExprTree	*rank;

		// parse the expression and insert it into ad in left context
	return( src.SetSource( expr.c_str( ) ) 	&& 
		src.ParseExpression( rank ) 		&&
		SetRankExpr( coll, rank ) );
}


bool View::
SetRankExpr( ClassAdCollectionServer *coll, ExprTree *rank )
{
	ClassAd					*ad;
	bool					rval = true;
	ViewMember				vm;
	MemberIndex::iterator	mIdxItr;
	ViewMembers::iterator	vmi;
	string				key;
	Value					rankValue;

		// insert expression into ad in left context
	if( !( ad=evalEnviron.GetLeftAd() ) || !ad->Insert( ATTR_RANK, rank ) ) {
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
				// some error
			rval = false;
			evalEnviron.RemoveRightAd( );
			continue;
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

	return( rval );
}

bool View::
SetPartitionExprs( ClassAdCollectionServer *coll, const string &expr )
{
	Source		src;
	ExprList	*exprList;

		// parse the expression and insert it into ad in left context
	return( src.SetSource( expr.c_str( ) ) 	&& 
		src.ParseExprList( exprList ) && SetPartitionExprs( coll, exprList ) );
}


bool View::
SetPartitionExprs( ClassAdCollectionServer *coll, ExprList *el )
{
		// insert expression list into view info
	ClassAd *ad = evalEnviron.GetLeftAd( );
	if( !el ) {
		ad->Delete( "PartitionExprs" );
	} else if( !ad->InsertAttr( "PartitionExprs", el ) ) {
		delete el;
		return( false );
	}

		// re-establish partition views; first delete all partition views
	PartitionedViews::iterator  mi;
	for( mi = partitionedViews.begin( ); mi != partitionedViews.end( ); mi++ ) {
		delete mi->second;
	}
	partitionedViews.clear( );

		// if there are no partition attrs, no need to do anything more
	if( !el ) return( true );
	
		// re-partition content
	ViewMembers::iterator	vmi;
	string				key, signature;
	bool					rval = true;
	View					*partition;

	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
			// get signature of this ad
		vmi->GetKey( key );
		if( ( ad = coll->GetClassAd( key ) ) == NULL ) {
			rval = false;
			continue;
		}
		signature = makePartitionSignature( ad );

			// check if we have a partition with this signature
		if( partitionedViews.find( signature ) == partitionedViews.end( ) ) {
				// no partition ... make a new one
			if( ( partition = new View( this ) ) == NULL ) {
				return( false );
			}
			if( !coll->RegisterView( viewName + ":" + signature, partition ) ) {
				return( false );
			}
			partition->SetViewName( viewName + ":" + signature );
			partitionedViews[signature] = partition;
		} else {
				// use the partition that's already there
			partition = partitionedViews[signature];
		}

			// add classad to the partition
		rval = rval && partition->ClassAdInserted( coll, key, ad );
	}

	return( rval );
}


ExprTree *View::
GetConstraintExpr( )
{
	ClassAd		*lAd;
	ExprTree	*tree;
		// get rank expression from ad in left context
	return( (lAd=evalEnviron.GetLeftAd( )) && 
			(tree=lAd->Lookup( ATTR_REQUIREMENTS )) ?  tree : NULL );
}


ExprTree *View::
GetRankExpr( )
{
	ClassAd		*lAd;
	ExprTree	*tree;
		// get rank expression from ad in left context
	return( (lAd=evalEnviron.GetLeftAd( )) && (tree=lAd->Lookup( ATTR_RANK )) ?
		tree : NULL );
}



bool View::
IsMember( const string &key )
{
	return( memberIndex.find( key ) != memberIndex.end( ) );
}


bool View::
InsertSubordinateView( ClassAdCollectionServer *coll, ClassAd *viewInfo )
{
	View					*newView = new View( this );
	ViewMembers::iterator	vmi;
	string				key;
	ClassAd					*ad;
	bool					rval = true;
	char					*name= NULL;;

	if( !newView ) return( false );

	if( viewInfo ) {
		if( !( ad = viewInfo->Copy( ) ) ) {
			return( false );
		}
		ad->InsertAttr( "ParentView", viewName.c_str( ) );
		newView->evalEnviron.ReplaceLeftAd( ad );
		ad->EvaluateAttrString( "ViewName", name );
	} else {
		newView->evalEnviron.DeepInsertAttr( "adcl.ad", "ParentView",
			viewName.c_str( ) );
	}

	newView->SetViewName( ViewName( name ) );
	if( !coll->RegisterView( ViewName( name ), newView ) ) {
		delete newView;
		delete [] name;
		return( false );
	}
	delete [] name;
	subordinateViews.push_front( newView );

		// insert current view content into new view
	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
		vmi->GetKey( key );
		if( ( ad = coll->GetClassAd( key ) ) == NULL ) {
			rval = false;
			continue;
		}
		rval = rval && newView->ClassAdInserted( coll, key, ad );
	}

	return( rval );
}


bool View::
InsertPartitionedView( ClassAdCollectionServer *coll, ClassAd *viewInfo, 
	ClassAd *rep )
{
	string	signature  = makePartitionSignature( rep );
	View		*partition;
	ClassAd		*ad;
	char		*tmp=NULL;

		// must have a partitioning
	if( signature.empty( ) ) {
		return( false );
	}

		// check if a partition with the given signature already exists
	if( partitionedViews.find( signature ) != partitionedViews.end( ) ) {
			// partition already exists
		return( false );
	}
		// create new partition
	if( ( partition = new View( this ) ) == NULL ) {
		return( false );
	}

	if( viewInfo && viewInfo->EvaluateAttrString( "ViewName", tmp ) ) {
		partition->SetViewName( string( tmp ) );
		if( !coll->RegisterView( string( tmp ), partition ) ) {
			delete partition;
			return( false );
		}
	} else {
		partition->SetViewName( viewName + ":" + signature );
		if( !coll->RegisterView( viewName + ":" + signature, partition ) ) {
			delete partition;
			return( false );
		}
	}
	if( tmp ) delete [] tmp;

		// enter into hash_map
	partitionedViews[signature] = partition;

		// setup view info of partition
	if( viewInfo ) {
		if( !( ad = viewInfo->Copy( ) ) ) {
			return( false );
		}
		ad->InsertAttr( "ParentView", viewName.c_str( ) );
		partition->evalEnviron.ReplaceLeftAd( ad );
	} else {
		partition->evalEnviron.DeepInsertAttr( "adcl.ad", "ParentView",
			viewName.c_str( ) );
	}

		// NOTE:  since this is a partition which hasn't been created 
		// previously, none of the classads in this view need be inserted
		// into the new partition
	return( true );
}


bool View::
DeleteChildView( ClassAdCollectionServer *coll, const ViewName &vName )
{
	return( DeleteSubordinateView( coll, vName ) || 
			DeletePartitionedView( coll, vName ) );
}


bool View::
DeleteSubordinateView( ClassAdCollectionServer *coll, const ViewName &vName )
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
	return( false );
}


bool View::
DeletePartitionedView( ClassAdCollectionServer *coll, const ViewName &vName )
{
	PartitionedViews::iterator 	mi;

	for( mi = partitionedViews.begin( ); mi != partitionedViews.end( ); mi++ ) {
		if( mi->second->GetViewName( ) == vName ) {
				// cannot delete a non-empty partition
			if( mi->second->Size( ) != 0 ) {
				return( false );
			}
			mi->second->DeleteView( coll );
			delete mi->second;
			partitionedViews.erase( mi );
			return( true );
		}
	}

	return( false );
}


void View::
DeleteView( ClassAdCollectionServer *coll )
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
DeletePartitionedView( ClassAdCollectionServer *coll, ClassAd *rep )
{
	string signature = makePartitionSignature( rep );

	if( signature.empty( ) ) {
		return( false );
	}

	ViewName	vName = viewName + ":" + signature;
	return( DeletePartitionedView( coll, vName ) );
}


// The convention is that each view verifies its constraint before accepting
// an ad.  However, the view assumes that in the case of view partitions, the
// parent view will have identified the correct child partition to use.
bool View::
ClassAdInserted( ClassAdCollectionServer *coll, const string &key, 
	ClassAd *ad )
{
	PartitionedViews::iterator	partition;
	string						signature;
	ViewMember						vm;
	bool 							match, rval=true;
	Value							rankValue;
	View							*childView;

		// check if the ad satisfies the view's constraint
	if( !evalEnviron.ReplaceRightAd( ad ) || 
		!evalEnviron.EvaluateAttrBool( "RightMatchesLeft", match ) ) {
		evalEnviron.RemoveRightAd( );
		return( false );
	}

		// if the constraint was not satisfied, the ad can be ignored
	if( !match ) {
		evalEnviron.RemoveRightAd( );
		return( true );
	}

		// obtain the rank value of the ad
	if( !evalEnviron.EvaluateAttr( "LeftRankValue", rankValue ) ) {
		evalEnviron.RemoveRightAd( );
		return( false );
	}
	evalEnviron.RemoveRightAd( );

		// insert into every subordinate child view
	SubordinateViews::iterator	xi;
	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		rval = rval && (*xi)->ClassAdInserted( coll, key, ad );
	}

		// find partition to insert into
	signature = makePartitionSignature( ad );
	if( !signature.empty( ) ) {
		partition = partitionedViews.find( signature );
		if( partition == partitionedViews.end( ) ) {
				// no appropriate partition --- create one and insert
			if( ( childView = new View( this ) ) == 0 || 
				!coll->RegisterView( viewName + ":" + signature, childView ) ) {
				if( childView ) delete childView;
				return( false );
			}
			partitionedViews[signature] = childView;
		} else {
			childView = partitionedViews[signature];
		}

			// update the partition
		rval = rval && childView->ClassAdInserted( coll, key, ad );
	}

		// insert ad into list of view members and update index
	vm.SetKey( key );
	vm.SetRankValue( rankValue );
	memberIndex[key] = viewMembers.insert( vm );

	return( rval );
}



bool View::
ClassAdPreModify( ClassAdCollectionServer *coll, ClassAd *ad )
{
	SubordinateViews::iterator	xi;
	PartitionedViews::iterator	mi;
	bool							rval = true;

		// stash signature of ad
	oldAdSignature = makePartitionSignature( ad );

		// perform premodify on all subordinate children ...
	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		rval = rval && (*xi)->ClassAdPreModify( coll, ad );
	}

		// ... and partition children
	for( mi = partitionedViews.begin( ); mi != partitionedViews.end( ); mi++ ) {
		rval = rval && mi->second->ClassAdPreModify( coll, ad );
	}

	return( rval );
}


bool View::
ClassAdModified( ClassAdCollectionServer *coll, const string &key, 
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
	if( !evalEnviron.ReplaceRightAd( mad ) || 
		!evalEnviron.EvaluateAttrBool( "RightMatchesLeft", match ) ||
		!evalEnviron.EvaluateAttr( "LeftRankValue", rankValue ) ) {
		evalEnviron.RemoveRightAd( );
		return( false );
	}
	evalEnviron.RemoveRightAd( );

	if( wasMember && match ) {
		string 	sig;
			// was and still is a member; check if rank has changed
		Operation::Operate( IS_OP, rankValue, oldAdRank, equal );
		if( !equal.IsBooleanValue( sameRank ) || !sameRank ) {
				// rank changed ... need to re-order
			ViewMember	vm;
				// remove old view member ...
			vm.SetRankValue( oldAdRank );
			vm.SetKey( key );
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
					oldAdSignature.erase( oldAdSignature.begin( ), 
						oldAdSignature.end( ) );
					return( false );
				}
					// delete from old partition
				rval = mi->second->ClassAdDeleted( coll, key, mad );
			}

				// is there a partition with the new signature?
			if( !sig.empty( ) ) {
				mi = partitionedViews.find( sig );
				if( mi != partitionedViews.end( ) ) {
						// yes ... insert into this partition
					rval = rval && mi->second->ClassAdInserted(coll, key, mad);
				} else {
						// no ... create a new partition
					if( ( newPartition = new View( this ) ) == 0 ) {
						oldAdSignature.erase( oldAdSignature.begin( ), 
							oldAdSignature.end( ) );
						return( false );
					} 
					if( !coll->RegisterView( viewName+":"+sig, newPartition ) ){
						delete newPartition;
						return( false );
					}
					rval=rval && newPartition->ClassAdInserted(coll, key, mad);
					partitionedViews[sig] = newPartition;
				}
			}
		}

			// send modification notification to all subordinate children
		SubordinateViews::iterator xi;
		for( xi=subordinateViews.begin( ); xi!=subordinateViews.end( ); xi++ ){
			rval = rval && (*xi)->ClassAdModified( coll, key, mad );
		}
	} else if( !wasMember && match ) {
			// wasn't a member, but now is --- insert the ad
		rval = ClassAdInserted( coll, key, mad );
	} else if( wasMember && !match ) {
			// was a member, but now isnt --- delete the ad
		rval = ClassAdDeleted( coll, key, mad );
	} else {
		// wasn't a member and still isn't --- nothing to do
	}

	oldAdSignature.erase( oldAdSignature.begin( ), oldAdSignature.end( ) );
	return( rval );
}


bool View::
ClassAdDeleted( ClassAdCollectionServer *coll, const string &key, 
	ClassAd *ad )
{
	ViewMembers::iterator	vmi;
	bool					rval = true;

		// check if the classad is in the view
	if( memberIndex.find( key ) == memberIndex.end( ) ) {
			// not in view; nothing to do
		return( true );
	}

		// delete from member index and member list
	vmi = memberIndex[key];
	memberIndex.erase( key );
	viewMembers.erase( vmi );

		// delete from every subordinate child view
	SubordinateViews::iterator	xi;
	for( xi = subordinateViews.begin( ); xi != subordinateViews.end( ); xi++ ) {
		rval = rval && (*xi)->ClassAdDeleted( coll, key, ad );
	}

		// delete from partition view
	string signature = makePartitionSignature( ad );
	if( !signature.empty( ) ) {
		PartitionedViews::iterator mi = partitionedViews.find( signature );
		if( mi == partitionedViews.end( ) ) {
				// an internal error
			return( false );
		}

			// delete from the partition
		rval = rval && mi->second->ClassAdDeleted( coll, key, ad );
	}

	return( rval );
}


string View::
makePartitionSignature( ClassAd *ad )
{
	ExprListIterator	itr;
    Sink    	snk;
    char    	*tmp=NULL;
    int     	alen=0;
    Value   	value;
	bool		error = false;
	ClassAd		*oad, *info;
	ExprList	*el;

		// stash the old ad from the environment and insert the new ad in
	oad = evalEnviron.RemoveRightAd( );
	evalEnviron.ReplaceRightAd( ad );
	if( !( info = evalEnviron.GetLeftAd( ) ) ) {
		evalEnviron.RemoveRightAd( );
		return( string( "ERROR" ) );
	}
	if( !info->Lookup( "PartitionExprs" ) ) {
			// no partitioning
		evalEnviron.RemoveRightAd( );
		return( string( "" ) );
	}
	if( !info->EvaluateAttr( "PartitionExprs", value ) ||
			!value.IsListValue( el ) ) {
		evalEnviron.RemoveRightAd( );
		return( string( "ERROR" ) );
	}

		// go through the expression list and form a value vector
    snk.SetSink( tmp, alen, false );
	if( !snk.SendToSink( "<|", 2 ) ) { 
		error = true;	
	}
	itr.Initialize( el );
	while( itr.NextValue( value ) ) {
		if( !value.ToSink( snk ) || !snk.SendToSink( "|", 1 ) ) {
			error = true;
			break;
        }
    }
	if( !snk.SendToSink( ">",1 ) ) {
		error = true;
	}
    snk.FlushSink( );

		// put the old ad back in the environmnet
	evalEnviron.RemoveRightAd( );
	evalEnviron.ReplaceRightAd( oad );

		// if there's an error, return the null string
	if( error ) {
		if( tmp ) delete [] tmp;
		return( string( "ERROR" ) );
	} 

		// return the computed signature
   	string signature = tmp;
   	delete [] tmp;
    return( signature );
}


bool View::
extractPartitionAttrs( vector<string> &attrVec, ExprTree *attrExpr )
{
	ExprList		*el;
	ExprListIterator itr;
	Value			val;
	char			*strVal;
	int				i;

	if( attrExpr->GetKind( ) != EXPR_LIST_NODE ) {
		return( false );
	}
	el = (ExprList *) attrExpr;
	attrVec.resize( el->Number( ) );

	itr.Initialize( el );
	i = 0;
	while( itr.NextValue( val ) ) {
		if( !val.IsStringValue( strVal ) ) {
			return( false );
		}
		attrVec[i] = string( strVal );
		i++;
	}

	return( true );
}


bool View::
Display( FILE *file )
{
	ViewMembers::iterator	vmi;
	Sink					snk;
	Value					val;
	string					key;
	FormatOptions			fo;
	ClassAd					*viewInfo;

	snk.SetSink( file );
	fo.SetClassAdIndentation( );
	snk.SetFormatOptions( &fo );
	snk.SetTerminalChar( '\n' );

		// display view info
	viewInfo = GetViewInfo( );
	if( !viewInfo || !viewInfo->ToSink( snk ) ) {
		if( viewInfo ) delete viewInfo;
		return( false );
	}
	snk.FlushSink( );
	delete viewInfo;
	for( vmi = viewMembers.begin( ); vmi != viewMembers.end( ); vmi++ ) {
		vmi->GetKey( key );
		vmi->GetRankValue( val );
		if( !val.ToSink( snk ) || !snk.SendToSink( " ", 1 ) ||
				!snk.SendToSink( (void*)key.c_str( ), key.size( ) ) ||
				!snk.FlushSink( ) ) {
			return( false );
		}
	}

	return( true );
}


bool ViewMemberLT::
operator()( const ViewMember &vm1, const ViewMember &vm2 ) const
{
	Value		val1, val2;
	ValueType	vt1, vt2;

	vm1.GetRankValue( val1 );
	vm2.GetRankValue( val2 );

	vt1 = val1.GetType( );
	vt2 = val2.GetType( );
	if( vt1 < vt2 ) {
		return( true );
	}

	if( vt1 == vt2 && vt1 != CLASSAD_VALUE && vt2 != LIST_VALUE ) {
		Value	result;
		bool	lessThan;
		Operation::Operate( LESS_THAN_OP, val1, val2, result );
		return( result.IsBooleanValue( lessThan ) && lessThan );
	}

	return( false );
}


END_NAMESPACE
