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

#if !defined( COLLECTION_SERVER_H )
#define COLLECTION_SERVER_H

#include "collection.h"


BEGIN_NAMESPACE( classad );

class ServerTransaction;
//class Sock;

class ClassAdProxy {
public:
	ClassAdProxy( ){ ad=NULL; };
	~ClassAdProxy( ){ };

	ClassAd	*ad;
};


typedef hash_map<string,View*,StringHash> ViewRegistry;
typedef hash_map<string,ClassAdProxy,StringHash> ClassAdTable;
typedef hash_map<string,ServerTransaction*,StringHash> XactionTable;
//typedef deque<string> XactID_S;

class ClassAdCollectionServer : public ClassAdCollectionInterface {
public:
	ClassAdCollectionServer( );
	virtual ~ClassAdCollectionServer( );

		// Logfile control
	virtual bool InitializeFromLog( const string &filename );


		// View creation/deletion/interrogation
    virtual bool CreateSubView( const ViewName &viewName,
				const ViewName &parentViewName,
				const string &constraint, const string &rank,
				const string &partitionExprs );
	virtual bool CreatePartition( const ViewName &viewName,
				const ViewName &parentViewName,
				const string &constraint, const string &rank,
				const string &partitionExprs, ClassAd *rep );
	virtual bool DeleteView( const ViewName &viewName );
	virtual bool SetViewInfo( const ViewName &viewName,
				const string &constraint, const string &rank,
				const string &partitionAttrs );
	virtual bool GetViewInfo( const ViewName &viewName, ClassAd *&viewInfo );
		// Child view interrogation
	virtual bool GetSubordinateViewNames( const ViewName &viewName,
				vector<string>& views );
	virtual bool GetPartitionedViewNames( const ViewName &viewName,
				vector<string>& views );
	virtual bool FindPartitionName( const ViewName &viewName, ClassAd *rep, 
				ViewName &partition );


		// ClassAd manipulation 
	virtual bool AddClassAd( const string &key, ClassAd *newAd );
	virtual bool UpdateClassAd( const string &key, ClassAd *updateAd );
	virtual bool ModifyClassAd( const string &key, ClassAd *modifyAd );
	virtual bool RemoveClassAd( const string &key );
		// ClassAd interrogation
	virtual ClassAd *GetClassAd(const string &key );

		 
		// Transaction management
	virtual bool OpenTransaction( const string &transactionName );
	virtual bool CloseTransaction(const string &transactionName,bool commit,
				int &outcome);

	virtual bool IsMyActiveTransaction( const string &transactionName );
	virtual void GetMyActiveTransactions( vector<string>& );
	virtual bool IsActiveTransaction( const string &transactionName );
	virtual bool GetAllActiveTransactions( vector<string>& );
	virtual bool IsCommittedTransaction( const string &transactionName );
	virtual bool GetAllCommittedTransactions( vector<string>& );
								
		// Debugging function
	bool DisplayView( const ViewName &viewName, FILE * );

		// handle remote request function
		// returns +1 on success, -1 on failure, 0 on client disconnection

        //int HandleClientRequest( int command, Sock *clientSock );

protected:
	virtual bool OperateInRecoveryMode( ClassAd * );
	//virtual bool OperateInNetworkMode( int, ClassAd *, Sock * );
	virtual bool LogState( FILE * );

private:
	friend class View;
	friend class ServerTransaction;
	friend class ClassAd;
	friend class LocalCollectionQuery;

		// remote interrogation service function
	//bool HandleReadOnlyCommands( int, ClassAd *, Sock * );
	//bool HandleQuery( ClassAd *, Sock * );

		// low level execution modules
	bool PlayClassAdOp ( int, ClassAd * );
	bool PlayXactionOp ( int, const string &, ClassAd *, ServerTransaction *& );
	bool PlayViewOp ( int, ClassAd * );

		// View registry interface
	bool RegisterView( const ViewName &viewName, View * );
	bool UnregisterView( const ViewName &viewName );

		// Internal tables and associated state
	ViewRegistry	viewRegistry;
	ClassAdTable	classadTable;
	View			viewTree;
	XactionTable	xactionTable;
        //XactID_S xactid_table; 
	bool LogViews( FILE *log, View *view, bool subView );
};

END_NAMESPACE

#endif
