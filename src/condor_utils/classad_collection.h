/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "HashTable.h"
#include "MyString.h"
#include "classad_log.h"

// these two definitions allow us to dereference a template type
//template<typename> struct dereference;
//template <typename T> struct dereference<T*> { typedef typename T type; };
// for example:
// if AD is type foo*, then
//    AD cad = new typename dereference<AD>::type();
// expands to
//    foo * cad = new foo();

// The GenericClassAdCollection is a thin wrapper around the ClassAdLog class
// It provides helper functions and a layer of abstraction.
// It does not provide (or rather, it *no longer* provides) any collection management

template <typename K, typename AD>
class GenericClassAdCollection : private ClassAdLog<K, AD> {

public:

  typename ClassAdLog<K,AD>::filter_iterator GetFilteredIterator(const classad::ExprTree &requirements, int timeslice_ms) {
    typename ClassAdLog<K,AD>::filter_iterator it(*this, &requirements, timeslice_ms);
    return it;
  }
  typename ClassAdLog<K,AD>::filter_iterator GetIteratorEnd() {
    typename ClassAdLog<K,AD>::filter_iterator it(*this, NULL, 0, true);
    return it;
  }

  //------------------------------------------------------------------------
  /**@name Constructor and Destructor
  */
  //@{

  /** Constructor (initialization). No logging is done if the log
      filename is not given using this constructor. We start with an
      empty repository.
    @return nothing
  */
  GenericClassAdCollection(const ConstructLogEntry * pctor)
	: ClassAdLog<K,AD>(pctor)
  {
  }

  /** Constructor (initialization). It reads the log file and initializes
      the class-ads (that are read from the log file) in memory.
    @param filename the name of the log file.
    @return nothing
  */
  GenericClassAdCollection(const ConstructLogEntry * pctor,const char* filename,int max_historical_logs=0)
	: ClassAdLog<K,AD>(filename,max_historical_logs,pctor)
  {
  }

  /** Destructor - frees the memory used by the collections
    @return nothing
  */
  ~GenericClassAdCollection() {};

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
  void BeginTransaction() { ClassAdLog<K,AD>::BeginTransaction(); }

  /** Commit a transaction
    @return nothing
  */
  void CommitTransaction(const char * comment=NULL) { ClassAdLog<K,AD>::CommitTransaction(comment); }

  /** Commit a transaction without forcing a sync to disk
    @return nothing
  */
  void CommitNondurableTransaction(const char * comment=NULL) { ClassAdLog<K,AD>::CommitNondurableTransaction(comment); }

  /** Abort a transaction
    @return true if a transaction aborted, false if no transaction active
  */
  bool AbortTransaction() { return ClassAdLog<K,AD>::AbortTransaction(); }

  bool InTransaction() { return ClassAdLog<K,AD>::InTransaction(); }

  /** Get a list of all new keys created in this transaction
	  @param new_keys List object to populate
   */
  void ListNewAdsInTransaction( std::list<std::string> &new_keys ) {
	return ClassAdLog<K,AD>::ListNewAdsInTransaction( new_keys );
  }

  bool GetTransactionKeys( std::set<std::string> &keys ) {
	return ClassAdLog<K,AD>::GetTransactionKeys( keys );
  }

  int SetTransactionTriggers(int mask) { return ClassAdLog<K,AD>::SetTransactionTriggers(mask); }
  int GetTransactionTriggers() { return ClassAdLog<K,AD>::GetTransactionTriggers(); }

	  // increase non-durable commit level
	  // if > 0, begin non-durable commits
	  // return old level
  int IncNondurableCommitLevel() { return ClassAdLog<K,AD>::IncNondurableCommitLevel(); }
	  // decrease non-durable commit level and verify that it
	  // matches old_level
	  // if == 0, resume durable commits
  void DecNondurableCommitLevel(int old_level ) { ClassAdLog<K,AD>::DecNondurableCommitLevel( old_level ); }

		// Flush the log output buffer (but do not fsync).
		// This is useful if non-durable events have been recently logged.
		// Flushing will allow other processes that read the log to see
		// the events that might otherwise hang around in the output buffer
		// for a long time.
  void FlushLog() { ClassAdLog<K,AD>::FlushLog(); }

  		// Force the log output buffer to non-volatile storage (disk).  
		// This means doing both a flush and fsync.
  void ForceLog() { ClassAdLog<K,AD>::ForceLog(); }

  ///
  Transaction* getActiveTransaction() { return ClassAdLog<K,AD>::getActiveTransaction(); }
  ///
  bool setActiveTransaction(Transaction* & transaction) { return ClassAdLog<K,AD>::setActiveTransaction(transaction); }


  /** Lookup an attribute's value in the current transaction. 
      @param key the key with which the class-ad was inserted into the repository.
      @param name the name of the attribute.
      @param val the value of the attrinute (output parameter).
      @return true on success, false otherwise.
  */
  bool LookupInTransaction(const K& key, const char *name, char *&val) { return (ClassAdLog<K,AD>::LookupInTransaction(key,name,val)==1); }
  
  /** Truncate the log file by creating a new "checkpoint" of the repository
    @return true on success; false if log could not be rotated (in which
	case we are continuing to use the old log file)
  */
  bool TruncLog() { return ClassAdLog<K,AD>::TruncLog(); }

  void SetMaxHistoricalLogs(int max) { ClassAdLog<K,AD>::SetMaxHistoricalLogs(max); }
  int GetMaxHistoricalLogs() { return ClassAdLog<K,AD>::GetMaxHistoricalLogs(); }

  time_t GetOrigLogBirthdate() { return ClassAdLog<K,AD>::GetOrigLogBirthdate(); }

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
  bool NewClassAd(const K& key, const char* mytype, const char* targettype) {
	const std::string str = key;
	this->AppendLog(new LogNewClassAd(str.c_str(),mytype,targettype, this->GetTableEntryMaker()));
	return true;
  }


  /** Insert a new class-ad with the specified key.
      The new class-ad will be a copy of the ad supplied.
      @param key The class-ad's key.
      @param ad The class-ad to copy into the repository.
      @return true on success, false otherwise.
  */
  bool NewClassAd(K key, AD ad) {
	const std::string str = key;
	LogRecord* log = new LogNewClassAd(str.c_str(),GetMyTypeName(*ad),GetTargetTypeName(*ad), this->GetTableEntryMaker());
	this->AppendLog(log);
	for (auto itr = ad->begin(); itr != ad->end(); itr++ ) {
		log = new LogSetAttribute(str.c_str(),itr->first.c_str(),ExprTreeToString(itr->second));
		this->AppendLog(log);
	}
	return true;
  }

  /** Destroy the class-ad with the specified key.
      @param key The class-ad's key.
      @return true on success, false otherwise.
  */
  bool DestroyClassAd(const K & key) {
	const std::string str = key;
	this->AppendLog(new LogDestroyClassAd(str.c_str(), this->GetTableEntryMaker()));
	return true;
  }

  /** Set an attribute in a class-ad.
      @param key The class-ad's key.
      @param name the name of the attribute.
      @param value the value of the attribute.
      @param is_dirty the parameter should be marked dirty in the classad.
      @return true on success, false otherwise.
  */
  bool SetAttribute(const K& key, const char* name, const char* value, const bool is_dirty=false) {
	const std::string str = key;
	this->AppendLog(new LogSetAttribute(str.c_str(),name,value,is_dirty));
	return true;
  }

  /** Delete an attribute in a class-ad.
      @param key The class-ad's key.
      @param name the name of the attribute.
      @return true on success, false otherwise.
  */
  bool DeleteAttribute(const K& key, const char* name) {
	const std::string str = key;
	this->AppendLog(new LogDeleteAttribute(str.c_str(),name));
	return true;
  }

  /** Clear all parameter dirty bits in a class-ad.
      @param key The class-ad's key.
      @return true on success, false otherwise.
  */
  bool ClearClassAdDirtyBits(const K& key) {
	AD Ad;
	if (this->table.lookup(key,Ad) < 0)
		return false;
	ClassAd* cad = Ad;
	cad->ClearAllDirtyFlags();
	return true;
  }


  /** Get a class-ad from the repository.
      Note that the class-ad returned cannot be modified directly.
      @param key The class-ad's key.
      @param Ad A pointer to the class-ad (output parameter).
      @return true on success, false otherwise.
  */
  bool Lookup(const K & key, AD & Ad) { return this->table.lookup(key, Ad) >= 0; }
  bool LookupClassAd(const K& key, ClassAd*& cad) {
	AD Ad(0); // shouldn't have to init this here, but g++ can't figure out that it will never be used unless it's first initialized.
	if (this->table.lookup(key, Ad) <0) {
		return false;
	}
	cad = Ad;
	return true;
  }

  bool Iterate(AD & Ad)         { return this->table.iterate(Ad) == 1; }
  bool Iterate(K& key, AD& Ad)  { return this->table.iterate(key,Ad) == 1; }

  bool AddAttrsFromTransaction(const K& key, ClassAd & ad) { return ClassAdLog<K,AD>::AddAttrsFromTransaction(key,ad); }
  bool AddAttrNamesFromTransaction(const K& key, classad::References & attrs) { return ClassAdLog<K,AD>::AddAttrNamesFromTransaction(key,attrs); }

  /** Start iterations on all class-ads in the repository.
      @return nothing.
  */
  void StartIterateAllClassAds() { this->table.startIterations(); }

  /** Get the next class-ad in the repository.
      @param Ad A pointer to next the class-ad (output parameter).
      @return true on success, false otherwise.
  */
  bool IterateAllClassAds(ClassAd*& cad) {
	AD Ad(0);
	if ( !  this->Iterate(Ad))
		return false;
	cad = Ad;
	return true;
  }

  /** Get the next class-ad in the repository and its key.
      @param Ad A pointer to next the class-ad (output parameter).
	  @param KeyBuf A pointer to a buffer which will receive the key (output param).
      @return true on success, false otherwise.
  */
  bool IterateAllClassAds(ClassAd*& cad, K& KeyBuf) {
	AD Ad(0);
	if ( ! this->Iterate(KeyBuf, Ad))
		return false;
	cad = Ad;
	return true;
  }

  // this is for DEBUG PURPOSES ONLY!!!
  HashTable<K,AD>* Table() { return &this->table; }
};

// Declare the old (non-templated) ClassAdCollection as a specialization of this type
typedef GenericClassAdCollection<std::string, ClassAd*> ClassAdCollection;

#endif
