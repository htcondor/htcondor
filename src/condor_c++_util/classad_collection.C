#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "classad_collection.h"

static char *_FileName_ = __FILE__;
static bool makePartitionHashKey( ClassAd *, StringSet &, MyString & );

BaseCollection::
BaseCollection(const MyString& rank) : childItors( 4 ), contentItors( 4 )
{
    ClassAd *ad = new ClassAd( );
    if( ad ) {
        if( rank.Value() && strcmp( rank.Value(), "" ) != 0 ) {
            ad->Insert( ATTR_RANK, rank.Value( ) );
        } else {
            ad->InsertAttr( ATTR_RANK, 0 );
        }
    }
    rankCtx.ReplaceLeftAd( ad );
	childItors.fill( NULL );
	contentItors.fill( NULL );
}

BaseCollection::
BaseCollection(ExprTree *tree) : childItors( 4 ), contentItors( 4 )
{
    ClassAd *ad = new ClassAd( );
    if( ad ) {
        if( tree ) {
            ad->Insert( ATTR_RANK, tree->Copy( ) );
        } else {
            ad->InsertAttr( ATTR_RANK, 0 );
        }
    }
    rankCtx.ReplaceLeftAd( ad );
	childItors.fill( NULL );
	contentItors.fill( NULL );
}

BaseCollection::
~BaseCollection( )
{
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
RegisterChildItor( CollChildIterator *itor )
{
	int last = childItors.getlast( );
	for( int i = 0 ; i <= last ; i++ ) {
		if( childItors[i] == itor ) return;
	}
	childItors[last+1] = itor;
}


void BaseCollection::
UnregisterChildItor( CollChildIterator *itor )
{
	int last = childItors.getlast( );
	for( int i = 0; i <= last; i++ ) {
		if( childItors[i] == itor ) {
			childItors[i] = childItors[last];
			childItors[last] = NULL;
		}
	}
}


void BaseCollection::
RegisterContentItor( CollContentIterator *itor )
{
	int last = contentItors.getlast( );
	for( int i = 0 ; i <= last ; i++ ) {
		if( contentItors[i] == itor ) return;
	}
	contentItors[last+1] = itor;
}


void BaseCollection::
UnregisterContentItor( CollContentIterator *itor )
{
	int last = contentItors.getlast( );
	for( int i = 0; i <= last; i++ ) {
		if( contentItors[i] == itor ) {
			contentItors[i] = contentItors[last];
			contentItors[last] = NULL;
		}
	}
}


ConstraintCollection::
ConstraintCollection(const MyString& rank, const MyString& constraint)
	: BaseCollection(rank)
{
	ClassAd *ad = new ClassAd( );
	ad->Insert( ATTR_REQUIREMENTS, constraint.Value( ) );
	constraintCtx.ReplaceLeftAd( ad );
}

ConstraintCollection::
ConstraintCollection(ExprTree *rank, ExprTree *constraint)
	: BaseCollection(rank)
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

//-----------------------------------------------------------------------
// Constructor (initialization)
//-----------------------------------------------------------------------

ClassAdCollection::ClassAdCollection(const char* filename) 
  : Collections(97, HashFunc), ClassAdLog(filename)
{
	LastCoID=0;
	Collections.insert(LastCoID,new ExplicitCollection("",true));

	HashKey HK;
	char key[_POSIX_PATH_MAX];
	ClassAd* Ad;
	table.startIterations();
	while(table.iterate(HK,Ad)) {
		HK.sprint(key);
		AddClassAd(0,key,Ad);
	}
}

//-----------------------------------------------------------------------
// Constructor (initialization)
//-----------------------------------------------------------------------

ClassAdCollection::ClassAdCollection() 
  : Collections(97, HashFunc), ClassAdLog()
{
  	LastCoID=0;
  	Collections.insert(LastCoID,new ExplicitCollection("",true));
}

//-----------------------------------------------------------------------
// Destructor - frees the memory used by the collections
//-----------------------------------------------------------------------

ClassAdCollection::~ClassAdCollection() {
  	DeleteCollection(0);
}

//-----------------------------------------------------------------------
/** Insert a new Class Ad -
 this inserts a classad with a given key, type, and target type. The class ad is
inserted as an empty class ad (use SetAttribute to set its attributes).
This operation is logged. The class ad is inserted into the root collection,
and into its children if appropriate.
Input: key - the unique key for the ad
       mytype - the ad's type
       targettype - the target type
Output: true on success, false otherwise
*/
//-----------------------------------------------------------------------

bool ClassAdCollection::NewClassAd(const char* key)
{
  	LogRecord* log=new LogNewClassAd(key);
  	ClassAdLog::AppendLog(log);
  	return AddClassAd(0,key);
}

//-----------------------------------------------------------------------

bool ClassAdCollection::NewClassAd(const char* key, ClassAd* ad)
{
	LogRecord 		*log=new LogNewClassAd(key);
	ExprTree 		*expr;
	char 			*name;
	ClassAdIterator itor;

	ClassAdLog::AppendLog(log);
	itor.Initialize( *ad );
	while( itor.NextAttribute( name, expr ) ) {
		LogRecord* log=new LogSetAttribute(key,name,expr);
		ClassAdLog::AppendLog(log);
	}
	return AddClassAd(0,key);
}

//-----------------------------------------------------------------------
/** Delete a Class Ad
Delete a class ad. The ad is deleted from all the collections. This operation
is logged. 
Input: key - the class ad's key.
Output: true on success, false otherwise
*/
//-----------------------------------------------------------------------

bool ClassAdCollection::DestroyClassAd(const char *key)
{
	LogRecord* log=new LogDestroyClassAd(key);
	ClassAdLog::AppendLog(log);
	return RemoveClassAd(0,key);
}

//-----------------------------------------------------------------------
/** Change attribute of a class ad - this operation is logged.
Any effects on collection membership will also take place.
Input: key - the class ad's key
       name - the attribute name
       value - the attribute value
Output: true on success, false otherwise
*/
//-----------------------------------------------------------------------

bool ClassAdCollection::SetAttribute(const char *key, const char *name, 
	ExprTree *expr)
{
	LogRecord* log=new LogSetAttribute(key,name,expr);
	ClassAdLog::AppendLog(log);
	return ChangeClassAd(key);
}

bool ClassAdCollection::SetAttribute(const char *key, const char *name, const
	char *expr, int len )
{
	Source 	src;
	ExprTree *tree;
	src.SetSource( expr, len );
	if( !src.ParseExpression( tree ) || !SetAttribute( key, name, tree ) ) {
		return false;
	}
	delete tree;
	return true;
}

	


//-----------------------------------------------------------------------
/** Delete an attribute of a class ad - this operation is logged.
Any effects on collection membership will also take place.
Input: key - the class ad's key
       name - the attribute name
Output: true on success, false otherwise
*/
//-----------------------------------------------------------------------

bool ClassAdCollection::DeleteAttribute(const char *key, const char *name)
{
	LogRecord* log=new LogDeleteAttribute(key,name);
	ClassAdLog::AppendLog(log);
	return ChangeClassAd(key);
}

//-----------------------------------------------------------------------
/** Create an Explicit Collection - create a new collection as a child of
an exisiting collection. This operation, as well as other collection management
operation is not logged.
Input: ParentCoID - the ID of the parent collection
       Rank - the rank expression for the collection
       FullFlag - indicates wether ads which are added to the parent should 
	   	automatically be added to the child
Output: the new collectionID
*/
//-----------------------------------------------------------------------

int ClassAdCollection::CreateExplicitCollection(int ParentCoID, 
	const MyString& Rank, bool FullFlag)
{
	// Lookup the parent
	BaseCollection* Parent;
	if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

	// Initialize and insert to collection table
	ExplicitCollection* Coll=new ExplicitCollection(Rank,FullFlag);
	int CoID=LastCoID+1;
	if (Collections.insert(CoID,Coll)==-1) return -1;
	LastCoID=CoID;

	// Add to parent's children
	Parent->Children.Add(CoID);

	// Add Parents members to new collection
	RankedClassAd RankedAd;
	Parent->Members.StartIterations();
	while(Parent->Members.Iterate(RankedAd)) {
		AddClassAd(CoID,RankedAd.OID);
	}
	return CoID;
}

//-----------------------------------------------------------------------
/** Create a constraint Collection - create a new collection as a child of
an exisiting collection. This operation, as well as other collection management
operation is not logged.
Input: ParentCoID - the ID of the parent collection
       Rank - the rank expression for the collection
       Constraint - the constraint expression (must result in a boolean)
Output: the new collectionID
*/
//-----------------------------------------------------------------------

int ClassAdCollection::CreateConstraintCollection(int ParentCoID, 
	const MyString& Rank, const MyString& Constraint)
{
	// Lookup the parent
	BaseCollection* Parent;
	if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

	// Initialize and insert to collection table
	ConstraintCollection* Coll=new ConstraintCollection(Rank,Constraint);
	int CoID=LastCoID+1;
	if (Collections.insert(CoID,Coll)==-1) return -1;
	LastCoID=CoID;

	// Add to parent's children
	Parent->Children.Add(CoID);

	// Add Parents members to new collection
	RankedClassAd RankedAd;
	Parent->Members.StartIterations();
	while(Parent->Members.Iterate(RankedAd)) {
		AddClassAd(CoID,RankedAd.OID);
	}

	return CoID;
}

//-----------------------------------------------------------------------

int ClassAdCollection::CreatePartition(int ParentCoID, const MyString& Rank, 
	StringSet& AttrList)
{
	// Lookup the parent
	BaseCollection* Parent;
	if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

	// Initialize and insert to collection table
	PartitionParent* Coll=new PartitionParent(Rank,AttrList);
	int CoID=LastCoID+1;
	if (Collections.insert(CoID,Coll)==-1) return -1;
	LastCoID=CoID;

	// Add to parent's children
	Parent->Children.Add(CoID);

	// Add Parents members to new collection
	RankedClassAd RankedAd;
	Parent->Members.StartIterations();
	while(Parent->Members.Iterate(RankedAd)) {
		AddClassAd(CoID,RankedAd.OID);
	}

	return CoID;
}

int ClassAdCollection::CreatePartition(int ParentCoID, ExprTree *Rank, 
	StringSet& AttrList)
{
	// Lookup the parent
	BaseCollection* Parent;
	if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

	// Initialize and insert to collection table
	PartitionParent* Coll=new PartitionParent(Rank,AttrList);
	int CoID=LastCoID+1;
	if (Collections.insert(CoID,Coll)==-1) return -1;
	LastCoID=CoID;

	// Add to parent's children
	Parent->Children.Add(CoID);

	// Add Parents members to new collection
	RankedClassAd RankedAd;
	Parent->Members.StartIterations();
	while(Parent->Members.Iterate(RankedAd)) {
		AddClassAd(CoID,RankedAd.OID);
	}

	return CoID;
}


int ClassAdCollection::FindPartition( int ParentCoID, ClassAd *rep )
{
	BaseCollection 	*Parent;
	PartitionParent *partitionParent;
	MyString		partitionValues;
	int				CoID;

	if( Collections.lookup( ParentCoID, Parent ) == -1 ) return -1;
	partitionParent = (PartitionParent*) Parent;
	if(!makePartitionHashKey(rep,partitionParent->Attributes,partitionValues)){
		return -1;
	}

	if( partitionParent->childPartitions.lookup( partitionValues, CoID )==-1 ) {
		return( -1 );
	}

	return( CoID );
}


//-----------------------------------------------------------------------
/** Delete a collection
Input: CoID - the ID of the collection
Output: true on success, false otherwise
*/
//-----------------------------------------------------------------------

bool ClassAdCollection::DeleteCollection(int CoID)
{
	return TraverseTree(CoID,&ClassAdCollection::RemoveCollection);
}

//-----------------------------------------------------------------------
/// Include Class Ad in a collection (private method)
//-----------------------------------------------------------------------

bool ClassAdCollection::AddClassAd(int CoID, const MyString& OID)
{
	// Get the ad
	ClassAd* Ad;
	if (table.lookup(HashKey(OID.Value()),Ad)==-1) return false;

	return AddClassAd(CoID,OID,Ad);
}

//-----------------------------------------------------------------------
/// Include Class Ad in a collection (private method)
//-----------------------------------------------------------------------

bool ClassAdCollection::AddClassAd(int CoID, const MyString& OID, ClassAd* Ad)
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(CoID,Coll)==-1) return false;

	// Check if ad matches the collection
	if (!CheckClassAd(Coll,OID,Ad)) return false;

	// Create the Ranked Ad
	RankedClassAd RankedAd(OID, Coll->GetRankValue( Ad ));
	if (Coll->Members.Exist(RankedAd)) return false;

	// Insert the add in the correct place in the list of members
	RankedClassAd CurrRankedAd;
	bool Inserted=false;
	Coll->Members.StartIterations();
	while (Coll->Members.Iterate(CurrRankedAd)) {
		if (RankedAd.Rank<=CurrRankedAd.Rank) {
			Coll->Members.Insert(RankedAd);
			Inserted=true;
			break;
		}
	}
	if (!Inserted) Coll->Members.Insert(RankedAd);

	// Insert into chldren
	int ChildCoID;
	BaseCollection* ChildColl;
	Coll->Children.StartIterations();
	while (Coll->Children.Iterate(ChildCoID)) {
		AddClassAd(ChildCoID,OID,Ad);
	}

	return true;
}

//-----------------------------------------------------------------------

bool ClassAdCollection::CheckClassAd(BaseCollection* Coll,const MyString& OID, 
	ClassAd* Ad) 
{
	PartitionParent* 	ParentColl=(PartitionParent*) Coll;
	MyString			PartitionValues;

  	if (Coll->Type()!=PartitionParent_e) {
		return Coll->CheckClassAd(Ad);
	}

	if( !makePartitionHashKey( Ad, ParentColl->Attributes, PartitionValues ) ) {
		return( false );
	}

	/*
		printf("AttrValue=%s\n",Values.Value()); 
	*/

	int CoID;
	PartitionChild* ChildColl=NULL;
	if( ParentColl->childPartitions.lookup( PartitionValues, CoID ) == -1 ) {
			// no such child partition; create a new child partition
		ChildColl=new PartitionChild(ParentColl->GetRankExpr(),PartitionValues);
		CoID=LastCoID+1;
		if (Collections.insert(CoID,ChildColl)==-1) return false;
		if( ParentColl->childPartitions.insert( PartitionValues, CoID )==-1 ) {
			return( false );
		}
		LastCoID=CoID;

			// Add to parent's children
		ParentColl->Children.Add(CoID);
	} 

	// Add to child
	AddClassAd(CoID,OID,Ad);
	return false;
}

//-----------------------------------------------------------------------

bool ClassAdCollection::EqualSets(StringSet& S1, StringSet& S2) 
{
	MyString OID1;
	MyString OID2;

	S1.StartIterations();
	S2.StartIterations();

	while(S1.Iterate(OID1)) {
		if (!S2.Iterate(OID2)) return false;
		if (OID1!=OID2) return false;
	}
	return (!S2.Iterate(OID2));
}

//-----------------------------------------------------------------------
/// Remove Class Ad from a collection (private method)
//-----------------------------------------------------------------------

bool ClassAdCollection::RemoveClassAd(int CoID, const MyString& OID)
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(CoID,Coll)==-1) return false;

	// Check if ad is in the collection and remove it
	if( !Coll->Members.Exist(RankedClassAd(OID)) && 
		Coll->Type()!=PartitionParent_e) {
		return false;
	}
	Coll->Members.Remove(RankedClassAd(OID));

	// remove from children
	int ChildCoID;
	BaseCollection* ChildColl;
	Coll->Children.StartIterations();
	while (Coll->Children.Iterate(ChildCoID)) {
		RemoveClassAd(ChildCoID,OID);
	}

	return true;
}

//-----------------------------------------------------------------------
/// Change a class-ad (private method)
//-----------------------------------------------------------------------

bool ClassAdCollection::ChangeClassAd(const MyString& OID)
{
	RemoveClassAd(0,OID);
	return AddClassAd(0,OID);
}

//-----------------------------------------------------------------------
/// Remove a collection (private method)
//-----------------------------------------------------------------------

bool ClassAdCollection::RemoveCollection(int CoID, BaseCollection* Coll)
{
	delete Coll;
	if (Collections.remove(CoID)==-1) return false;
	return true;
}

//-----------------------------------------------------------------------
/// Start iterating on cllection IDs
//-----------------------------------------------------------------------

void ClassAdCollection::StartIterateAllCollections()
{
	Collections.startIterations();
}

//-----------------------------------------------------------------------
/// Get the next collection ID
//-----------------------------------------------------------------------

bool ClassAdCollection::IterateAllCollections(int& CoID)
{
	BaseCollection* Coll;
	if (Collections.iterate(CoID,Coll)) return true;
	return false;
}

//-----------------------------------------------------------------------
/// Start iterating on child collections of some parent collection
//-----------------------------------------------------------------------

bool ClassAdCollection::StartIterateChildCollections(int ParentCoID)
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(ParentCoID,Coll)==-1) return false;

	Coll->Children.StartIterations();
	return true;
}

//-----------------------------------------------------------------------
/// Get the next child collection
//-----------------------------------------------------------------------

bool ClassAdCollection::IterateChildCollections(int ParentCoID, int& CoID)
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(ParentCoID,Coll)==-1) return false;

	if (Coll->Children.Iterate(CoID)) return true;
	return false;
}

//-----------------------------------------------------------------------
/// Start iterating on class ads in a collection
//-----------------------------------------------------------------------

bool ClassAdCollection::StartIterateClassAds(int CoID)
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(CoID,Coll)==-1) return false;

	Coll->Members.StartIterations();
	return true;
}

bool ClassAdCollection::IterateClassAds( int CoID, ClassAd*& ad )
{
	RankedClassAd ra;
	if( !IterateClassAds( CoID, ra ) ) return false;
	if( table.lookup( HashKey( ra.OID.Value() ), ad ) == -1 ) return false;
	return true;
}

bool ClassAdCollection::IterateClassAds( int CoID, char *key )
{
	RankedClassAd ra;
	if( !IterateClassAds( CoID, ra ) ) return false;
	strcpy( key, ra.OID.Value( ) );
	return( true );
}
//-----------------------------------------------------------------------
/// Get the next class ad and rank in the collection
//-----------------------------------------------------------------------

bool ClassAdCollection::IterateClassAds(int CoID, RankedClassAd& RankedAd)
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(CoID,Coll)==-1) return false;

	if (Coll->Members.Iterate(RankedAd)) return true;
	return false;
}

//-----------------------------------------------------------------------
/// Find a collection's type
//-----------------------------------------------------------------------

int ClassAdCollection::GetCollectionType(int CoID)
{
	BaseCollection* Coll;
	if (Collections.lookup(CoID,Coll)==-1) return -1;
	return Coll->Type();
}

//-----------------------------------------------------------------------
/// Traverse the collection tree
//-----------------------------------------------------------------------

bool ClassAdCollection::TraverseTree(int CoID, 
	bool (ClassAdCollection::*Func)(int,BaseCollection*))
{
	BaseCollection* CurrNode;
	int ChildCoID;
	if (Collections.lookup(CoID,CurrNode)==-1) return false;
	CurrNode->Children.StartIterations();
	while (CurrNode->Children.Iterate(ChildCoID)) {
	  if (!TraverseTree(ChildCoID,Func)) return false;
	}
	return (this->*Func)(CoID,CurrNode);
}

//-----------------------------------------------------------------------
/// Print out all the collections
//-----------------------------------------------------------------------
  
void ClassAdCollection::Print()
{
  	RankedClassAd RankedAd;
  	BaseCollection* Coll;
  	int 		CoID;
  	MyString 	OID;
  	int 		ChildCoID;
	Sink		snk;

  	printf("-----------------------------------------\n");
  	Collections.startIterations();
  	while (Collections.iterate(CoID,Coll)) {
    	printf("CoID=%d Type=%d Rank=",CoID,Coll->Type());

			// print out the rank expression
    	ExprTree *tree = Coll->GetRankExpr();
		snk.SetSink( stdout );
		tree->ToSink( snk );
		snk.FlushSink( );
		printf( "\n" );

    	printf("Children: ");
    	Coll->Children.StartIterations();
    	while (Coll->Children.Iterate(ChildCoID)) {
			printf("%d ",ChildCoID);
		}

    	printf("\nMembers: ");
    	Coll->Members.StartIterations();
    	while(Coll->Members.Iterate(RankedAd)) {
			printf("%s(%.1f) ",RankedAd.OID.Value(),RankedAd.Rank);
		}
    	printf("\n-----------------------------------------\n");
  	}
}

//-----------------------------------------------------------------------
/// Print out a single collection
//-----------------------------------------------------------------------
  
void ClassAdCollection::Print(int CoID)
{
  	RankedClassAd RankedAd;
  	BaseCollection* Coll;
  	MyString 	OID;
  	int 		ChildCoID;

  	if (Collections.lookup(CoID,Coll)==-1) return;
  	printf("-----------------------------------------\n");

  	printf("CoID=%d Type=%d Rank=",CoID,Coll->Type());

	ExprTree	*tree = Coll->GetRankExpr( );
	Sink		sink;
	sink.SetSink( stdout );
	tree->ToSink( sink );
	sink.FlushSink( );
	printf( "\n" );

  	printf("Children: ");
  	Coll->Children.StartIterations();
  	while (Coll->Children.Iterate(ChildCoID)) {
		printf("%d ",ChildCoID);
	}

  	printf("\nMembers: ");
  	Coll->Members.StartIterations();
  	while(Coll->Members.Iterate(RankedAd)) {
		printf("%s(%.1f) ",RankedAd.OID.Value(),RankedAd.Rank);
	}

  printf("\n-----------------------------------------\n");
}

int
partitionHashFcn( const MyString &str, int numBkts )
{
	const char *hstr = str.Value( );
	int	  	len = strlen( hstr );
	int		hashVal = 0;

	for( int i = 0 ; i < len ; i++ ) {
		hashVal += hstr[i];
	}
	return( hashVal % numBkts );
}

static bool
makePartitionHashKey( ClassAd *ad, StringSet &attrs, MyString &val )
{
	Sink	snk;
	char	*tmp=NULL;
	int		alen=0;
	MyString attrName;

	snk.SetSink( tmp, alen, false );
	attrs.StartIterations( );
	while (attrs.Iterate(attrName)) {
			// we should perhaps serialize the flattened expression
		ExprTree* expr=ad->Lookup(attrName.Value());
		if (!expr) return( false );
		expr->ToSink( snk );
	}
	snk.FlushSink( );
	val = tmp;
	delete [] tmp;
	return( true );
}

CollChildIterator::
CollChildIterator( )
{
	baseCollection = NULL;
	collManager = NULL;
	collID 		= -1;
	status		= COLL_ITOR_INVALID;
}

CollChildIterator::
CollChildIterator( const CollChildIterator& i )
{
	baseCollection = i.baseCollection;
	collManager = i.collManager;
	collID		= i.collID;
	itor		= i.itor;
	status		= i.status;

		// if we're copying from a valid itor, register the new itor
	if( !( status & COLL_ITOR_INVALID ) ) {
		baseCollection->RegisterChildItor( this );
	}
}


CollChildIterator::
~CollChildIterator( )
{
		// unregister to remove the iterator from the active itor list
	if( !( status & COLL_ITOR_INVALID ) ) {
		baseCollection->UnregisterChildItor( this );
	}
}


int CollChildIterator::
CurrentCollection( int &childID )
{
	if( !( status & COLL_ITOR_OK ) ) return( status );
	if( !itor.Current( childID ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
}


int CollChildIterator::
NextCollection( int &childID )
{
	if( ( status & COLL_ITOR_OK ) || ( status & COLL_BEFORE_START ) ) {
		if( !itor.Next( childID ) ) status |= COLL_AT_END;
	}
	status &= ~COLL_BEFORE_START;
	return( status );
}


void CollChildIterator::
initialize( ClassAdCollection *cm, BaseCollection* bc, int ID, 
	const IntegerSet& set )
{
	if( ( bc != NULL ) && ( ID >= 0 ) ) {
		status = COLL_BEFORE_START;
		baseCollection 	= bc;
		collManager		= cm;
		collID = ID;
		itor.Initialize( set );
	} else {
		collManager = NULL;
		baseCollection = NULL;
		collID = -1;
		status = COLL_ITOR_INVALID;
	}
}

void CollChildIterator::
updateForInsertion( )
{
	status |= COLL_ITEM_ADDED;
}

void CollChildIterator::
updateForDeletion( int delID )
{
	int cur;
	status |= COLL_ITEM_REMOVED;
	if( status & COLL_BEFORE_START ) {
		return;
	} else if( status & COLL_ITOR_OK ) {
		if( !itor.Current( cur ) ) { 
			EXCEPT( "Should not reach here" );
		}
		if( cur == delID ) {
			int dummy;
			status |= COLL_ITOR_MOVED;
			if( !itor.Next( dummy ) ) {
				status |= COLL_AT_END;
				status &= ~COLL_ITOR_OK;
			}
		}
	} else {
		status |= COLL_AT_END;
		status &= ~COLL_ITOR_OK;
	}
}

void CollChildIterator::
invalidate( )
{
	status = COLL_ITOR_INVALID;
	collID = -1;
	collManager = NULL;
	baseCollection = NULL;
}



CollContentIterator::
CollContentIterator( )
{
	baseCollection = NULL;
	collManager = NULL;
	collID 		= -1;
	status		= COLL_ITOR_INVALID;
}

CollContentIterator::
CollContentIterator( const CollContentIterator& i )
{
	baseCollection = i.baseCollection;
	collManager = i.collManager;
	collID		= i.collID;
	itor		= i.itor;
	status		= i.status;

		// if we're copying from a valid itor, register the new itor
	if( !( status & COLL_ITOR_INVALID ) ) {
		baseCollection->RegisterContentItor( this );
	}
}


CollContentIterator::
~CollContentIterator( )
{
		// unregister to remove the iterator from the active itor list
	if( !( status & COLL_ITOR_INVALID ) ) {
		baseCollection->UnregisterContentItor( this );
	}
}


int CollContentIterator::
CurrentAd( const ClassAd *&classad ) const
{
	RankedClassAd 	ra;
	ClassAd			*ad;
	if( !( status & COLL_ITOR_OK ) ) return( status );
	if( !itor.Current( ra ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
	if( collManager->table.lookup( HashKey( ra.OID.Value() ), ad ) == -1 ) {
		EXCEPT( "Should not reach here" );
	}
	classad = ad;
	return( status );
}


int CollContentIterator::
CurrentAdKey( char *key ) const
{
	RankedClassAd ra;
	if( !( status & COLL_ITOR_OK ) ) return( status );
	if( !itor.Current( ra ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
	strcpy( key, ra.OID.Value( ) );
	return( status );
}

int CollContentIterator::
CurrentAdRank( double &rank ) const
{
	RankedClassAd ra;
	if( !( status & COLL_ITOR_OK ) ) return( status );
	if( !itor.Current( ra ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
	rank = ra.Rank;
	return( status );
}


int CollContentIterator::
NextAd( const ClassAd *&classad )
{
	RankedClassAd	ra;
	ClassAd			*ad;

	if( ( status & COLL_ITOR_OK ) || ( status & COLL_BEFORE_START ) ) {
		if( !itor.Next( ra ) ) status |= COLL_AT_END;
	}
	if( collManager->table.lookup( HashKey( ra.OID.Value() ), ad ) == -1 ) {
		EXCEPT( "Should not reach here" );
	}
	status &= ~COLL_BEFORE_START;
	classad = ad;
	return( status );
}


void CollContentIterator::
initialize( ClassAdCollection *cm, BaseCollection* bc, int ID, 
	const RankedAdSet& set )
{
	if( ( bc != NULL ) && ( ID >= 0 ) ) {
		status = COLL_BEFORE_START;
		collManager = cm;
		baseCollection = bc;
		collID = ID;
		itor.Initialize( set );
	} else {
		baseCollection = NULL;
		collManager = NULL;
		collID = -1;
		status = COLL_ITOR_INVALID;
	}
}

void CollContentIterator::
updateForInsertion( )
{
	status |= COLL_ITEM_ADDED;
}

void CollContentIterator::
updateForDeletion( const RankedClassAd &ra )
{
	RankedClassAd cur;
	status |= COLL_ITEM_REMOVED;
	if( status & COLL_BEFORE_START ) {
		return;
	} else if( status & COLL_ITOR_OK ) {
		if( !itor.Current( cur ) ) { 
			EXCEPT( "Should not reach here" );
		}
		if( cur == ra ) {
			RankedClassAd ra;
			status |= COLL_ITOR_MOVED;
			if( !itor.Next( ra ) ) {
				status |= COLL_AT_END;
				status &= ~COLL_ITOR_OK;
			}
		}
	} else {
		status |= COLL_AT_END;
		status &= ~COLL_ITOR_OK;
	}
}

void CollContentIterator::
invalidate( )
{
	status = COLL_ITOR_INVALID;
	collID = -1;
	collManager = NULL;
	baseCollection = NULL;
}
