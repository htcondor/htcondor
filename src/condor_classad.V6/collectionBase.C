/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include "common.h"
#include "collection.h"
#include "collectionBase.h"
#include "transaction.h"

BEGIN_NAMESPACE( classad )

ClassAdCollection::
ClassAdCollection( ) : viewTree( NULL )
{
		// create "root" view
	viewTree.SetViewName( "root" );
	RegisterView( "root", &viewTree );
	log_fp = NULL;
}


ClassAdCollection::
~ClassAdCollection( )
{
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

	// Note that the log_fp will be closed by our parent class destructor. 
	return;
}


bool ClassAdCollection::
InitializeFromLog( const string &filename )
{
		// clear out log file related info
	if( log_fp ) {
		fclose( log_fp );
		log_fp = NULL;
	}

		// clear out tables
	viewTree.DeleteView( this );
	ClassAdTable::iterator	ci;
	for( ci = classadTable.begin( ); ci != classadTable.end( ); ci++ ) {
		delete ci->second.ad;
	}
	classadTable.clear( );
	XactionTable::iterator	xti;
	for( xti = xactionTable.begin( ); xti != xactionTable.end( ); xti++ ) {
		delete xti->second;
	}
	xactionTable.clear( );

		// reset root view info
	if( !RegisterView( "root", &viewTree ) ) {
		CondorErrno = ERR_FATAL_ERROR;
		CondorErrMsg = "internal error:  unable to create root view";
		return( false );
	}
	ClassAd *ad = new ClassAd;
	if( !ad ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( false );
	}
	Value				val;
	vector<ExprTree*>	vec;
	if( !ad->InsertAttr( ATTR_REQUIREMENTS, true ) 						||
			!ad->Insert( ATTR_RANK, Literal::MakeLiteral( val ) )		||
			!ad->Insert( ATTR_PARTITION_EXPRS, ExprList::MakeExprList( vec ) )||
			!viewTree.SetViewInfo( this, ad ) ) {
		CondorErrMsg += "; failed to initialize from log";
		return( false );
	}
	
				
		// load info from new log file
	logFileName = filename;
	if( !filename.empty( ) ) {
		if( !ReadLogFile( ) ) {
			CondorErrMsg += "; could not initialize from file " + filename;
			return( false );
		}
	}
	return( true );
}


//------------------------------------------------------------------------------
// Recovery mode interface
//------------------------------------------------------------------------------

bool ClassAdCollection::
OperateInRecoveryMode( ClassAd *logRec )
{
	int 	opType=0;

	logRec->EvaluateAttrInt( ATTR_OP_TYPE, opType );

	switch( opType ) {
		case ClassAdCollOp_OpenTransaction:
		case ClassAdCollOp_AbortTransaction:
		case ClassAdCollOp_CommitTransaction:
		case ClassAdCollOp_ForgetTransaction: {
			string				xactionName;
			ServerTransaction	*xaction;
				// xaction ops require a transaction name
			if( !logRec->EvaluateAttrString( "XactionName", xactionName ) ) {
				CondorErrno = ERR_NO_TRANSACTION_NAME;
				CondorErrMsg = "log record has no 'XactionName'";
				delete logRec;
				return( false );
			}
				// FIXME: put the xactionErrCause in CondorErrMsg
			if( !PlayXactionOp( opType, xactionName, logRec, xaction ) ) {
				delete logRec;
				if( xaction ) delete xaction;
				xactionTable.erase( xactionName );
				return( false );
			}
			delete logRec;

				// if a xaction is being opened, we're done
			if( opType == ClassAdCollOp_OpenTransaction ) return( true );

				// if we're committing the xaction, change to "committed" state
			if( opType == ClassAdCollOp_CommitTransaction ) {
				if( xaction ) delete xaction;
				xactionTable[xactionName] = NULL;
				return( true );
			}

				// other ops already taken care of by PlayXactionOp
			return( true );
		}

		case ClassAdCollOp_CreateSubView:
		case ClassAdCollOp_CreatePartition:
		case ClassAdCollOp_DeleteView:
		case ClassAdCollOp_SetViewInfo: {
			bool rval = PlayViewOp( opType, logRec );
			delete logRec;
			return( rval );
		}

		case ClassAdCollOp_AddClassAd:
		case ClassAdCollOp_UpdateClassAd:
		case ClassAdCollOp_ModifyClassAd:
		case ClassAdCollOp_RemoveClassAd: {
			string	xactionName, key;

			if( !logRec->EvaluateAttrString( ATTR_KEY, key ) ) {
				CondorErrno = ERR_NO_KEY;
				CondorErrMsg = "bad or missing 'key'";
				return( false );
			}

			if( !logRec->EvaluateAttrString( "XactionName", xactionName ) ) {
					// not in xaction; just play the operation
				bool rval = PlayClassAdOp( opType, logRec );
				delete logRec;
				return( rval );
			}
				// in transaction; add record to transaction
			ServerTransaction		*xaction;
			XactionTable::iterator 	itr = xactionTable.find( xactionName );
			if( itr == xactionTable.end( ) ) {
				CondorErrno = ERR_NO_SUCH_TRANSACTION;
				CondorErrMsg = "transaction " + xactionName + " doesn't exist";
				delete logRec;
				return( false );
			}
			xaction = itr->second;
			xaction->AppendRecord( opType, key, logRec );
			return( true );
		}
	}
	EXCEPT( "illegal operation in log:  should not reach here" );
	return( false );
}

//------------------------------------------------------------------------------
// Main execution engine --- operations in recovery mode, network mode and
// the local interface mode usually funnel into these routines
//------------------------------------------------------------------------------
bool ClassAdCollection::
PlayXactionOp( int opType, const string &xactionName, ClassAd *logRec,
	ServerTransaction *&serverXaction )
{
	serverXaction = NULL;
	switch( opType ) {
		case ClassAdCollOp_OpenTransaction: {
			if( xactionTable.find( xactionName ) != xactionTable.end( ) ) {
				CondorErrno = ERR_TRANSACTION_EXISTS;
				CondorErrMsg = "xaction " + xactionName + " already exists";
				return( false );
			}

			ServerTransaction	*xaction = new ServerTransaction;
			if( !xaction ) {
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
				return( false );
			}

				// enter transaction into transaction table
			bool local;
			if( logRec->EvaluateAttrBool( "LocalTransaction",local ) && local ){
				xaction->SetLocalXaction( true );
			}

			xaction->SetCollectionServer( this );
			xactionTable[xactionName] = xaction;
			xaction->SetXactionName( xactionName );
			serverXaction = xaction;
			return( true );
		}

		case ClassAdCollOp_AbortTransaction: {
			ServerTransaction		*xaction;
			XactionTable::iterator	itr = xactionTable.find( xactionName );

			if( itr == xactionTable.end( ) ) {
				CondorErrno = ERR_NO_SUCH_TRANSACTION;
				CondorErrMsg = "transaction" + xactionName + " not found";
				return( false );
			}
				// kill transaction
			xaction = itr->second;
			if( xaction ) delete xaction;
			xactionTable.erase( itr );
			return( true );
		}
			
		case ClassAdCollOp_CommitTransaction: {
			ServerTransaction		*xaction;
			XactionTable::iterator	itr = xactionTable.find( xactionName );

			if( itr == xactionTable.end( ) ) {
				CondorErrno = ERR_NO_SUCH_TRANSACTION;
				CondorErrMsg = "transaction " + xactionName + " not found";
				return( false );
			}
			xaction = itr->second;
			if( !xaction ) {
				CondorErrno = ERR_BAD_TRANSACTION_STATE;
				CondorErrMsg = "transaction already committed";
				return( false );
			}
			serverXaction = xaction;
			if( !xaction->Commit( ) ) return( false );
				// if its a local transaction, clear it out
			if( xaction->GetLocalXaction( ) ) {
				if( xaction ) delete xaction;
				xactionTable.erase( itr );
				serverXaction = NULL;
			}
			return( true );
		}

		case ClassAdCollOp_ForgetTransaction: {
			ServerTransaction		*xaction;
			XactionTable::iterator	itr = xactionTable.find( xactionName );

				// remove the transaction from the transaction table
			if( itr == xactionTable.end( ) ) {
				CondorErrno = ERR_NO_SUCH_TRANSACTION;
				CondorErrMsg = "transaction " + xactionName + " doesn't exist"
					" to be closed";
				return( false );
			}
			xaction = itr->second;

			if( xaction ) {
					// transaction not committed; treat as abort
				delete xaction;
			}
			xactionTable.erase( itr );
			return( true );
		}

		default:
			EXCEPT( "not a transaction op:  should not reach here" );
			return( false );
	}
}



bool ClassAdCollection::
PlayViewOp( int opType, ClassAd *logRec )
{
	ClassAd	*viewInfo;

	switch( opType ) {

        case ClassAdCollOp_CreateSubView: {
            ViewRegistry::iterator itr;
            string	parentViewName;
            View    *parentView;
			ClassAd	*viewInfo;

            if( !logRec->EvaluateAttrString(ATTR_PARENT_VIEW_NAME,
					parentViewName) || 
					( itr = viewRegistry.find(parentViewName) )
                    == viewRegistry.end( ) ) {
				CondorErrno = ERR_NO_SUCH_VIEW;
				CondorErrMsg = "view " + parentViewName + " not found";
				return( false );
            }
            parentView = itr->second;

				// make the viewInfo classad --- mostly the logRec itself
			if( !( viewInfo = logRec->Copy( ) ) ) {
				return( false );
			}
			viewInfo->Delete( "OpType" );
			return( parentView->InsertSubordinateView( this, viewInfo ) );
        }

        case ClassAdCollOp_CreatePartition: {
            ViewRegistry::iterator itr;
            string  parentViewName;
            View    *parentView;
			Value	val;
			ClassAd	*rep;

            if( !logRec->EvaluateAttrString(ATTR_PARENT_VIEW_NAME,
					parentViewName) ||
					( itr = viewRegistry.find(parentViewName) )
					== viewRegistry.end( ) ) {
				CondorErrno = ERR_NO_SUCH_VIEW;
				CondorErrMsg = "view " + parentViewName + " not found";
				return( false );
            }
			if( !logRec->EvaluateAttr( "Representative", val ) || 
					!val.IsClassAdValue( rep ) ) {
				CondorErrno = ERR_NO_REPRESENTATIVE;
				CondorErrMsg = "no representative classad for partition found";
				return( false );
			}
            parentView = itr->second;

				// make the viewInfo classad --- mostly the logRec itself
			if( !( viewInfo = logRec->Copy( ) ) ) {
				return( false );
			}
			viewInfo->Delete( "OpType" );
			viewInfo->Remove( "Representative" );
            return( parentView->InsertPartitionedView( this, viewInfo, rep ) );
        }

        case ClassAdCollOp_DeleteView: {
            ViewRegistry::iterator  itr;
            string	viewName;
            View    *parentView;

            if( !logRec->EvaluateAttrString( ATTR_VIEW_NAME, viewName ) ||
                ( itr=viewRegistry.find(viewName) ) == viewRegistry.end( ) ) {
				CondorErrno = ERR_NO_SUCH_VIEW;
				CondorErrMsg = "view " + viewName + " not found to be deleted";
				return( false );
            }
            if( ( parentView = itr->second->GetParent( ) ) == NULL ) {
				CondorErrno = ERR_NO_PARENT_VIEW;
				CondorErrMsg = "view " + viewName + " has no parent view";
				return( false );
            }

            parentView->DeleteChildView( this, viewName );
			delete logRec;
			return( true );
        }

        case ClassAdCollOp_SetViewInfo: {
            ViewRegistry::iterator itr;
            string	viewName;
            ClassAd *tmp;
            View    *view;
            Value   cv;

            if( !logRec->EvaluateAttrString( ATTR_VIEW_NAME, viewName ) ||
                	(itr=viewRegistry.find(viewName)) == viewRegistry.end( ) ) {
				CondorErrno = ERR_NO_SUCH_VIEW;
                CondorErrMsg = "could not find view " + viewName;
				return( false );
            }
            view = itr->second;
            if( !logRec->EvaluateAttr( ATTR_VIEW_INFO, cv ) ||
					!cv.IsClassAdValue(tmp)){
				CondorErrno = ERR_BAD_VIEW_INFO;
				CondorErrMsg = "view info bad or missing";
				return( false );
            }
			if( !( viewInfo = tmp->Copy( ) ) ) {
				return( false );
			}
				// make sure the "root" view always has 'Requirements=true'
			if( viewName=="root" ) {
				if( !viewInfo->InsertAttr(ATTR_REQUIREMENTS,true) ) {
					return( false );
				}
			}
				// handoff view info classad to new view
			return( view->SetViewInfo( this, viewInfo ) );
        }

		default:
			EXCEPT( "not a view op:  should not reach here" );
			return( false );
	}
}


bool ClassAdCollection::
PlayClassAdOp( int opType, ClassAd *rec )
{
	switch( opType ) {
		case ClassAdCollOp_AddClassAd: {
			ClassAdTable::iterator	itr;
			string					key;
			ClassAd					*ad, *newAd;
			ClassAdProxy 			proxy;
			Value					cv;

			if( !rec->EvaluateAttrString( "Key", key ) ) {
				CondorErrno = ERR_NO_KEY;
				CondorErrMsg = "bad or missing 'key' attribute";
				return( false );
			}
			if( !rec->EvaluateAttr( "Ad", cv ) || !cv.IsClassAdValue( ad ) ||
				!( newAd = ad->Copy( ) ) ) {
				CondorErrno = ERR_BAD_CLASSAD;
				CondorErrMsg = "bad or missing 'ad' attribute";
				return( false );
			}
			newAd->SetParentScope( NULL );

				// check if an ad is already present
			if( ( itr = classadTable.find( key ) ) != classadTable.end( ) ) {
					// delete old ad
				ad = itr->second.ad;
				viewTree.ClassAdDeleted( this, key, ad );
				classadTable.erase( itr );
				delete ad;
			}
				// insert new ad
			if( !viewTree.ClassAdInserted( this, key, newAd ) ) {
				CondorErrMsg += "; could not insert classad";
				return( false );
			}
			proxy.ad = newAd;
			classadTable[key] = proxy;
			return( true );
		}

		case ClassAdCollOp_UpdateClassAd: {
			ClassAdTable::iterator itr;
			string		key;
			ClassAd		*ad, *update;
			Value		cv;

			if( !rec->EvaluateAttrString( "Key", key ) ) {
				CondorErrno = ERR_NO_KEY;
				CondorErrMsg = "bad or missing 'key' attribute";
				return( false );
			}
			if( !rec->EvaluateAttr( "Ad", cv ) || !cv.IsClassAdValue(update) ) {
				CondorErrno = ERR_BAD_CLASSAD;
				CondorErrMsg = "bad or missing 'ad' attribute";
				return( false );
			}
			itr = classadTable.find( key );
			if( itr == classadTable.end( ) ) {
				CondorErrno = ERR_NO_SUCH_CLASSAD;
				CondorErrMsg = "no classad " + key + " to update";
				return( false );
			}
			ad = itr->second.ad;
			viewTree.ClassAdPreModify( this, ad );

			ad->Update( *update );
			if( !viewTree.ClassAdModified( this, key, ad ) ) {
				CondorErrMsg += "; failed when modifying classad";
				return( false );
			}
			return( true );
		}
			
		case ClassAdCollOp_ModifyClassAd: {
			ClassAdTable::iterator itr;
			string		key;
			ClassAd		*ad, *update;
			Value		cv;

			if( !rec->EvaluateAttrString( "Key", key ) ) {
				CondorErrno = ERR_NO_KEY;
                CondorErrMsg = "bad or missing 'key' attribute";
                return( false );
			}
			if( !rec->EvaluateAttr( "Ad", cv ) || !cv.IsClassAdValue(update) ) {
                CondorErrno = ERR_BAD_CLASSAD;
                CondorErrMsg = "bad or missing 'ad' attribute";
                return( false );                
			}
			itr = classadTable.find( key );
			if( itr == classadTable.end( ) ) {
                CondorErrno = ERR_NO_SUCH_CLASSAD;
                CondorErrMsg = "no classad " + key + " to modify";
                return( false );
			}
			ad = itr->second.ad;
			viewTree.ClassAdPreModify( this, ad );

			ad->Modify( *update );
			if( !viewTree.ClassAdModified( this, key, ad ) ) {
                CondorErrMsg += "; failed when modifying classad";
                return( false );
			}
			return( true );
		}

		case ClassAdCollOp_RemoveClassAd: {
			ClassAdTable::iterator itr;
			string		key;
			ClassAd		*ad;
			Value		cv;

			if( !rec->EvaluateAttrString( "Key", key ) ) {
                CondorErrno = ERR_NO_KEY;
                CondorErrMsg = "bad or missing 'key' attribute";
                return( false );
			}
			itr = classadTable.find( key );
			if( itr == classadTable.end( ) ) {
                CondorErrno = ERR_NO_SUCH_CLASSAD;
                CondorErrMsg = "no classad " + key + " to modify";
                return( false );
			}
			ad = itr->second.ad;
			classadTable.erase( itr );
			viewTree.ClassAdDeleted( this, key, ad );
			delete ad;
			return( true );
		}

		default:
			break;
	}

	EXCEPT( "internal error:  Should not reach here" );
	return( false );
}

//-----------------------------------------------------------------------------
// "Local" classad collection interface
//-----------------------------------------------------------------------------

bool ClassAdCollection::
CreateSubView( const ViewName &viewName, const ViewName &parentViewName,
	const string &constraint, const string &rank, const string &partitionExprs )
{
	bool rval;
	ClassAd *rec = _CreateSubView( viewName, parentViewName, 
			constraint, rank, partitionExprs );
	if( !rec ) return( false );
	rval = WriteLogEntry( log_fp, rec ) && 
			PlayViewOp( ClassAdCollOp_CreateSubView, rec );
	delete rec;
	return( rval );
}


bool ClassAdCollection::
CreatePartition( const ViewName &viewName, const ViewName &parentViewName,
	const string &constraint, const string &rank, const string &partitionExprs,
	ClassAd *rep )
{
	bool rval;
	ClassAd *rec = _CreatePartition( viewName, parentViewName, 
			constraint, rank, partitionExprs, rep);
	if( !rec ) return( false );
	rval = WriteLogEntry( log_fp, rec ) && 
			PlayViewOp( ClassAdCollOp_CreatePartition, rec );
	delete rec;
	return( rval );
}


bool ClassAdCollection::
DeleteView( const ViewName &viewName ) 
{
	bool rval;
	ClassAd *rec = _DeleteView( viewName );
	if( !rec ) return( false );
	rval = WriteLogEntry( log_fp, rec ) &&
			PlayViewOp( ClassAdCollOp_DeleteView, rec );
	delete rec;
	return( rval );
}


bool ClassAdCollection::
SetViewInfo( const ViewName &viewName, const string &constraint, 
	const string &rank, const string &partitionAttrs )
{
	bool rval;
	ClassAd *rec = _SetViewInfo( viewName, constraint, rank,
			partitionAttrs );
	if( !rec ) return( false );
	rval = WriteLogEntry( log_fp, rec ) &&
			PlayViewOp( ClassAdCollOp_SetViewInfo, rec );
	delete rec;
	return( rval );
}


bool ClassAdCollection::
GetViewInfo( const ViewName &viewName, ClassAd *&info )
{
	ViewRegistry::iterator i;

		// lookup view registry for view
	i = viewRegistry.find( viewName );
	if( i == viewRegistry.end( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "view " + viewName + " not found";
		info = NULL;
		return( false );
	}
	info = i->second->GetViewInfo( );
	return( true );
}


bool ClassAdCollection::
GetSubordinateViewNames( const ViewName &viewName, vector<string>& views )
{
	ViewRegistry::iterator	itr;

	if( ( itr = viewRegistry.find( viewName ) ) == viewRegistry.end( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "view " + viewName + " not found";
		return( false );
	}

	itr->second->GetSubordinateViewNames( views );
	return( true );
}


bool ClassAdCollection::
GetPartitionedViewNames( const ViewName &viewName, vector<string>& views )
{
	ViewRegistry::iterator	itr;

	if( ( itr = viewRegistry.find( viewName ) ) == viewRegistry.end( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "view " + viewName + " not found";
		return( false );
	}

	itr->second->GetPartitionedViewNames( views );
	return( true );
}


bool ClassAdCollection::
FindPartitionName( const ViewName &viewName, ClassAd *rep, ViewName &partition )
{
	ViewRegistry::iterator	itr;

	if( ( itr = viewRegistry.find( viewName ) ) == viewRegistry.end( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "view " + viewName + " not found";
		return( false );
	}
	return( itr->second->FindPartition( rep, partition ) );
}


bool ClassAdCollection::
AddClassAd( const string &key, ClassAd *newAd )
{
	if( currentXactionName != "" ) {
		ClassAd *rec = _AddClassAd( currentXactionName, key, newAd );
		if( !rec ) return( false );

		ServerTransaction       *xaction;
		XactionTable::iterator  itr = xactionTable.find( currentXactionName );
		if( itr == xactionTable.end( ) ) {
			CondorErrno = ERR_NO_SUCH_TRANSACTION;
			CondorErrMsg = "transaction " +currentXactionName+ " doesn't exist";
			delete rec;
			return( false );
		}
		xaction = itr->second;
		xaction->AppendRecord( ClassAdCollOp_AddClassAd, key, rec );
	} else {
		ClassAdTable::iterator	itr = classadTable.find( key );
		ClassAd					*ad;
		ClassAdProxy			proxy;

		if( itr != classadTable.end( ) ) {
				// some ad already present --- replace 
			ad = itr->second.ad;
			viewTree.ClassAdDeleted( this, key, ad );
			delete ad;
			classadTable.erase( itr );
		}

		if( !viewTree.ClassAdInserted( this, key, newAd ) ) {
			delete newAd;
			return( false );
		}

		proxy.ad = newAd;
		classadTable[key] = proxy;
			// log if possible
		if( log_fp ) {
			ClassAd *rec = _AddClassAd( "", key, newAd );
			if( !WriteLogEntry( log_fp, rec ) ) {
				CondorErrMsg += "; failed to log add classad";
				rec->Remove( ATTR_AD );
				delete rec;
				return( false );
			}
			rec->Remove( ATTR_AD );
			delete rec;
		}
	}

	return( true );
}


bool ClassAdCollection::
UpdateClassAd( const string &key, ClassAd *updAd )
{
	if( currentXactionName != "" ) {
		ClassAd *rec = _UpdateClassAd( currentXactionName,key,updAd );
		if( !rec ) return( false );

		ServerTransaction       *xaction;
		XactionTable::iterator  itr = xactionTable.find( currentXactionName );
		if( itr == xactionTable.end( ) ) {
			CondorErrno = ERR_NO_SUCH_TRANSACTION;
			CondorErrMsg = "transaction " +currentXactionName+ " doesn't exist";
			delete rec;
			return( false );
		}
		xaction = itr->second;
		xaction->AppendRecord( ClassAdCollOp_UpdateClassAd, key, rec );
	} else {
		ClassAdTable::iterator	itr = classadTable.find( key );
		ClassAd					*ad;

		if( itr == classadTable.end( ) ) {
			CondorErrno = ERR_NO_SUCH_CLASSAD;
			CondorErrMsg = "classad " + key + " doesn't exist to update";
			delete updAd;
			return( false );
		}
		ad = itr->second.ad;
		viewTree.ClassAdPreModify( this, ad );
		ad->Update( *updAd );
		if( !viewTree.ClassAdModified( this, key, ad ) ) {
			delete updAd;
			return( false );
		}

			// log if possible 
		if( log_fp ) {
			ClassAd *rec = _UpdateClassAd( "", key, updAd );
			if( !WriteLogEntry( log_fp, rec ) ) {
				CondorErrMsg += "; failed to log update classad";
				delete rec;
				return( false );
			}
			delete rec;
		}
	}
		
	return( true );
}


bool ClassAdCollection::
ModifyClassAd( const string &key, ClassAd *modAd )
{
	if( currentXactionName != "" ) {
		ClassAd *rec=_ModifyClassAd( currentXactionName, key, modAd );
		if( !rec ) return( false );

		ServerTransaction       *xaction;
		XactionTable::iterator  itr = xactionTable.find( currentXactionName );
		if( itr == xactionTable.end( ) ) {
			CondorErrno = ERR_NO_SUCH_TRANSACTION;
			CondorErrMsg = "transaction " +currentXactionName+ " doesn't exist";
			delete rec;
			return( false );
		}
		xaction = itr->second;
		xaction->AppendRecord( ClassAdCollOp_ModifyClassAd, key, rec );
	} else {
		ClassAdTable::iterator	itr = classadTable.find( key );
		ClassAd					*ad;
		if( itr == classadTable.end( ) ) {
			CondorErrno = ERR_NO_SUCH_CLASSAD;
			CondorErrMsg = "classad " + key + " doesn't exist to modify";
			delete modAd;
			return( false );
		}
		ad = itr->second.ad;
		viewTree.ClassAdPreModify( this, ad );
		ad->Modify( *modAd );
		if( !viewTree.ClassAdModified( this, key, ad ) ) {
			delete modAd;
			return( false );
		}

			// log if possible
		if( log_fp ) {
			ClassAd *rec = _ModifyClassAd( "", key, modAd );
			if( !WriteLogEntry( log_fp, rec ) ) {
				delete rec;
				CondorErrMsg += "; failed to log modify classad";
				return( false );
			}
			delete rec;
		}
	}

	return( true );
}


bool ClassAdCollection::
RemoveClassAd( const string &key )
{
	if( currentXactionName != "" ) {
		ClassAd *rec = _RemoveClassAd( currentXactionName, key );
		if( !rec ) return( false );

		ServerTransaction       *xaction;
		XactionTable::iterator  itr = xactionTable.find( currentXactionName );
		if( itr == xactionTable.end( ) ) {
			CondorErrno = ERR_NO_SUCH_TRANSACTION;
			CondorErrMsg = "transaction " +currentXactionName+ " doesn't exist";
			delete rec;
			return( false );
		}
		xaction = itr->second;
		xaction->AppendRecord( ClassAdCollOp_RemoveClassAd, key, rec );
	} else {
		ClassAdTable::iterator	itr = classadTable.find( key );
		ClassAd					*ad;

		if( itr == classadTable.end( ) ) return( true );
		ad = itr->second.ad;
		viewTree.ClassAdDeleted( this, key, ad );
		delete ad;
		classadTable.erase( itr );

			// log if possible
		if( log_fp ) {
			ClassAd *rec = _RemoveClassAd( "", key );
			if( !WriteLogEntry( log_fp, rec ) ) {
				delete rec;
				CondorErrMsg += "; failed to log modify classad";
				return( false );
			}
			delete rec;
		}
	}

	return( true );
}


ClassAd *ClassAdCollection::
GetClassAd( const string &key )
{
	ClassAdTable::iterator	itr = classadTable.find( key );
	if( itr == classadTable.end( ) ) {
		CondorErrno = ERR_NO_SUCH_CLASSAD;
		CondorErrMsg = "classad " + key + " not found";
		return( NULL );
	} 
	itr->second.ad->SetParentScope( NULL );
	return( itr->second.ad );
}


bool ClassAdCollection::
OpenTransaction( const string &transactionName )
{
		// check if a transaction with the given name already exists
	if( xactionTable.find( transactionName ) != xactionTable.end( ) ) {
			// yes ... fail
		CondorErrno = ERR_TRANSACTION_EXISTS;
		CondorErrMsg = "transaction " + transactionName + " already exists";
		return( false );
	}

		// create a transaction with the given name and insert into table
	ServerTransaction	*xtn = new ServerTransaction( );
	if( !xtn ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( false );
	}
	xtn->SetXactionName( transactionName );
	xtn->SetCollectionServer( this );
	xtn->SetLocalXaction( true );
	xactionTable[transactionName] = xtn;
	currentXactionName = transactionName;

	return( true );
}


bool ClassAdCollection::
CloseTransaction( const string &transactionName, bool commit, int &outcome )
{
	XactionTable::iterator	itr = xactionTable.find( transactionName );
	ServerTransaction		*xtn;
	bool					rval = true;

	outcome = XACTION_UNKNOWN;

		// no such transaction?
	if( itr == xactionTable.end( ) ) {
		CondorErrno = ERR_NO_SUCH_TRANSACTION;
		CondorErrMsg = "transaction " + transactionName + " not found";
		return( false );
	}
	xtn = itr->second;

		// if not committing, kill the transaction
	if( !commit ) {
		delete xtn;
		xactionTable.erase( itr );
		outcome = XACTION_ABORTED;
		return( true );
	}

		// commit and log xaction
	outcome = ( rval=xtn->Commit( ) ) ? XACTION_COMMITTED : XACTION_ABORTED;
	if( rval && !xtn->Log( log_fp, &unparser ) ) {
		CondorErrMsg += "; could not log transaction";
		rval = false;
	}

		// kill transaction
	delete xtn;
	xactionTable.erase( itr );
	return( rval );
}


bool ClassAdCollection::
IsMyActiveTransaction( const string &transactionName )
{
	XactionTable::iterator	itr = xactionTable.find( transactionName );
	return( itr != xactionTable.end( ) && itr->second && 
		itr->second->GetLocalXaction( ) );
}


void ClassAdCollection::
GetMyActiveTransactions( vector<string>& xactions )
{
	XactionTable::iterator	itr;

	xactions.clear( );
	for( itr = xactionTable.begin( ); itr != xactionTable.end( ); itr++ ) {
		if( itr->second && itr->second->GetLocalXaction( ) ) {
			xactions.push_back( itr->first );
		}
	}
}


bool ClassAdCollection::
IsActiveTransaction( const string &transactionName )
{
	XactionTable::iterator itr = xactionTable.find( transactionName );
	return( itr != xactionTable.end( ) && itr->second );
}


bool ClassAdCollection::
IsCommittedTransaction( const string &transactionName )
{
	XactionTable::iterator	itr = xactionTable.find( transactionName );
	return( itr != xactionTable.end( ) && !itr->second );
}


bool ClassAdCollection::
GetAllActiveTransactions( vector<string>& xactions )
{
	XactionTable::iterator	itr;

	xactions.clear( );
	for( itr = xactionTable.begin( ); itr != xactionTable.end( ); itr++ ) {
		if( itr->second ) xactions.push_back( itr->first );
	}
	return( true );
}


bool ClassAdCollection::
GetAllCommittedTransactions( vector<string>& xactions )
{
	XactionTable::iterator	itr;

	xactions.clear( );
	for( itr = xactionTable.begin( ); itr != xactionTable.end( ); itr++ ) {
		if( !itr->second ) {
			xactions.push_back( itr->first );
		}
	}
	return( true );
}





bool ClassAdCollection::
RegisterView( const string &viewName, View *view )
{
		// signal error if view already exists
	if( viewRegistry.find( viewName ) != viewRegistry.end( ) ) {
		CondorErrno = ERR_VIEW_PRESENT;
		CondorErrMsg = "cannot register view " + viewName + "; already present";
		return( false );
	}
		// else insert view into registry
	viewRegistry[viewName] = view;
	return( true );
}


bool ClassAdCollection::
UnregisterView( const string &viewName )
{
		// signal error if the view doesn't exist
	if( viewRegistry.find( viewName ) == viewRegistry.end( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "view " + viewName + " not present to unregister";
		return( false );
	}
		// else remove from registry
	viewRegistry.erase( viewName );
	return( true );
}


bool ClassAdCollection::
DisplayView( const ViewName &viewName, FILE *file )
{
	ViewRegistry::iterator itr = viewRegistry.find( viewName );
	if( itr == viewRegistry.end( ) ) {
		return( false );
	}
	return( itr->second->Display( file ) );
}


// ---------------------------------------------------------------------------
// Log operations
// ---------------------------------------------------------------------------
bool ClassAdCollection::
LogState( FILE *fp )
{
		// write log records for all the views
	if( !LogViews( fp, &viewTree, true ) ) {
		CondorErrMsg += "; failed to log state";
		return( false );
	}

		// log classads
	ClassAdTable::iterator	itr;
	ClassAd		logRec;
	ClassAd					*ad;

	if( !logRec.InsertAttr( "OpType", ClassAdCollOp_AddClassAd ) ) {
		CondorErrMsg += "; failed to log state";
		return( false );
	}

	for( itr = classadTable.begin( ); itr != classadTable.end( ); itr++ ) {
           ClassAd *tmp;
           string dd(itr->first);
           tmp = GetClassAd(dd);//GetClassAd( itr->first );
           string buff;
           ClassAdUnParser unparser;
           unparser.Unparse(buff,tmp);
         
            
           logRec.InsertAttr( "Key", itr->first );
           ad = GetClassAd( itr->first );
           logRec.Insert( "Ad", ad );
                
	   //if( !logRec.InsertAttr( "Key", itr->first.c_str( ) )||
	   //			!( ad = GetClassAd( itr->first ) )			||
	   //		!logRec.InsertAttr( "Ad", ad ) 				||
           
           buff="";
           unparser.Unparse(buff,&logRec);

		if(		!WriteLogEntry( fp, &logRec, false ) ) {
			CondorErrMsg += "; failed to log ad, could not log state";
			logRec.Remove( "Ad" );
			return( false );
		}
                buff="";
            
                unparser.Unparse(buff,&logRec);


		logRec.Remove( "Ad" );
	}

	if( fsync( fileno( log_fp ) ) < 0 ) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "fsync() failed when logging state";
		return( false );
	}

	return( true );
}
	

bool ClassAdCollection::
LogViews( FILE *fp, View *view, bool subView )
{
		// first log the current view ...
	{
		ClassAd	logRec;
		ClassAd	*ad = view->GetViewInfo( );

			// insert operation type and view info
		if( !ad || !ad->InsertAttr( "OpType", subView ?
				ClassAdCollOp_CreateSubView:ClassAdCollOp_CreatePartition ) ) {
			if( ad ) delete ad;
			CondorErrMsg += "; failed to log views";
			return( false );
		}
			// dump view info into log record 
		logRec.Update( *ad );
		delete ad;
		if( !WriteLogEntry( fp, &logRec, false ) ) {
			CondorErrMsg += "; failed to log views";
			return( false );
		}
	}

		// next log subordinate child views ...
	SubordinateViews::iterator	xi;
	for( xi=view->subordinateViews.begin( ); xi!=view->subordinateViews.end( ); 
			xi++ ) {
		if( !LogViews( log_fp, *xi, true ) ) return( false );
	}

		// ... and partition child views
	PartitionedViews::iterator	mi;
	for( mi=view->partitionedViews.begin( ); mi!=view->partitionedViews.end( ); 
			mi++ ) {
		if( !LogViews( log_fp, mi->second, false ) ) return( false );
	}

	return( true );
}


END_NAMESPACE
