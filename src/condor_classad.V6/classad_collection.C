#include "condor_common.h"
#include "condor_debug.h"

#include "classad_collection.h"
#include "classad_collection_ops.h"

static char *_FileName_ = __FILE__;
static int  hashFunction( const MyString&, int );

//-----------------------------------------------------------------------
// Constructor (initialization)
//-----------------------------------------------------------------------

ClassAdCollection::ClassAdCollection(const char* filename, ExprTree* rank)
  : Collections(97, HashFunc), table( 1024, hashFunction )
{
    log_filename[0] = '\0';
    active_transaction = NULL;
    log_fp = NULL;
    LastCoID=0;
    Collections.insert(LastCoID,new ExplicitCollection(NULL,rank,true));
    if (filename) ReadLog(filename);
}

//-----------------------------------------------------------------------

ClassAdCollection::ClassAdCollection(const char* filename, const MyString& rank)
  : Collections(97, HashFunc), table( 1024, hashFunction )
{
    log_filename[0] = '\0';
    active_transaction = NULL;
    log_fp = NULL;
    LastCoID=0;
    Collections.insert(LastCoID,new ExplicitCollection(NULL,rank,true));
    if (filename) ReadLog(filename);
}

//-----------------------------------------------------------------------

bool ClassAdCollection::ReadLog(const char *filename)
{
    strcpy(log_filename, filename);
    int fd;

        // open the file with open to get the permissions right (in case the
        // the file is being created)  then wrap a FILE  around it
    if( ( fd = open(log_filename, O_RDWR | O_CREAT, 0600) ) < 0 ) {
        dprintf(D_ALWAYS, "failed to open log %s, errno = %d", log_filename, 
			errno);
		return( false );
    }
    if( ( log_fp = fdopen( fd, "r+" ) ) == NULL ) {
        dprintf( D_ALWAYS, "failed to fdopen() log %s, errno = %d", 
			log_filename, errno );
		return( false );
    }

        // Read all of the log records
    LogRecord   *log_rec;
    while ((log_rec = ReadLogEntry(log_fp)) != 0) {
        switch (log_rec->get_op_type()) {
        case CondorLogOp_BeginTransaction:
            if (active_transaction) {
                dprintf(D_ALWAYS, "Warning: Encountered nested transactions "
                    "in %s, log may be bogus...", filename);
            } else {
                active_transaction = new Transaction();
            }
            delete log_rec;
            break;

        case CondorLogOp_EndTransaction:
            if (!active_transaction) {
                dprintf(D_ALWAYS, "Warning: Encountered unmatched end "
                    "transaction in %s, log may be bogus...", filename);
            } else {
                    // commit in memory only
                active_transaction->Commit(NULL, (void*)this);
                delete active_transaction;
                active_transaction = NULL;
            }
            delete log_rec;
            break;

        default:
            if (active_transaction) {
                active_transaction->AppendLog(log_rec);
            } else {
                log_rec->Play((void *)this);
                delete log_rec;
            }
        }
    }
    if (active_transaction) {   // abort incomplete transaction
        delete active_transaction;
        active_transaction = NULL;
    }
	TruncLog();

	return( true );
}

//-----------------------------------------------------------------------------

ClassAdCollection::~ClassAdCollection()
{
  	DeleteCollection(0);

    if (active_transaction) delete active_transaction;

    // HashTable class will not delete the ClassAd pointers we have
    // inserted, so we delete them here...
    table.startIterations();
    ClassAd *ad;
    HashKey key;
    while (table.iterate(key, ad) == 1) {
        delete ad;
    }
}

//-----------------------------------------------------------------------------

bool
ClassAdCollection::AppendLog(LogRecord *log)
{
	bool rval;

    if (active_transaction) {
        if (EmptyTransaction) {
            LogBeginTransaction *log = new LogBeginTransaction;
            active_transaction->AppendLog(log);
            EmptyTransaction = false;
        }
        active_transaction->AppendLog(log);
		return( true );
    } else {
        if( log_fp ) {
            if (!log->Write(log_fp)) {
                EXCEPT("write to %s failed, errno = %d", log_filename, errno);
            }
            fflush( log_fp );
            if (fsync( fileno( log_fp ) ) < 0) {
                EXCEPT("fsync of %s failed, errno = %d", log_filename, errno);
            }
        }
        rval = log->Play((void *)this);
        delete log;
		return( rval );
    }
}

//----------------------------------------------------------------------------

bool
ClassAdCollection::TruncLog()
{
    char    tmp_log_filename[_POSIX_PATH_MAX];
    int     new_log_fd;
    FILE    *new_log_fp;

	if (!log_fp) return( false );

    sprintf(tmp_log_filename, "%s.tmp", log_filename);
    new_log_fd = open(tmp_log_filename, O_RDWR | O_CREAT | O_TRUNC , 0600);
    if (new_log_fd < 0) {
        dprintf(D_ALWAYS, "failed to truncate log: open(%s) returns %d\n",
                tmp_log_filename, new_log_fd);
        return false;
    }
    if( ( new_log_fp = fdopen( new_log_fd, "r+" ) ) == NULL ) {
        dprintf( D_ALWAYS, "failed to truncate log: fdopen(%s) failed, "
            "errno = %d\n", tmp_log_filename, errno );
        return false;
    }

    LogState(new_log_fp);
    fclose(log_fp);
    fclose(new_log_fp);
#if defined(WIN32)
    if (MoveFileEx(tmp_log_filename, log_filename, MOVEFILE_REPLACE_EXISTING)
            == 0) {
        dprintf(D_ALWAYS, "failed to truncate log: MoveFileEx failed with "
            "error %d\n", GetLastError());
        return false;
    }
#else
    if (rename(tmp_log_filename, log_filename) < 0) {
        dprintf(D_ALWAYS, "failed to truncate log: rename(%s, %s) returns "
                "errno %d", tmp_log_filename, log_filename, errno);
        return( false );
    }
#endif

    if( ( log_fp = fopen( log_filename, "a+" ) ) == NULL ) {
        EXCEPT( "Failed to reopen %s, errno = %d", log_filename, errno );
    }

	return( false );
}

//----------------------------------------------------------------------------

void
ClassAdCollection::BeginTransaction()
{
    assert(!active_transaction);
    active_transaction = new Transaction();
    EmptyTransaction = true;
}

//----------------------------------------------------------------------------

void
ClassAdCollection::AbortTransaction()
{
    // Sometimes we do an AbortTransaction() when we don't know if there was
    // an active transaction.  This is allowed.
    if (active_transaction) {
        delete active_transaction;
        active_transaction = NULL;
    }
}

//----------------------------------------------------------------------------
// Commit a transaction
//----------------------------------------------------------------------------

bool
ClassAdCollection::CommitTransaction()
{
    assert(active_transaction);
    if (!EmptyTransaction) {
        LogEndTransaction *log = new LogEndTransaction;
        active_transaction->AppendLog(log);
        if( !active_transaction->Commit(log_fp, (void*)this) ) return( false );
    }
    delete active_transaction;
    active_transaction = NULL;
	return( true );
}

//----------------------------------------------------------------------------
// Lookup a class-ad
//----------------------------------------------------------------------------

bool ClassAdCollection::IsActiveTransaction()
{
	return (active_transaction!=NULL);
}

//----------------------------------------------------------------------------
// Lookup a class-ad
//----------------------------------------------------------------------------

ClassAd* ClassAdCollection::LookupClassAd(const char* key)
{
    ClassAd* Ad=NULL;
    table.lookup(HashKey(key), Ad);
    return Ad;
}

//----------------------------------------------------------------------------
// Lookup a class-ad including transaction updates (returns a new ad)
//----------------------------------------------------------------------------

ClassAd* ClassAdCollection::LookupInTransaction(const char *key)
{
	ClassAd* Ad=LookupClassAd(key);
	bool Duplicated=false;
	ClassAd* tmpAd;
    if (active_transaction) {
	    for (LogRecord *log = active_transaction->FirstEntry(); log;
			log = active_transaction->NextEntry(log)) {
			switch (log->get_op_type()) {
				case CondorLogOp_CollNewClassAd:
					if (strcmp(((LogCollNewClassAd*)log)->get_key(),key)==0) {
						if (Duplicated) {
							delete Ad;
							Duplicated=false;
						}
						Ad=((LogCollNewClassAd*)log)->get_ad();
					}
					break;
				case CondorLogOp_CollUpdateClassAd:
					if(strcmp(((LogCollUpdateClassAd*)log)->get_key(),key)==0){
						if (!Duplicated) {
							Ad=new ClassAd(*Ad);
							Duplicated=false;
						}
						tmpAd=((LogCollUpdateClassAd*)log)->get_ad();
						Ad->Update(*tmpAd);
					}
					break;
				case CondorLogOp_CollModifyClassAd:
					if(strcmp(((LogCollModifyClassAd*)log)->get_key(),key)==0){
						if( !Duplicated ) {
							Ad = new ClassAd( *Ad );
							Duplicated = false;
						}
						tmpAd = ((LogCollModifyClassAd*)log)->get_ad();
						Ad->Modify( *tmpAd );
					}
					break;
				case CondorLogOp_CollDestroyClassAd:
					if(strcmp(((LogCollDestroyClassAd*)log)->get_key(),key)==0){
						if (Duplicated) {
							delete Ad;
							Duplicated=false;
						}
						Ad=NULL;
					}
					break;
				default:
					break;
			}
		}
	}
	if (!Duplicated && Ad) Ad=new ClassAd(*Ad);
	return Ad;
}

//----------------------------------------------------------------------------

void
ClassAdCollection::LogState( FILE *fp )
{
    LogRecord   *log=NULL;
    ClassAd     *ad=NULL;
    HashKey     hashval;
    char        key[_POSIX_PATH_MAX];
    ClassAdIterator itor;

    table.startIterations();
    while(table.iterate(ad) == 1) {
        table.getCurrentKey(hashval);
        strcpy( key, hashval.Value() );
        log = new LogCollNewClassAd(key,ad);
        if (!log->Write(fp)) {
            EXCEPT("write to %s failed, errno = %d", log_filename, errno);
        }
        delete log;
    }
    if (fsync(fileno(fp)) < 0) {
        EXCEPT("fsync of %s failed, errno = %d", log_filename, errno);
    }
}

//----------------------------------------------------------------------------

LogRecord* ClassAdCollection::ReadLogEntry(FILE* fp)
{
    LogRecord       *log_rec;
    LogRecord       head_only;

    if (!head_only.ReadHeader(fp)) return NULL;
    log_rec = InstantiateLogEntry(fp, head_only.get_op_type());
    if (!head_only.ReadTail(fp)) {
        delete log_rec;
        return NULL;
    }
    return log_rec;
}

//----------------------------------------------------------------------------

LogRecord* ClassAdCollection::InstantiateLogEntry(FILE* fp, int type)
{
    LogRecord   *log_rec;

    switch(type) {
        case CondorLogOp_CollNewClassAd:
            log_rec = new LogCollNewClassAd(NULL, NULL);
            break;
        case CondorLogOp_CollDestroyClassAd:
            log_rec = new LogCollDestroyClassAd(NULL);
            break;
        case CondorLogOp_CollUpdateClassAd:
            log_rec = new LogCollUpdateClassAd(NULL, NULL);
            break;
		case CondorLogOp_CollModifyClassAd:
			log_rec = new LogCollModifyClassAd(NULL, NULL);
			break;
        case CondorLogOp_BeginTransaction:
            log_rec = new LogBeginTransaction();
            break;
        case CondorLogOp_EndTransaction:
            log_rec = new LogEndTransaction();
            break;
        default:
            return NULL;
            break;
    }
    if (!log_rec->ReadBody(fp)) {
		delete log_rec;
		return NULL;
    }
    return log_rec;
}

//-----------------------------------------------------------------------

bool ClassAdCollection::GetRankValue(char* key, float& rank_value, int CoID=0)
{
  BaseCollection* Coll=GetCollection(CoID);
  if (!Coll) return false;
  RankedClassAd RankedAd(key);
  if (Coll->Members.GetKey(RankedAd)!=1) return false;
  rank_value=RankedAd.Rank;
  return true;
}

//-----------------------------------------------------------------------

int ClassAdCollection::Size(int CoID)
{
  BaseCollection* coll=GetCollection(CoID);
  if (!coll) return -1;
  return coll->MemberCount();
}

//-----------------------------------------------------------------------

void ClassAdCollection::NewClassAd(const char* key, ClassAd* ad)
{
  	LogRecord* log=new LogCollNewClassAd(key,ad);
  	AppendLog(log);
}

//-----------------------------------------------------------------------

bool ClassAdCollection::PlayNewClassAd(const char* key, ClassAd* ad)
{
    HashKey hkey(key);
    ClassAd* dummy;
    if( table.lookup(hkey, dummy) == 0 ) {
        // Class-ad already exists in collection!
        return( false );
    }
    return( table.insert(key, ad) >= 0 && AddClassAd(0,key,ad) );
}

//-----------------------------------------------------------------------
// Delete a Class Ad
//-----------------------------------------------------------------------

void ClassAdCollection::DestroyClassAd(const char *key)
{
	LogRecord* log=new LogCollDestroyClassAd(key);
	AppendLog(log);
}

//-----------------------------------------------------------------------

bool ClassAdCollection::PlayDestroyClassAd(const char* key)
{
    HashKey hkey(key);
    ClassAd *ad;
    if (table.lookup(hkey, ad) < 0) {
        return( false );
    }
    table.remove(hkey);
    RemoveClassAd(0,key);
    delete ad;
	return( true );
}

//-----------------------------------------------------------------------
// Change attribute of a class ad - this operation is logged.
//-----------------------------------------------------------------------

void ClassAdCollection::UpdateClassAd(const char *key, ClassAd* ad)
{
	LogRecord* log=new LogCollUpdateClassAd(key,ad);
	AppendLog(log);
}

//-----------------------------------------------------------------------

bool ClassAdCollection::PlayUpdateClassAd(const char *key, ClassAd* upd_ad)
{
    ClassAd* ad;
    if (table.lookup(HashKey(key), ad) < 0) {
		if( !( ad=new ClassAd(*upd_ad) ) || !PlayNewClassAd( key,ad ) ) {
			return( false );
		}
	}
    ad->Update(*upd_ad);
    ChangeClassAd(key);
	return( true );
}

//-----------------------------------------------------------------------
// Modify a class ad - this operation is logged.
//-----------------------------------------------------------------------
void ClassAdCollection::ModifyClassAd( ClassAd *ad )
{
	char *key;
	if( ad->EvaluateAttrString( ATTR_KEY, key ) ) {
		ModifyClassAd( key, ad );
	}
}

void ClassAdCollection::ModifyClassAd( const char *key, ClassAd *mod_ad )
{
	LogRecord *log = new LogCollModifyClassAd( key, mod_ad );
	AppendLog( log );
}


void ClassAdCollection::QueryCollection( int CoID, ClassAd *query, Sink &s )
{
	CollConstrContentItor	itor;
	bool					wantPrelude;
	bool					wantList;
	bool					wantResults;
	bool					wantPostlude;
	ExprList				*projectionAttrs;
	Value					value;
	ClassAd					*ad;
	ByteSink				*bs;
	bool					first;

		// sanity checks
	if( !query || ( ( bs = s.GetSink( ) ) == NULL ) ) {
		return;
	}

		// options check and default setttings
	if( !query->EvaluateAttrBool( ATTR_WANT_LIST, wantList ) ) {
		wantList = false;
	}
	if( !query->EvaluateAttrBool( ATTR_WANT_PRELUDE, wantPrelude ) ) {
		wantPrelude = false;
	}
	if( !query->EvaluateAttrBool( ATTR_WANT_RESULTS, wantResults ) ) {
		wantResults = true;
	}
	if( !query->EvaluateAttrBool( ATTR_WANT_POSTLUDE, wantPostlude ) ) {
		wantPostlude = true;
	}
	if( !query->EvaluateAttr( ATTR_PROJECT_THROUGH, value ) || 
		!value.IsListValue( projectionAttrs ) ) {
		projectionAttrs = NULL;
	}

	if( wantList && !bs->PutBytes( "{", 1 ) ) {
		return;
	}

	itor.RegisterQuery( query );
	InitializeIterator( CoID, &itor );
	first = true;
	while( !itor.AtEnd( ) ) {
		itor.CurrentAd( ad );
		if( wantList && !first && !bs->PutBytes( ",", 1 ) ) {
			return;
		}
		if( !ad->ToSink( s ) ) {
			return;
		}
		itor.NextAd( );
		first = false;
	}

	if( wantList && !bs->PutBytes( "}", 1 ) ) {
		return;
	}

	return;
}


bool ClassAdCollection::PlayModifyClassAd( const char *key, ClassAd *mod_ad )
{
	ClassAd *ad;
	bool	del;
	Value	val;

	if( !mod_ad ) return( false );

	if( mod_ad->EvaluateAttr(ATTR_NEW_AD,val) && val.IsClassAdValue(ad) ) {
		PlayNewClassAd( key, ad->Copy( ) );
		return( true );
	}

	if( mod_ad->EvaluateAttrBool( ATTR_DELETE_AD, del ) && del ) {
		PlayDestroyClassAd( key );
		return( true );
	}
		
	if( table.lookup( HashKey( key ), ad ) < 0 ) {
		if( !( ad = new ClassAd( ) ) || !PlayNewClassAd( key, ad ) ) {
			return( false );
		}
	}
	ad->Modify( *mod_ad );
	ChangeClassAd( key );
	return( true );
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
	ConstraintCollection* Coll=new ConstraintCollection(Parent,Rank,Constraint);
	int CoID=LastCoID+1;
	if (Collections.insert(CoID,Coll)==-1) return -1;
	LastCoID=CoID;

	// Add to parent's children
	Parent->Children.Add(CoID);
	Parent->NotifyChildItorsInsertion( );

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
	PartitionParent* Coll=new PartitionParent(Parent,Rank,AttrList);
	int CoID=LastCoID+1;
	if (Collections.insert(CoID,Coll)==-1) return -1;
	LastCoID=CoID;

	// Add to parent's children
	Parent->Children.Add(CoID);
	Parent->NotifyChildItorsInsertion( );

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
	PartitionParent* Coll=new PartitionParent(Parent,Rank,AttrList);
	int CoID=LastCoID+1;
	if (Collections.insert(CoID,Coll)==-1) return -1;
	LastCoID=CoID;

	// Add to parent's children
	Parent->Children.Add(CoID);
	Parent->NotifyChildItorsInsertion( );

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
    BaseCollection* Coll;
    if (Collections.lookup(CoID,Coll)==-1) return false;
	BaseCollection* ParentColl=Coll->GetParent();
    if (ParentColl) {
      ParentColl->Children.Remove(CoID);
    }
    
	return (TraverseTree(CoID,&ClassAdCollection::RemoveCollection));
      
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
	if (!CheckClassAd(CoID,Coll,OID,Ad)) return false;

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
			Coll->NotifyContentItorsInsertion( );
			Inserted=true;
			break;
		}
	}
	if (!Inserted) {
		Coll->Members.Insert(RankedAd);
		Coll->NotifyContentItorsInsertion( );
	}

	// Insert into chldren
	int ChildCoID;
	Coll->Children.StartIterations();
	while (Coll->Children.Iterate(ChildCoID)) {
		AddClassAd(ChildCoID,OID,Ad);
	}

	return true;
}

//-----------------------------------------------------------------------

bool ClassAdCollection::CheckClassAd(int CoID, BaseCollection* Coll,
	const MyString& OID, ClassAd* Ad) 
{
	PartitionParent* 	ParentColl=(PartitionParent*) Coll;
	MyString			PartitionValues;

  	if (Coll->Type()!=PartitionParent_e) {
		return Coll->CheckClassAd(Ad);
	}

	if( !makePartitionHashKey( Ad, ParentColl->Attributes, PartitionValues ) ) {
		return( false );
	}

	PartitionChild* ChildColl=NULL;
	if( ParentColl->childPartitions.lookup( PartitionValues, CoID ) == -1 ) {
			// no such child partition; create a new child partition
		ChildColl=new PartitionChild(ParentColl,ParentColl->GetRankExpr(),
										PartitionValues);
		CoID=LastCoID+1;
		if (Collections.insert(CoID,ChildColl)==-1) return false;
		if( ParentColl->childPartitions.insert( PartitionValues, CoID )==-1 ) {
			return( false );
		}
		LastCoID=CoID;

			// Add to parent's children
		ParentColl->Children.Add(CoID);
		ParentColl->NotifyChildItorsInsertion( );
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
	RankedClassAd	rad( OID );

	if (Collections.lookup(CoID,Coll)==-1) return false;

	// Check if ad is in the collection and remove it
	if( !Coll->Members.Exist( rad ) && 
		Coll->Type()!=PartitionParent_e) {
		return false;
	}
	Coll->NotifyContentItorsDeletion( rad );
	Coll->Members.Remove( rad );

	// remove from children
	int ChildCoID;
	Coll->Children.StartIterations();
	while (Coll->Children.Iterate(ChildCoID)) {
		RemoveClassAd(ChildCoID,OID);
	}

	return true;
}

//-----------------------------------------------------------------------

BaseCollection* ClassAdCollection::GetCollection(int CoID) {
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return NULL;
  return Coll;
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
	Coll->NotifyChildItorsDeletion( CoID );
	delete Coll;
	if (Collections.remove(CoID)==-1) return false;
	return true;
}

//-----------------------------------------------------------------------
/// Start iterating on class ads in a collection
//-----------------------------------------------------------------------
bool ClassAdCollection::
InitializeIterator( int CoID, CollContentIterator* itor )
{
    BaseCollection *bc;

    if( !itor || Collections.lookup( CoID, bc ) == -1 ) return false;
    bc->RegisterContentItor( itor, this, CoID );
		// spool iterator once to get it out of BEFORE_START state
	itor->NextAd( );
    return( true );
}

bool ClassAdCollection::
InitializeIterator( int CoID, CollChildIterator* itor )
{
    BaseCollection *bc;
    if( !itor || Collections.lookup( CoID, bc ) == -1 ) return false;
    bc->RegisterChildItor( itor, this, CoID );
		// spool iterator once to get it out of BEFORE_START state
	itor->NextCollection( );
    return( true );
}


bool ClassAdCollection::StartIterateClassAds(int CoID)
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(CoID,Coll)==-1) return false;

	Coll->Members.StartIterations();
	return true;
}

//-----------------------------------------------------------------------

bool ClassAdCollection::IterateClassAds( char* key, int CoID )
{
	RankedClassAd ra;
	if( !IterateClassAds( ra, CoID ) ) return false;
	strcpy( key, ra.OID.Value( ) );
	return( true );
}

//-----------------------------------------------------------------------

bool ClassAdCollection::IterateClassAds( RankedClassAd& RankedAd, int CoID )
{
	// Get collection pointer
	BaseCollection* Coll;
	if (Collections.lookup(CoID,Coll)==-1) return false;

	if (Coll->Members.Iterate(RankedAd)) return true;
	return false;
}

//----------------------------------------------------------------------------------
// Start iterating on child collections of some parent collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::StartIterateChildCollections(int ParentCoID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(ParentCoID,Coll)==-1) return false;
 
  Coll->Children.StartIterations();
  return true;
}

//----------------------------------------------------------------------------------
// Get the next child collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::IterateChildCollections(int& CoID, int ParentCoID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(ParentCoID,Coll)==-1) return false;
 
  if (Coll->Children.Iterate(CoID)) return true;
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

	ExprTree		*tree = Coll->GetRankExpr( );
	Sink			sink;

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

//-----------------------------------------------------------------------
/// Print all the ads in the collection
//-----------------------------------------------------------------------
  
void ClassAdCollection::PrintAllAds()
{
	HashKey 	HK;
	char 		key[_POSIX_PATH_MAX];
	ClassAd		*Ad;
	Sink 		s;
	FormatOptions fo;

	s.SetSink(stdout);
	fo.SetClassAdIndentation( true );
	s.SetFormatOptions( &fo );

	table.startIterations();
	while(table.iterate(HK,Ad)) {
		strcpy( key, HK.Value() );
		printf("Key=%s : ",key);
		Ad->ToSink(s);
		s.FlushSink();
		printf("\n");
	}
}

//-----------------------------------------------------------------------
  
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

bool ClassAdCollection::
makePartitionHashKey( ClassAd *ad, StringSet &attrs, MyString &val )
{
	Sink	snk;
	char	*tmp=NULL;
	int		alen=0;
	Value	value;
	MyString attrName;
	ExprTree *tree;

	snk.SetSink( tmp, alen, false );
	attrs.StartIterations( );
	while (attrs.Iterate(attrName)) {
		if( ( tree = ad->Lookup( attrName.Value() ) ) == NULL ||
			!tree->Evaluate( value ) 	||
			!value.ToSink( snk ) 	|| 
			!snk.SendToSink( "@@", 2 ) ) {
				return( false );
		}
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
		baseCollection->RegisterChildItor( this, collManager, collID );
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


bool CollChildIterator::
CurrentCollection( int &childID )
{
	if( !( status & COLL_ITOR_OK ) ) return( false );

        // clear out old flags
    status &= ~( COLL_ITOR_MOVED | COLL_ITEM_ADDED | COLL_ITEM_REMOVED );

	if( !itor.Current( childID ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
	return( true );
}


int CollChildIterator::
NextCollection( int &childID )
{
    if( status & COLL_ITOR_INVALID ) {
        status = COLL_ITOR_INVALID;
        return( 0 );
    }

    if( ( status & COLL_ITOR_OK ) || ( status & COLL_BEFORE_START ) ) {
            // clear out old flags
        status &= ~( COLL_ITOR_MOVED | COLL_ITEM_ADDED | COLL_ITEM_REMOVED );
        if( !itor.Next( childID ) ) {
            status |= COLL_AT_END;
            status &= ~COLL_ITOR_OK;
            status &= ~COLL_BEFORE_START;
            return( 0 );
        } else {
            status |= COLL_ITOR_OK;
        }
    }
    return( ( status == COLL_ITOR_OK ) ? +1 : -1 );
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
		baseCollection->RegisterContentItor( this, collManager, collID );
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


bool CollContentIterator::
CurrentAd( const ClassAd *&classad )
{
	RankedClassAd 	ra;
	ClassAd			*ad;
	if( !( status & COLL_ITOR_OK ) ) return( false );

        // clear out old flags
    status &= ~( COLL_ITOR_MOVED | COLL_ITEM_ADDED | COLL_ITEM_REMOVED );

	if( !itor.Current( ra ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
	if( collManager->table.lookup( HashKey( ra.OID.Value() ), ad ) == -1 ) {
		EXCEPT( "Should not reach here" );
	}
	classad = ad;
	return( true );
}


bool CollContentIterator::
CurrentAdKey( char *key )
{
	RankedClassAd ra;
	if( !( status & COLL_ITOR_OK ) ) return( status );

        // clear out old flags
    status &= ~( COLL_ITOR_MOVED | COLL_ITEM_ADDED | COLL_ITEM_REMOVED );

	if( !itor.Current( ra ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
	strcpy( key, ra.OID.Value( ) );
	return( true );
}

bool CollContentIterator::
CurrentAdRank( double &rank )
{
	RankedClassAd ra;
	if( !( status & COLL_ITOR_OK ) ) return( false );

        // clear out old flags
    status &= ~( COLL_ITOR_MOVED | COLL_ITEM_ADDED | COLL_ITEM_REMOVED );

	if( !itor.Current( ra ) ) { 
		EXCEPT( "Should not reach here" ); 
	}
	rank = ra.Rank;
	return( true );
}


int CollContentIterator::
NextAd( const ClassAd *&classad )
{
    RankedClassAd   ra;
    ClassAd         *ad;

    if( status & COLL_ITOR_INVALID ) {
        status = COLL_ITOR_INVALID;
        return( 0 );
    }

    if( ( status & COLL_ITOR_OK ) || ( status & COLL_BEFORE_START ) ) {
        if( !itor.Next( ra ) ) {
            status |= COLL_AT_END;
            status &= ~COLL_ITOR_OK;
            status &= ~COLL_BEFORE_START;
            return( 0 );
        }
            // clear out old flags
        status &= ~( COLL_ITOR_MOVED | COLL_ITEM_ADDED | COLL_ITEM_REMOVED );
    }
    if( collManager->table.lookup( HashKey( ra.OID.Value() ), ad ) == -1 ) {
        EXCEPT( "Should not reach here" );
    }
    status &= ~COLL_BEFORE_START;
    status |= COLL_ITOR_OK;
    classad = ad;
    return( ( status == COLL_ITOR_OK ) ? +1 : -1 );
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


CollConstrContentItor::
CollConstrContentItor( )
{
	RegisterConstraint( "true" );
}


CollConstrContentItor::
CollConstrContentItor( const CollConstrContentItor& i ) : mad( i.mad ) 
{
}


CollConstrContentItor::
CollConstrContentItor( const char *c )
{
	RegisterConstraint( c );
}


CollConstrContentItor::
CollConstrContentItor( ExprTree *c )
{
	ClassAd *ad = mad.GetLeftAd( );
	if( ad ) ad->Insert( ATTR_REQUIREMENTS, c );
}


CollConstrContentItor::
~CollConstrContentItor( )
{
}


bool CollConstrContentItor::
RegisterConstraint( const char *c )
{
	ClassAd *ad = mad.GetLeftAd( );
	return( ad ? ad->Insert( ATTR_REQUIREMENTS, c ) : false );
}


bool CollConstrContentItor::
RegisterConstraint( ExprTree *c )
{
	ClassAd *ad = mad.GetLeftAd( );
	return( ad ? ad->Insert( ATTR_REQUIREMENTS, c ) : false );
}


bool CollConstrContentItor::
RegisterQuery( ClassAd *q )
{
	if( !q ) return( false );
	return( mad.ReplaceLeftAd( q ) );
}


int CollConstrContentItor::
NextAd( const ClassAd *&res )
{
	int 	rval;
	bool	match, matched = false;
	ClassAd	*ad=NULL;

	while( !matched && ( rval = CollContentIterator::NextAd( ad ) ) ) {
		mad.ReplaceRightAd( ad );
		matched = mad.EvaluateAttrBool( "rightmatchesleft", match ) && match;
		mad.RemoveRightAd( );
	}
	res = ad;
	return( rval );
}


int hashFunction( const MyString& key, int numBkts )
{
	const char *val = key.Value( );
	int 	i = 0, hashVal = 0;

	while( val[i] ) {
		hashVal += val[i];
		i++;
	}
	return( hashVal % numBkts );
}
