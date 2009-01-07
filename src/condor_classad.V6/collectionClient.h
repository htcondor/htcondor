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


#if !defined COLLECTION_CLIENT_H
#define COLLECTION_CLIENT_H

#include "classad/view.h"
#include "classad/collection.h"

class Sock;
class ClientTransaction;

BEGIN_NAMESPACE( classad )

typedef classad_hash_map<std::string,ClientTransaction*> LocalXactionTable;

class ClassAdCollectionClient : public ClassAdCollectionInterface {
public:
	ClassAdCollectionClient( );
	virtual ~ClassAdCollectionClient( );

		// Initialization and recovery
	virtual bool InitializeFromLog( const std::string &filename );
	void Recover( int &committed, int &aborted, int &unknown );

		// View management
	virtual bool CreateSubView( const ViewName &viewName,
				const ViewName &parentViewName,
				const std::string &constraint, const std::string &rank,
				const std::string &partitionExprs );
	virtual bool CreatePartition( const ViewName &viewName,
				const ViewName &parentViewName,
				const std::string &constraint, const std::string &rank,
				const std::string &partitionExprs, ClassAd *rep );
	virtual bool DeleteView( const ViewName &viewName );
	virtual bool SetViewInfo( const ViewName &viewName, 
				const std::string &constraint, const std::string &rank, 
				const std::string &partitionAttrs );
	virtual bool GetViewInfo( const ViewName &viewName, ClassAd *&viewInfo );
		// Child view interrogation
	virtual bool GetSubordinateViewNames( const ViewName &viewName,
				std::vector<std::string>& views );
	virtual bool GetPartitionedViewNames( const ViewName &viewName,
				std::vector<std::string>& views );
	virtual bool FindPartitionName( const ViewName &viewName, ClassAd *rep, 
				ViewName &partition );


		// ClassAd manipulation
	virtual bool AddClassAd( const std::string &key, ClassAd *newAd );
	virtual bool UpdateClassAd( const std::string &key, ClassAd *updateAd );
	virtual bool ModifyClassAd( const std::string &key, ClassAd *modifyAd );
	virtual bool RemoveClassAd( const std::string &key );
		// ClassAd interrogation
	virtual ClassAd *GetClassAd(const std::string &key );


		// Transaction management
	virtual bool OpenTransaction( const std::string &transactionName );
	virtual bool CloseTransaction(const std::string &transactionName,bool commit,
				int &outcome);

	virtual bool IsMyActiveTransaction( const std::string &transactionName );
	virtual void GetMyActiveTransactions( std::vector<std::string>& );
	virtual bool IsActiveTransaction( const std::string &transactionName );
	virtual bool GetAllActiveTransactions( std::vector<std::string>& );
	virtual bool IsMyPendingTransaction( const std::string &transactionName );
	virtual void GetMyPendingTransactions( std::vector<std::string>& );
	virtual bool IsCommittedTransaction( const std::string &transactionName );
	virtual bool IsMyCommittedTransaction( const std::string &transactionName );
	virtual void GetMyCommittedTransactions( std::vector<std::string>& );
	virtual bool GetAllCommittedTransactions( std::vector<std::string>& );

		// Server connection and disconnection
	bool Connect( const std::string &address, int port, bool UseTCP=true );
	bool Disconnect( );


protected:
	virtual bool OperateInRecoveryMode( ClassAd * );
	virtual bool LogState( FILE * );

private:
    ClassAdCollectionClient(const ClassAdCollectionClient &collection)            { return;       }
    ClassAdCollectionClient &operator=(const ClassAdCollectionClient &collection) { return *this; }

		// helper functions
	bool SendOpToServer( int, ClassAd *, int, ClassAd *& );
	bool _GetViewNames( int, const ViewName &, std::vector<std::string>& );
	bool _CheckTransactionState( int, const std::string &, Value & );
	bool _GetAllxxxTransactions( int, std::vector<std::string>& );
	bool DoCommitProtocol( const std::string &, int &, ClientTransaction * );
	bool GetServerXactionState( const std::string &, int & );

		// info about transactions
	LocalXactionTable	localXactionTable;

		// current connection to the server
	std::string	serverAddr;
	int		serverPort;
	Sock	*serverSock;

		// to manage the log file ...
	std::string	logFileName;
	FILE	*log_fp;
};

END_NAMESPACE

#endif
