/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __COLLECTION_BASE_H__
#define __COLLECTION_BASE_H__

#include "collection.h"

//#ifdef ADDCACHE
  #include "indexfile.h"
  #include <string>
  #include <sys/time.h>
  #include <unistd.h>
//#endif
#define MaxCacheSize 5

BEGIN_NAMESPACE( classad );

class ServerTransaction;

class ClassAdProxy {
public:
	ClassAdProxy( ){ ad=NULL; };
	~ClassAdProxy( ){ };

	ClassAd	*ad;
};


typedef hash_map<string,View*,StringHash> ViewRegistry;
typedef hash_map<string,ClassAdProxy,StringHash> ClassAdTable;
typedef hash_map<string,ServerTransaction*,StringHash> XactionTable;

class ClassAdCollection : public ClassAdCollectionInterface {
public:
	ClassAdCollection( );
	virtual ~ClassAdCollection( );
        ClassAdCollection(bool CacheOn);
	// Logfile control
	virtual bool InitializeFromLog( const string &filename,const string storagefile, const string checkpointfile  );


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

//#ifdef  ADDCACHE
        bool dump_collection();
        IndexFile ClassAdStorage;
	int  WriteCheckPoint();
	bool TruncateStorageFile();
//#endif


protected:
	virtual bool OperateInRecoveryMode( ClassAd * );
	virtual bool LogState( FILE * );

	friend class View;
	friend class ServerTransaction;
	friend class ClassAd;
	friend class LocalCollectionQuery;


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

	bool LogViews( FILE *log, View *view, bool subView );
        void Setup(bool CacheOn);
        bool Cache;
//#ifdef  ADDCACHE
        bool ReplaceClassad(string &key);
	bool SetDirty(string key);
        bool ClearDirty(string key);
	bool CheckDirty(string key);
        bool GetStringClassAd(string key,string &WriteBackClassad);       
        bool SwitchInClassAd(string key);
        int  ReadStorageEntry(int sfiled, int &offset,string &ckey);        
        bool ReadCheckPointFile();
	int Max_Classad;
        int CheckPoint;      
	map<string,int> DirtyClassad;
        timeval LatestCheckpoint;
        string CheckFileName;
        int test_checkpoint;
//#endif

};

END_NAMESPACE

#endif
