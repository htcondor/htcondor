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
#include "classad/collection.h"
#include "classad/collectionBase.h"
#include "classad/transaction.h"

using std::string;
using std::vector;
using std::pair;


namespace classad {

#if defined ( WIN32 )
#if !defined ( fileno )
#define fileno _fileno
#endif
#endif

ServerTransaction::
ServerTransaction( )
{
	server 			= NULL;
	local 			= false;
	xactionErrno	= ERR_OK;
	xactionErrMsg	= "";
	xactionErrCause	= NULL;
}


ServerTransaction::
~ServerTransaction( )
{
	ClearRecords( );
	if( xactionErrCause ) {
		delete xactionErrCause;
	}
}


void ServerTransaction::
AppendRecord(int op,const string &key,ClassAd *rec)
{
	XactionRecord 	xrec;

		// set up and save an xaction record
	xrec.op = op;
	xrec.key = key;
	xrec.rec = rec;
	xrec.backup = NULL;

	opList.push_back( xrec );
}


void ServerTransaction::
ClearRecords( )
{
	CollectionOpList::iterator	itr;

	for( itr = opList.begin( ); itr != opList.end( ); itr++ ) {
		if( itr->rec ) delete (itr->rec);
		if( itr->backup ) delete (itr->backup);
	}
	opList.erase( opList.begin( ), opList.end( ) );
}


bool ServerTransaction::
Commit( )
{
	bool						undoNeeded = false;
	CollectionOpList::iterator	itr;
	ClassAd						*ad;
	
	printf("in commit");
		// sanity check
	if( !server ) return( false );

		// play all the operations into the data structure
    for( itr = opList.begin( ); itr != opList.end( ); itr++ ) {
			// we're going to write over/destroy an ad; save it first
		if( ( ad = server->GetClassAd( itr->key ) ) ) {
			if( !( itr->backup = (ClassAd *) ad->Copy( ) ) ) {
				xactionErrno = CondorErrno;
				xactionErrMsg = CondorErrMsg;
				xactionErrCause = itr->rec;
				itr->rec = NULL;
				return( false );
			}
		}

			// play the operation on the collection
		if( !server->PlayClassAdOp( itr->op, itr->rec ) ) {
				// if the operation failed, we need to undo the xaction
			xactionErrCause = itr->rec;
			itr->rec = NULL;
			undoNeeded = true;
			break;
		}
    }
	if( !undoNeeded ) return( true );

		// we need to undo the effects of the transaction
	CollectionOpList::iterator	uitr;
	ClassAdTable::iterator		caitr;
	
		// undo all operations upto (not including) the failed operation
	for( uitr = opList.begin( ); uitr != itr; uitr++ ) {
		caitr = server->classadTable.find( uitr->key );	

			// if we have to undo a "remove classad" op ...
		if(uitr->op==ClassAdCollectionInterface::ClassAdCollOp_RemoveClassAd) {
				// try to restore a backup only if we have a backup at all ...
			if( uitr->backup ) {
				ClassAdProxy	proxy;

					// ... re-insert the previous (backed up) classad
				if( !server->viewTree.ClassAdInserted( server, uitr->key,
						uitr->backup ) ) {
						// failure during undo is fatal
					CondorErrno = ERR_FATAL_ERROR;
					CondorErrMsg += "; could not undo failed transaction";
					return( false );
				}
					// put backup into classad table
				proxy.ad = uitr->backup;
				uitr->backup = NULL;
				server->classadTable[uitr->key] = proxy;

			}
		} else {
				// we are undoing an add, update or modify operation
			Value	val;
			ClassAd	*collAd = caitr->second.ad;;

			if( uitr->backup == NULL ) {
					// if the collection did not have an ad in the first 
					// place just delete the new ad from the collection
				server->classadTable.erase( caitr );
				server->viewTree.ClassAdDeleted( server, uitr->key, collAd );
			} else {
					// we need to replace the collAd with the backup
				server->viewTree.ClassAdPreModify( server, collAd );
				server->viewTree.ClassAdModified(server,uitr->key,uitr->backup);
				caitr->second.ad = uitr->backup;
				uitr->backup = NULL;
			}
		}
	}

		// clear out transaction and signal failure
	ClearRecords( );
	CondorErrno = xactionErrno;
	CondorErrMsg = xactionErrMsg;
	return( false );
}


bool ServerTransaction::
Log( FILE *fp, ClassAdUnParser *unp )
{
    ClassAd 			rec;
	CollectionOpList::iterator	itr;
	string						buf;

	if( !fp ) { return( true );};

        // write out a "OpenTransaction" record
    if(!rec.InsertAttr(ATTR_OP_TYPE,
			ClassAdCollectionInterface::ClassAdCollOp_OpenTransaction)||
            !rec.InsertAttr( "XactionName", xactionName ) 				||
			( local && !rec.InsertAttr( "LocalTransaction", true ) ) ) {
		CondorErrMsg += "; FATAL ERROR: failed to log transaction";
        return( false );
    }
       
	unp->Unparse( buf, &rec );
	if( fprintf( fp, "%s\n", buf.c_str( ) ) < 0 ) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "FATAL ERROR: failed fprintf() on log, errno=";
		CondorErrMsg += std::to_string(errno);
		return( false );
	}

        // log all the operations in the transaction
    for( itr = opList.begin( ); itr != opList.end( ); itr++ ) {
		buf = "";
		unp->Unparse( buf, itr->rec );
		if( fprintf( fp, "%s\n", buf.c_str( ) ) < 0 ) {
			CondorErrno = ERR_FILE_WRITE_FAILED;
			CondorErrMsg = "FATAL ERROR: failed fprintf() on log, errno=";
			CondorErrMsg += std::to_string(errno);
			return( false );
		}
    }
    
        // write out a "CommitTransaction" record and flush the sink
    if(!rec.InsertAttr(ATTR_OP_TYPE,
			ClassAdCollectionInterface::ClassAdCollOp_CommitTransaction)){
		CondorErrMsg += "; FATAL ERROR: failed to log transaction";
        return( false );
    }
        buf = "";
	unp->Unparse( buf, &rec );
	if( fprintf( fp, "%s\n", buf.c_str( ) ) < 0 ) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "FATAL ERROR: failed fprintf() on log, errno=";
		CondorErrMsg += std::to_string(errno);
		return( false );
	}

		// sync ...
        fflush(fp);
        /*
	int status1=fsync( fileno( fp ) );
        fflush(fp); 
        */
    return( true );
}


ClassAd *ServerTransaction::
ExtractErrorCause( )
{
	ClassAd *rec = xactionErrCause;
	xactionErrCause = NULL;
	return( rec );
}

//--------------------- client transaction implementation ---------------

ClientTransaction::
ClientTransaction( )
{
	state = ACTIVE;
	addr = "";
	port = 0;
}

ClientTransaction::
~ClientTransaction( )
{
}


bool ClientTransaction::
LogCommit( FILE *fp, ClassAdUnParser *unp )
{
	ClassAd	rec;
	string	buf;

    if(!rec.InsertAttr(ATTR_OP_TYPE,
			ClassAdCollectionInterface::ClassAdCollOp_CommitTransaction)
			|| !rec.InsertAttr( "XactionName", xactionName ) 	
			|| !rec.InsertAttr( "ServerAddr", addr )	
			|| !rec.InsertAttr( "ServerPort", port )	) {
		CondorErrMsg += "FATAL ERROR: failed to log transaction";
		return( false );
	}
	unp->Unparse( buf, &rec );
	if( fprintf( fp, "%s\n", buf.c_str( ) ) < 0 ) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "FATAL ERROR: failed fprintf()";
		return( false );
	}
	fsync( fileno( fp ) );
	return( true );
}


bool ClientTransaction::
LogAckCommit( FILE *fp, ClassAdUnParser *unp )
{
	if( state != PENDING ) {
		CondorErrno = ERR_BAD_TRANSACTION_STATE;
		CondorErrMsg = "transaction expected to be in COMMITTED state";
		return( false );
	}

	ClassAd	rec;
	string	buf;

    if(!rec.InsertAttr(ATTR_OP_TYPE,
			ClassAdCollectionInterface::ClassAdCollOp_AckCommitTransaction )||
            !rec.InsertAttr( "XactionName", xactionName) ) {
		CondorErrMsg += "FATAL ERROR: failed to log transaction";
		return( false );
	}
	unp->Unparse( buf, &rec );
	if( fprintf( fp, "%s\n", buf.c_str( ) ) < 0 ) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "FATAL ERROR: failed fprintf()";
		return( false );
	}
	fsync( fileno( fp ) );
	return( true );
}


bool ClientTransaction::
LogAbort( FILE *fp, ClassAdUnParser *unp )
{
	if( state != PENDING ) {
		CondorErrno = ERR_BAD_TRANSACTION_STATE;
		CondorErrMsg = "transaction expected to be in COMMITTED state";
		return( false );
	}

	ClassAd	rec;
	string	buf;

    if(!rec.InsertAttr(ATTR_OP_TYPE,
			ClassAdCollectionInterface::ClassAdCollOp_AbortTransaction)
			|| !rec.InsertAttr( "XactionName", xactionName.c_str( ) ) ) {
		CondorErrMsg += "FATAL ERROR: failed to log transaction";
		return( false );
	}
	unp->Unparse( buf, &rec );
	if( fprintf( fp, "%s\n", buf.c_str( ) ) < 0 ) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "FATAL ERROR: failed fprintf()";
		return( false );
	}
	fsync( fileno( fp ) );
	return( true );
}

}
