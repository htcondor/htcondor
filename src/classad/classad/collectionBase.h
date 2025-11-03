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


#ifndef __CLASSAD_COLLECTION_BASE_H__
#define __CLASSAD_COLLECTION_BASE_H__

#include "classad/collection.h"
#include "classad/indexfile.h"

#ifndef WIN32
  #include <sys/time.h>
  #include <unistd.h>
#endif

#define MaxCacheSize 5

namespace classad {

class ServerTransaction;

class ClassAdProxy {
public:
    ClassAdProxy( ){ ad=NULL; };
    ~ClassAdProxy( ){ };

    ClassAd *ad;
};

typedef classad_map<std::string,View*> ViewRegistry;
typedef classad_map<std::string,ClassAdProxy> ClassAdTable;
typedef classad_map<std::string,ServerTransaction*> XactionTable;

/** A ClassAdCollection stores a set of ClassAds. It is similar to a
    hash table, but with more interesting querying capabilities. */
class ClassAdCollection : public ClassAdCollectionInterface {
public:
    /**@name Constructors/Destructor
     * Note that the ClassAdCollection constructors talk about caching. 
     * If caching is off, all ClassAds are stored in memory. For large
     * collections, this may be a problem. If caching is on, only some 
     * ClassAds will be kept in memory, and all of them will be written
     * to disk. This is similar to virtual memory. Please note, however, 
     * that caching has been barely tested, and is not production ready.
     */
    //@{
    /** Default constructor for a Collection. Caching will be turned off. 
    */
    ClassAdCollection( );

    /** Constructor that allows you to enable or disable caching.
        @param cacheOn true if you want caching on, false otherwise
    */
    ClassAdCollection(bool cacheOn);

    /// Destructor. This will delete all ClassAds in the Collection
    virtual ~ClassAdCollection( );
    //@}

    /**@name Persistence
     * ClassAd collections are most useful when they are stored
     * persistently to disk, otherwise they will not be saved
     * between invocations of your program. The log file is a journal
     * of the changes to the Collection. It is created or read
     * by InitializeFromLog, and it can be compressed by TruncateLog()
     */
    //@{
    /** Setup persistence and caching.
     *  This function has two purposes, and should probably be two functions.
     *  First, it either creates the persistent log file if it is not there, or
     *  reads the collection back from the log file. 
     *  Second, it sets up the caching, if caching is turned on. 
     *  @param logfile The name of the persistent log file.
     *  @param storagefile The name of file to keep the cache. Leave empty if you are not using a cache
     *  @param checkpointfile The name of the checkpoint file for the cache.  Leave empty if you are not using a cache
     */
    virtual bool InitializeFromLog( const std::string &logfile,
                                    const std::string storagefile="", 
                                    const std::string checkpointfile=""  );

    /** This function compresses the persistent log file. Before compression,
     *  the log file contains a list of changes to the collection. After
     *  compression, it contains the state of the collection. 
     * 
     */
	virtual bool TruncateLog(void);
    //@}

    /**@name Manipulating ClassAds in a collection
     * This set of functions allows you to manipulate ClassAds in a Collection.
     * There are a couple of important things to be aware of. First, once you
     * successfully give a ClassAd to a collection, it is owned by the collection, 
     * and you should not delete it. 
     * (If it is not successful--for instance, if AddClassAd() fails--you still own
     * the ClassAd.) Second, once a ClassAd is owned by the collection, you should
     * not directly modify it. Instead you should use UpdateClassAd or ModifyClassAd().
     */
    //@{
    /** Add a ClassAd to the collection. If a ClassAd is already in the Collection 
     *  for the given key, the one in the Collection is deleted and the new one 
     *  replaces it. If this function succeeds, the Collection now owns the ClassAd,
     *  and you should not change it or delete it directly. If this function fails,
     *  you still own the ClassAd and are responsible for deleting it when appropriate. 
     *  @param key The unique identifier for this ClassAd
     *  @param newAd The new add to put into the Collection
     */
    virtual bool AddClassAd( const std::string &key, ClassAd *newAd );
    virtual bool UpdateClassAd( const std::string &key, ClassAd *updateAd );

    // Note: modifyAd is deleted before this function returns.
    virtual bool ModifyClassAd( const std::string &key, ClassAd *modifyAd );
    virtual bool RemoveClassAd( const std::string &key );

    // ClassAd interrogation
    virtual ClassAd *GetClassAd(const std::string &key );
    //@}

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
    virtual const View *GetView( const ViewName &viewName );
    virtual bool ViewExists( const ViewName &viewName);
    // Child view interrogation
    virtual bool GetSubordinateViewNames( const ViewName &viewName,
                                          std::vector<std::string>& views );
    virtual bool GetPartitionedViewNames( const ViewName &viewName,
                                          std::vector<std::string>& views );
    virtual bool FindPartitionName( const ViewName &viewName, ClassAd *rep, 
                                    ViewName &partition );

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

    bool dump_collection();
    IndexFile ClassAdStorage;
    int  WriteCheckPoint();
    bool TruncateStorageFile();

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
    ViewRegistry    viewRegistry;
    ClassAdTable    classadTable;
    View            viewTree;
    XactionTable    xactionTable;

    bool LogViews( FILE *log, View *view, bool subView );
    void Setup(bool CacheOn);
    bool Cache;
    bool SelectClassadToReplace(std::string &key);
    bool SetDirty(std::string key);
    bool ClearDirty(std::string key);
    bool CheckDirty(std::string key);
    bool GetStringClassAd(std::string key,std::string &WriteBackClassad);       
    bool SwitchInClassAd(std::string key);
    int  ReadStorageEntry(int sfiled, int &offset,std::string &ckey);        
    bool ReadCheckPointFile();
    void MaybeSwapOutClassAd(void);

    int Max_Classad;
    int CheckPoint;      
    std::map<std::string,int> DirtyClassad;
    timeval LatestCheckpoint;
    std::string CheckFileName;
    int test_checkpoint;
    
 private:
    ClassAdCollection(const ClassAdCollection &) : ClassAdCollectionInterface(), viewTree(NULL) { return; }
    ClassAdCollection &operator=(const ClassAdCollection &) { return *this; }

    bool AddClassAd_Transaction(const std::string &key, ClassAd *newAd);
    bool AddClassAd_NoTransaction(const std::string &key, ClassAd *newAd);
    
};

}

#endif
