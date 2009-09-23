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

#ifndef _ClassAdCollection_H
#define _ClassAdCollection_H

//--------------------------------------------------------------------------

#include "condor_classad.h"
//#include "Set.h"
#include "HashTable.h"
#include "MyString.h"
#include "classad_log.h"

#include "classad_collection_types.h"

//--------------------------------------------------------------------------

typedef HashTable<int,BaseCollection*> CollectionHashTable;

//--------------------------------------------------------------------------

//@author Adiel Yoaz
//@include: classad_collection_types.h

/** This is the repository main class. Using the methods of this class
    the user can create and delete class-ads, change their attributes,
    control transactions, create logical collections of class-ads, and
    iterate through the class-ads and the collections. Note that only 
    the operations relating to class-ads are logged. Collection operations
    are not logged, therefore collections are not persistent.

    @author Adiel Yoaz
*/

class ClassAdCollection : private ClassAdLog {

public:

  //------------------------------------------------------------------------
  /**@name Constructor and Destructor
  */
  //@{

  /** Constructor (initialization). No logging is done if the log
      filename is not given using this constructor. We start with an
      empty repository.
    @return nothing
  */
  ClassAdCollection();

  /** Constructor (initialization). It reads the log file and initializes
      the class-ads (that are read from the log file) in memory.
    @param filename the name of the log file.
    @return nothing
  */
  ClassAdCollection(const char* filename,int max_historical_logs=0);

  /** Destructor - frees the memory used by the collections
    @return nothing
  */
  ~ClassAdCollection();

  //@}
  //------------------------------------------------------------------------
  /**@name Transaction control methods
  * Note that when no transaction is active, all persistent operations (creating and
  * deleting class-ads, changing attributes), are logged immediatly. When a transaction
  * is active (after BeginTransaction has been called), the changes are saved only
  * when the transaction is commited.
  */
  //@{

  /** Begin a transaction
    @return nothing
  */
  void BeginTransaction() { ClassAdLog::BeginTransaction(); }

  /** Commit a transaction
    @return nothing
  */
  void CommitTransaction() { ClassAdLog::CommitTransaction(); }

  /** Commit a transaction without forcing a sync to disk
    @return nothing
  */
  void CommitNondurableTransaction() { ClassAdLog::CommitNondurableTransaction(); }

  /** Abort a transaction
    @return true if a transaction aborted, false if no transaction active
  */
  bool AbortTransaction() { return ClassAdLog::AbortTransaction(); }

  bool InTransaction() { return ClassAdLog::InTransaction(); }

	  // increase non-durable commit level
	  // if > 0, begin non-durable commits
	  // return old level
  int IncNondurableCommitLevel() { return ClassAdLog::IncNondurableCommitLevel(); }
	  // decrease non-durable commit level and verify that it
	  // matches old_level
	  // if == 0, resume durable commits
  void DecNondurableCommitLevel(int old_level ) { ClassAdLog::DecNondurableCommitLevel( old_level ); }

		// Flush the log output buffer (but do not fsync).
		// This is useful if non-durable events have been recently logged.
		// Flushing will allow other processes that read the log to see
		// the events that might otherwise hang around in the output buffer
		// for a long time.
  void FlushLog() { ClassAdLog::FlushLog(); }

  ///
  Transaction* getActiveTransaction() { return ClassAdLog::getActiveTransaction(); }
  ///
  bool setActiveTransaction(Transaction* & transaction) { return ClassAdLog::setActiveTransaction(transaction); }


  /** Lookup an attribute's value in the current transaction. 
      @param key the key with which the class-ad was inserted into the repository.
      @param name the name of the attribute.
      @param val the value of the attrinute (output parameter).
      @return true on success, false otherwise.
  */
  bool LookupInTransaction(const char *key, const char *name, char *&val) { return (ClassAdLog::LookupInTransaction(key,name,val)==1); }
  
  /** Truncate the log file by creating a new "checkpoint" of the repository
    @return nothing
  */
  void TruncLog() { ClassAdLog::TruncLog(); }

  void SetMaxHistoricalLogs(int max) { ClassAdLog::SetMaxHistoricalLogs(max); }
  int GetMaxHistoricalLogs() { return ClassAdLog::GetMaxHistoricalLogs(); }

  time_t GetOrigLogBirthdate() { return ClassAdLog::GetOrigLogBirthdate(); }

  //@}
  //------------------------------------------------------------------------
  /**@name Method to control the class-ads in the repository
  */
  //@{

  /** Insert a new empty class-ad with the specified key.
      Also set it's type and target type attributes.
      @param key The class-ad's key.
      @param mytype The class-ad's MyType attribute value.
      @param targettype The class-ad's TargetType attribute value.
      @return true on success, false otherwise.
  */
  bool NewClassAd(const char* key, const char* mytype, const char* targettype);

  /** Insert a new class-ad with the specified key.
      The new class-ad will be a copy of the ad supplied.
      @param key The class-ad's key.
      @param ad The class-ad to copy into the repository.
      @return true on success, false otherwise.
  */
  bool NewClassAd(const char* key, ClassAd* ad);

  /** Destroy the class-ad with the specified key.
      @param key The class-ad's key.
      @return true on success, false otherwise.
  */
  bool DestroyClassAd(const char* key);

  /** Set an attribute in a class-ad.
      @param key The class-ad's key.
      @param name the name of the attribute.
      @param value the value of the attrinute.
      @return true on success, false otherwise.
  */
  bool SetAttribute(const char* key, const char* name, const char* value);

  /** Delete an attribute in a class-ad.
      @param key The class-ad's key.
      @param name the name of the attribute.
      @return true on success, false otherwise.
  */
  bool DeleteAttribute(const char* key, const char* name);

  /** Get a class-ad from the repository.
      Note that the class-ad returned cannot be modified directly.
      @param key The class-ad's key.
      @param Ad A pointer to the class-ad (output parameter).
      @return true on success, false otherwise.
  */
  bool LookupClassAd(const char* key, ClassAd*& Ad) { return (table.lookup(HashKey(key), Ad)==0); }

  bool AddAttrsFromTransaction(const char* key, ClassAd & ad) { return (ClassAdLog::AddAttrsFromTransaction(key,ad)); }
  
  //@}
  //------------------------------------------------------------------------
  /**@name Collection Operations
  * Note: these operations are not persistent - not logged.
  */
  //@{

  /*  NOT USED:
      Create an explicit collection, as a child of another collection.
      An explicit collection can include any subset of ads which are in its parent.
      The user can actively include and exclude ads from this collection.
      @param ParentCoID The ID of the parent collection.
      @param Rank The rank expression. Determines how the ads will be ordered in the collection.
      @param FullFlag The flag which indicates automatic insertion of class-ads from the parent.
      @return the ID of the new collection, or -1 in case of failure.
  */
//  int CreateExplicitCollection(int ParentCoID, const MyString& Rank, bool FullFlag=false);

  /*  NOT USED
	  Create a constraint collection, as a child of another collection.
      A constraint collection always contains the subset of ads from the parent, which
      match the constraint.
      @param ParentCoID The ID of the parent collection.
      @param Rank The rank expression. Determines how the ads will be ordered in the collection.
      @param Constraint sets the constraint expression for this collection.
      @return the ID of the new collection, or -1 in case of failure.
  */
//  int CreateConstraintCollection(int ParentCoID, const MyString& Rank, const MyString& Constraint);


  /*  NOT USED
      Create a partition collection, as a child of another collection.
      A partiton collection defines a partition based on a set of attributes. For
      each distinct set of values (corresponding to these attributes), a new
      child collection will be created, which will contain all the class-ads from the
      parent collection that have these values. The partition collection itself doesn't
      hold any class-ads, only its children do (the iteration methods for getting
      child collections can be used to retrieve them).
      @param ParentCoID The ID of the parent collection.
      @param Rank The rank expression. Determines how the ads will be ordered in the child collections.
      @param AttrList The set of attribute names used to define the partition.
      @return the ID of the new collection, or -1 in case of failure.
  */
//  int CreatePartition(int ParentCoID, const MyString& Rank, StringSet& AttrList);

  /** Deletes a collection and all of its descendants from the collection tree.
      @param CoID The ID of the collection to be deleted.
      @return true on success, false otherwise.
  */
  bool DeleteCollection(int CoID);

  //@}
  //------------------------------------------------------------------------
  /**@name Iteration methods
  */
  //@{

  /** Start iterations on all collections
    @return nothing
  */
  void StartIterateAllCollections();

  /** Get the next collection ID
      @param CoID The ID of the next collection (output parameter).
      @return true on success, false otherwise.
  */
  bool IterateAllCollections(int& CoID);

  /** Start iterations on child collections of a specified collection.
      @param ParentCoID The ID of the parent of the collections to be iterated on.
      @return true on success, false otherwise.
  */
  bool StartIterateChildCollections(int ParentCoID);

  /** Get the next child of the specified parent collection.
      @param ParentCoID The ID of the parent of the collections to be iterated on.
      @param CoID The ID of the next collection (output parameter).
      @return true on success, false otherwise.
  */
  bool IterateChildCollections(int ParentCoID, int& CoID);

  /** Start iterations on all class-ads in the repository.
      @return nothing.
  */
  void StartIterateAllClassAds() { table.startIterations(); }

  /** Get the next class-ad in the repository.
      @param Ad A pointer to next the class-ad (output parameter).
      @return true on success, false otherwise.
  */
  bool IterateAllClassAds(ClassAd*& Ad) { return (table.iterate(Ad)==1); }

  /** Get the next class-ad in the repository and its key.
      @param Ad A pointer to next the class-ad (output parameter).
	  @param KeyBuf A pointer to a buffer which will receive the key (output param).
      @return true on success, false otherwise.
  */
  bool IterateAllClassAds(ClassAd*& Ad, HashKey& KeyBuf) 
		{ return (table.iterate(KeyBuf,Ad)==1); }

  //@}
  //------------------------------------------------------------------------
  /**@name Misc methods
  */
  //@{

  /** Find out a collection's type (explicit, constraint, ...).
      @return the type of the specified collection: 0=explicit, 1=constraint.
  */
  int GetCollectionType(int CoID);

  /** Prints the whole repository (for debugging purposes).
      @return nothing.
  */
  void Print();

  /** Prints a single collection in the repository (for debugging purposes).
      @return nothing.
  */
  void Print(int CoID);

  /// A hash function used by the hash table objects (used internally).
  static unsigned int HashFunc(const int& Key) { return (unsigned int)Key; }

  //@}
  //------------------------------------------------------------------------

private:

  /** Start iterations on class-ads in a collection.
      Returns true on success, false otherwise.
  */
  bool StartIterateClassAds(int CoID);

  /** Get the next class-ad and its numeric rank in the collection.
      Returns true on success, false otherwise.
  */
  bool IterateClassAds(int CoID, RankedClassAd& OID);

  //------------------------------------------------------------------------
  // Data Members
  //------------------------------------------------------------------------

  /// The hash table that maps collection IDs to collection objects
  CollectionHashTable Collections;

  /// The last collection ID used
  int LastCoID;

  //------------------------------------------------------------------------
  // Methods that are used internally
  //------------------------------------------------------------------------

  ///
  bool AddClassAd(int CoID, const MyString& OID);

  ///
  bool AddClassAd(int CoID, const MyString& OID, ClassAd* ad);

  ///
  bool RemoveClassAd(int CoID, const MyString& OID);

  ///
  bool ChangeClassAd(const MyString& OID);

  ///
  bool RemoveCollection(int CoID, BaseCollection* Coll);

  ///
  bool TraverseTree(int CoID, bool (ClassAdCollection::*Func)(int,BaseCollection*));

  ///
  static float GetClassAdRank(ClassAd* Ad, const MyString& RankExpr);

  ///
  static bool EqualSets(StringSet& S1, StringSet& S2);

  ///
  bool CheckClassAd(BaseCollection* Coll, const MyString& OID, ClassAd* Ad);

};

#endif
