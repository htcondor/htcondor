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

#ifndef __COLLECTION_BASE_H__
#define __COLLECTION_BASE_H__

#include "collection.h"

//#ifdef ADDCACHE
  #include "indexfile.h"
  #include <string>
#ifndef WIN32
  #include <sys/time.h>
  #include <unistd.h>
#endif
//#endif
#define MaxCacheSize 5

BEGIN_NAMESPACE( classad )

class ServerTransaction;

class ClassAdProxy {
public:
	ClassAdProxy( ){ ad=NULL; };
	~ClassAdProxy( ){ };

	ClassAd	*ad;
};


typedef classad_hash_map<std::string,View*,StringHash> ViewRegistry;
typedef classad_hash_map<std::string,ClassAdProxy,StringHash> ClassAdTable;
typedef classad_hash_map<std::string,ServerTransaction*,StringHash> XactionTable;

class ClassAdCollection : public ClassAdCollectionInterface {
public:
	ClassAdCollection( );
	virtual ~ClassAdCollection( );
        ClassAdCollection(bool CacheOn);
	// Logfile control
	virtual bool InitializeFromLog( const std::string &filename,
									const std::string storagefile="", 
									const std::string checkpointfile=""  );


	// View creation/deletion/interrogation
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
	virtual bool ViewExists( const ViewName &viewName);
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
	virtual bool IsCommittedTransaction( const std::string &transactionName );
	virtual bool GetAllCommittedTransactions( std::vector<std::string>& );
								
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
	bool PlayXactionOp ( int, const std::string &, ClassAd *, ServerTransaction *& );
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
        bool ReplaceClassad(std::string &key);
	bool SetDirty(std::string key);
        bool ClearDirty(std::string key);
	bool CheckDirty(std::string key);
        bool GetStringClassAd(std::string key,std::string &WriteBackClassad);       
        bool SwitchInClassAd(std::string key);
        int  ReadStorageEntry(int sfiled, int &offset,std::string &ckey);        
        bool ReadCheckPointFile();
	int Max_Classad;
        int CheckPoint;      
	std::map<std::string,int> DirtyClassad;
        timeval LatestCheckpoint;
        std::string CheckFileName;
        int test_checkpoint;
//#endif

private:
        ClassAdCollection(const ClassAdCollection &collection) : viewTree(NULL) { return;       }
        ClassAdCollection &operator=(const ClassAdCollection &collection) { return *this; }
};

END_NAMESPACE

#endif
