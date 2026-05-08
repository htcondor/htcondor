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

#endif // EXPAND_GLOBS

// functions for handling the queue statement
int queue_connect(SubmitHash &submit_hash);


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

