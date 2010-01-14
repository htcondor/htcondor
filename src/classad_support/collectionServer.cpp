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
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_io.h"
#include "classad/common.h"
#include "classad/transaction.h"
#include "collectionServer.h"

using namespace std;

BEGIN_NAMESPACE( classad )

ClassAdCollectionServer::
ClassAdCollectionServer( ) : ClassAdCollection()
{
	// Our parent, ClassAdCollection, handles everything. 
	return;
}


ClassAdCollectionServer::
~ClassAdCollectionServer( )
{
	// Our parent, ClassAdCollection, handles everything. 
	return; 
}


//------------------------------------------------------------------------------
// Remote network mode interface
//------------------------------------------------------------------------------
int ClassAdCollectionServer::
HandleClientRequest( int command, Sock *clientSock )
{
	ClassAd	*rec=NULL;
	char	*tmp=NULL;
	string	buffer;

	CondorErrno = ERR_OK;
	CondorErrMsg = "";
	
	// if the operation is "connect", just keep the sock and return
	if( command == ClassAdCollOp_Connect ) {
		clientSock->decode( );
		clientSock->eom( );
		return( 1 );
	}

	// if the operation is "disconnect", just close the client sock
	if( command == ClassAdCollOp_Disconnect ) {
		clientSock->decode( );
		clientSock->eom( );
		return( 0 );
	}

	// read the classad off the wire
	if( !clientSock->get( tmp ) || !clientSock->eom( ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to read client request";
		return( -1 );
	}

	// parse the classad
	buffer = tmp;
	free( tmp );

	// the 'rec' classad below is deleted by HandleQuery, HandleReadOnly...,
	// or OperateInNetworkMode as necessary
	if( !( rec = parser.ParseClassAd( buffer ) ) ) {
		CondorErrMsg += "; failed to parse client request";
		return( -1 );
	}
	// handle query command
	if( command == ClassAdCollOp_QueryView ) {
		return( HandleQuery( rec, clientSock ) ? +1 : -1 );
	}

	// handle read only commands separate from modify commands
	if( command >= __ClassAdCollOp_ReadOps_Begin__ &&
			command <= __ClassAdCollOp_ReadOps_End__ ) {
		return( HandleReadOnlyCommands( command, rec, clientSock ) ? +1 : -1 );
	}

	// else handle command in "network mode" ...
	return( OperateInNetworkMode( command, rec, clientSock ) ? +1 : -1 );
}

bool ClassAdCollectionServer::
OperateInNetworkMode( int opType, ClassAd *logRec, Sock *clientSock )
{
	int		ackOpType;
	bool	failed = false;
	ClassAd	ack;
               
	switch( opType ) {
		// acks for open and commit xaction operations
		case ClassAdCollOp_OpenTransaction:
		case ClassAdCollOp_CommitTransaction:
		case ClassAdCollOp_AbortTransaction:
		case ClassAdCollOp_ForgetTransaction: {
			string				xactionName;
			ServerTransaction	*xaction;
				
			logRec->EvaluateAttrString( "XactionName", xactionName );

			// set the ack type
			if( opType == ClassAdCollOp_OpenTransaction ) {
				ackOpType = ClassAdCollOp_AckOpenTransaction;
				ack.InsertAttr( ATTR_OP_TYPE,ClassAdCollOp_AckOpenTransaction );
			} else if( opType == ClassAdCollOp_CommitTransaction ) {
				ackOpType = ClassAdCollOp_AckCommitTransaction;
				ack.InsertAttr(ATTR_OP_TYPE,ClassAdCollOp_AckCommitTransaction);
			} // (abort and forget don't have acks)

			// play the transaction operation 
			if( !PlayXactionOp( opType, xactionName, logRec, xaction ) ) {
					// if failed, insert error cause in ack and kill xaction
				failed = true;
				delete logRec;
				ClassAd *errorCause = xaction->ExtractErrorCause( );
				ack.Insert( "ErrorCause", errorCause );
				if( xaction ) delete xaction;
				xactionTable.erase( xactionName );
				break;
			}

			// don't delete logRec just yet --- need it for WriteLogEntry
			// in ForgetTransaction case below

			if( opType == ClassAdCollOp_CommitTransaction ) {
				// log transaction if possible (the xaction is logged only
				// after it is played on the in-memory data structure so that
				// we make a persistent record of the xaction iff the
				// in-memory commit succeeds)
				if( !xaction || !xaction->Log( log_fp, &unparser ) ) {
					delete logRec;
					CondorErrno = ERR_FATAL_ERROR;
					CondorErrMsg = "FATAL ERROR: in memory commit succeeded, "
						"but log failed";
					return( false );
				}
			} else if( opType == ClassAdCollOp_ForgetTransaction ) {
				// no ack for ForgetTransaction
				bool rval = WriteLogEntry( log_fp, logRec );
				delete logRec;
				return( rval );
			} else if( opType == ClassAdCollOp_AbortTransaction ) {
				// no ack for abort transaction, and no logging either
				delete logRec;
				return( true );
			}
			// opType must be OpenTransaction or CommitTransaction here 
			// (if CommitTransaction, the xaction has also been logged)
			delete logRec;
			break;
		}

		// all view ops require acks
		case ClassAdCollOp_CreateSubView:
		case ClassAdCollOp_CreatePartition:
		case ClassAdCollOp_DeleteView:
		case ClassAdCollOp_SetViewInfo: {
			ackOpType = ClassAdCollOp_AckViewOp;
			ack.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_AckViewOp );
			failed= !PlayViewOp(opType,logRec) || !WriteLogEntry(log_fp,logRec);
			delete logRec;
			break;
		}

		// acks for classad ops only if requested
		case ClassAdCollOp_AddClassAd:
		case ClassAdCollOp_UpdateClassAd:
		case ClassAdCollOp_ModifyClassAd:
		case ClassAdCollOp_RemoveClassAd: {
			string 	xactionName, key;
			bool	wantAck = false;

			logRec->EvaluateAttrString( "XactionName", xactionName );
			logRec->EvaluateAttrBool( "WantAck", wantAck );
			logRec->EvaluateAttrString( "Key", key );
			ack.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_AckClassAdOp );
			ackOpType = ClassAdCollOp_AckClassAdOp;

			if( xactionName == "" ) {
				// not in xaction; just play the operation
				failed = !PlayClassAdOp( opType, logRec ) ||
					!WriteLogEntry(log_fp,logRec );
				delete logRec;
				if( !wantAck ) return( !failed );
				break;
			}
			// in transaction; add record to transaction
			ServerTransaction		*xaction;
			XactionTable::iterator 	itr = xactionTable.find( xactionName );
			if( itr == xactionTable.end( ) ) {
				CondorErrno = ERR_NO_SUCH_TRANSACTION;
				CondorErrMsg = "transaction "+xactionName+" doesn't exist";
				if( wantAck ) {
					ack.Insert( "ErrorCause", logRec );
					break;
				} else {
					delete logRec;	
				}
				return( true );
			}
			xaction = itr->second;
			xaction->AppendRecord( opType, key, logRec );
			if( !wantAck ) return( true );	
		}
	}

	// send the ack to the client
	string	buffer;
	ack.InsertAttr( "CondorErrno", failed ? CondorErrno : ERR_OK );
	ack.InsertAttr( "CondorErrMsg", failed ? CondorErrMsg : "" );
	unparser.Unparse( buffer, &ack );
	clientSock->encode( );
	if(!clientSock->put(ackOpType) || !clientSock->put((char*)buffer.c_str()) ||
			!clientSock->end_of_message( ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "unable to send ack to client";
		return( false );
	}

	return( true );
}


bool ClassAdCollectionServer::
HandleQuery( ClassAd *query, Sock *clientSock )
{
	string				viewName, key, buffer;
	MatchClassAd		mad;
	vector<string>		attrs;
	bool				wantResults, wantPostlude;
	ExprTree			*tProjAttrs;

	// unpack attributes; assume reasonable defaults on error
	if( !query->EvaluateAttrString( ATTR_VIEW_NAME, viewName ) ) {
		viewName = "root";
	}
	if( !query->EvaluateAttrBool( "WantResults", wantResults ) ) {
		wantResults = true;
	}
	if( !query->EvaluateAttrBool( "WantPostlude", wantPostlude ) ) {
		wantPostlude = false;
	}
	if( wantResults ) {
		// unpack projection attributes
		tProjAttrs = query->Lookup( "ProjectionAttrs" );
		if( tProjAttrs && tProjAttrs->GetKind( )==ExprTree::EXPR_LIST_NODE ) {
			ExprListIterator	itr;
			Value				val;
			string				attr;

			itr.Initialize( (ExprList*)tProjAttrs );
			while( !itr.IsAfterLast( ) ) {
				itr.CurrentValue( val );
				if( val.IsStringValue( attr ) ) {
					attrs.push_back( attr );
				}
				itr.NextExpr( );
			}
			if( attrs.size( ) == 0 ) {
				tProjAttrs = NULL;
			}
		}
	}

	// setup to evaluate query
	View					*view;
	ClassAd					*ad;
	ViewMembers::iterator	vmi;
	ViewRegistry::iterator	vri = viewRegistry.find( viewName );
	int						count = 0;
	bool					match;
	vector<string>::iterator itr;
	ExprTree				*tree;

	if( vri == viewRegistry.end( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "view " + viewName + " not found";
		goto cleanup;
	}

	view = vri->second;
	mad.ReplaceLeftAd( query );
	clientSock->encode( );

	// iterate over view members
    for( vmi=view->viewMembers.begin(); vmi!=view->viewMembers.end(); vmi++ ) {
		// test the match
        vmi->GetKey( key );
		ad = GetClassAd( key );
		mad.ReplaceRightAd( ad );
		match = false;
        mad.EvaluateAttrBool( "RightMatchesLeft", match );
		mad.RemoveRightAd( );

		if( match ) {
        	count++;
			// send ad if necessary
			if( wantResults ) {
				buffer = "[Key=\"" + key;
				buffer += "\";Ad=";
				// if no projection attrs specified, send whole ad
				if( !tProjAttrs ) {
					unparser.Unparse( buffer, ad );
				} else {
					// project ad's attributes
					buffer += "[";
					for( itr=attrs.begin( ); itr!=attrs.end( ); itr++ ) {
						if( ( tree = ad->Lookup( *itr ) ) ) {
							buffer += *itr;
							buffer += " = ";
							unparser.Unparse( buffer, tree );
							buffer += ";";
						}
					}
					buffer += "]";
				}
				buffer += "]";
				if( !clientSock->put((char*)buffer.c_str()) ) {
					CondorErrno = ERR_COMMUNICATION_ERROR;
					CondorErrMsg = "failed to send query results";
					return( false );
				}
			}
		}
    }

	// send <done>
	if( !clientSock->put("<done>") ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to send result delimiter <done>";
		return( false );
	}

cleanup:

	// if postlude is requested, send it
	if( wantPostlude ) {
		// reuse query ad memory
		query->Clear( );
		query->InsertAttr( ATTR_VIEW_NAME, viewName );
		query->InsertAttr( "NumResults", count );
		query->InsertAttr( "CondorErrno", CondorErrno );
		query->InsertAttr( "CondorErrMsg", CondorErrMsg );
		buffer = "";
		unparser.Unparse( buffer, query );
		if( !clientSock->put( (char*)buffer.c_str( ) ) ) {
			CondorErrno = ERR_COMMUNICATION_ERROR;
			CondorErrMsg = "failed to send query postlude";
			return( false );
		}
	}

	if( !clientSock->eom( ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "failed to send eom() to client";
		return( false );
	}

	return( true );
}


bool ClassAdCollectionServer::
HandleReadOnlyCommands( int command, ClassAd *rec, Sock *clientSock )
{
	string 				buffer;
	ClassAd	ack;

	ack.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_AckReadOp );
	switch( command ) {
		case ClassAdCollOp_GetClassAd: {
			string 	key;
			ClassAd	*ad;
			if( !rec->EvaluateAttrString( ATTR_KEY, key ) ) {
				CondorErrno = ERR_NO_KEY;
				CondorErrMsg = "bad or missing key";
			} else if( ( ad = GetClassAd( key ) ) ) {
				ack.Insert( ATTR_AD, ad );
				CondorErrno = ERR_OK;
				CondorErrMsg = "";
			}
			ack.InsertAttr( "CondorErrno", CondorErrno );
			ack.InsertAttr( "CondorErrMsg", CondorErrMsg );
			unparser.Unparse( buffer, &ack );
			// remove ad from ack so that it is not destroyed!
			ack.Remove( ATTR_AD );
			break;
		}

    	case ClassAdCollOp_GetViewInfo: {
			string 	viewName;
			ClassAd	*viewInfo;
			if( !rec->EvaluateAttrString( ATTR_VIEW_NAME, viewName ) ) {
				CondorErrno = ERR_NO_VIEW_NAME;
				CondorErrMsg = "bad or missing view name";
			} else if( GetViewInfo( viewName, viewInfo ) ) {
				ack.Insert( ATTR_VIEW_INFO, viewInfo );
				CondorErrno = ERR_OK;
				CondorErrMsg = "";
			}
			ack.InsertAttr( "CondorErrno", CondorErrno );
			ack.InsertAttr( "CondorErrMsg", CondorErrMsg );
			unparser.Unparse( buffer, &ack );
			break;
		}

    	case ClassAdCollOp_GetPartitionedViewNames:
    	case ClassAdCollOp_GetSubordinateViewNames: {
			string 						viewName;
			vector<string>				views;
			vector<string>::iterator	vi;
			bool						rval;

			if( !rec->EvaluateAttrString( ATTR_VIEW_NAME, viewName ) ) {
				ack.InsertAttr( "CondorErrno", ERR_NO_VIEW_NAME );
				ack.InsertAttr( "CondorErrMsg","bad or missing view name" );
				unparser.Unparse( buffer, &ack );
				break;
			} 
				
			rval = (command==ClassAdCollOp_GetPartitionedViewNames) ?
						GetPartitionedViewNames( viewName, views ):
						GetSubordinateViewNames( viewName, views );
			// if unsuccessful
			if( !rval ) {
				ack.InsertAttr( "CondorErrno", CondorErrno );
				ack.InsertAttr( "CondorErrMsg", CondorErrMsg );
				unparser.Unparse( buffer, &ack );
				break;
			}

			// else, pack view names into ack
			vector<ExprTree*>	names;
			Value				val;
			for( vi=views.begin( ); vi!=views.end( ); vi++ ) {
				val.SetStringValue( *vi );
				names.push_back( Literal::MakeLiteral( val ) );
			}
			ack.Insert( command==ClassAdCollOp_GetPartitionedViewNames?
				ATTR_PARTITIONED_VIEWS : ATTR_SUBORDINATE_VIEWS,
				ExprList::MakeExprList( names ) );
			ack.InsertAttr( "CondorErrno", ERR_OK );
			ack.InsertAttr( "CondorErrMsg", "" );
			unparser.Unparse( buffer, &ack );
			break;
		}

    	case ClassAdCollOp_FindPartitionName: {
			string 	viewName, partition;
			ClassAd	*rep;
			Value	val;
			bool	rval;

			if( !rec->EvaluateAttrString( ATTR_VIEW_NAME, viewName ) ) {
				ack.InsertAttr( "CondorErrno", ERR_NO_VIEW_NAME );
				ack.InsertAttr( "CondorErrMsg", "bad or missing view name" );
				unparser.Unparse( buffer, &ack );
				break;
			}
			if( !rec->EvaluateAttr(ATTR_AD, val) || !val.IsClassAdValue(rep) ) {
				ack.InsertAttr( "CondorErrno", ERR_BAD_CLASSAD );
				ack.InsertAttr("CondorErrMsg","bad or missing representative");
				unparser.Unparse( buffer, &ack );
				break;
			}
			rval = FindPartitionName( viewName, rep, partition );
			ack.InsertAttr( "Result", rval );
			ack.InsertAttr( "PartitionName", partition );
			ack.InsertAttr( "CondorErrno", CondorErrno );
			ack.InsertAttr( "CondorErrMsg", CondorErrMsg );
			unparser.Unparse( buffer, &ack );
		}

    	case ClassAdCollOp_IsActiveTransaction:
    	case ClassAdCollOp_IsCommittedTransaction: {
			string	xactionName;
			bool	rval;

			if( !rec->EvaluateAttrString( "XactionName", xactionName ) ) {
				ack.InsertAttr( "CondorErrno", ERR_NO_TRANSACTION_NAME );
				ack.InsertAttr("CondorErrMsg","bad or missing transaction "
					"name");
				unparser.Unparse( buffer, &ack );
				break;
			}
			rval = ( command==ClassAdCollOp_IsActiveTransaction ) ?
				IsActiveTransaction( xactionName ) : 
				IsCommittedTransaction( xactionName );
			ack.InsertAttr( "Result", rval );
			ack.InsertAttr( "CondorErrno", CondorErrno );
			ack.InsertAttr( "CondorErrMsg", CondorErrMsg );
			unparser.Unparse( buffer, &ack );
			break;
		}

		case ClassAdCollOp_GetServerTransactionState: {
			string	xactionName;
			int		rval;

			if( !rec->EvaluateAttrString( "XactionName", xactionName ) ) {
				ack.InsertAttr( "CondorErrno", ERR_NO_TRANSACTION_NAME );
				ack.InsertAttr("CondorErrMsg","bad or missing transaction "
					"name");
				unparser.Unparse( buffer, &ack );
				break;
			}
			if( IsActiveTransaction( xactionName ) ) {
				rval = ServerTransaction::ACTIVE;
			} else if( IsCommittedTransaction( xactionName ) ) {
				rval = ServerTransaction::COMMITTED;
			} else {
				rval = ServerTransaction::ABSENT;
			}
			ack.InsertAttr( "Result", rval );
			ack.InsertAttr( "CondorErrno", CondorErrno );
			ack.InsertAttr( "CondorErrMsg", CondorErrMsg );
			unparser.Unparse( buffer, &ack );
			break;
		}


    	case ClassAdCollOp_GetAllActiveTransactions:
    	case ClassAdCollOp_GetAllCommittedTransactions: {
			vector<string>				xactionNames;
			vector<string>::iterator	xi;

			if( command==ClassAdCollOp_GetAllActiveTransactions ) {
				GetAllActiveTransactions( xactionNames );
			} else {
				GetAllCommittedTransactions( xactionNames );
			}
				// pack names into ack
			vector<ExprTree*>	names;
			Value				val;
			for( xi=xactionNames.begin( ); xi!=xactionNames.end( ); xi++ ) {
				val.SetStringValue( *xi );
				names.push_back( Literal::MakeLiteral( val ) );
			}
			ack.Insert( command==ClassAdCollOp_GetAllActiveTransactions?
				"ActiveTransactions" : "CommittedTransactions",
				ExprList::MakeExprList( names ) );
			ack.InsertAttr( "CondorErrno", ERR_OK );
			ack.InsertAttr( "CondorErrMsg", "" );
			unparser.Unparse( buffer, &ack );
			break;
		}

		default:
			CLASSAD_EXCEPT( "invalid command: %d should not get here", command );
			return( false );
	}

	// send reply to client
	delete rec;
	clientSock->encode( );
	if( !clientSock->put((int)ClassAdCollOp_AckReadOp) || 
			!clientSock->put((char*)buffer.c_str( )) || !clientSock->eom( ) ) {
		CondorErrno = ERR_COMMUNICATION_ERROR;
		CondorErrMsg = "unable to ack read command";
		return( false );
	}

	return( true );
}



END_NAMESPACE
