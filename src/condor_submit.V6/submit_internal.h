/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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

#if !defined(_SUBMIT_INTERNAL_H)
#define _SUBMIT_INTERNAL_H

#include "dc_schedd.h"

#define PLUS_ATTRIBS_IN_CLUSTER_AD 1

// uncomment this to get (broken) legacy behavior that attributes in
// SUBMIT_ATTRS/SUBMIT_EXPRS behave as if they were statements in your submit file
//#define SUBMIT_ATTRS_IS_ALSO_CONDOR_PARAM 1

// functions in submit_glob.cpp
#define EXPAND_GLOBS_WARN_EMPTY (1<<0)
#define EXPAND_GLOBS_FAIL_EMPTY (1<<1)
#define EXPAND_GLOBS_ALLOW_DUPS (1<<2)
#define EXPAND_GLOBS_WARN_DUPS  (1<<3)
#define EXPAND_GLOBS_TO_DIRS    (1<<4) // when you want dirs only
#define EXPAND_GLOBS_TO_FILES   (1<<5) // when you want files only

int submit_expand_globs(StringList &items, int options, std::string & errmsg);

// functions for handling the queue statement
int queue_connect();
int queue_begin(StringList & vars, bool new_cluster); // called before iterating items
void queue_end(StringList & vars, bool fEof); // called when done iterating items for a single queue statement, and at end of file

int queue_item(int num, StringList & vars, char * item, int item_index, int options, const char * delims, const char * ws);
// option flags for queue_item.
#define QUEUE_OPT_WARN_EMPTY_FIELDS (1<<0)
#define QUEUE_OPT_FAIL_EMPTY_FIELDS (1<<1)


// this copied from condor_qmgr.h. TODO fix to refer rather than re-declare
#ifndef _LIBQMGR_H
typedef struct {
	bool dummy;
} Qmgr_connection;

typedef unsigned char SetAttributeFlags_t;
const SetAttributeFlags_t NONDURABLE = (1<<0); // do not fsync
	// NoAck tells the remote version of SetAttribute to not send back a
	// return code.  If the operation fails, the connection will be closed,
	// so failure will be detected in CommitTransaction().  This is useful
	// for improving performance when setting lots of attributes.
const SetAttributeFlags_t SetAttribute_NoAck = (1<<1);
const SetAttributeFlags_t SETDIRTY = (1<<2);
const SetAttributeFlags_t SHOULDLOG = (1<<3);
#endif

// Abstract the schedd's queue protocol so we can NOT call it when simulating
//
class AbstractScheddQ {
public:
	virtual ~AbstractScheddQ() {}
	virtual int get_NewCluster() = 0;
	virtual int get_NewProc(int cluster_id) = 0;
	virtual int destroy_Cluster(int cluster_id, const char *reason = NULL) = 0;
	virtual int set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags=0 ) = 0;
	virtual int set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags = 0 ) = 0;
	virtual int send_SpoolFileIfNeeded(ClassAd& ad) = 0;
	virtual int send_SpoolFile(char const *filename) = 0;
	virtual int send_SpoolFileBytes(char const *filename) = 0;
	virtual bool disconnect(bool commit_transaction, CondorError & errstack) = 0;
	virtual int  get_type() = 0;
protected:
	AbstractScheddQ() {}
};

enum {
	AbstractQ_TYPE_NONE = 0,
	AbstractQ_TYPE_SCHEDD_RPC = 1,
	AbstractQ_TYPE_SIM,
	AbstractQ_TYPE_FILE,
};

class ActualScheddQ : public AbstractScheddQ {
public:
	ActualScheddQ() : qmgr(NULL) {}
	virtual ~ActualScheddQ();
	virtual int get_NewCluster();
	virtual int get_NewProc(int cluster_id);
	virtual int destroy_Cluster(int cluster_id, const char *reason = NULL);
	virtual int set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags=0 );
	virtual int set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags = 0 );
	virtual int send_SpoolFileIfNeeded(ClassAd& ad);
	virtual int send_SpoolFile(char const *filename);
	virtual int send_SpoolFileBytes(char const *filename);
	virtual bool disconnect(bool commit_transaction, CondorError & errstack);
	virtual int  get_type() { return AbstractQ_TYPE_SCHEDD_RPC; }

	bool Connect(DCSchedd & MySchedd, CondorError & errstack);
private:
	Qmgr_connection * qmgr;
};

class SimScheddQ : public AbstractScheddQ {
public:
	SimScheddQ(int starting_cluster=0);
	virtual ~SimScheddQ();
	virtual int get_NewCluster();
	virtual int get_NewProc(int cluster_id);
	virtual int destroy_Cluster(int cluster_id, const char *reason = NULL);
	virtual int set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags=0 );
	virtual int set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags = 0 );
	virtual int send_SpoolFileIfNeeded(ClassAd& ad);
	virtual int send_SpoolFile(char const *filename);
	virtual int send_SpoolFileBytes(char const *filename);
	virtual bool disconnect(bool commit_transaction, CondorError & errstack);
	virtual int  get_type() { return AbstractQ_TYPE_SIM; }

	bool Connect(FILE* fp, bool close_on_disconnect);
private:
	int cluster;
	int proc;
	bool close_file_on_disconnect;
	FILE * fp;
};


#endif // _SUBMIT_INTERNAL_H

