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

#ifndef _CLASSADLOGREADER_H_
#define _CLASSADLOGREADER_H_

#include "ClassAdLogEntry.h"
#include "ClassAdLogParser.h"
#include "ClassAdLogProber.h"

#include "classad/classad.h"

enum PollResultType {
	POLL_SUCCESS,
	POLL_FAIL,
	POLL_ERROR
};

class ClassAdLogConsumer;

class ClassAdLogIterator;

class ClassAdLogIterEntry
{
public:
	typedef enum {
		ET_INIT = 0,
		ET_ERR,
		ET_NOCHANGE,
		ET_RESET,
		ET_END,
		NEW_CLASSAD = CondorLogOp_NewClassAd,
		DESTROY_CLASSAD = CondorLogOp_DestroyClassAd,
		SET_ATTRIBUTE = CondorLogOp_SetAttribute,
		DELETE_ATTRIBUTE = CondorLogOp_DeleteAttribute,
	} EntryType;

	EntryType getEntryType() const {return m_type;}
	const std::string &getAdType() const {return m_adtype;}
	const std::string &getAdTarget() const {return m_adtarget;}
	const std::string &getKey() const {return m_key;}
	const std::string &getValue() const {return m_value;}
	const std::string &getName() const {return m_name;}

	bool isDone() const {return m_type == ET_ERR || m_type == ET_NOCHANGE || m_type == ET_END;}

private:
	friend class ClassAdLogIterator;

	ClassAdLogIterEntry(EntryType type) : m_type(type) {}

	void setAdType(const std::string &adtype) {m_adtype = adtype;}
	void setAdTarget(const std::string &adtarget) {m_adtarget = adtarget;}
	void setKey(const std::string &key) {m_key = key;}
	void setValue(const std::string &value) {m_value = value;}
	void setName(const std::string &name) {m_name = name;}

	EntryType m_type;
	std::string m_adtype;
	std::string m_adtarget;
	std::string m_key;
	std::string m_value;
	std::string m_name;
};


class FileSentry;


class ClassAdLogReaderV2;


class ClassAdLogIterator : std::iterator<std::input_iterator_tag, ClassAdLogIterEntry* >
{
	friend class ClassAdLogReaderV2;

public:
        ~ClassAdLogIterator() {}

        ClassAdLogIterEntry* operator *() const {return m_current.get();}

        ClassAdLogIterEntry* operator ->() const {return m_current.get();}

        ClassAdLogIterator operator++();
        ClassAdLogIterator operator++(int);

        bool operator==(const ClassAdLogIterator &rhs);
        bool operator!=(const ClassAdLogIterator &rhs) {return !(*this == rhs);}

private:
        ClassAdLogIterator(const std::string &fname);
	ClassAdLogIterator() : m_current(new ClassAdLogIterEntry(ClassAdLogIterEntry::ET_END)), m_eof(false) {}

	void Next();
	bool Load();
	bool Process(const ClassAdLogEntry &log_entry);

	classad_shared_ptr<ClassAdLogParser> m_parser;
        classad_shared_ptr<ClassAdLogProber> m_prober;
	classad_shared_ptr<ClassAdLogIterEntry> m_current;
	classad_shared_ptr<FileSentry> m_sentry;
	std::string m_fname;
	bool m_eof;
};


class ClassAdLogReaderV2 {

public:
	ClassAdLogReaderV2(const std::string &fname) : m_fname(fname) {}
	ClassAdLogIterator begin() {return ClassAdLogIterator(m_fname);}
	ClassAdLogIterator end() {return ClassAdLogIterator();}

private:
	std::string m_fname;
};

class ClassAdLogReader {
public:
	ClassAdLogReader(ClassAdLogConsumer *consumer);
	~ClassAdLogReader();
	PollResultType Poll();
	void SetClassAdLogFileName(char const *fname);
	char const *GetClassAdLogFileName();

private:
	ClassAdLogConsumer *m_consumer;
	ClassAdLogProber prober;
	ClassAdLogParser parser;

	bool BulkLoad();
	bool IncrementalLoad();
	bool ProcessLogEntry(ClassAdLogEntry *log_entry,
						 ClassAdLogParser *caLogParser);
};


class ClassAdLogConsumer
{
public:

		/** Notes on Keys:
		 **
		 ** Keys are in the form of: ClusterId.ProcId
		 **
		 ** Keys are for two types of ClassAds: Cluster and
		 ** Job. Cluster ads are the parent of all Job ads that share
		 ** the same ClusterId. This is typically called Chaining, and
		 ** allow for attributes common to all jobs in a cluster to
		 ** live in a single location - it is a space saving
		 ** optimization for both in memory and on disk job ads.
		 **
		 ** To distinguish a key for a cluster vs a job, a cluster's
		 ** key's ClusterId always begins with a 0 and its ProcId is
		 ** -1. The portion of the ClusterId after the 0 is shared
		 ** with all jobs in that cluster. There is only one cluster
		 ** ad per cluster.
		 **
		 ** For instance, key = 01.-1 is the cluster ad for cluster
		 ** 1, and key = 1.0 is a job in cluster 1.
		 **/

		/** Notes on Ordering:
		 **
		 ** There is no guarantee that a Cluster ad will appear in
		 ** NewClassAd before Job ads for the cluster.
		 **
		 ** There is no guarantee on the order which attributes will
		 ** arrive following a NewClassAd. An error can be signaled if
		 ** a SetAttribute for a key preceeds NewClassAd for that key.
		 **/


		/**
		 * Called to clear the entire memory of the consumer. This
		 * happens when the ClassAdLogReader encounters a problem in the
		 * log, on startup, or when the log is compressed. It should
		 * invalidate all previous knowledge of the consumer.
		 */
	virtual void Reset() { }

		/**
		 * Called when a new ClassAd is encountered.
		 *
		 * See Notes on Keys above.
		 */
	virtual bool NewClassAd(const char * /*key*/,
							const char * /*type*/,
							const char * /*target*/) { return true; }

		/**
		 * Called when a ClassAd deletion is encountered.
		 *
		 * See Notes on Keys above.
		 */
	virtual bool DestroyClassAd(const char * /*key*/) { return true; }

		/**
		 * Called when an attribute assignment is encountered.
		 *
		 * An error can be raised if the key has not already been
		 * encountered from NewClassAd.
		 *
		 * The values must be parsed into their appropriate type. For
		 * old classads this can be done with condor_parser.h's
		 * ParseClassAdRvalExpr.
		 *
		 * See Notes on Keys above.
		 */
	virtual bool SetAttribute(const char * /*key*/,
							  const char * /*name*/,
							  const char * /*value*/) { return true; }

		/**
		 * Called when an attribute deletion is encountered.
		 *
		 * See Notes on Keys above.
		 */
	virtual bool DeleteAttribute(const char * /*key*/,
								 const char * /*name*/) { return true; }

		/**
		 * Simple helpful way for the ClassAdLogConsumer to learn about
		 * who it is working for, set by the ClassAdLogReader.
		 */
	virtual void SetClassAdLogReader(ClassAdLogReader * /*reader*/) { }
		/**
		 * Virtual destructor
		 * Suppresses compiler warnings
		 */
	virtual ~ClassAdLogConsumer() {}
};

#endif
