#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "common.h"
#include "exprTree.h"
#include "matchClassad.h"
#include "classad_collection_types.h"

BaseCollection::
BaseCollection(BaseCollection* parent, const MyString& rank) : childItors( 4 ), contentItors( 4 )
{
    Parent=parent;
    ClassAd *ad = new ClassAd( );
    if( ad ) {
        if( rank.Value() && strcmp( rank.Value(), "" ) != 0 ) {
            ad->Insert( ATTR_RANK, rank.Value( ) );
        } else {
            ad->InsertAttr( ATTR_RANK, 0 );
        }
    }
	lastChildItor = 0;
	lastContentItor = 0;
    rankCtx.ReplaceLeftAd( ad );
	childItors.fill( NULL );
	contentItors.fill( NULL );
}

BaseCollection::
BaseCollection(BaseCollection* parent, ExprTree *tree) : childItors( 4 ), contentItors( 4 )
{
    Parent=parent;
    ClassAd *ad = new ClassAd( );
    if( ad ) {
        if( tree ) {
            ad->Insert( ATTR_RANK, tree->Copy( ) );
        } else {
            ad->InsertAttr( ATTR_RANK, 0 );
        }
    }
	lastChildItor = 0;
	lastContentItor = 0;
    rankCtx.ReplaceLeftAd( ad );
	childItors.fill( NULL );
	contentItors.fill( NULL );
}

BaseCollection::
~BaseCollection( )
{
        // invalidate active iterators
    for( int i = 0 ; i < lastChildItor ; i++ ) {
        if( childItors[i] ) childItors[i]->invalidate( );
    }
    for( int i = 0 ; i < lastContentItor ; i++ ) {
        if( contentItors[i] ) contentItors[i]->invalidate( );
    }
}

ExprTree *BaseCollection::
GetRankExpr()
{
    ClassAd *ad = rankCtx.GetLeftAd( );
    return( ad ? ad->Lookup( ATTR_RANK ) : NULL );
}

double BaseCollection::
GetRankValue( ClassAd* ad )
{
    double d;
	rankCtx.ReplaceRightAd( ad );
    if( !rankCtx.EvaluateAttrNumber( "leftrankvalue", d ) ) {
        rankCtx.RemoveRightAd( );
        return -1;
    }
    rankCtx.RemoveRightAd( );
    return d;
}


void BaseCollection::
RegisterChildItor( CollChildIterator *itor, ClassAdCollection *cm, int ID )
{
    bool found = false;
    if( !itor ) return;
    for( int i = 0 ; i < lastChildItor ; i++ ) {
        if( childItors[i] == itor ) {
            found = true;
            break;
        }
    }
    if( !found ) {
        childItors[lastChildItor] = itor;
        lastChildItor++;
    }
    itor->initialize( cm, this, ID, Children );
}

void BaseCollection::
UnregisterChildItor( CollChildIterator *itor )
{
    if( !itor ) return;
    for( int i = 0; i < lastChildItor; i++ ) {
        if( childItors[i] == itor ) {
            lastChildItor--;
            childItors[i] = childItors[lastChildItor];
            childItors[lastChildItor] = NULL;
            return;
        }
    }
}


void BaseCollection::
RegisterContentItor( CollContentIterator *itor, ClassAdCollection *cm, int ID )
{
    bool found = false;
    if( !itor ) return;
    for( int i = 0 ; i < lastContentItor ; i++ ) {
        if( contentItors[i] == itor ) {
            found = true;
            break;
        }
    }
    if( !found ) {
        contentItors[lastContentItor] = itor;
        lastContentItor++;
    }
    itor->initialize( cm, this, ID, Members );
}


void BaseCollection::
UnregisterContentItor( CollContentIterator *itor )
{
    if( !itor ) return;
    for( int i = 0; i < lastContentItor; i++ ) {
        if( contentItors[i] == itor ) {
            lastContentItor--;
            contentItors[i] = contentItors[lastContentItor];
            contentItors[lastContentItor] = NULL;
            return;
        }
    }
}

void BaseCollection::
NotifyContentItorsInsertion( )
{
    for( int i = 0 ; i < lastContentItor ; i++ ) {
        if( contentItors[i] ) {
            contentItors[i]->updateForInsertion( );
        }
    }
}

void BaseCollection::
NotifyContentItorsDeletion( const RankedClassAd &ad )
{
    for( int i = 0 ; i < lastContentItor ; i++ ) {
        if( contentItors[i] ) {
            contentItors[i]->updateForDeletion( ad );
        }
    }
}

void BaseCollection::
NotifyChildItorsInsertion( )
{
    for( int i = 0 ; i < lastChildItor ; i++ ) {
        if( childItors[i] ) {
            childItors[i]->updateForInsertion( );
        }
    }
}

void BaseCollection::
NotifyChildItorsDeletion( int item )
{
    for( int i = 0 ; i < lastChildItor ; i++ ) {
        if( childItors[i] ) {
            childItors[i]->updateForDeletion( item );
        }
    }
}


ConstraintCollection::
ConstraintCollection(BaseCollection* parent, const MyString& rank, const MyString& constraint)
	: BaseCollection(parent,rank)
{
	ClassAd *ad = new ClassAd( );
	ad->Insert( ATTR_REQUIREMENTS, constraint.Value( ) );
	constraintCtx.ReplaceLeftAd( ad );
}

ConstraintCollection::
ConstraintCollection(BaseCollection* parent,ExprTree *rank, ExprTree *constraint)
	: BaseCollection(parent,rank)
{
	if( constraint && rank ) {
		ClassAd *ad = new ClassAd( );
		ad->Insert( ATTR_REQUIREMENTS, constraint->Copy( ) );
		constraintCtx.ReplaceLeftAd( ad );
	} else {
		constraint = rank = NULL;
	}
}

bool ConstraintCollection::
CheckClassAd(ClassAd* Ad)
{
	bool rval, b;
	Value val;
	constraintCtx.ReplaceRightAd( Ad );
	rval = ( constraintCtx.EvaluateAttr( "rightmatchesleft", val ) &&
		 val.IsBooleanValue( b ) && b );
	constraintCtx.RemoveRightAd( );
	return( rval );
}

bool PartitionChild::
CheckClassAd(ClassAd*)
{
	return true;
}

