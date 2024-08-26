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
#include <iostream>
#include "classad/sink.h"

#ifdef WIN32
#include <sys/timeb.h>
#endif

using std::string;
using std::vector;
using std::pair;
using std::map;
using std::cout;
using std::endl;


namespace classad {

#include <assert.h>

ClassAdCollection::
ClassAdCollection( ) : viewTree( NULL )
{
	Setup(false);
}

ClassAdCollection::
ClassAdCollection(bool cacheOn) : viewTree( NULL ) 
{
	Setup(cacheOn);
	return;
}
void ClassAdCollection::
Setup(bool cacheOn)
{
	Cache = cacheOn;
	test_checkpoint=0;
	// create "root" view
	viewTree.SetViewName( "root" );
	RegisterView( "root", &viewTree );
	log_fp = NULL;
	return;
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
InitializeFromLog( const string &logfile, const string storagefile, const string checkpointfile )
{
        int     storagefd;
        CheckFileName=checkpointfile; 
	// open the file
	if (Cache==true){
          //Read all the ClassAd from storage file and build up the index
	  if( ( storagefd = open( storagefile.c_str( ), O_RDWR | O_CREAT, 0600 ) ) < 0 ) {	    
	    CondorErrno = ERR_CACHE_FILE_ERROR;
	    CondorErrMsg = "failed to open storage file " + storagefile + " errno=" + std::to_string(errno);
	    return( false );
	  };
	  
	  ClassAdStorage.Init(storagefd);
	  int offset;
	  string key;
	  
	  //scan the storage file to create the index on the classad, 
	  while (ReadStorageEntry(storagefd,offset,key)>1){
	    ClassAdStorage.UpdateIndex(key,offset);
	  };
	  Max_Classad=0; 
	}
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
	vector<ExprTree*>	vec;
	ExprTree * pExpr=0;
	if( !ad->InsertAttr( ATTR_REQUIREMENTS, true ) 						||
			!ad->Insert( ATTR_RANK, (pExpr=Literal::MakeUndefined()) )		||
			!ad->Insert( ATTR_PARTITION_EXPRS, (pExpr=ExprList::MakeExprList( vec )) )||
			!viewTree.SetViewInfo( this, ad ) ) {
		CondorErrMsg += "; failed to initialize from log";
		return( false );
	}
	
	//Check the whether there is a checkpoint, if so we recover from the latest checkpoint
	if (Cache==true){
          ReadCheckPointFile();	 
	}		
		// load info from new log file
	logFileName = logfile;
	if( !logfile.empty( ) ) {
		if( !ReadLogFile( ) ) {
			CondorErrMsg += "; could not initialize from file " + logfile;
			return( false );
		}
	}
	return( true );
}

// One might ask why this function is virtual, yet we define it to
// just call the base class. The reason is that I want to be able to
// document how to use TruncateLog in the Doxygen comments in the
// header file, so that users have a single source for the
// documentation.
bool ClassAdCollection::
TruncateLog(void)
{
    return ClassAdCollectionInterface::TruncateLog();
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // the seek, read, open, close, fileno, etc are deprecated, use _seek, etc instead.
#endif

bool ClassAdCollection::
ReadCheckPointFile(){
          CheckPoint=0;     	
	  int fd_check;
	  if( ( fd_check = open( CheckFileName.c_str(), O_RDWR | O_CREAT, 0600 ) ) < 0 ) {
            CondorErrno = ERR_CACHE_FILE_ERROR;
	    CondorErrMsg = "internal error:  unable to open checkpoint file";
		return false;
	  }
	  string buffer;
	  int fempty=lseek(fd_check,0,SEEK_END);
	  if (fempty!=0){
	    lseek(fd_check,0,SEEK_SET);
	    char  k[1];
	    string OneLine="";
	    int l;
	    
	    while ((l=read(fd_check,k,1))>0){
	      string n(k,1);
	      if (n=="\n"){
		break; 
	      }else{
		OneLine=OneLine+n;
	      }           
	    }	
	    
	    if (OneLine!=""){
	      string buf;
	      ClassAdParser local_parser;
	      ClassAd* cla=local_parser.ParseClassAd(OneLine,true);;
	      string name="Time";
	      cla->EvaluateAttrString(name,buf);
	      
	      size_t i_p=buf.find(".");
	      string sec=buf.substr(0,i_p);
	      string usec=buf.substr(i_p+1,buf.size()-i_p);
	      
	      LatestCheckpoint.tv_sec=atol(sec.c_str());
	      LatestCheckpoint.tv_usec=atol(usec.c_str());
	      delete(cla);  
	    }else{
	      LatestCheckpoint.tv_sec=0;
	      LatestCheckpoint.tv_usec=0;
	    }
	  }else{
	    CheckPoint=1;
	  }
	close(fd_check);
	return true;
}

//------------------------------------------------------------------------------
// Recovery mode interface
//------------------------------------------------------------------------------

bool ClassAdCollection::
OperateInRecoveryMode( ClassAd *logRec )
{
	int 	opType=0;

	logRec->EvaluateAttrInt( ATTR_OP_TYPE, opType );
	if (Cache==true){
          //If the current entry is checkpoint, we check it with the latest checkpoint.
          //we only recover after the latest checkpoint
	  if (opType == ClassAdCollOp_CheckPoint) {
	    string buf;
	    logRec->EvaluateAttrString("Time",buf);
	    
	    size_t i_p=buf.find(".");
	    string sec=buf.substr(0,i_p);
	    string usec=buf.substr(i_p+1,buf.size()-i_p);
	    long temp_sec=atol(sec.c_str());
	    long temp_usec=atol(usec.c_str());
	    if ((temp_sec<LatestCheckpoint.tv_sec)||((temp_sec==LatestCheckpoint.tv_sec)&&(temp_usec<LatestCheckpoint.tv_usec))){
	    }else{
	      CheckPoint=1;             
	   //in order to recover views, we should re-insert all the classads in the storage into views
            string key;
	    if ((ClassAdStorage.First(key))<0){
	      
	    }else{
	      do {
		tag ptr;
		ClassAdStorage.FindInFile(key,ptr);
		string oneentry= ClassAdStorage.GetClassadFromFile(key,ptr.offset);
		if (oneentry == ""){
                  delete(logRec);
		  CondorErrno = ERR_CACHE_CLASSAD_ERROR;
		  CondorErrMsg = "No classad " + key + " can be found from storage file";
		  return( false );	     
		}
	    ClassAdParser local_parser;
		ClassAd *cla=local_parser.ParseClassAd(oneentry,true);
		SAL_assume(cla != NULL)
		ClassAd *content = (ClassAd*)(cla->Lookup("Ad"));
		if( !viewTree.ClassAdInserted( this, key, content ) ) {
		  CondorErrMsg += "; could not insert classad";
		  return( false );
		}
                delete cla;
	      }while((ClassAdStorage.Next(key))!=0);
	    } 	                
	    }
	    delete(logRec);
	    return (true);          
	  };
	}
	switch( opType ) {
		case ClassAdCollOp_OpenTransaction:
		case ClassAdCollOp_AbortTransaction:
		case ClassAdCollOp_CommitTransaction:
		case ClassAdCollOp_ForgetTransaction: {
		  if ((Cache==false)||(CheckPoint==1)){
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
		  }else{
		    delete logRec;
                    return (true);
		  };
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
		  if ((Cache==false)||(CheckPoint==1)){
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
		 }else{
		    delete logRec;
                    return (true);
		  };
		}
	}
	CLASSAD_EXCEPT( "illegal operation in log:  should not reach here" );
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
			CLASSAD_EXCEPT( "not a transaction op:  should not reach here" );
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
			ClassAd	*local_viewInfo;

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
			if( !( local_viewInfo = (ClassAd*) logRec->Copy( ) ) ) {
				return( false );
			}
			local_viewInfo->Delete( "OpType" );
			return( parentView->InsertSubordinateView( this, local_viewInfo ) );
        }

        case ClassAdCollOp_CreatePartition: {
            ViewRegistry::iterator itr;
            string  parentViewName;
            View    *parentView;
			Value	val;
			ClassAd	*rep = NULL;

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
			if( !( viewInfo = (ClassAd *) logRec->Copy( ) ) ) {
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
			//delete logRec;
			return( true );
        }

        case ClassAdCollOp_SetViewInfo: {
            ViewRegistry::iterator itr;
            string	viewName;
            ClassAd *tmp = NULL;
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
			if( !( viewInfo = (ClassAd *) tmp->Copy( ) ) ) {
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
			CLASSAD_EXCEPT( "not a view op:  should not reach here" );
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

            newAd = NULL;
			if( !rec->EvaluateAttrString( "Key", key ) ) {
				CondorErrno = ERR_NO_KEY;
				CondorErrMsg = "bad or missing 'key' attribute";
				return( false );
			}
			if( !rec->EvaluateAttr( "Ad", cv ) || !cv.IsClassAdValue( ad ) ||
				!( newAd = (ClassAd *) ad->Copy( ) ) ) {
				CondorErrno = ERR_BAD_CLASSAD;
				CondorErrMsg = "bad or missing 'ad' attribute";
				return( false );
			}
			newAd->SetParentScope( NULL );

				// check if an ad is already present
			if (( itr = classadTable.find( key ) ) != classadTable.end( )) {
					// delete old ad
				ad = itr->second.ad;
				viewTree.ClassAdDeleted( this, key, ad );
				classadTable.erase( itr );
				delete ad;
				if (Cache==true){
				  Max_Classad--;
				}
			} else {
				//if it is not in cache but in file, delete it from file
				if (Cache==true) {      
					tag ptr;
					if (ClassAdStorage.FindInFile( key,ptr )){
						ClassAdStorage.DeleteFromStorageFile(key);
					};
				}
			}  
			if (Cache == true){
			  //If the cache in memory is full, we have to swap out a ClassAd
			  if ( Max_Classad ==  MaxCacheSize){
			    string write_back_key;
			    if (!SelectClassadToReplace(write_back_key)){
			      CondorErrno = ERR_CANNOT_REPLACE;
			      CondorErrMsg = "failed in replacing classad in cache";
			    };
			    if (CheckDirty(write_back_key)){
			      string WriteBackClassad;
			      
			      if (!GetStringClassAd(write_back_key,WriteBackClassad)){
				CondorErrMsg = "failed in get classad from cache";
			      };
			      
			      ClassAdStorage.WriteBack(write_back_key,WriteBackClassad);
			      ClearDirty(write_back_key);
			    };          
			    itr=classadTable.find(write_back_key);
			    ad=itr->second.ad;
			    delete ad;                                          
			    classadTable.erase(write_back_key);
			    Max_Classad--;
			  }
			}
				// insert new ad
			if( !viewTree.ClassAdInserted( this, key, newAd ) ) {
			  CondorErrMsg += "; could not insert classad";
			  return( false );
			}
			proxy.ad = newAd;
			classadTable[key] = proxy;
			if ( Cache==true){
			  SetDirty(key);
			  Max_Classad++;	
			}
			return( true );
		}

		case ClassAdCollOp_UpdateClassAd: {
			ClassAdTable::iterator itr;
			string		key;
			ClassAd		*ad, *update = NULL;
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
			if ( Cache==true){
			  if (itr==classadTable.end()){
			    tag ptr;
			    if (ClassAdStorage.FindInFile( key,ptr )){
			      if (!SwitchInClassAd(key)){
				CondorErrMsg = "can not switch in classad";
				return( false );
			      }
			    }else{
			      CondorErrno = ERR_NO_SUCH_CLASSAD;
			      CondorErrMsg = "no classad " + key + " to update";
			      return( false );
			    };
			  }			
			  itr = classadTable.find( key );
			}else{
			  if( itr == classadTable.end( ) ) {
			    CondorErrno = ERR_NO_SUCH_CLASSAD;
			    CondorErrMsg = "no classad " + key + " to update";
			    return( false );
			  }
			}
			ad = itr->second.ad;
			viewTree.ClassAdPreModify( this, ad );

			ad->Update( *update );
			if( !viewTree.ClassAdModified( this, key, ad ) ) {
				CondorErrMsg += "; failed when updating classad";
				return( false );
			}
			return( true );
		}
			
		case ClassAdCollOp_ModifyClassAd: {
			ClassAdTable::iterator itr;
			string		key;
			ClassAd		*ad, *update = NULL;
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
			if ( Cache==true){
			  if (itr==classadTable.end()){
			    tag ptr;
			    if (ClassAdStorage.FindInFile( key,ptr )){
			      if (!SwitchInClassAd(key)){
				CondorErrMsg = "can not switch in classad";
				return( false );
			      }
			    }else{
			      CondorErrno = ERR_NO_SUCH_CLASSAD;
			      CondorErrMsg = "no classad " + key + " to modify";
			      return( false );
			    };
			  }
			}else{
			  itr = classadTable.find( key );
			  if( itr == classadTable.end( ) ) {
			    CondorErrno = ERR_NO_SUCH_CLASSAD;
			    CondorErrMsg = "no classad " + key + " to modify";
			    return( false );
			  }
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
			if ( Cache==true){
			  tag ptr;
			  int deleted=0;
			  if (ClassAdStorage.FindInFile(key,ptr)){
			    ClassAdStorage.DeleteFromStorageFile(key);
			    deleted=1;
			  }
			  
			  if( itr == classadTable.end( )){
			    if(deleted==0){ 
			      CondorErrno = ERR_NO_SUCH_CLASSAD;
			      CondorErrMsg = "no classad " + key + " to remove";
			      return( false );
			    }else{
			      return (true);
			    }
			  }else{
			    Max_Classad--;
			    ad = itr->second.ad;
			    classadTable.erase( itr );
			    viewTree.ClassAdDeleted( this, key, ad );
			    delete ad;
			    return( true );
			  }
			}else{
			  if( itr == classadTable.end( ) ) {
			    CondorErrno = ERR_NO_SUCH_CLASSAD;
			    CondorErrMsg = "no classad " + key + " to remove";
			    return( false );
			  }
			  ad = itr->second.ad;
			  classadTable.erase( itr );
			  viewTree.ClassAdDeleted( this, key, ad );
			  delete ad;
			  return( true );
			}
		}	
	default:
	  break;
	}
	
	CLASSAD_EXCEPT( "internal error:  Should not reach here" );
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

const View * ClassAdCollection::
GetView( const ViewName &viewName )
{
	ViewRegistry::iterator i;
    const View *view;

	i = viewRegistry.find( viewName );
	if( i == viewRegistry.end( ) ) {
		CondorErrno = ERR_NO_SUCH_VIEW;
		CondorErrMsg = "view " + viewName + " not found";
		view = NULL;
	} else {
        view = i->second;
    }
	return view;
}

bool ClassAdCollection::
ViewExists( const ViewName &viewName)
{
    bool                    view_exists;
	ViewRegistry::iterator  i;

	i = viewRegistry.find(viewName);
	if (i == viewRegistry.end()){
        view_exists = false;
    } else {
        view_exists = true;
    }
    return view_exists;
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
AddClassAd(const string &key, ClassAd *newAd)
{
    bool success;

    if (currentXactionName != "") {
        success = AddClassAd_Transaction(key, newAd);
    } else {
        success = AddClassAd_NoTransaction(key, newAd);
    }

    return success;
}

bool ClassAdCollection::
AddClassAd_Transaction(const string &key, ClassAd *newAd)
{
    bool success;

    // First, get the transaction. 
    ServerTransaction       *xaction;
    XactionTable::iterator  itr = xactionTable.find(currentXactionName);
    if (itr == xactionTable.end()) {
        CondorErrno = ERR_NO_SUCH_TRANSACTION;
        CondorErrMsg = "transaction " + currentXactionName + " doesn't exist";
        success = false;
    } else {
        // Next, create our change that will be added to the transaction.
        ClassAd *rec = _AddClassAd( currentXactionName, key, newAd );
        if (!rec){
            success = false;
        } else {
            // Finally, stuff this change into the transaction
            xaction = itr->second;
            xaction->AppendRecord(ClassAdCollOp_AddClassAd, key, rec);
            success = true;
        }
    }
    return success;
}

bool ClassAdCollection::
AddClassAd_NoTransaction(const string &key, ClassAd *newAd)
{
    ClassAdTable::iterator  itr;
    ClassAd                 *old_ad; // A pre-existing ad with the given key.
    ClassAdProxy            proxy;
    bool                    is_duplicate_ad;
    bool                    success;
    bool                    reinsert_old_ad;
    
    success         = true;
    old_ad          = NULL;
    is_duplicate_ad = false;
    reinsert_old_ad = false;

    // First, see if we have an old ad that we need to replace
    // and delete it from the table--as long as it's not an exact duplicate
    itr = classadTable.find(key);
    if (itr != classadTable.end()) {
        old_ad = itr->second.ad;
        if (old_ad == newAd) {
            is_duplicate_ad = true;
            old_ad          = NULL; // Avoid deleting it later.
        } else {
            viewTree.ClassAdDeleted( this, key, old_ad );
            classadTable.erase( itr );
            if (Cache==true) {
                Max_Classad--;
            }
        }
    } else if (Cache == true) {
        ClassAdStorage.DeleteFromStorageFile(key);
    }

    // Make sure all the views that need to incorporate this ad do so
    if (!is_duplicate_ad && !viewTree.ClassAdInserted(this, key, newAd)) {
        success = false;
        reinsert_old_ad = true;
    } else {
        // If we are using a cache, update it.
        if (!is_duplicate_ad && Cache==true) {
            MaybeSwapOutClassAd();
            SetDirty(key);
            Max_Classad++;
        }

        // Now insert the new ad into the in-memory table.
        proxy.ad = newAd;
        classadTable[key] = proxy;
        
        // Log what we did, if we have a log
        if (log_fp) {
            ClassAd *rec = _AddClassAd("", key, newAd);
            if (!WriteLogEntry(log_fp, rec)) {
                CondorErrMsg += "; failed to log add classad";
                
                // If we failed to log it, we must delete it from the collection
                itr = classadTable.find(key);
                if (itr != classadTable.end()) {
                    classadTable.erase(itr);
                    viewTree.ClassAdDeleted(this, key, newAd);
                }
                
                reinsert_old_ad = true;
                success = false;
            }
            rec->Remove(ATTR_AD);
            delete rec;
        }
    }

    // If something has failed, and we were replacing an ad,
    // we need to put it back into the collection.
    if (reinsert_old_ad && old_ad != NULL) {
        // Update the cache as necessary
        if (Cache) {
            MaybeSwapOutClassAd();
            SetDirty(key);
            Max_Classad++;
        }
        
        // Now insert the old ad into the in-memory table.
        proxy.ad = old_ad;
        classadTable[key] = proxy;
        old_ad = NULL; // mark it as NULL so we don't delete it.
    }
    
    if (old_ad != NULL) {
        delete old_ad;
    }

    return success;
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
		if ( Cache==true){
			if (itr==classadTable.end()) {
				tag ptr;
				if (ClassAdStorage.FindInFile( key,ptr )) {
					if (!SwitchInClassAd(key)) {
						CondorErrMsg = "can not switch in classad";
						return( false );
					}
				} else {
					CondorErrno = ERR_NO_SUCH_CLASSAD;
					CondorErrMsg = "no classad " + key + " to update";
					return( false );
				};
			}			
			itr = classadTable.find( key );
		} else if( itr == classadTable.end( ) ) {
		    CondorErrno = ERR_NO_SUCH_CLASSAD;
		    CondorErrMsg = "no classad " + key + " to update";
		    return( false );
		}

		ad = itr->second.ad;
		viewTree.ClassAdPreModify( this, ad );
		ad->Update( *updAd );
		if( !viewTree.ClassAdModified( this, key, ad ) ) {
			delete updAd;
			return( false );
		}

		if (Cache==true){
		  SetDirty(key); 
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
		if ( Cache==true){
 		  if (itr == classadTable.end()){
			  tag ptr;
			  if (ClassAdStorage.FindInFile( key,ptr )){
				  if (!SwitchInClassAd(key)){
					  CondorErrMsg = "can not switch in classad";
					  return( false );
				  }
			  } else {
				  CondorErrno = ERR_NO_SUCH_CLASSAD;
				  CondorErrMsg = "no classad " + key + " to update";
				  delete modAd;
				  return( false );
			  }
			  itr = classadTable.find( key );		  
		  }
		} else if ( itr == classadTable.end( ) ) {
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
		if (Cache==true){
		  SetDirty(key);
		}
		// log if possible
		if( log_fp ) {
		  ClassAd *rec = _ModifyClassAd( "", key, modAd );
		  if( !WriteLogEntry( log_fp, rec , true) ) {
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
		if (Cache==true){
		  tag ptr;
		  int deleted=0;
		  if (ClassAdStorage.FindInFile(key,ptr)){
		    ClassAdStorage.DeleteFromStorageFile(key);
		    deleted=1;
		  }
		  
		  if( itr == classadTable.end( ) ){
		    if (deleted==0){
		    }else{
		      return( true );
		    }
		    
		  }else{
		    Max_Classad--;
		    ad = itr->second.ad;
		    viewTree.ClassAdDeleted( this, key, ad );
		    delete ad;
		    classadTable.erase( itr );
		  }
		}else{
		  if( itr == classadTable.end( ) ) return( true );
		  ad = itr->second.ad;
		  viewTree.ClassAdDeleted( this, key, ad );
		  delete ad;
		  classadTable.erase( itr );
		}
			// log if possible
		if( log_fp ) {
			ClassAd *rec = _RemoveClassAd( "", key );
			if( !WriteLogEntry( log_fp, rec,true ) ) {
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
	
	if (Cache==true){
	  tag ptr;
	  if (itr==classadTable.end()){
	    if (ClassAdStorage.FindInFile( key,ptr )){
	      if (!SwitchInClassAd(key)){
		CondorErrMsg = "can not switch in classad";
		return( NULL );
	      }	      
	    }else{
	      CondorErrno = ERR_NO_SUCH_CLASSAD;
	      CondorErrMsg = "no classad " + key + " to update";
	      return( NULL );
	    };
	  }
	  itr=classadTable.find( key );
	}else{
	  if( itr == classadTable.end( ) ) {
	    CondorErrno = ERR_NO_SUCH_CLASSAD;
	    CondorErrMsg = "classad " + key + " not found";
	    return( NULL );
	  } 
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
	if (!LogViews( fp, &viewTree, true)) {
		CondorErrMsg += "; failed to log state";
		return( false );
	}

	if (Cache == true) {
		ClassAd		logRec;
		int         sf_offset;
		string      cla_key;
		string      cla_s;

		// First write dirty ClassAds into storagefile, make it a
		// consistant state
		WriteCheckPoint();

		//dump all the ClassAd into new log file
		sf_offset=ClassAdStorage.First(cla_key);
		while (sf_offset !=- 1) {
			cla_s=ClassAdStorage.GetClassadFromFile(cla_key,sf_offset);
			if (cla_s == "") {
				CondorErrno = ERR_CACHE_CLASSAD_ERROR;
				CondorErrMsg = "No classad " + cla_key 
					+ " can be found from storage file";
				return( false );	     
			}
			ClassAdParser local_parser;
			ClassAd *cla=local_parser.ParseClassAd(cla_s,true);
			if ( ! cla) { CLASSAD_EXCEPT("parser failed to allocate an ad"); }
			if (!cla->InsertAttr("OpType", ClassAdCollOp_AddClassAd )) {
				CondorErrMsg += "; failed to log state";
				return( false );
			}
			if (!WriteLogEntry(fp,cla,true)) {
				CondorErrMsg += "; failed to log ad, could not log state";   
			}   
			sf_offset=ClassAdStorage.Next(cla_key);
			delete(cla);                 
		}
	} else {
		// log classads
		ClassAdTable::iterator	itr;
		ClassAd	                logRec;
		ClassAd                 *ad;
		
		if ( !logRec.InsertAttr( "OpType", ClassAdCollOp_AddClassAd ) ) {
			CondorErrMsg += "; failed to log state";
			return( false );
		}
	  
	  for ( itr = classadTable.begin( ); itr != classadTable.end( ); itr++ ) {
		  ClassAd *tmp;
		  string dd(itr->first);
		  tmp = GetClassAd(dd);
		  string buff;
		  ClassAdUnParser local_unparser;
		  local_unparser.Unparse(buff,tmp);
		  
		  logRec.InsertAttr( "Key", itr->first );
		  ad = GetClassAd( itr->first );
		  logRec.Insert( "Ad", ad );
		  
		  buff="";
		  local_unparser.Unparse(buff, &logRec);
		  
		  if(!WriteLogEntry(fp, &logRec, true)) {
			  CondorErrMsg += "; failed to log ad, could not log state";
			  logRec.Remove( "Ad" );
			  return( false );
		  }
		  buff = "";            
		  local_unparser.Unparse(buff,&logRec);
		  logRec.Remove( "Ad" );
	  }
	}
	if( fsync( fileno( fp ) ) < 0 ) {
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
	ViewName  view_name;

	view_name = view->GetViewName();
	if (view_name != "root") {
		ClassAd	logRec;
		ClassAd	*ad = view->GetViewInfo( );

		// insert operation type and view info
		if (!ad || !ad->InsertAttr( "OpType", subView ?
				ClassAdCollOp_CreateSubView:ClassAdCollOp_CreatePartition)) {
			if (ad) delete ad;
			CondorErrMsg += "; failed to log views";
			return( false );
		}

		// dump view info into log record 
		logRec.Update(*ad);
		delete ad;
		if (!WriteLogEntry(fp, &logRec, true)) {
			CondorErrMsg += "; failed to log views";
			return( false );
		}
	}

	// next log subordinate child views ...
	SubordinateViews::iterator	xi;
	for (xi=view->subordinateViews.begin( ); xi!=view->subordinateViews.end( ); 
			xi++) {
		if (!LogViews(fp, *xi, true)) {
			return (false);
		}
	}

	// ... and partition child views
	PartitionedViews::iterator	mi;
	for(mi=view->partitionedViews.begin( ); mi!=view->partitionedViews.end( ); 
			mi++) {
		if (!LogViews(fp, mi->second, false)) {
			return( false );
		}
	}

	return( true );
}

bool ClassAdCollection::
SwitchInClassAd(string key){
  ClassAdTable::iterator itr;
  tag ptr;
  if (Max_Classad==MaxCacheSize){
    //pick up a classad, write it back to storage file
           string write_back_key;
	   if (!SelectClassadToReplace(write_back_key)){
	          CondorErrno = ERR_CANNOT_REPLACE;
		  CondorErrMsg = "failed in replacing classad in cache";
	   };
	   
	   if (CheckDirty(write_back_key)){
	          string WriteBackClassad;
	     
		  if (!GetStringClassAd(write_back_key,WriteBackClassad )){
		    CondorErrMsg = "failed in get classad from cache";
		  };
		  
		  ClassAdStorage.WriteBack(write_back_key,WriteBackClassad );
		  ClearDirty(write_back_key);
	   };     
	   ClassAd *ad;     
	   itr=classadTable.find(write_back_key);
	   ad=itr->second.ad;
	   delete ad;                                          
	   classadTable.erase(write_back_key);
	   Max_Classad--;
  }
  //bring the classad which will be updated into cache
  if (!ClassAdStorage.FindInFile( key,ptr )){
           CondorErrno = ERR_CACHE_SWITCH_ERROR;
	   CondorErrMsg = "internal error:  unable to find the classad in storage file";
           return (false);
  };
  string theclassad=ClassAdStorage.GetClassadFromFile(key,ptr.offset);
  if (theclassad!=""){
           ClassAdParser local_parser;
	   ClassAd *storage_cla=local_parser.ParseClassAd(theclassad,true);
	   if (storage_cla == NULL){
	          CondorErrno = ERR_PARSE_ERROR;
	          CondorErrMsg = "internal error:  unable to parse the classad";
		  return false;
	   }; 
	   ClassAd *back_cla=(ClassAd*)(storage_cla->Lookup("Ad"));
	   if ( back_cla == NULL){
	          CondorErrno = ERR_PARSE_ERROR;
	          CondorErrMsg = "internal error:  unable to parse the classad";
		  return false;
	   };
	   string back_key;
	   
	   storage_cla->EvaluateAttrString( "Key",back_key);
	   if (back_key==key){
	          ClassAdProxy proxy;
		  proxy.ad=back_cla;
		  classadTable[key]=proxy;
		  Max_Classad++;;
	   }else{
             CondorErrno = ERR_CACHE_SWITCH_ERROR;
	     CondorErrMsg = "No classad " + key + " to update";
	     return( false );
	   }
  }else{
           CondorErrno = ERR_CACHE_SWITCH_ERROR;
           CondorErrMsg = "No classad " + key + " to update";
	   return( false );
  }
  return (true);
}

int ClassAdCollection::
WriteCheckPoint(){
  //get the latest time mark
	  timeval ctime;

#ifndef WIN32
	  gettimeofday(&ctime,NULL);
#else
	  struct timeb tb; 
	  ftime(&tb);
	  ctime.tv_sec = tb.time;
	  ctime.tv_usec = tb.millitm*1000; //need microseconds
#endif

      LatestCheckpoint.tv_sec=ctime.tv_sec;
	  LatestCheckpoint.tv_usec=ctime.tv_usec;
	  string arr_s = std::to_string(ctime.tv_sec) + '.' + std::to_string(ctime.tv_usec);
	  ClassAd cla;
	  //Dump all the dirty ClassAd into storagefile
	  map<string,int>::iterator itr=DirtyClassad.begin();
	  while (itr!=DirtyClassad.end()){
	    if (itr->second==1){
	           string ad;
		   GetStringClassAd(itr->first,ad);	      
		   ClassAdStorage.WriteBack(itr->first,ad);
	    }
	           ClearDirty(itr->first);
		   itr++; 
	  };
	  
	  cla.InsertAttr( ATTR_OP_TYPE, ClassAdCollOp_CheckPoint );
	  cla.InsertAttr( "Time",arr_s);   
	  
	  if (!WriteLogEntry(log_fp,&cla,true)){
	           return false;
	  };
	  int fd_check;
	  if( ( fd_check = open( CheckFileName.c_str(), O_RDWR | O_CREAT, 0600 ) ) < 0 ) {
                   CondorErrno = ERR_CACHE_FILE_ERROR;
				   CondorErrMsg = "failed to open checkpoint file " + CheckFileName + " errno=" + std::to_string(errno);
		   return( false );
	  }
	  string buffer;
	  unparser.Unparse( buffer, &cla );
	  buffer += "\n";
	  int result = write(fd_check,(void*)(buffer.c_str()),(unsigned int)buffer.size());
	  if (result < 0) {
			CondorErrMsg = "failed to write to checkpoint file " + CheckFileName + " errno=" + std::to_string(errno);
			fsync(fd_check);
			close(fd_check);
			return( false );
	  }
	  fsync(fd_check);
	  close(fd_check);
	  return true;
	  
}
//Given a offset, we read the key for the classad out
int ClassAdCollection::
ReadStorageEntry(int local_sfiled, int &offset,string &ckey){
         string OneLine;  
	 do{
	        offset=lseek(local_sfiled,0,SEEK_CUR);
		char  OneCharactor[1];
		OneLine = "";
		int l;
		while ((l=read(local_sfiled,OneCharactor,1))>0){
		  string n(OneCharactor ,1);
		  if (n=="\n"){
		    break; 
		  }else{
		    OneLine=OneLine+n;
		  }           
		}
		if (OneLine=="") break;
	 }while(OneLine[0]=='*');
	 
	 if (OneLine=="") {
	        return 1; //end of file
	 }else{
	   
	        ClassAdParser local_parser;
		ClassAd *cla=local_parser.ParseClassAd(OneLine ,true);
		
		cla->EvaluateAttrString("Key",ckey);  
		delete cla;    
		return 2; //good	   
	 }
}

#ifdef _MSC_VER
#pragma warning(pop) // restore 4996
#endif

// This function has a clear problem: it can find errors, 
// but they are not propagated or reacted to. This is bad. 
void ClassAdCollection::
MaybeSwapOutClassAd(void)
{
    ClassAd                 *swapout_ad;
    ClassAdTable::iterator  itr;

    if (Max_Classad == MaxCacheSize) {
        // First, we randomly pick a Classad that can be removed from 
        // the in-memory cache. 
        string write_back_key;
        if (!SelectClassadToReplace(write_back_key)) {
            CondorErrno = ERR_CANNOT_REPLACE;
            CondorErrMsg = "failed in replacing classad in cache";
        } else {
            // Does the ClassAd need to be written to disk before
            // it is removed from memory?
            if (CheckDirty(write_back_key)){
                string WriteBackClassad;
            
                if (!GetStringClassAd(write_back_key,WriteBackClassad)){
                    CondorErrMsg = "failed in get classad from cache";
                } else {
                    ClassAdStorage.WriteBack(write_back_key,WriteBackClassad );
                    ClearDirty(write_back_key);
                }
            }
            // Now we remove that ClassAd from memory
            itr = classadTable.find(write_back_key);
            swapout_ad = itr->second.ad;
            delete swapout_ad;                                          
            classadTable.erase(write_back_key);
            Max_Classad--;
        }
    }
    return;
}
    
bool ClassAdCollection::
SelectClassadToReplace(string &key)
{
    int                    i;
    ClassAdTable::iterator m;
    int                    count;

    i     = (int)((classadTable.size())*rand() /(RAND_MAX+1.0));
    m     = classadTable.begin();
    count = 0;

    while (count!=i){ 
        m++;
        count++;
    }
    key=(m->first);
    return true;
}

bool ClassAdCollection::
SetDirty(string key)
{
    DirtyClassad[key] = 1;
    return true;
}

bool ClassAdCollection::
ClearDirty(string key)
{
    DirtyClassad.erase(key);
    return true;
}

bool ClassAdCollection::
CheckDirty(string key)
{
    map <string,int>::iterator m = DirtyClassad.find(key);
    if ((m != DirtyClassad.end()) && (m->second > 0)) {
        return true;
    } else {
        return false;
    }
}

bool ClassAdCollection::
GetStringClassAd(string key, string &ad){ 
         ClassAdTable::iterator	itr;
	 ClassAd classad;
	 ClassAd *newad;
	 ClassAdUnParser local_unparser;
	 
	 classad.InsertAttr( "Key", key );
	 if( ( itr = classadTable.find( key ) ) != classadTable.end( ) ) {
	   newad=(ClassAd *) ((itr->second).ad)->Copy();
	   classad.Insert("Ad", newad);
	 }else{
	   return false;
	 };
	 local_unparser.Unparse(ad,&classad);
	 //delete newad;
	 return true;
}

bool ClassAdCollection::
TruncateStorageFile(){
         bool result;
	 if ((result=ClassAdStorage.TruncateStorageFile())==true){
	   return true;
	 }else{
	   return false;
	 }
}

bool ClassAdCollection::
dump_collection(){
         ClassAdTable::iterator	itr;
	 for (itr= classadTable.begin();itr!=classadTable.end();itr++){
		 cout << "dump_collection key= " << itr->first << std::endl;
	 }
	 return true;
}

}
