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

#include "submit_protocol.h"

#define PLUS_ATTRIBS_IN_CLUSTER_AD 1

// uncomment this to get (broken) legacy behavior that attributes in
// SUBMIT_ATTRS/SUBMIT_EXPRS behave as if they were statements in your submit file
//#define SUBMIT_ATTRS_IS_ALSO_CONDOR_PARAM 1

#ifndef EXPAND_GLOBS_WARN_EMPTY
// functions in submit_glob.cpp
#define EXPAND_GLOBS_WARN_EMPTY (1<<0)
#define EXPAND_GLOBS_FAIL_EMPTY (1<<1)
#define EXPAND_GLOBS_ALLOW_DUPS (1<<2)
#define EXPAND_GLOBS_WARN_DUPS  (1<<3)
#define EXPAND_GLOBS_TO_DIRS    (1<<4) // when you want dirs only
#define EXPAND_GLOBS_TO_FILES   (1<<5) // when you want files only

int submit_expand_globs(StringList &items, int options, std::string & errmsg);
#endif // EXPAND_GLOBS

// functions for handling the queue statement
int queue_connect();

int queue_item(int num, const std::vector<std::string> & vars, char * item, int item_index, int options, const char * delims, const char * ws);
// option flags for queue_item.
#define QUEUE_OPT_WARN_EMPTY_FIELDS (1<<0)
#define QUEUE_OPT_FAIL_EMPTY_FIELDS (1<<1)

class SimScheddQ : public AbstractScheddQ {
public:
	SimScheddQ(int starting_cluster=0);
	virtual ~SimScheddQ();
	virtual int get_NewCluster(CondorError & errstack);
	virtual int get_NewProc(int cluster_id);
	virtual int get_Capabilities(ClassAd& reply);
	virtual int get_ExtendedHelp(std::string &content);
	virtual int destroy_Cluster(int cluster_id, const char *reason = NULL);
	virtual int set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags=0 );
	virtual int set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags = 0 );
	virtual int send_SpoolFile(char const *filename);
	virtual int send_SpoolFileBytes(char const *filename);
	virtual bool disconnect(bool commit_transaction, CondorError & errstack);
	virtual int  get_type() { return AbstractQ_TYPE_SIM; }
	virtual bool has_late_materialize(int &ver) { ver = 2; return true; }
	virtual bool allows_late_materialize() { return true; }
	virtual bool has_extended_submit_commands(ClassAd &cmds);
	virtual bool has_extended_help(std::string & filename); // helpfile for extended submit commands
	virtual bool has_send_jobset(int & /*ver*/) { return true; }
	virtual int set_Factory(int cluster, int qnum, const char * filename, const char * text);
	virtual int send_Itemdata(int cluster, SubmitForeachArgs & o);
	virtual int send_Jobset(int cluster, const ClassAd * jobset_ad);

	// set a file that send_Itemdata should "echo" items into. If this file is not set
	// items are sent but not echoed.
	bool echo_Itemdata(const char * filename);

	bool Connect(FILE* fp, bool close_on_disconnect, bool log_all);
private:
	int cluster;
	int proc;
	bool close_file_on_disconnect;
	bool log_all_communication;
	std::string echo_itemdata_filepath;
	FILE * fp;
};


// global struct that we use to keep track of where we are so we
// can give useful error messages.
enum {
	PHASE_INIT=0,       // before we begin reading from the submit file
	PHASE_READ_SUBMIT,  // while reading the submit file, and not on a Queue line
	PHASE_DASH_APPEND,  // while processing -a arguments (happens when we see the Queue line)
	PHASE_QUEUE,        // while processing the Queue line from a submit file
	PHASE_QUEUE_ARG,    // while processing the -queue argument
	PHASE_COMMIT,       // after processing the submit file/arguments
};
struct SubmitErrContext {
	int phase;          // one of PHASE_xxx enum
	int step;           // set to Step loop variable during queue iteration
	int item_index;     // set to itemIndex/Row loop variable during queue iteration
	const char * macro_name; // set to macro name during submit hashtable lookup/expansion
	const char * raw_macro_val; // set to raw macro value during submit hashtable expansion
};
extern struct SubmitErrContext  ErrContext;

int write_factory_file(const char * filename, const void* data, int cb, mode_t access);

// used by refactoring of main submit loop.

// convert the Foreach arguments to a from <file> type of argument, and create the file
// if the foreach mode is foreach_not, then this function does nothing.
//
int convert_to_foreach_file(SubmitHash & hash, SubmitForeachArgs & o, int ClusterId, bool spill_items);
int SendClusterAd (ClassAd * ad);


#endif // _SUBMIT_INTERNAL_H

