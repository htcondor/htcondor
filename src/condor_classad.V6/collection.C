#include "condor_common.h"
#include "collection.h"
#include "collection_ops.h"
#include "log_transaction.h"

BEGIN_NAMESPACE( classad )

ClassAdCollectionServer::
ClassAdCollectionServer( const string &filename )
	: viewTree( NULL )
{
		// create "root" view
	viewTree.SetViewName( "root" );
	viewTree.SetRankExpr( this, "undefined" );
	RegisterView( "root", &viewTree );

	log_fp = NULL;
	sink   = NULL;
	logFileName = filename;
	if( !logFileName.empty( ) ) {
		if( !ReadLogFile( ) || !( sink = new Sink ) ) {
			EXCEPT( "could not recover state" );
		}
		sink->SetSink( log_fp );
	}
}


bool ClassAdCollectionServer::
SetRootRankExpr( ExprTree *rankExpr )
{
	return( viewTree.SetRankExpr( this, rankExpr ) );
}


bool ClassAdCollectionServer::
SetRootRankExpr( const string &rankExpr )
{
	return( viewTree.SetRankExpr( this, rankExpr ) );
}


ClassAdCollectionServer::
~ClassAdCollectionServer( )
{
	if( log_fp ) fclose( log_fp );
	if( sink   ) delete( sink );

		// clear out classad table
	ClassAdTable::iterator	ci;
	for( ci = classadTable.begin( ); ci != classadTable.end( ); ci++ ) {
		delete ci->second.ad;
	}
	classadTable.clear( );

		// clear out transaction table
	XactionTable::iterator	xti;
	for( xti = xactionTable.begin( ); xti != xactionTable.end( ); xti++ ) {
		delete xti->second;
	}
	xactionTable.clear( );
}


bool ClassAdCollectionServer::
ReadLogFile( )
{
	int 	fd;
	Source	source;

		// open the file and wrap a source around it
	if( ( fd = open( logFileName.c_str( ), O_RDWR | O_CREAT, 0600 ) ) < 0 ) {
		dprintf( D_ALWAYS, "failed to open log %s, errno =i %d\n", 
			logFileName.c_str( ), errno );
		return( false );
	}
	if( ( log_fp = fdopen( fd, "r+" ) ) == NULL ) {
		dprintf( D_ALWAYS, "failed to fdopen(%d) file %s, errno=%d\n", fd,
			logFileName.c_str( ), errno );
		close( fd );
		return( false );
	}
	if( !source.SetSource( log_fp ) ) {
		dprintf( D_ALWAYS, "failed to set file %s as a source\n", 
			logFileName.c_str( ) );
		fclose( log_fp );
		return( false );
	}

		// read log records
	CollectionLogRecord	*logRec;
	while( ( logRec = ReadLogEntry( &source ) ) != 0 ) {
		if( !ExecuteLogRecord( logRec ) ) {
			return( false );
		}
	}

	return( true );
}


bool ClassAdCollectionServer::
ExecuteLogRecord( CollectionLogRecord *logRec ) 
{
	string	xactionName;
	int 		opType;

	if( !Check( logRec ) ) {
		dprintf( D_ALWAYS, "log record failed check\n" );
		delete logRec;
		return( false );
	}

	if( !logRec->EvaluateAttrInt( "OpType", opType ) ) {
		dprintf( D_ALWAYS, "log record has no 'OpType'\n" );
		delete logRec;
		return( false );
	}

	if( !logRec->GetStringAttr( "XactionName", xactionName ) ) {
		dprintf( D_ALWAYS, "log record has no 'XactionName'\n" );
		delete logRec;
		return( false );
	}

	switch( opType ) {
		case ClassAdLogOp_OpenTransaction: {
			Transaction	*xaction = new Transaction;
			if( !xaction ) {
				delete logRec;
				return( false );
			}

				// enter transaction into transaction table
			xactionTable[xactionName] = xaction;
			xaction->SetXactionName( xactionName );
			delete logRec;
			return( true );
		}
			
		case ClassAdLogOp_CloseTransaction: {
			Transaction				*xaction;
			XactionTable::iterator	itr = xactionTable.find( xactionName );
			if( itr == xactionTable.end( ) ) {
				delete logRec;
				return( false );
			}
			xaction = itr->second;

				// log the transaction to disk --- ready to be committed
			if( !xaction->Log( sink ) ) {
				dprintf( D_ALWAYS, "failed to log transaction\n" );
				return( false );
			}

			delete logRec;
			return( true );
		}

		case ClassAdLogOp_FinishTransaction: {
			Transaction				*xaction;
			XactionTable::iterator	itr = xactionTable.find( xactionName );
			bool					commit=true, rval=true;

			if( itr == xactionTable.end( ) ) {
				delete logRec;
				return( false );
			}
			xaction = itr->second;

				// check if the transaction should be committed or aborted
			if( !logRec->EvaluateAttrBool( "Commit", commit ) || commit ) {
					// play the transaction on the collection
				if( !( rval = xaction->Play( this ) ) ) {
					dprintf( D_ALWAYS, "failed to commit transaction\n" );
				}
			}

				// delete the transaction from the transaction table
			delete xaction;
			xactionTable.erase( itr );
			delete logRec;
			return( rval );
		}

		default: {
				// play the individual operation on the data structure
			bool rval = Play( logRec );	
			delete logRec;
			return( rval );
		}
	}
}


bool ClassAdCollectionServer::
AppendLogRecord( Sink *snk, CollectionLogRecord *logRec )
{
    string xactionName;
    Transaction *xaction;
    XactionTable::iterator  xi;

    if( !logRec->GetStringAttr( "XactionName", xactionName ) ||
        ( ( xi = xactionTable.find( xactionName ) ) == xactionTable.end( ) ) ) {
		delete logRec;
        return( false );
    }
    xaction = xi->second;
    xaction->AppendLog( logRec );

	if( snk ) {
		if( !logRec->Write( snk ) ) {
			dprintf( D_ALWAYS, "could not append log record: write to "
				"%s failed, errno = %d,", logFileName.c_str( ), errno );
			return( false );
		}
		if( !snk->FlushSink( ) ) {
			dprintf( D_ALWAYS,"could not flush sink on file %s, errno = %d",
						logFileName.c_str( ), errno );
			return( false );
		}
	}

	return( true );
}


bool ClassAdCollectionServer::
TruncateLog( )
{
	string	tmpLogFileName;
	int			newLog_fd;
	FILE		*newLog_fp;
	Sink		newSink;

	if( logFileName.empty( ) ) return( false );

		// first create a temporary file
	tmpLogFileName = logFileName + ".tmp";
	newLog_fd=open( tmpLogFileName.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600 );
	if( newLog_fd < 0 ) {
		dprintf( D_ALWAYS, "failed to truncate log: open(%s) returns %d, "
			"errno = %d\n", tmpLogFileName.c_str( ), newLog_fd, errno );
		return( false );
	}
    if( ( newLog_fp = fdopen( newLog_fd, "r+" ) ) == NULL ) {
        dprintf( D_ALWAYS, "failed to truncate log: fdopen(%s) failed, "
            "errno = %d\n", tmpLogFileName.c_str( ), errno );
        return( false );
    }
		// wrap a sink around this file and dump current state to it
	newSink.SetSink( newLog_fp );
	if( !LogState( &newSink ) ) {
		return( false );
	}

		// close the log files
	fclose( log_fp );
	fclose( newLog_fp );

		// move the new/tmp log file over the old log
#if defined(WIN32)
    if( MoveFileEx(tmpLogFileName.c_str( ), logFileName.c_str( ), 
		MOVEFILE_REPLACE_EXISTING) == 0 ) {
        dprintf( D_ALWAYS, "failed to truncate log: MoveFileEx failed with "
            "error %d\n", GetLastError());
        return false;
    }
#else
    if( rename(tmpLogFileName.c_str( ), logFileName.c_str( ) ) < 0 ) {
        dprintf( D_ALWAYS, "failed to truncate log: rename(%s, %s) returns "
        	"errno %d", tmpLogFileName.c_str( ), logFileName.c_str( ), errno);
        return( false );
    }
#endif

		// re-open new log file
    if( ( log_fp = fopen( logFileName.c_str( ), "a+" ) ) == NULL ) {
        dprintf( D_ALWAYS, "failed to reopen %s, errno = %d", 
			logFileName.c_str( ), errno );
		return( false );
    }

	sink->SetSink( log_fp );
    return( true );
}


bool ClassAdCollectionServer::
LogState( Sink *snk )
{
		// write log records for all the views
	if( !LogViews( snk, &viewTree, true ) ) {
		return( false );
	}

		// log classads
	ClassAdTable::iterator	itr;
	CollectionLogRecord		logRec;
	ClassAd					*ad;

	if( !logRec.InsertAttr( "OpType", ClassAdLogOp_AddClassAd ) ) {
		return( false );
	}

	for( itr = classadTable.begin( ); itr != classadTable.end( ); itr++ ) {
		if( !logRec.InsertAttr( "Key", itr->first.c_str( ) )||
				!( ad = GetClassAd( itr->first ) )			||
				!logRec.InsertAttr( "Ad", ad ) 				||
				!logRec.Write( snk ) ) {
			logRec.Remove( "Ad" );
			return( false );
		}

		logRec.Remove( "Ad" );
	}

		// *** NEED TO LOG OPEN TRANSACTIONS

		// sync file to complete
	if( !snk->FlushSink( ) ) {
		dprintf( D_ALWAYS, "error flushing/fsyncing log, errno=%d\n", errno );
		return( false );
	}

	return( true );
}
	

bool ClassAdCollectionServer::
LogViews( Sink *snk, View *view, bool subView )
{
		// first log the current view ...
	{
		CollectionLogRecord	logRec;
		ClassAd				*ad = view->GetViewInfo( );

			// insert operation type and view info
		if( !ad || !ad->InsertAttr( "OpType", subView ?
				ClassAdLogOp_CreateSubView:ClassAdLogOp_CreatePartition ) ) {
			if( ad ) delete ad;
			return( false );
		}
			// dump view info into log record 
		logRec.Update( *ad );
		delete ad;
		if( !logRec.Write( snk ) ) {
			return( false );
		}
	}

		// next log subordinate child views ...
	SubordinateViews::iterator	xi;
	for( xi=view->subordinateViews.begin( ); xi!=view->subordinateViews.end( ); 
			xi++ ) {
		if( !LogViews( snk, *xi, true ) ) return( false );
	}

		// ... and partition child views
	PartitionedViews::iterator	mi;
	for( mi=view->partitionedViews.begin( ); mi!=view->partitionedViews.end( ); 
			mi++ ) {
		if( !LogViews( snk, mi->second, false ) ) return( false );
	}

	return( true );
}


CollectionLogRecord *ClassAdCollectionServer::
ReadLogEntry( Source *src )
{
    CollectionLogRecord	*log_rec = new CollectionLogRecord;

	if( !log_rec || !log_rec->Read( src ) ) {
		if( log_rec ) delete log_rec;
		return( NULL );
	}
	
    return log_rec;
}


bool ClassAdCollectionServer::
CreateSubView( const string &xactionName, const ViewName &viewName, 
	const ViewName &parentViewName, const string &constraint, 
	const string &rank, const string &partitionExprs )
{
	CollectionLogRecord	*rec;
	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )							||
			!rec->InsertAttr( "OpType", ClassAdLogOp_CreateSubView )	||
			!rec->InsertAttr( "ViewName", viewName.c_str( ) ) 			||
			!rec->InsertAttr( "ParentViewName", parentViewName.c_str() )||
			!rec->Insert( ATTR_REQUIREMENTS, constraint.c_str( ) )		||
			!rec->Insert( ATTR_RANK, rank.c_str( ) )				||
			!rec->Insert( "PartitionExprs", rank.c_str( ) ) ) {
		delete rec;
		return( false );
	}
	return( AppendLogRecord( sink, rec ) );
}


bool ClassAdCollectionServer::
CreatePartition( const string &xactionName, const ViewName &viewName, 
	const ViewName &parentViewName, const string &constraint, 
	const string &rank, const string &partitionExprs, ClassAd *rep )
{
	CollectionLogRecord	*rec;
	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )							||
			!rec->InsertAttr( "OpType", ClassAdLogOp_CreatePartition )	||
			!rec->InsertAttr( "ViewName", viewName.c_str( ) ) 			||
			!rec->InsertAttr( "ParentViewName", parentViewName.c_str() )||
			!rec->Insert( ATTR_REQUIREMENTS, constraint.c_str( ) )		||
			!rec->Insert( ATTR_RANK, constraint.c_str( ) )				||
			!rec->Insert( "PartitionExprs", constraint.c_str( ) ) 		||
			!rec->Insert( "Representative", rep ) ) {
		delete rec;
		return( false );
	}
	return( AppendLogRecord( sink, rec ) );
}


bool ClassAdCollectionServer::
DeleteView( const string &xactionName, const ViewName &viewName )
{
	CollectionLogRecord	*rec;
	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )						||
			!rec->InsertAttr( "OpType", ClassAdLogOp_DeleteView )	||
			!rec->InsertAttr( "ViewName", viewName.c_str( ) ) ) {
		delete rec;
		return( false );
	}
	return( AppendLogRecord( sink, rec ) );
}


bool ClassAdCollectionServer::
SetViewInfo( const string &xactionName, const ViewName &viewName, 
	const string &constraint, const string &rank, 
	const string &partitionExprs )
{
	CollectionLogRecord	*rec;
	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )						||
			!rec->InsertAttr( "OpType", ClassAdLogOp_SetViewInfo )	||
			!rec->InsertAttr( "ViewName", viewName.c_str( ) ) 		||
			!rec->Insert( ATTR_REQUIREMENTS, constraint.c_str( ) )	||
			!rec->Insert( ATTR_RANK, constraint.c_str( ) )			||
			!rec->Insert( "PartitionExprs", constraint.c_str( ) ) ) {
		delete rec;
		return( false );
	}
	return( AppendLogRecord( sink, rec ) );
}


bool ClassAdCollectionServer::
GetViewInfo( const ViewName &viewName, ClassAd *&info )
{
	ViewRegistry::iterator i;

		// lookup view registry for view
	i = viewRegistry.find( viewName );
	if( i == viewRegistry.end( ) ) {
		info = NULL;
		return( false );
	}
	info = i->second->GetViewInfo( );
	return( true );
}



bool ClassAdCollectionServer::
AddClassAd( const string &xactionName, const string &key,ClassAd *ad )
{
	CollectionLogRecord *rec;
	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )						||
			!rec->InsertAttr( "OpType", ClassAdLogOp_AddClassAd ) 	||
			!rec->InsertAttr( "Key", key.c_str( ) )					||
			!rec->Insert( "Ad", ad ) ) {
		delete rec;
		return( false );
	}
	return( AppendLogRecord( sink, rec ) );
}



bool ClassAdCollectionServer::
UpdateClassAd( const string &xactionName, const string &key,
	ClassAd *ad )
{
	CollectionLogRecord *rec;
	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )						||
			!rec->InsertAttr( "OpType", ClassAdLogOp_UpdateClassAd )||
			!rec->InsertAttr( "Key", key.c_str( ) )					||
			!rec->Insert( "Ad", ad ) ) {
		delete rec;
		return( false );
	}
	return( AppendLogRecord( sink, rec ) );
}



bool ClassAdCollectionServer::
ModifyClassAd( const string &xactionName, const string &key, 
	ClassAd *ad )
{
	CollectionLogRecord *rec;
	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )						||
			!rec->InsertAttr( "OpType", ClassAdLogOp_ModifyClassAd )||
			!rec->InsertAttr( "Key", key.c_str( ) )					||
			!rec->Insert( "Ad", ad ) ) {
		delete rec;
		return( false );
	}
	return( AppendLogRecord( sink, rec ) );
}


bool ClassAdCollectionServer::
RemoveClassAd( const string &xactionName, const string &key )
{
	CollectionLogRecord *rec;

	if( !( rec = new CollectionLogRecord( ) ) ) {
		return( false );
	}
	if( !rec->SetTransactionName( xactionName )							||
			!rec->InsertAttr( "OpType", ClassAdLogOp_RemoveClassAd ) 	||
			!rec->InsertAttr( "Key", key.c_str( ) ) ) {
		delete rec;
		return( false );
	}
	return(	AppendLogRecord( sink, rec ) );
}


bool ClassAdCollectionServer::
OpenTransaction( const string &transactionName )
{
		// check if a transaction with the given name already exists
	if( xactionTable.find( transactionName ) != xactionTable.end( ) ) {
			// yes ... fail
		return( false );
	}

		// create a transaction with the given name and insert into table
	Transaction	*xtn = new Transaction( );
	if( !xtn ) return( false );
	xactionTable[transactionName] = xtn;

	return( true );
}


bool ClassAdCollectionServer::
CloseTransaction( const string &transactionName, bool commit )
{
	XactionTable::iterator	itr = xactionTable.find( transactionName );
	Transaction				*xtn;
	bool 					rval = true;

		// no such transaction?
	if( itr == xactionTable.end( ) ) {
		return( false );
	}
	xtn = itr->second;
	if( sink && !xtn->Log( sink ) ) {
		return( false );
	}

	if( commit ) {
		rval = rval && xtn->Play( this );
	}

	delete xtn;
	xactionTable.erase( itr );
	return( rval );
}


bool ClassAdCollectionServer::
Check( CollectionLogRecord *rec ) 
{
	int	opType;

		// get the operation represented by the classad
	if( !rec->EvaluateAttrInt( "OpType", opType ) ) {
		return( false );
	}

		// in all cases other than OpenTransaction, there must be a valid
		// transaction name
	string tName;
	if( !rec->GetStringAttr( "XactionName", tName ) ) {
		return( false );
	}
	if( opType != ClassAdLogOp_OpenTransaction ) {
		if( xactionTable.find( tName ) == xactionTable.end( ) ) {
			return( false );
		}
	}

		// do sanity/validity checks for operations
	switch( opType ) {

		case ClassAdLogOp_CreateSubView: 
		case ClassAdLogOp_CreatePartition: {
			char *viewName=NULL, *parentViewName=NULL;

				// get the names of the parent view and the child view
			if( !rec->EvaluateAttrString( "ViewName", viewName ) ||
				!rec->EvaluateAttrString( "ParentViewName", parentViewName ) ) {
				return( false );
			}

				// *** NEED TO DO SANITY CHECK ON PARTITION ATTRIBUTES(??)

				// 1. the parent view should exist
				// 2. the sub view should not
			if( viewRegistry.find( string( parentViewName ) ) == 
					viewRegistry.end( ) ||
				viewRegistry.find( string( viewName ) ) != 
					viewRegistry.end( ) ) {
				return( false );
			}
			break;
		}

		case ClassAdLogOp_SetViewInfo:
		case ClassAdLogOp_DeleteView: {
			char *viewName=NULL;

			if( !rec->EvaluateAttrString( "ViewName", viewName ) ) {
				return( false );
			}

				// ensure the named view exists, and is not the root view
			if( viewRegistry.find( string( viewName ) ) != 
					viewRegistry.end( ) || viewName == "root" ) {
				return( false );
			}

			break;
		}

		case ClassAdLogOp_AddClassAd: {
			char 	*key=NULL;
			Value	val;

			if( !rec->EvaluateAttrString( "Key", key ) ) {
				return( false );
			}

				// ensure that no classad with given key exists
			if( classadTable.find( string(key) ) != classadTable.end() ) {
				return( false );
			}
			
				// ensure that the attribute named "ad" evaluates to a classad
			if( !rec->EvaluateAttr( "Ad", val ) || !val.IsClassAdValue( ) ) {
				return( false );
			}

			break;
		}


		case ClassAdLogOp_RemoveClassAd:
		case ClassAdLogOp_ModifyClassAd:
		case ClassAdLogOp_UpdateClassAd: {
			char 	*key=NULL;
			Value	val;

			if( !rec->EvaluateAttrString( "Key", key ) ) {
				return( false );
			}

				// ensure that a classad with given key exists
			if( classadTable.find( string(key) ) == classadTable.end() ) {
				return( false );
			}
			
				// if modifying or updating ensure that there is an Ad ...
			if( opType != ClassAdLogOp_RemoveClassAd ) {
				if( !rec->EvaluateAttr( "Ad", val ) || !val.IsClassAdValue() ) {
					return( false );
				}
			}

			break;
		}


		case ClassAdLogOp_OpenTransaction: {
				// ensure that no transaction already exists with name
			if( xactionTable.find( string(tName) ) != xactionTable.end()){
				return( false );
			}
			break;
		}


		case ClassAdLogOp_CloseTransaction:
		case ClassAdLogOp_FinishTransaction:
				// named transaction must exist --- that's all
				// (already tested before entering switch)
			break;


		default:
			return( false );
	}


	return( true );
}


bool ClassAdCollectionServer::
Play( CollectionLogRecord *rec )
{
	int				opType;
	Transaction		*xaction;
	string 	tName;

		// get the operation type
	if( !rec->EvaluateAttrInt( "OpType", opType ) ) {
		EXCEPT( "internal error:  could not find 'OpType'" );
	}

		// get the transaction
	if( !rec->GetStringAttr( "XactionName", tName ) ) {
		EXCEPT( "internal error:  could not find transaction name" );
	}
	xaction = xactionTable.find( tName )->second;


	switch( opType ) {
		case ClassAdLogOp_CreateSubView: {
			ViewRegistry::iterator itr;
			char	*parentViewName=NULL;
			View	*parentView;

			if( !rec->EvaluateAttrString( "ParentViewName", parentViewName ) ||
				( itr = viewRegistry.find(string(parentViewName)) )
				 	== viewRegistry.end( ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			parentView = itr->second;
			return( parentView->InsertSubordinateView( this, rec ) );
		}

		case ClassAdLogOp_CreatePartition: {
			ViewRegistry::iterator itr;
			char	*parentViewName=NULL;
			View	*parentView;

			if( !rec->EvaluateAttrString( "ParentViewName", parentViewName ) ||
				( itr = viewRegistry.find(string(parentViewName)) )
					== viewRegistry.end( ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			parentView = itr->second;
			return( parentView->InsertSubordinateView( this, rec ) );
		}

		case ClassAdLogOp_DeleteView: {	
			ViewRegistry::iterator 	itr;
			string				viewName;
			View					*parentView;

			if( !rec->GetStringAttr( "ViewName", viewName ) ||
				( itr=viewRegistry.find(viewName) ) == viewRegistry.end( ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			if( ( parentView = itr->second->GetParent( ) ) == NULL ) {
				EXCEPT( "internal error: did not commit" );
			}
			return( parentView->DeleteChildView( this, viewName ) );
		}

		case ClassAdLogOp_SetViewInfo: {
			ViewRegistry::iterator itr;
			char	*viewName=NULL;
			ClassAd	*viewInfo;
			View	*view;
			Value	cv;

			if( !rec->EvaluateAttrString( "ViewName", viewName ) ||
				( itr = viewRegistry.find( string( viewName ) ) )
					==viewRegistry.end( ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			view = itr->second;
			if(!rec->EvaluateAttr("ViewInfo",cv)||!cv.IsClassAdValue(viewInfo)){
				EXCEPT( "internal error:  failed to commit" );
			}
			return( view->SetViewInfo( this, viewInfo ) );
		}

		case ClassAdLogOp_AddClassAd: {
			string	key;
			ClassAd		*ad, *newAd;
			ClassAdProxy proxy;
			Value		cv;

			if( !rec->GetStringAttr( "Key", key ) ) {
				EXCEPT( "internal error:  failed to commit" );
			}
			if( !rec->EvaluateAttr( "Ad", cv ) || !cv.IsClassAdValue( ad ) ||
				!( newAd = ad->Copy( ) ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			if( !viewTree.ClassAdInserted( this, key, newAd ) ) {
				delete newAd;
				return( false );
			}
			proxy.ad = newAd;
			classadTable[key] = proxy;
			return( true );
		}

		case ClassAdLogOp_UpdateClassAd: {
			ClassAdTable::iterator itr;
			string	key;
			ClassAd		*ad, *update;
			Value		cv;
			bool		rval;

			if( !rec->GetStringAttr( "Key", key ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			if( !rec->EvaluateAttr( "Ad", cv ) || !cv.IsClassAdValue(update) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			itr = classadTable.find( key );
			if( itr == classadTable.end( ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			ad = itr->second.ad;

			rval = viewTree.ClassAdPreModify( this, ad );
			if( rval ) {
				ad->Update( *update );
				return( viewTree.ClassAdModified( this, key, ad ) );
			}
			return( false );
		}
			
		case ClassAdLogOp_ModifyClassAd: {
			ClassAdTable::iterator itr;
			string	key;
			ClassAd		*ad, *update;
			Value		cv;

			if( !rec->GetStringAttr( "Key", key ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			if( !rec->EvaluateAttr( "Ad", cv ) || !cv.IsClassAdValue(update) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			itr = classadTable.find( key );
			if( itr == classadTable.end( ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			ad = itr->second.ad;

			return( viewTree.ClassAdPreModify( this, ad ) && 
					ad->Modify( *update ) &&
					viewTree.ClassAdModified( this, key, ad ) );
		}

		case ClassAdLogOp_RemoveClassAd: {
			ClassAdTable::iterator itr;
			string	key;
			ClassAd		*ad;
			Value		cv;
			bool		rval;

			if( !rec->GetStringAttr( "Key", key ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			itr = classadTable.find( key );
			if( itr == classadTable.end( ) ) {
				EXCEPT( "internal error:  did not commit" );
			}
			ad = itr->second.ad;
			classadTable.erase( itr );
			rval = viewTree.ClassAdDeleted( this, key, ad );
			delete ad;
			return( rval );
		}

		default:
			break;
	}

	EXCEPT( "Should not reach here" );
	return( false );
}


ClassAd *ClassAdCollectionServer::
GetClassAd( const string &key )
{
	ClassAdTable::iterator	itr = classadTable.find( key );
	if( itr == classadTable.end( ) ) {
		return( NULL );
	} 
	return( itr->second.ad );
}

bool ClassAdCollectionServer::
RegisterView( const string &viewName, View *view )
{
		// signal error if view already exists
	if( viewRegistry.find( viewName ) != viewRegistry.end( ) ) {
		return( false );
	}
		// else insert view into registry
	viewRegistry[viewName] = view;
	return( true );
}


bool ClassAdCollectionServer::
UnregisterView( const string &viewName )
{
		// signal error if the view doesn't exist
	if( viewRegistry.find( viewName ) == viewRegistry.end( ) ) {
		return( false );
	}
		// else remove from registry
	viewRegistry.erase( viewName );
	return( true );
}


bool ClassAdCollectionServer::
DisplayView( const ViewName &viewName, FILE *file )
{
	ViewRegistry::iterator itr = viewRegistry.find( viewName );
	if( itr == viewRegistry.end( ) ) {
		return( false );
	}
	return( itr->second->Display( file ) );
}


END_NAMESPACE
