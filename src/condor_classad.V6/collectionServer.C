/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_io.h"
#include "transaction.h"
#include "collectionServer.h"

BEGIN_NAMESPACE( classad )

ClassAdCollectionServer::
ClassAdCollectionServer( ) : viewTree( NULL )
{
		// create "root" view
	viewTree.SetViewName( "root" );
	RegisterView( "root", &viewTree );

	log_fp = NULL;
}


ClassAdCollectionServer::
~ClassAdCollectionServer( )
{
	if( log_fp ) fclose( log_fp );

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

bool ClassAdCollectionServer::
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
				if( !xaction || !xaction->Log( log_fp, unparser ) ) {
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
		if( tProjAttrs && tProjAttrs->GetKind( ) == EXPR_LIST_NODE ) {
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
			EXCEPT( "invalid command: %d should not get here", command );
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


//------------------------------------------------------------------------------
// Main execution engine --- operations in recovery mode, network mode and
// the local interface mode usually funnel into these routines
//------------------------------------------------------------------------------
bool ClassAdCollectionServer::
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



bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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

bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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
			ClassAd *rec = _UpdateClassAd( "", key, modAd );
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


bool ClassAdCollectionServer::
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


ClassAd *ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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
	if( rval && !xtn->Log( log_fp, unparser ) ) {
		CondorErrMsg += "; could not log transaction";
		rval = false;
	}

		// kill transaction
	delete xtn;
	xactionTable.erase( itr );
	return( rval );
}


bool ClassAdCollectionServer::
IsMyActiveTransaction( const string &transactionName )
{
	XactionTable::iterator	itr = xactionTable.find( transactionName );
	return( itr != xactionTable.end( ) && itr->second && 
		itr->second->GetLocalXaction( ) );
}


void ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
IsActiveTransaction( const string &transactionName )
{
	XactionTable::iterator itr = xactionTable.find( transactionName );
	return( itr != xactionTable.end( ) && itr->second );
}


bool ClassAdCollectionServer::
IsCommittedTransaction( const string &transactionName )
{
	XactionTable::iterator	itr = xactionTable.find( transactionName );
	return( itr != xactionTable.end( ) && !itr->second );
}


bool ClassAdCollectionServer::
GetAllActiveTransactions( vector<string>& xactions )
{
	XactionTable::iterator	itr;

	xactions.clear( );
	for( itr = xactionTable.begin( ); itr != xactionTable.end( ); itr++ ) {
		if( itr->second ) xactions.push_back( itr->first );
	}
	return( true );
}


bool ClassAdCollectionServer::
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





bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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


bool ClassAdCollectionServer::
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
bool ClassAdCollectionServer::
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
		if( !logRec.InsertAttr( "Key", itr->first.c_str( ) )||
				!( ad = GetClassAd( itr->first ) )			||
				!logRec.InsertAttr( "Ad", ad ) 				||
				!WriteLogEntry( fp, &logRec, false ) ) {
			CondorErrMsg += "; failed to log ad, could not log state";
			logRec.Remove( "Ad" );
			return( false );
		}

		logRec.Remove( "Ad" );
	}

	if( fsync( fileno( log_fp ) ) < 0 ) {
		CondorErrno = ERR_FILE_WRITE_FAILED;
		CondorErrMsg = "fsync() failed when logging state";
		return( false );
	}

	return( true );
}
	

bool ClassAdCollectionServer::
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
