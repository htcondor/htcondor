/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_pidenvid.h"
#include "procapi_t.h"

/////////////////////////////test1/////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int getProcInfo_test(bool verbose) {

  int success = 1;
  unsigned long approx_mem;
  int status;
  
  
  
  if(verbose){
  printf ( "\n..................................\n" );
  printf ( "This test forks off a tree of processes and then uses\n");
  printf ( "getProcInfo to get data about each of them.  It then\n");
  printf ( "checks some of this data against the values it expects\n");
  printf ( "to be there and checks other data relative to its parent\n");
  }
  

  int num_nodes = get_num_nodes(PROC_INFO_DEPTH, PROC_INFO_BREADTH);

  PID_ENTRY* pids = fork_tree(PROC_INFO_DEPTH, PROC_INFO_BREADTH, 0, verbose);

  // now check the getProcInfo for every node in the tree
  // main body of tests for this tester
  for(int i = 0; i < num_nodes; i++){
    
    piPTR pi = NULL;
    piPTR ppi = NULL;
    int pid = pids[i].pid;
    int ppid = pids[i].ppid;

    // check some of the data in the structure
    if(ProcAPI::getProcInfo( pid, pi, status) == PROCAPI_FAILURE){
      printf("Error process %d:\n", pid);
      printf("unable to retrieve process %d information with getProcInfo\n", pid);
      success = -1;
    }

    if(verbose){
      printf("Proc Info for process %d, child of %d of depth %d:\n\n", pid, ppid, pids[i].depth);
      ProcAPI::printProcInfo( pi );
    }

    // check the pid and ppid in pi against those given when the processes were
    // forked originally
    if(pi->pid != pid){
      printf("Error process %d:\n", pid);
      printf("node pid %d, incorrect as returned in procInfo %d\n", pid, pi->pid);
      success = -1;
    }   
    if(pi->ppid != ppid){
      printf("Error process %d:\n", pid);
      printf("node ppid %d, incorrect as returned in procInfo %d\n", ppid, pi->ppid);
      success = -1;
    }

    // test the rssize returned in pi against what was alloacted
    int rss = 1024 * get_approx_mem(1,1); //(one node)
    if(pi->rssize != 0 &&   /* Maybe process done, entirely paged out */
       pi->rssize < rss){
      printf("Error process %d:\n", pid);
      printf("rssize as returned by getProcInfo %d is less than was allocated %d\n", pi->rssize, rss);
      success = -1;
    }
    if(pi->rssize > rss + rss*PROC_INFO_RSS_MARGIN){
      printf("Error process %d:\n", pid);
      printf("rssize as returned by getProcInfo %d is much larger than was allocated %d\n", pi->rssize, rss);
      success = -1;
    }

    // the image size should be greater than the resident set size
    if(pi->rssize > pi->imgsize){
      printf("Error process %d:\n", pid);
      printf("imgsize %d as returned by getProcInfo is less than rssize %d\n", pi->imgsize, pi->rssize);
      success = -1;
    }

    // time in user mode + system mode should not be greater then age
    if((pi->user_time + pi->sys_time) > pi->age){
      printf("Error process %d:\n", pid);
      printf("time in user mode %d + sys mode %d is greater than total age %d\n", pi->user_time, pi->sys_time, pi->age);
      success = -1;
    }
    
    // now get the parents info and do some more checks
    if(ProcAPI::getProcInfo(ppid, ppi, status) == PROCAPI_FAILURE){
      printf("Error process %d:\n", pid);
      printf("Unable to retrieve parent process %d information with getProcInfo\n", ppid);
      success = -1;
    }
    
    // the parent must be older than the child
    if(ppi->age < pi->age){
      printf("Error process %d:\n", pid);
      printf("parent age as returned by getProcInfo %d is less than child age %d\n", ppi->age, pi->age);
      success = -1;
    }
    // the parent should have been created first also
    if(ppi->creation_time > pi->creation_time){
      printf("Error process %d:\n", pid);
      printf("parent creation time %d is after child creation time %d\n", ppi->creation_time, pi->creation_time );
      success = -1;
    }

    delete pi;
    delete ppi;
  }

 
  

  end_tree(pids, num_nodes);

  return success;
}








