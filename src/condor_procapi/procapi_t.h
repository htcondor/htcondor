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

#ifndef  PROCAPI_T_H
#define  PROCAPI_T_H
#include "condor_common.h"
#include "procapi.h"
#include "condor_pidenvid.h"

// This is the Procapi Tester 
// currently there are seven different tests.  Five of them test the five 
// public functions that make up procapi.  Two others,  permissions and 
//cpuusage test more specific things 

//DEFINE SOME CONSTANTS 

// number of times to run each test
const int PROC_INFO_NUMTIMES = 1;
const int PERM_NUMTIMES = 1;
const int SET_INFO_NUMTIMES = 1;
const int CPU_USAGE_NUMTIMES = 1;
const int MEM_INFO_NUMTIMES = 1;


// these constants pertain to the fork tree///////////////
// the number of megs you want a child process to allocate
const int MEMFACTOR = 3;
// currently these constants are used by do_work for all calls to fork_tree
// might be useful to make them specefiable on a per use basis
const int BUSY_WORK_TIME = 100000000;


//constants for getproc_t //////////////////////////
// the depth and breadth to make the fork tree
const int PROC_INFO_BREADTH = 3;
const int PROC_INFO_DEPTH = 3;
// the leniency to give the rss value returned compared to the expected value
// (50%)
const float PROC_INFO_RSS_MARGIN = .50f;

//constants for set_t///////////////////////////////////
const int SET_INFO_DEPTH = 3;
const int SET_INFO_BREADTH = 2;
// the margin of error allowed (%)when cheking values in the pi struct for 
// family vs. set info
const float SET_INFO_RSS_MARGIN = .35f;
const float SET_INFO_IMG_MARGIN = .1f;
// margin in number of faults
const float SET_INFO_MIN_MARGIN = .2f;
const float SET_INFO_MAJ_MARGIN = .2f;

// run_tests is called by the main function and runs each of the seven tests
// a number of times specified above.  It returns 1 if all the tests completed
// successfully,  -1 if at least one failed  
int run_tests(void);


// tests getProcInfo 
int getProcInfo_test(bool verbose);
// tests permission can I see the proc info for init
int permission_test(bool verbose);
// tests getProcSetInfo against getFamily info
int getProcSetInfo_test(bool verbose);
// tests cpuusage via getProcInfo
int cpuusage_test(bool verbose);

// takes an argument of the job name you want to run and periodically prints 
// information about it performs no checks to see if the data is correct
void test_monitor(char *jobname);

// hold information about a node in a fork tree
typedef struct _PID_MAP_ENTRY{
  // the pid of the process at this node
  pid_t pid;
  // the pid of my parent
  pid_t ppid;
  // the breadth this node is at...  0 is the left most child etc...
  int breadth;
  // the depth this node is at in the tree 0 is the root and so on
  int depth;
  // the depth of the subtree with this node as the root
  int subtree_depth;
  

} PID_ENTRY;


// fork_tree forks off process in a tree formation of a specified depth and 
// breadth.  The PID_ENTRY array returned has one entry for each node in the 
// tree.  After the fork tree is no longer in use end_tree must be called with 
// the PID_ENTRY * returned and the number of nodes or the forked processess
// will not exit(0);

// index 0 in the PID_ENTRY returned will always be the head node   
PID_ENTRY* fork_tree(int depth, int breadth, int tree_breadth_change, bool verbose);


// used by fork tree to make the forked processes do something interesting
// allocates k kilobytes and spins for an amount of time specified above by
// a constant
void do_work(int k, bool verbose);

// does the majority of the work for fork_tree
PID_ENTRY* recursive_fork(int depth, int breadth, bool verbose);

// cleans up the tree when it is no longer needed must be used with fork tree
void end_tree(PID_ENTRY* pids, int num_pids);


// returns the amount of memory that should have been allocated in a fork_tree
// of given depth and breadth (uses mem factor)
int get_approx_mem(int depth, int breadth);

//returns the number of nodes in a tree of specified breadth and depth
int get_num_nodes(int depth, int breadth);

#endif




