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

#if !defined COLLECTION_H
#define COLLECTION_H

#include "view.h"
#include "source.h"
#include "sink.h"

BEGIN_NAMESPACE( classad )

	// outcome from a commit
enum { XACTION_ABORTED, XACTION_COMMITTED, XACTION_UNKNOWN };

class ClassAdCollectionInterface {
public:
	ClassAdCollectionInterface( );
	virtual ~ClassAdCollectionInterface( );


        // Logfile control
	virtual bool InitializeFromLog( const string &filename ) = 0;
	bool TruncateLog( );


		// View creation/deletion/interrogation
	virtual bool CreateSubView( const ViewName &viewName,
				const ViewName &parentViewName,
				const string &constraint, const string &rank,
				const string &partitionExprs ) = 0;
	virtual bool CreatePartition( const ViewName &viewName,
				const ViewName &parentViewName,
				const string &constraint, const string &rank,
				const string &partitionExprs, ClassAd *rep ) = 0;
	virtual bool DeleteView( const ViewName &viewName ) = 0;
	virtual bool SetViewInfo( const ViewName &viewName, 
				const string &constraint, const string &rank, 
				const string &partitionAttrs ) = 0;
	virtual bool GetViewInfo( const ViewName &viewName, ClassAd *&viewInfo )=0;
		// Child view interrogation
	virtual bool GetSubordinateViewNames( const ViewName &viewName,
				vector<string>& views ) = 0;
	virtual bool GetPartitionedViewNames( const ViewName &viewName,
				vector<string>& views ) = 0;
	virtual bool FindPartitionName( const ViewName &viewName, ClassAd *rep, 
				ViewName &partition ) = 0;


		// ClassAd manipulation 
	virtual bool AddClassAd( const string &key, ClassAd *newAd ) = 0;
	virtual bool UpdateClassAd( const string &key, ClassAd *updateAd ) = 0;
	virtual bool ModifyClassAd( const string &key, ClassAd *modifyAd ) = 0;
	virtual bool RemoveClassAd( const string &key ) = 0;
	virtual ClassAd *GetClassAd(const string &key ) = 0;


		// Mode management
	bool SetAcknowledgementMode( AckMode );
	AckMode GetAcknowledgementMode( ) const { return( amode ); }


		// Transaction management
	virtual bool OpenTransaction( const string &xactionName) = 0;
	bool SetCurrentTransaction( const string &xactionName );
	void GetCurrentTransaction( string &xactionName ) const;
	virtual bool CloseTransaction( const string &xactionName, bool commit,
				int &outcome )=0;

	virtual bool IsMyActiveTransaction( const string &xactionName ) = 0;
	virtual void GetMyActiveTransactions( vector<string>& ) = 0;
	virtual bool IsActiveTransaction( const string &xactionName ) = 0;
	virtual bool GetAllActiveTransactions( vector<string>& ) = 0;
	virtual bool IsCommittedTransaction( const string &xactionName ) = 0;
	virtual bool GetAllCommittedTransactions( vector<string>& ) = 0;


		// misc
	static inline const char *GetOpString( int op ) {
		return( op>=ClassAdCollOp_NoOp && op<=__ClassAdCollOp_MiscOps_End__ ? 
				CollOpStrings[op-ClassAdCollOp_NoOp] : "(unknown)" );
	}

protected:
		// Utility functions to make collection log records
	ClassAd *_CreateSubView( const ViewName &viewName,
				const ViewName &parentViewName,
				const string &constraint, const string &rank,
				const string &partitionExprs );
	ClassAd *_CreatePartition( const ViewName &viewName,
				const ViewName &parentViewName,
				const string &constraint, const string &rank,
				const string &partitionExprs, ClassAd *rep );
	ClassAd *_DeleteView( const ViewName &viewName );
	ClassAd *_SetViewInfo( const ViewName &viewName, 
				const string &constraint, const string &rank, 
				const string &partitionAttrs );
	ClassAd *_AddClassAd( const string &xactionName, 
				const string &key, ClassAd *newAd );
	ClassAd *_UpdateClassAd( const string &xactionName, 
				const string &key, ClassAd *updateAd );
	ClassAd *_ModifyClassAd( const string &xactionName, 
				const string &key, ClassAd *modifyAd );
	ClassAd *_RemoveClassAd( const string &xactionName,
				const string &key );

		// mode management data
	AckMode			amode;
	string			currentXactionName;

		// function which executes log records in recovery mode
	virtual bool OperateInRecoveryMode( ClassAd* ) = 0;

		// utility functions to operate on logs and log metadata
	ClassAd 		*ReadLogEntry( FILE * );
	bool			WriteLogEntry( FILE *, ClassAd *, bool sync=true );
	bool 			ReadLogFile( );
	string 			logFileName;
	ClassAdParser	parser;
	ClassAdUnParser	unparser;
	FILE			*log_fp;

		// override for client and server
	virtual bool LogState( FILE * ) = 0;
};


END_NAMESPACE

#endif
