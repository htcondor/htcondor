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

#include "condor_common.h"
#include "condor_io.h"
#include "classad/common.h"
#include "collectionClient.h"
#include "classad/transaction.h"

using namespace std;

BEGIN_NAMESPACE( classad )

ClassAdCollectionClient::
ClassAdCollectionClient( )
{
	log_fp = NULL;
}


ClassAdCollectionClient::
~ClassAdCollectionClient( )
{
	if( serverSock ) { 
		delete serverSock;
	}
	if( log_fp ) {
		fclose( log_fp );
	}
}


bool ClassAdCollectionClient::
InitializeFromLog( const string &filename )
{
		// clear out transaction table
	Disconnect( );

		// load new state
    logFileName = filename;
    if( !filename.empty( ) ) {
        if( !ReadLogFile( ) ) {
            CondorErrMsg += "; could not initialize from file " + filename;
            return( false );
        }
    }
    return( true );
}


void ClassAdCollectionClient::
Recover( int &committed, int &aborted, int &unknown )
{
	LocalXactionTable::iterator	itr;
	ClientTransaction			*xaction;
	int							outcome;

	committed = aborted = unknown = 0;

		// attempt to commit all pending transactions
	for( itr=localXactionTable.begin( ); itr!=localXactionTable.end(); itr++) {
		xaction = itr->second;
		if( xaction->GetXactionState( )!=ClientTransaction::PENDING ) continue;

		CloseTransaction( itr->first, true, outcome );
		switch( outcome ) {
			case XACTION_COMMITTED: committed++;break;
			case XACTION_ABORTED:	aborted++;	break;
			case XACTION_UNKNOWN:	unknown++;	break;
		}
	}
}


bool ClassAdCollectionClient::
OperateInRecoveryMode( ClassAd *rec )
{
	int	opType=-1;

	rec->EvaluateAttrInt( ATTR_OP_TYPE, opType );

	switch( opType ) {
		case ClassAdCollOp_CommitTransaction: {
			ClientTransaction	*xaction;
			string 	xactionName, serverAddr;
			int		serverPort;

				// obtain transaction state
			rec->EvaluateAttrString( ATTR_XACTION_NAME, xactionName );
			rec->EvaluateAttrString( "ServerAddr", serverAddr );
			rec->EvaluateAttrInt( "ServerPort", serverPort );

			if( !(xaction = new ClientTransaction ) ) {
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
				return( false );
			}

				// restore transaction state
			xaction->SetXactionName( xactionName );
			xaction->SetXactionState( ClientTransaction::PENDING );
			xaction->SetServerAddr( serverAddr, serverPort );

				// enter into transaction table
			localXactionTable[xactionName] = xaction;

			return( true );
		}


			// ack commit and abort  have the same action
		case ClassAdCollOp_AckCommitTransaction:
		case ClassAdCollOp_AbortTransaction: {
			ClientTransaction	*xaction;
			string				xactionName;
			LocalXactionTable::iterator	itr;
			
			rec->EvaluateAttrString( ATTR_XACTION_NAME, xactionName );
			if( ( itr=localXactionTable.find( xactionName ) ) ==
					localXactionTable.end( ) ) {
				CondorErrno = ERR_NO_SUCH_TRANSACTION;
				CondorErrMsg = "transaction "+xactionName+" doesn't exist";
				return( false );
			}
			xaction = itr->second;

			delete xaction;
			localXactionTable.erase( itr );
			return( true );
		}

		default:
			break;
	}

	return( false );
}



bool ClassAdCollectionClient::
Connect( const string &addr, int port, bool useTCP )
{
		// we allow only one connection at a time
	if( serverSock && !Disconnect( ) ) return( false );

		// create a socket ...
	if( !( serverSock = useTCP ? (Sock *)new ReliSock( ) : (Sock *)new SafeSock ) ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( false );
	}

		// connect ...
	if( !serverSock->connect( (char*) addr.c_str( ), port ) ) {
		CondorErrno = ERR_CONNECT_FAILED;
		CondorErrMsg = "failed to connect to server" + addr;
		delete serverSock;
		serverSock = NULL;
		return( false );
	}

		// if using TCP, send a "connect" command
	serverSock->encode( );
	if( useTCP && ( !serverSock->put( int(ClassAdCollOp_Connect) ) || 
			!serverSock->eom( ) ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to send connect request to server";
		delete serverSock;
		serverSock = NULL;
		return( false );
	}

		// done
	serverAddr = addr;
	serverPort = port;
	return( true );
}


bool ClassAdCollectionClient::
Disconnect( )
{
	int outcome;

		// already disconnected?
	if( !serverSock ) return( true );

		// abort all active transactions
	LocalXactionTable::iterator	itr;
	ClientTransaction			*xaction;
	string						xactionName;
	bool						rval = true;

	for( itr=localXactionTable.begin(); itr!=localXactionTable.end(); itr++) {
		xaction = itr->second;
		if( xaction && xaction->GetXactionState( )==ClientTransaction::ACTIVE ){
			xaction->GetXactionName( xactionName );
			rval = rval && CloseTransaction( xactionName, false, outcome );
			delete xaction;
			localXactionTable.erase( xactionName );
		}
	}

		// disconnect from server (send command only if TCP)
	if( serverSock->type( ) == Stream::reli_sock ) {
		serverSock->encode( );
		serverSock->put( (int)ClassAdCollOp_Disconnect );
		serverSock->eom( );
		serverSock->close( );
	}

	delete serverSock;
	serverSock = NULL;
	currentXactionName = "";
	return( rval );
}


bool ClassAdCollectionClient::
OpenTransaction( const string &transactionName )
{
	ClientTransaction *xaction;

		// check if we are bound to a server
	if( !serverSock ) {
		CondorErrno = ERR_CLIENT_NOT_CONNECTED;
		CondorErrMsg = "client not bound to server";
		return( false );
	}
	if( serverSock->type( ) != Stream::reli_sock ) {
		CondorErrno = ERR_BAD_CONNECTION_TYPE;
		CondorErrMsg = "cannot open transaction over unreliable connection";
		return( false );
	}

		// check if the transaction name is being used locally
	if( localXactionTable.find( transactionName ) != localXactionTable.end( ) ){
		CondorErrno = ERR_TRANSACTION_EXISTS;
		CondorErrMsg = "transaction " + transactionName + " already exists";
		return( false );
	}

		// records for open transaction and ack from server
	ClassAd	rec, *ack=NULL;
	rec.InsertAttr( ATTR_XACTION_NAME, transactionName );
	rec.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_OpenTransaction );

		// send the record to the server
	if( !SendOpToServer( ClassAdCollOp_OpenTransaction, &rec,
			ClassAdCollOp_AckOpenTransaction, ack ) ) {
		if( ack ) delete ack;
		return( false );
	}

		// create a client transaction object and enter into local table
	delete ack;
	if( !( xaction = new ClientTransaction( ) ) ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( false );
	}
	xaction->SetXactionName( transactionName );
	xaction->SetServerAddr( serverAddr, serverPort );
	localXactionTable[transactionName] = xaction;

		// use new transaction as current transaction
	currentXactionName = transactionName;
	return( true );
}


bool ClassAdCollectionClient::
DoCommitProtocol( const string &xactionName, int &outcome, 
	ClientTransaction *xaction )
{
	ClassAd	rec, *ack;
	int		xactionState = xaction->GetXactionState( );
	int		op = ClassAdCollOp_CommitTransaction; 
	int		ackOp = ClassAdCollOp_AckCommitTransaction;

	rec.InsertAttr( ATTR_XACTION_NAME, xactionName );
	rec.InsertAttr( ATTR_OP_TYPE, op );

		// log only if the transaction is active (i.e., no commit rec in log)
	if( xactionState == ClientTransaction::ACTIVE && log_fp && 
			!xaction->LogCommit( log_fp, &unparser ) ) {
		CondorErrMsg += "; FATAL ERROR: failed to log commit record";
		return( false );
	} 
	xaction->SetXactionState( ClientTransaction::PENDING );	

		// send operation to server and get ack 
	if( !SendOpToServer( op, &rec, ackOp, ack ) ) {
		outcome = XACTION_UNKNOWN;
		CondorErrMsg += "; failed to close transaction";
		return( false );
	}

		// check if there was a server error (xaction was aborted)
	if( CondorErrno != ERR_OK ) {
		if( log_fp ) xaction->LogAbort( log_fp, &unparser );
		delete xaction;
		localXactionTable.erase( xactionName );
		outcome = XACTION_ABORTED;
		if( currentXactionName == xactionName ) currentXactionName = "";
		return( false );
	}

		// write the ack record to the log file
	if( !WriteLogEntry( log_fp, ack, true ) ) {
		outcome = XACTION_UNKNOWN;
		delete ack;
		return( false );
	}

		// send the "forget transaction" command to the server
		// (even if we can't send this message, we know the xaction was
		// committed --- the server will forget the xaction in its own time)
	delete ack;
	ack = NULL;
	outcome = XACTION_COMMITTED;
	op = ClassAdCollOp_ForgetTransaction;
	rec.InsertAttr( ATTR_OP_TYPE, op );
	if( currentXactionName == xactionName ) currentXactionName = "";
	SendOpToServer( op, &rec, ackOp, ack );  // won't get an ack for this

		// done
	delete xaction;
	localXactionTable.erase( xactionName );
	return( true );
}


bool ClassAdCollectionClient::
CloseTransaction( const string &xactionName, bool commit, int &outcome )
{
	outcome = XACTION_UNKNOWN;

		// check if the transaction exists
	LocalXactionTable::iterator	itr = localXactionTable.find( xactionName );
	if( itr == localXactionTable.end( ) ){
		CondorErrno = ERR_NO_SUCH_TRANSACTION;
		CondorErrMsg = "transaction " + xactionName + " doesn't exist";
		return( false );
	}
	ClientTransaction *xaction = itr->second;

		// make sure we're connected to the right server
	string	xactionServerAddr;
	int		xactionServerPort;
	Sock	*savedSock;
	string	savedServerAddr;
	int		savedServerPort;
	bool	rval=false;
	
	xaction->GetServerAddr( xactionServerAddr, xactionServerPort );

		// "save" current connection
	savedSock = serverSock;
	savedServerAddr = serverAddr;
	savedServerPort	= serverPort;
	if( xactionServerAddr != serverAddr || xactionServerPort != serverPort ) {
		if( !( serverSock = new ReliSock( ) ) || 
				!serverSock->connect( (char*)xactionServerAddr.c_str(), 
				xactionServerPort ) ) {
			CondorErrno = ERR_COMMUNICATION_ERROR;
			CondorErrMsg = "failed to connect to server " + xactionServerAddr;
			if( serverSock ) delete serverSock;
			serverSock = savedSock;
			return( false );
		}
		serverAddr = xactionServerAddr;
		serverPort = xactionServerPort;
	} 

		// check if we can talk to the server
	if( !serverSock ) {
		CondorErrno = ERR_CLIENT_NOT_CONNECTED;
		CondorErrMsg = "client not bound to server";
		if( serverSock != savedSock ) {
			delete serverSock;
			serverSock = savedSock;;
			serverAddr = savedServerAddr;
			serverPort = savedServerPort;
		}
		return( false );
	}
	if( serverSock->type( ) != Stream::reli_sock ) {
		CondorErrno = ERR_BAD_CONNECTION_TYPE;
		CondorErrMsg = "cannot close transaction over unreliable connection";
		if( serverSock != savedSock ) {
			delete serverSock;
			serverSock = savedSock;;
			serverAddr = savedServerAddr;
			serverPort = savedServerPort;
		}
		return( false );
	}


		// get the local transaction state
	int	xactionState = xaction->GetXactionState( );
	if( xactionState == ClientTransaction::ACTIVE ) {
		if( commit ) {
				// if committing, do the commit protocol
			rval = DoCommitProtocol( xactionName, outcome, xaction );
		} else {
				// aborting the transaction
			ClassAd	rec, *ack=NULL;
			int		op, ackOp;

			op = ClassAdCollOp_AbortTransaction;
			rec.InsertAttr( ATTR_XACTION_NAME, xactionName );
			rec.InsertAttr( ATTR_OP_TYPE, op );

				// ok if there's an error here --- server will clean up
			SendOpToServer( op, &rec, ackOp, ack );	// wont get an ack
			delete xaction;
			localXactionTable.erase( xactionName );

				// restore connection
			serverSock = savedSock;
			serverAddr = savedServerAddr;
			serverPort = savedServerPort;
			rval = true;
		}
	} else if( xactionState == ClientTransaction::PENDING ) {
		ClassAd	rec, *ack=NULL;
		int		op, ackOp;
		int 	serverXactionState;

			// pending: may be absent, active or committed in server
		outcome = XACTION_UNKNOWN;
		if( !GetServerXactionState( xactionName, serverXactionState ) ) {
			rval = false;
		} else {
			switch( serverXactionState ) {
					// if the server doesn't have this transaction abort 
				case ServerTransaction::ABSENT: {
					outcome = XACTION_ABORTED;
					delete xaction;
					localXactionTable.erase( xactionName );
						// return false if we wanted to commit
					rval = !commit;
					break;
				}

					// if the server already committed, just send the forget
				case ServerTransaction::COMMITTED: {
					op = ClassAdCollOp_ForgetTransaction;
					rec.InsertAttr( ATTR_XACTION_NAME, xactionName );
					rec.InsertAttr( ATTR_OP_TYPE, op );
						// ok if error here --- server will clean up
					SendOpToServer( op, &rec, ackOp, ack );	// wont get an ack
					outcome = XACTION_COMMITTED;
					delete xaction;
					localXactionTable.erase( xactionName );
						// return true if we wanted to commit
					rval = commit;
					break;
				}

					// if xaction is active in server 
				case ServerTransaction::ACTIVE: {
					if( commit ) {
							// do the commit protocol
						rval=DoCommitProtocol( xactionName, outcome, xaction );
					} else {
						op = ClassAdCollOp_AbortTransaction;
						rec.InsertAttr( ATTR_XACTION_NAME, xactionName );
						rec.InsertAttr( ATTR_OP_TYPE, op );
							// ok if error here --- server will clean up
						SendOpToServer( op, &rec, ackOp, ack ); // wont get ack
						delete xaction;
						localXactionTable.erase( xactionName );
						outcome = XACTION_ABORTED;
						rval = true;
						break;
					}
				}
					
				default:
					rval = false;
					break;
			}
		}
	}
		
		// restore connection if necessary
	if( serverSock != savedSock ) {
		delete serverSock;
		serverSock = savedSock;
		serverAddr = savedServerAddr;
		serverPort = savedServerPort;
	}

		// return return value
	return( rval );
}


bool ClassAdCollectionClient::
CreateSubView( const ViewName &viewName, const ViewName &parentViewName,
	const string &constraint, const string &rank, const string &partitionExprs )
{
	ClassAd *rec, *ack;
	bool			 rval;

		// create the op record
	rec=_CreateSubView(viewName,parentViewName,constraint,rank,partitionExprs);
	if( !rec ) return( false );

		// perform the operation on the server
	rval = SendOpToServer( ClassAdCollOp_CreateSubView, rec, 
	 		ClassAdCollOp_AckViewOp, ack );

	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


bool ClassAdCollectionClient::
CreatePartition( const ViewName &viewName, const ViewName &parentViewName,
	const string &constraint, const string &rank, const string &partitionExprs,
	ClassAd *rep )
{
	ClassAd *rec, *ack;
	bool			 rval;

		// create the op record
	rec = _CreatePartition( viewName, parentViewName, constraint, rank, 
		partitionExprs, rep );
	if( !rec ) return( false );

		// perform the operation on the server
	rval = SendOpToServer( ClassAdCollOp_CreatePartition, rec, 
	 		ClassAdCollOp_AckViewOp, ack );

	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


bool ClassAdCollectionClient::
DeleteView( const ViewName &viewName )
{
	ClassAd *rec, *ack;
	bool			 rval;

		// create the op record
	if( !( rec = _DeleteView( viewName ) ) ) {
		return( false );
	}

		// perform operation on server
	rval = SendOpToServer( ClassAdCollOp_DeleteView, rec, 
			ClassAdCollOp_AckViewOp, ack );

	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


bool ClassAdCollectionClient::
SetViewInfo( const ViewName &viewName, const string &constraint, 
	const string &rank, const string &partitionExprs )
{
	ClassAd *rec, *ack;
	bool			 rval;

		// create the op record
	if( !( rec=_SetViewInfo( viewName, constraint, rank, partitionExprs  ) ) ) {
		return( false );
	}

		// perform operation on server
	rval = SendOpToServer( ClassAdCollOp_SetViewInfo, rec, 
			ClassAdCollOp_AckViewOp, ack );

	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


bool ClassAdCollectionClient::
_GetViewNames( int command, const ViewName &view, vector<string>& viewNames)
{
	ClassAd				rec, *ack=NULL;
	Value				val;
	ExprList			*el;
	ExprListIterator	itr;
	string				strVal;

	viewNames.clear( );

	rec.InsertAttr( ATTR_OP_TYPE, command );
	rec.InsertAttr( ATTR_VIEW_NAME, view );
	
	if( !SendOpToServer( command, &rec, ClassAdCollOp_AckReadOp, ack ) || 
			CondorErrno != ERR_OK ) {
		if( ack ) delete ack;
		return( false );
	}

	if( !ack->EvaluateAttr( command==ClassAdCollOp_GetPartitionedViewNames ? 
			ATTR_PARTITIONED_VIEWS:ATTR_SUBORDINATE_VIEWS, val ) ||
			!val.IsListValue( (const ExprList *)el ) ) {
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "bad server ack --- bad or missing view names";
		return( false );
	}

	itr.Initialize( el );
	while( !itr.IsAfterLast( ) ) {
		if( !itr.CurrentValue( val ) || !val.IsStringValue( strVal ) ) {
			delete ack;
			viewNames.clear( );
			return( false );
		}
		viewNames.push_back( strVal );
		itr.NextExpr( );
	}

	return( true );
}


bool ClassAdCollectionClient::
GetSubordinateViewNames( const ViewName &view, vector<string>& viewNames)
{
	return(_GetViewNames(ClassAdCollOp_GetSubordinateViewNames,view,viewNames));
}


bool ClassAdCollectionClient::
GetPartitionedViewNames( const ViewName &view, vector<string>& viewNames)
{
	return(_GetViewNames(ClassAdCollOp_GetPartitionedViewNames,view,viewNames));
}


bool ClassAdCollectionClient::
FindPartitionName( const ViewName &view, ClassAd *rep, ViewName &parViewName)
{
	ClassAd	rec, *ack=NULL;

	rec.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_FindPartitionName );
	rec.InsertAttr( ATTR_VIEW_NAME, view );
	rec.Insert( ATTR_AD, rep );

	if( !SendOpToServer( ClassAdCollOp_FindPartitionName, &rec,
			ClassAdCollOp_AckReadOp, ack ) || CondorErrno != ERR_OK ) {
		rec.Remove( ATTR_AD );
		if( ack ) delete ack;
		parViewName = "";
		return( false );
	}

	rec.Remove( ATTR_AD );
	if( !ack->EvaluateAttrString( "PartitionName", parViewName ) ) {
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "bad server ack --- missing 'PartitionName' attribute";
		delete ack;
		return( false );
	}

	delete ack;
	return( true );
}


//-----------------------------------------------------------------------------

bool ClassAdCollectionClient::
AddClassAd( const string &key, ClassAd *newAd )
{
	ClassAd *rec, *ack;
	bool			 rval;

		// create the op record
	if( !( rec = _AddClassAd( currentXactionName, key, newAd  ) ) ) {
		return( false );
	}

		// perform operation on server
	rval = SendOpToServer( ClassAdCollOp_AddClassAd, rec, 
			ClassAdCollOp_AckClassAdOp, ack );

	rec->Remove( ATTR_AD );
	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


bool ClassAdCollectionClient::
UpdateClassAd( const string &key, ClassAd *updAd )
{
	ClassAd *rec, *ack;
	bool			 rval;

		// create the op record
	if( !( rec = _UpdateClassAd( currentXactionName, key, updAd  ) ) ) {
		return( false );
	}

		// perform operation on server
	rval = SendOpToServer( ClassAdCollOp_UpdateClassAd, rec, 
			ClassAdCollOp_AckClassAdOp, ack );

	rec->Remove( ATTR_AD );
	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


bool ClassAdCollectionClient::
ModifyClassAd( const string &key, ClassAd *modAd )
{
	ClassAd *rec, *ack;
	bool			 rval;

		// create the op record
	if( !( rec = _ModifyClassAd( currentXactionName, key, modAd  ) ) ) {
		return( false );
	}

		// perform operation on server
	rval = SendOpToServer( ClassAdCollOp_ModifyClassAd, rec, 
			ClassAdCollOp_AckClassAdOp, ack );

	rec->Remove( ATTR_AD );
	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


bool ClassAdCollectionClient::
RemoveClassAd( const string &key )
{
	ClassAd *rec, *ack;
	bool	 rval;

		// create the op record
	if( !( rec = _RemoveClassAd( currentXactionName, key ) ) ) {
		return( false );
	}

		// perform operation on server
	rval = SendOpToServer( ClassAdCollOp_RemoveClassAd, rec, 
			ClassAdCollOp_AckClassAdOp, ack );

	if( rec ) delete rec;
	if( ack ) delete ack;

	return( rval );
}


ClassAd *ClassAdCollectionClient::
GetClassAd( const string &key )
{
	ClassAd	rec, *ack=NULL, *ad;
	Value	val;

	rec.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_GetClassAd );
	rec.InsertAttr( ATTR_KEY, key );

	if( !SendOpToServer( ClassAdCollOp_GetClassAd, &rec, 
			ClassAdCollOp_AckReadOp, ack ) || CondorErrno != ERR_OK ) {
		if( ack ) delete ack;
		return( NULL );
	}

	if( !ack->EvaluateAttr( ATTR_AD, val ) || !val.IsClassAdValue( ad ) ) {
		delete ack;
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "bad server ack; bad or missing 'Ad' attribute";
		return( NULL );
	}

	ack->Remove( ATTR_AD );
	delete ack;
	return( ad );
}


bool ClassAdCollectionClient::
SendOpToServer( int opType, ClassAd *rec, int expectedOpType, ClassAd *&ack )
{
	int 	ackOpType, serverErrno;
	string	buffer, serverErrMsg;
	char	*tmp;
	bool 	dontWantAck;

	ack = NULL;

		// check server connection
	if( !serverSock ) {
		CondorErrno = ERR_CLIENT_NOT_CONNECTED;
		CondorErrMsg = "client not connected to server";
		return( false );
	}

		// do we or don't we want an ack for this operation?
	dontWantAck = opType == ClassAdCollOp_AbortTransaction 	||
				opType == ClassAdCollOp_ForgetTransaction	||
			( ( opType >= __ClassAdCollOp_ClassAdOps_Begin__ && 
				opType <= __ClassAdCollOp_ClassAdOps_End__ ) && 
				amode == DONT_WANT_ACKS );
	if( !dontWantAck && rec ) {
		rec->InsertAttr( "WantAck", true );
	}

		// if in transaction or we need acks or we're doing a transaction op, 
		// make sure we're using tcp
	if( ( currentXactionName != "" || !dontWantAck || 
			( opType>=__ClassAdCollOp_XactionOps_Begin__ &&
			opType <= __ClassAdCollOp_XactionOps_End__ ) ) && 
			serverSock->type( ) == Stream::safe_sock ) {
		CondorErrno = ERR_BAD_CONNECTION_TYPE;
		CondorErrMsg = "cannot perform operation over unreliable connection";
		return( false );
	}

 
		// send op record to server (some commands don't have rec classads)
	serverSock->encode( );
	if( rec ) unparser.Unparse( buffer, rec );
	if( !serverSock->put(opType) ||
			!(rec ? serverSock->put((char*)buffer.c_str()) : true ) ||
			!serverSock->end_of_message( ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to send [" +
			string(ClassAdCollectionInterface::GetOpString(opType)) +
			"] to server";
		return( false );
	}

		// no ack required?
	if( dontWantAck ) {
		ack = NULL;
		return( true );
	}

		// ack required
	serverSock->decode( );
	tmp = NULL;
	if( !serverSock->get( ackOpType ) || !serverSock->get( tmp ) ||
			!serverSock->end_of_message( ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to receive ack from server";
		return( false );
	}

		// make sure we got the right ack
	buffer = tmp;
	free( tmp );
	ack = NULL;

		// expected type?
	if( ackOpType != expectedOpType ) {
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "expected [" + 
			string(ClassAdCollectionInterface::GetOpString(expectedOpType)) +
				"] but got [" + 
			string(ClassAdCollectionInterface::GetOpString(ackOpType)) + 
				"]";
		return( false );
	}

		// valid classad?
	if( !(ack = parser.ParseClassAd( buffer ) ) ) {
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "bad server ack --- " + CondorErrMsg;
		return( false );
	} 

		// server side error?
	if( !ack->EvaluateAttrInt( "CondorErrno", serverErrno ) ||
			!ack->EvaluateAttrString( "CondorErrMsg", serverErrMsg ) ) {
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "missing error information";
		return( false );
	}

	CondorErrno = serverErrno;
	CondorErrMsg = serverErrMsg;

		// done
	return( true );
}



bool ClassAdCollectionClient::
IsMyActiveTransaction( const string &transactionName )
{
	LocalXactionTable::iterator itr = localXactionTable.find(transactionName);
	return( itr != localXactionTable.end( ) && itr->second &&
		itr->second->GetXactionState( ) == ClientTransaction::ACTIVE );
}


void ClassAdCollectionClient::
GetMyActiveTransactions( vector<string>& xactions )
{
	LocalXactionTable::iterator	itr;
	xactions.clear( );
	for( itr=localXactionTable.begin( ); itr!=localXactionTable.end( ); itr++ ){
		if( itr->second && itr->second->GetXactionState( ) ==
				ClientTransaction::ACTIVE ) {
			xactions.push_back( itr->first );
		}
	}
}


bool ClassAdCollectionClient::
IsMyPendingTransaction( const string &transactionName )
{
	LocalXactionTable::iterator itr = localXactionTable.find(transactionName);
	return( itr != localXactionTable.end( ) && itr->second &&
		itr->second->GetXactionState( ) == ClientTransaction::PENDING );
}


void ClassAdCollectionClient::
GetMyPendingTransactions( vector<string>& xactions )
{
	LocalXactionTable::iterator	itr;
	xactions.clear( );
	for( itr=localXactionTable.begin( ); itr!=localXactionTable.end( ); itr++ ){
		if( itr->second && itr->second->GetXactionState( ) ==
				ClientTransaction::PENDING ) {
			xactions.push_back( itr->first );
		}
	}
}


bool ClassAdCollectionClient::
IsMyCommittedTransaction( const string &transactionName )
{
	return( IsMyPendingTransaction( transactionName ) &&
			IsCommittedTransaction( transactionName ) );
}


void ClassAdCollectionClient::
GetMyCommittedTransactions( vector<string>& xactions )
{
	vector<string> pending;
	vector<string> committed;
	vector<string>::iterator	pitr, citr;

	GetMyPendingTransactions( pending );
	GetAllCommittedTransactions( committed );

		// for each pending xaction ...
	for( pitr = pending.begin(); pitr != pending.end( ); pitr++ ) {
			// see if the xaction is also committed
		for( citr = committed.begin( ); citr != committed.end( ); citr++ ) {
			if( *citr == *pitr ) {
					// yes ... this is one of my committed xactions
				xactions.push_back( *pitr );
				break;
			}
		}
	}
}



bool ClassAdCollectionClient::
_CheckTransactionState( int command, const string &xactionName, Value &result )
{
	ClassAd	rec, *ack=NULL;

	rec.InsertAttr( ATTR_OP_TYPE, command );
	rec.InsertAttr( ATTR_XACTION_NAME, xactionName );

	if( !SendOpToServer( command, &rec, ClassAdCollOp_AckReadOp, ack ) ||
			CondorErrno != ERR_OK ) {
		if( ack ) delete ack;
		return( false );
	}

	if( !ack->EvaluateAttr( "Result", result ) ) {
		delete ack;
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "bad server ack --- missing 'Result' attribute";
		return( false );
	}

	delete ack;
	return( true );
}


bool ClassAdCollectionClient::
IsActiveTransaction( const string &xactionName )
{
	Value 	val;
	bool	b;
	return( _CheckTransactionState( ClassAdCollOp_IsActiveTransaction,
		xactionName, val ) && val.IsBooleanValue(b) && b );
}


bool ClassAdCollectionClient::
IsCommittedTransaction( const string &xactionName )
{
	Value 	val;
	bool	b;
	return( _CheckTransactionState( ClassAdCollOp_IsCommittedTransaction,
		xactionName, val ) && val.IsBooleanValue(b) && b );
}


bool ClassAdCollectionClient::
GetServerXactionState( const string &xactionName, int &state )
{
	Value	val;
	return( _CheckTransactionState( ClassAdCollOp_GetServerTransactionState,
		xactionName, val ) && val.IsIntegerValue( state ) );
}


bool ClassAdCollectionClient::
_GetAllxxxTransactions( int command, vector<string>& xactions )
{
	ClassAd				*ack;
	Value				val;
	ExprList			*el;
	ExprListIterator	itr;
	string				strVal;

	xactions.clear( );

	if( !SendOpToServer( command, NULL, ClassAdCollOp_AckReadOp, ack ) ) {
		if( ack ) delete ack;
		return( false );
	}

	if( !ack->EvaluateAttr( command==ClassAdCollOp_GetAllActiveTransactions ? 
			"ActiveTransactions":"CommittedTransactions", val ) ||
			!val.IsListValue( (const ExprList *)el ) ) {
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "bad server ack --- bad or missing transaction names";
		return( false );
	}

	itr.Initialize( el );
	while( !itr.IsAfterLast( ) ) {
		if( !itr.CurrentValue( val ) || !val.IsStringValue( strVal ) ) {
			delete ack;
			xactions.clear( );
			return( false );
		}
		xactions.push_back( strVal );
		itr.NextExpr( );
	}

	return( true );
}


bool ClassAdCollectionClient::
GetAllActiveTransactions( vector<string>& xactions )
{
	return( _GetAllxxxTransactions( ClassAdCollOp_GetAllActiveTransactions,
		xactions ) );
}


bool ClassAdCollectionClient::
GetAllCommittedTransactions( vector<string>& xactions )
{
	return( _GetAllxxxTransactions( ClassAdCollOp_GetAllCommittedTransactions,
		xactions ) );
}


bool ClassAdCollectionClient::
GetViewInfo( const string &viewName, ClassAd*& vi )
{
	ClassAd rec, *ack=NULL;
	Value	val;

	rec.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_GetViewInfo );
	rec.InsertAttr( ATTR_VIEW_NAME, viewName );

	if( !SendOpToServer( ClassAdCollOp_GetViewInfo, &rec, 
			ClassAdCollOp_AckReadOp, ack ) || CondorErrno != ERR_OK ) {
		if( ack ) delete ack;
		return( false );
	}

	if( !ack->EvaluateAttr( ATTR_VIEW_INFO, val ) || !val.IsClassAdValue(vi) ){
		CondorErrno = ERR_BAD_SERVER_ACK;
		CondorErrMsg = "bad server ack; bad or missing 'ViewInfo' attribute";
		delete ack;
		vi = NULL;
		return( false );
	}

	ack->Remove( ATTR_VIEW_INFO );
	delete ack;
	return( true );
}


bool ClassAdCollectionClient::
LogState( FILE* fp )
{
	LocalXactionTable::iterator	itr;
	ClientTransaction			*xaction;
	ClassAd						rec;
	string						xactionName, serverAddr, buffer;
	int							serverPort;

		// log pending transactions only
	for( itr=localXactionTable.begin(); itr!=localXactionTable.end( ); itr++ ){
		xaction = itr->second;
		if( xaction->GetXactionState() != ClientTransaction::PENDING ) continue;

		xaction->GetXactionName( xactionName );
		xaction->GetServerAddr( serverAddr, serverPort );
		rec.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_CommitTransaction );
		rec.InsertAttr( ATTR_XACTION_NAME, xactionName );
		rec.InsertAttr( "ServerAddr", serverAddr );
		rec.InsertAttr( "ServerPort", serverPort );

		buffer = "";
		unparser.Unparse( buffer, &rec );
		if( fprintf( fp, "%s\n", buffer.c_str( ) ) < 0 ) {
			CondorErrno = ERR_FILE_WRITE_FAILED;
			CondorErrMsg = "failed to write to log";
			return( false );
		}
	}

	return( true );
}

END_NAMESPACE	// classad
