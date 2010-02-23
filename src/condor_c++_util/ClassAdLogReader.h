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

enum PollResultType {
	POLL_SUCCESS,
	POLL_FAIL,
	POLL_ERROR
};

class ClassAdLogConsumer;

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
	virtual bool NewClassAd(const char */*key*/,
							const char */*type*/,
							const char */*target*/) { return true; }

		/**
		 * Called when a ClassAd deletion is encountered.
		 *
		 * See Notes on Keys above.
		 */
	virtual bool DestroyClassAd(const char */*key*/) { return true; }

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
	virtual bool SetAttribute(const char */*key*/,
							  const char */*name*/,
							  const char */*value*/) { return true; }

		/**
		 * Called when an attribute deletion is encountered.
		 *
		 * See Notes on Keys above.
		 */
	virtual bool DeleteAttribute(const char */*key*/,
								 const char */*name*/) { return true; }

		/**
		 * Simple helpful way for the ClassAdLogConsumer to learn about
		 * who it is working for, set by the ClassAdLogReader.
		 */
	virtual void SetClassAdLogReader(ClassAdLogReader */*reader*/) { }
};

#endif
