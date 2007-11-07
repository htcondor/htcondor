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

#include "condor_common.h"
#include "condor_pidenvid.h"
#include "procapi_t.h"

/////////////////////////////test2/////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int getFamilyInfo_test(bool verbose) {

  int success = 1;
  piPTR pi = NULL;
  pid_t child;
  int status;

  PidEnvID penvid;

  pidenvid_init(&penvid);

  if(verbose){
  printf ( "\n....................................\n" );
  printf ( "This test forks off a tree of processess and then uses\n");
  printf ( "getFamilyInfo to get information on each process in the\n");
  printf ( "tree.  It then checks the family data against what is\n");
  printf ( "expected and against its parents family\n");
  }
  // this method used to explicitly depth fork some processes it now uses 
  // fork_tree to do achieve the same result
  
  
  PID_ENTRY* pids = fork_tree(FAMILY_INFO_DEPTH, FAMILY_INFO_BREADTH, 0, verbose);

  int num_nodes = get_num_nodes(FAMILY_INFO_DEPTH, FAMILY_INFO_BREADTH);

  // get the family info and test it for each node
  for(int i = 0; i < num_nodes; i++){
    int pid = pids[i].pid;
    int ppid = pids[i].ppid;
    piPTR pi = NULL;
    piPTR ppi = NULL;

    if(ProcAPI::getFamilyInfo( pid, pi, &penvid, status) == PROCAPI_FAILURE) {
      printf("Error process %d:\n", pid);
      printf("unable to retrieve process %d information with getFamilyInfo\n", pid);
      success = -1;
    }
    
    if(verbose){
      printf("Family Info for process %d, child of %d of depth %d:\n\n", pid, ppid, pids[i].depth);
      ProcAPI::printProcInfo(pi);
    }


    //check the pids
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

    printf("subtree depth%d\n",pids[i].subtree_depth );
    //test the rss
    int rss = 1024 * get_approx_mem(pids[i].subtree_depth, FAMILY_INFO_BREADTH);
    if(pi->rssize != 0 &&   /* Maybe process done, entirely paged out */
       pi->rssize < rss){
      printf("Error process %d:\n", pid);
      printf("rssize as returned by getFamilyInfo %d is less than was allocated %d\n", pi->rssize, rss);
      success = -1;
    }
    if(pi->rssize > rss + rss*FAMILY_INFO_RSS_MARGIN){
      printf("Error process %d:\n", pid);
      printf("rssize as returned by getFamilyInfo %d is much larger than was allocated %d\n", pi->rssize, rss);
      success = -1;
    }

    // the image size should be greater than the resident set size
    if(pi->rssize > pi->imgsize){
      printf("Error process %d:\n", pid);
      printf("imgsize %d as returned by getFamilyInfo is less than rssize %d\n", pi->imgsize, pi->rssize);
      success = -1;
    }
    

    // now get the parents info and do some more checks
    if(ProcAPI::getFamilyInfo(ppid, ppi, &penvid, status) == PROCAPI_FAILURE) {
      printf("Error process %d:\n", pid);
      printf("Unable to retrieve parent process %d information with getFamilyInfo\n", ppid);
      success = -1;
    }
    
    // the parents family rssize should be greater than the childs familys
    if(pi->rssize > ppi->rssize){
      printf("Error process %d:\n", pid);
      printf("parent family rssize %d is smaller than childs %d\n", ppi->rssize, pi->rssize); 
    }
    //same for image
    if(pi->imgsize > ppi->imgsize){
      printf("Error process %d:\n", pid);
      printf("parent family imgsize %d is smaller than childs %d\n", ppi->imgsize, pi->imgsize); 
    }
    //and age
    if(pi->age > ppi->age){
      printf("Error process %d:\n", pid);
      printf("parent family age %d is smaller than childs %d\n", ppi->age, pi->age); 
    }
    //and user and system times
    if(pi->user_time > ppi->user_time){
      printf("Error process %d:\n", pid);
      printf("parent user time %d is smaller than childs %d\n", ppi->user_time, pi->user_time); 
    }
    if(pi->sys_time > ppi->sys_time){
      printf("Error process %d:\n", pid);
      printf("parent sys time %d is smaller than childs %d\n", ppi->sys_time, pi->sys_time); 
    }
    // and faults
    if(pi->minfault > ppi->minfault){
      printf("Error process %d:\n", pid);
      printf("parent min faults %d is smaller than childs %d\n", ppi->minfault,pi->minfault); 
    }
    if(pi->majfault > ppi->minfault){
      printf("Error process %d:\n", pid);
      printf("parent maj faults %d is smaller than childs %d\n", ppi->majfault,pi->majfault); 
    }

    delete pi;
    delete ppi;

  }
 
  end_tree(pids, num_nodes);
    
  return success;
    
}







