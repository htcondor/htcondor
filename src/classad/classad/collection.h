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


#ifndef __CLASSAD_COLLECTION_H__
#define __CLASSAD_COLLECTION_H__

#include "classad/view.h"
#include "classad/source.h"
#include "classad/sink.h"

namespace classad {


class ClassAdCollectionInterface {
public:
	ClassAdCollectionInterface( );
	virtual ~ClassAdCollectionInterface( );

	enum {
	    ClassAdCollOp_NoOp                  = 10000,

		__ClassAdCollOp_ViewOps_Begin__,
		ClassAdCollOp_CreateSubView         = __ClassAdCollOp_ViewOps_Begin__,
		ClassAdCollOp_CreatePartition       ,
		ClassAdCollOp_DeleteView            ,
		ClassAdCollOp_SetViewInfo           ,
		ClassAdCollOp_AckViewOp             ,
		__ClassAdCollOp_ViewOps_End__       = ClassAdCollOp_AckViewOp,

		__ClassAdCollOp_ClassAdOps_Begin__,
		ClassAdCollOp_AddClassAd            =__ClassAdCollOp_ClassAdOps_Begin__,
		ClassAdCollOp_UpdateClassAd         ,
		ClassAdCollOp_ModifyClassAd         ,
		ClassAdCollOp_RemoveClassAd         ,
		ClassAdCollOp_AckClassAdOp          ,
		__ClassAdCollOp_ClassAdOps_End__    = ClassAdCollOp_AckClassAdOp,

		__ClassAdCollOp_XactionOps_Begin__,
		ClassAdCollOp_OpenTransaction       =__ClassAdCollOp_XactionOps_Begin__,
		ClassAdCollOp_AckOpenTransaction    ,
		ClassAdCollOp_CommitTransaction     ,
		ClassAdCollOp_AbortTransaction      ,
		ClassAdCollOp_AckCommitTransaction  ,
		ClassAdCollOp_ForgetTransaction     ,
		__ClassAdCollOp_XactionOps_End__    = ClassAdCollOp_ForgetTransaction,

		__ClassAdCollOp_ReadOps_Begin__     ,
		ClassAdCollOp_GetClassAd            = __ClassAdCollOp_ReadOps_Begin__,
		ClassAdCollOp_GetViewInfo           ,
		ClassAdCollOp_GetSubordinateViewNames,
		ClassAdCollOp_GetPartitionedViewNames,
		ClassAdCollOp_FindPartitionName     ,
		ClassAdCollOp_IsActiveTransaction   ,
		ClassAdCollOp_IsCommittedTransaction,
		ClassAdCollOp_GetAllActiveTransactions,
		ClassAdCollOp_GetAllCommittedTransactions,
		ClassAdCollOp_GetServerTransactionState,
		ClassAdCollOp_AckReadOp             ,
		__ClassAdCollOp_ReadOps_End__       = ClassAdCollOp_AckReadOp,

		__ClassAdCollOp_MiscOps_Begin__     ,
		ClassAdCollOp_Connect               = __ClassAdCollOp_MiscOps_Begin__,
		ClassAdCollOp_QueryView             ,
		ClassAdCollOp_Disconnect            ,
	    __ClassAdCollOp_MiscOps_End__       = ClassAdCollOp_Disconnect,
                ClassAdCollOp_CheckPoint
	};

	static const char * const CollOpStrings[];

	enum AckMode { _DEFAULT_ACK_MODE, WANT_ACKS, DONT_WANT_ACKS };

		// outcome from a commit
	enum { XACTION_ABORTED, XACTION_COMMITTED, XACTION_UNKNOWN };

        // Logfile control
	virtual bool InitializeFromLog( const std::string &filename,
									const std::string storagefile="", 
									const std::string checkpointfile="" ) = 0;
	virtual bool TruncateLog(void);

		// View creation/deletion/interrogation
	virtual bool CreateSubView( const ViewName &viewName,
				const ViewName &parentViewName,
				const std::string &constraint, const std::string &rank,
				const std::string &partitionExprs ) = 0;
	virtual bool CreatePartition( const ViewName &viewName,
				const ViewName &parentViewName,
				const std::string &constraint, const std::string &rank,
				const std::string &partitionExprs, ClassAd *rep ) = 0;
	virtual bool DeleteView( const ViewName &viewName ) = 0;
	virtual bool SetViewInfo( const ViewName &viewName, 
				const std::string &constraint, const std::string &rank, 
				const std::string &partitionAttrs ) = 0;
	virtual bool GetViewInfo( const ViewName &viewName, ClassAd *&viewInfo )=0;
		// Child view interrogation
	virtual bool GetSubordinateViewNames( const ViewName &viewName,
				std::vector<std::string>& views ) = 0;
	virtual bool GetPartitionedViewNames( const ViewName &viewName,
				std::vector<std::string>& views ) = 0;
	virtual bool FindPartitionName( const ViewName &viewName, ClassAd *rep, 
				ViewName &partition ) = 0;


		// ClassAd manipulation 
	virtual bool AddClassAd( const std::string &key, ClassAd *newAd ) = 0;
	virtual bool UpdateClassAd( const std::string &key, ClassAd *updateAd ) = 0;
	virtual bool ModifyClassAd( const std::string &key, ClassAd *modifyAd ) = 0;
	virtual bool RemoveClassAd( const std::string &key ) = 0;
	virtual ClassAd *GetClassAd(const std::string &key ) = 0;


		// Mode management
	bool SetAcknowledgementMode( AckMode );
	AckMode GetAcknowledgementMode( ) const { return( amode ); }


		// Transaction management
	virtual bool OpenTransaction( const std::string &xactionName) = 0;
	bool SetCurrentTransaction( const std::string &xactionName );
	void GetCurrentTransaction( std::string &xactionName ) const;
	virtual bool CloseTransaction( const std::string &xactionName, bool commit,
				int &outcome )=0;

	virtual bool IsMyActiveTransaction( const std::string &xactionName ) = 0;
	virtual void GetMyActiveTransactions( std::vector<std::string>& ) = 0;
	virtual bool IsActiveTransaction( const std::string &xactionName ) = 0;
	virtual bool GetAllActiveTransactions( std::vector<std::string>& ) = 0;
	virtual bool IsCommittedTransaction( const std::string &xactionName ) = 0;
	virtual bool GetAllCommittedTransactions( std::vector<std::string>& ) = 0;


		// misc
	static inline const char *GetOpString( int op ) {
		return( op>=ClassAdCollOp_NoOp && op<=__ClassAdCollOp_MiscOps_End__ ? 
				CollOpStrings[op-ClassAdCollOp_NoOp] : "(unknown)" );
	}

protected:
		// Utility functions to make collection log records
	ClassAd *_CreateSubView( const ViewName &viewName,
				const ViewName &parentViewName,
				const std::string &constraint, const std::string &rank,
				const std::string &partitionExprs );
	ClassAd *_CreatePartition( const ViewName &viewName,
				const ViewName &parentViewName,
				const std::string &constraint, const std::string &rank,
				const std::string &partitionExprs, ClassAd *rep );
	ClassAd *_DeleteView( const ViewName &viewName );
	ClassAd *_SetViewInfo( const ViewName &viewName, 
				const std::string &constraint, const std::string &rank, 
				const std::string &partitionAttrs );
	ClassAd *_AddClassAd( const std::string &xactionName, 
				const std::string &key, ClassAd *newAd );
	ClassAd *_UpdateClassAd( const std::string &xactionName, 
				const std::string &key, ClassAd *updateAd );
	ClassAd *_ModifyClassAd( const std::string &xactionName, 
				const std::string &key, ClassAd *modifyAd );
	ClassAd *_RemoveClassAd( const std::string &xactionName,
				const std::string &key );

		// mode management data
	AckMode			amode;
	std::string		currentXactionName;

		// function which executes log records in recovery mode
	virtual bool OperateInRecoveryMode( ClassAd* ) = 0;

		// utility functions to operate on logs and log metadata
	ClassAd 		*ReadLogEntry( FILE * );
	bool			WriteLogEntry( FILE *, ClassAd *, bool sync=true );
	bool 			ReadLogFile( );
	std::string		logFileName;
	ClassAdParser	parser;
	ClassAdUnParser	unparser;
	FILE			*log_fp;

        std::string                  StorageFileName;
        int                     sfiled;
		// override for client and server
	virtual bool LogState( FILE * ) = 0;

private:
    ClassAdCollectionInterface(const ClassAdCollectionInterface &)            { return;       }
    ClassAdCollectionInterface &operator=(const ClassAdCollectionInterface &) { return *this; }
};


}

#endif
