#ifndef _ClassAdCollection_H
#define _ClassAdCollection_H

//--------------------------------------------------------------------------

#include "condor_classad.h"
#include "Set.h"
#include "HashTable.h"
#include "MyString.h"
#include "classad_log.h"

#include "classad_collection_types.h"

//--------------------------------------------------------------------------

typedef HashTable<int,BaseCollection*> CollectionHashTable;

///--------------------------------------------------------------------------

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

  /** Constructor (initialization)
    input: filename - the name of the log file
  */
  ClassAdCollection(const char* filename);

  /// Destructor - frees the memory used by the collections
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

  /// Begin a transaction
  void BeginTransaction() { ClassAdLog::BeginTransaction(); }

  /// Commit a transaction
  void CommitTransaction() { ClassAdLog::CommitTransaction(); }

  /// Abort a transaction
  void AbortTransaction() { ClassAdLog::AbortTransaction(); }

  /** Lookup an attribute's value in the current transaction. 
      Returns true on success, false otherwise.
  */
  bool LookupInTransaction(const char *key, const char *name, char *&val) { return (ClassAdLog::LookupInTransaction(key,name,val)==1); }
  
  /// Truncate the log file by creating a new "checkpoint" of the repository
  void TruncLog() { ClassAdLog::TruncLog(); }

  //@}
  //------------------------------------------------------------------------
  /**@name Method to control the class-ads in the repository
  */
  //@{

  /** Insert a new empty class-ad with the specified key.
      Also set it's type and target type attributes.
      Returns true on success, false otherwise.
  */
  bool NewClassAd(const char* key, const char* mytype, const char* targettype);

  /** Destroy the class-ad with the specified key.
      Returns true on success, false otherwise.
  */
  bool DestroyClassAd(const char* key);

  /** Set an attribute in a class-ad.
      Returns true on success, false otherwise.
  */
  bool SetAttribute(const char* key, const char* name, const char* value);

  /** Delete an attribute in a class-ad.
      Returns true on success, false otherwise.
  */
  bool DeleteAttribute(const char* key, const char* name);

  /** Get a class-ad from the repository.
      Note that the class-ad returned cannot be modified directly.
      Returns true on success, false otherwise.
  */
  bool LookupClassAd(const char* key, ClassAd*& Ad) { return (table.lookup(HashKey(key), Ad)==0); }
  
  //@}
  //------------------------------------------------------------------------
  /**@name Collection Operations
  * Note: these operations are not persistent - not logged.
  */
  //@{

  /** Create an explicit collection, as a child of another collection.
      An explicit collection can include any subset of ads which are in its parent.
      The user can actively include and exclude ads from this collection.
      The Rank string determines how the ads will be ordered in the collection.
      The FullFlag determines if the ads in the parent collection are automatically
      added to the new collection (and also new ads inserted into the parent).
      Returns the ID of the new collection, or -1 in case of failure.
  */
  int CreateExplicitCollection(int ParentCoID, const MyString& Rank, bool FullFlag=false);

  /** Create a constraint collection, as a child of another collection.
      A constraint collection always contains the subset of ads from the parent, which
      match the constraint.
      The Rank string determines how the ads will be ordered in the collection.
      The Constraint string sets the constraint expression for this collection.
      Returns the ID of the new collection, or -1 in case of failure.
  */
  int CreateConstraintCollection(int ParentCoID, const MyString& Rank, const MyString& Constraint);

  // int CreatePartition(int ParentCoID, const MyString& Rank, char** AttrList);

  /** Deletes a collection and all of its descendants from the collection tree.
      Returns true on success, false otherwise.
  */
  bool DeleteCollection(int CoID);

  //@}
  //------------------------------------------------------------------------
  /**@name Iteration methods
  */
  //@{

  /// Start iterations on all collections
  void StartIterateAllCollections();

  /** Get the next collection ID
      Returns true on success, false otherwise.
  */
  bool IterateAllCollections(int& CoID);

  /** Start iterations on child collections of a specified collection.
      Returns true on success, false otherwise.
  */
  bool StartIterateChildCollections(int ParentCoID);

  /** Get the next child of the specified parent collection.
      Returns true on success, false otherwise.
  */
  bool IterateChildCollections(int ParentCoID, int& CoID);

  /** Start iterations on all class-ads in the repository.
      Returns true on success, false otherwise.
  */
  void StartIterateAllClassAds() { table.startIterations(); }

  /** Get the next class-ad in the repository.
      Returns true on success, false otherwise.
  */
  bool IterateAllClassAds(ClassAd*& Ad) { return (table.iterate(Ad)==1); }

  //@}
  //------------------------------------------------------------------------
  /**@name Misc methods
  */
  //@{

  /// Returns the type of the specified collection: explicit(0), constraint(1).
  int GetCollectionType(int CoID);

  /// Prints the whole repository (for debugging purposes).
  void Print();

  /// Prints a single collection in the repository (for debugging purposes).
  void Print(int CoID);

  /// A hash function used by the hash table objects
  static int HashFunc(const int& Key, int TableSize) { return (Key % TableSize); }

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
  bool ChangeClassAd(int CoID, const MyString& OID);

  ///
  bool ChangeClassAd(int CoID, const MyString& OID, ClassAd* ad);

  ///
  bool RemoveCollection(int CoID, BaseCollection* Coll);

  ///
  bool TraverseTree(int CoID, bool (ClassAdCollection::*Func)(int,BaseCollection*));

  ///
  static float GetClassAdRank(ClassAd* Ad, const MyString& RankExpr);

};

#endif
