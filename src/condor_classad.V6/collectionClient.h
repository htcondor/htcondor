/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#if !defined COLLECTION_CLIENT_H
#define COLLECTION_CLIENT_H

#include "view.h"
#include "collection.h"

class Sock;
class ClientTransaction;

BEGIN_NAMESPACE( classad )

typedef hash_map<string,ClientTransaction*,StringHash> LocalXactionTable;

class ClassAdCollectionClient : public ClassAdCollectionInterface {
public:
	ClassAdCollectionClient( );
	virtual ~ClassAdCollectionClient( );

		// Initialization and recovery
	virtual bool InitializeFromLog( const string &filename );
	void Recover( int &committed, int &aborted, int &unknown );

		// View management
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
	virtual bool IsMyPendingTransaction( const string &transactionName );
	virtual void GetMyPendingTransactions( vector<string>& );
	virtual bool IsCommittedTransaction( const string &transactionName );
	virtual bool IsMyCommittedTransaction( const string &transactionName );
	virtual void GetMyCommittedTransactions( vector<string>& );
	virtual bool GetAllCommittedTransactions( vector<string>& );

		// Server connection and disconnection
	bool Connect( const string &address, int port, bool UseTCP=true );
	bool Disconnect( );


protected:
	virtual bool OperateInRecoveryMode( ClassAd * );
	virtual bool LogState( FILE * );

private:
    ClassAdCollectionClient(const ClassAdCollectionClient &collection)            { return;       }
    ClassAdCollectionClient &operator=(const ClassAdCollectionClient &collection) { return *this; }

		// helper functions
	bool SendOpToServer( int, ClassAd *, int, ClassAd *& );
	bool _GetViewNames( int, const ViewName &, vector<string>& );
	bool _CheckTransactionState( int, const string &, Value & );
	bool _GetAllxxxTransactions( int, vector<string>& );
	bool DoCommitProtocol( const string &, int &, ClientTransaction * );
	bool GetServerXactionState( const string &, int & );

		// info about transactions
	LocalXactionTable	localXactionTable;

		// current connection to the server
	string	serverAddr;
	int		serverPort;
	Sock	*serverSock;

		// to manage the log file ...
	string	logFileName;
	FILE	*log_fp;
};

END_NAMESPACE

#endif
