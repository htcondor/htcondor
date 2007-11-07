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

/////////////////////////////test4/////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// Now instead of asking for a set of pids and calling 
// getProcSetInfo on it the test uses the shells pid and runs it on all of the
// processes owned by the shell.  It also uses getPidFamily to get all the 
// descendants of the shell
int getProcSetInfo_test(bool verbose) {
  
  int success = 1;
  int status;
  
  // get the pid of the process to be used as the head node this process
  pid_t head_pid =  getpid();
  pid_t* descendents; 
  int i;
  piPTR pi_s = NULL;
  piPTR pi_f = NULL;
  
  pid_t *pids;

  PidEnvID penvid;

  pidenvid_init(&penvid);

  if(verbose){
  printf( "\n..................................\n" );
  printf( "This is a test of getProcSetInfo(). \n");
  printf( "We fork off a tree of processess and put all of the pids\n");
  printf( "returned into an array of pids and call getProcSetInfo on\n");
  printf( "it.  The set is really the same as the family of the head\n");
  printf( "node so we get that family information and compare\n");
  }
  
  int num_nodes = get_num_nodes(SET_INFO_DEPTH, SET_INFO_BREADTH);
  PID_ENTRY* pid_ents = fork_tree(SET_INFO_DEPTH, SET_INFO_BREADTH, 0, verbose);

  
  pids = new pid_t[num_nodes];
  for(i = 0; i < num_nodes; i++){
    pids[i] = pid_ents[i].pid;
  }
  

  if(ProcAPI::getProcSetInfo ( pids, num_nodes, pi_s, status ) 
 							 	== PROCAPI_FAILURE)
  {
    printf("Error:\n");
    printf("Unable to getProcSetInfo\n");
    success = -1;
  }

  if(ProcAPI::getFamilyInfo( pid_ents[0].pid, pi_f, &penvid, status ) 
 					 			== PROCAPI_FAILURE)
  {
    printf("Error:\n");
    printf("Unable to getFamilyInfo\n");
    success = -1;
  }
  
  if(verbose){
    printf ( "\n The getProcSetInfo: \n" );
    ProcAPI::printProcInfo ( pi_s );
  }
  if(verbose){
    printf("\n The getFamilyInfo: \n"); 
    ProcAPI::printProcInfo ( pi_f );
  }

  
  // check the information from setInfo against that from familyInfo
  //  all the information should match with the exception of cpu usage and age

  // does the memory usage match? 
  int temp = pi_f->rssize;
  if(pi_s->rssize < temp - temp * SET_INFO_RSS_MARGIN||
     pi_s->rssize > temp + temp * SET_INFO_RSS_MARGIN){
    printf("Error:\n");
    printf("set info rss %d does not equal family info rss %d\n", pi_s->rssize, pi_f->rssize);
    success = -1;
  }

  temp = pi_f->imgsize;
  if(pi_s->imgsize < temp - temp * SET_INFO_IMG_MARGIN||
     pi_s->imgsize > temp + temp * SET_INFO_IMG_MARGIN){
    printf("Error:\n");
    printf("set info image %d does not equal family info image %d\n", pi_s->imgsize, pi_f->imgsize);
    success = -1;
  }
  
  // there process can't have taken away min faults in the time between 
  // retrieving data
  temp = pi_f->minfault;
  if(pi_f->minfault < pi_s->minfault) {
    printf("Error:\n");
    printf("set info minfault %d does not equal family info minfault %d\n", pi_s->minfault, pi_f->minfault);
    success = -1;
  }
  if(pi_s->minfault < temp - temp * SET_INFO_MIN_MARGIN) {
    printf("Error:\n");
    printf("set info minfault %d does not equal family info minfault %d\n", pi_s->minfault, pi_f->minfault);
    success = -1;
  }

  temp = pi_f->majfault;
  if(pi_f->majfault < pi_s->majfault) {
    printf("Error:\n");
    printf("set info majfault %d does not equal family info majfault %d\n", pi_s->majfault, pi_f->majfault);
    success = -1;
  }
  if(pi_s->majfault < temp - temp * SET_INFO_MAJ_MARGIN) {
    printf("Error:\n");
    printf("set info majfault %d does not equal family info majfault %d\n", pi_s->majfault, pi_f->majfault);
    success = -1;
  }
 
  // do the user and sys times match
  // since the set info is gptten first times can only go up
  if(pi_s->user_time > pi_f->user_time){
    printf("Error:\n");
    printf("set info user time %d does not equal family info user time %d\n", pi_s->user_time, pi_f->user_time);
    success = -1;
  }
  if(pi_s->sys_time > pi_f->sys_time){
    printf("Error:\n");
    printf("set info sys time %d does not equal family info sys time %d\n", pi_s->sys_time, pi_f->sys_time);
    success = -1;
  }
  if(pi_s->age > pi_f->age){
    printf("Error:\n");
    printf("set info age %d does not equal family info age %d\n", pi_s->age, pi_f->age);
    success = -1;
  }
    
  // since the process are stopped the cpu usage can only go down between 
  // getting set and family info  
  double temp_d = pi_f->cpuusage;
  if(pi_s->cpuusage < pi_f->cpuusage){
    printf("Error:\n");
    printf("set info cpuusage time %f does not equal family info cpuusage %f\n", pi_s->cpuusage, pi_f->cpuusage);
    success = -1;
  }

  delete pi_s;
  delete pi_f;
  delete [] pids;
  
  end_tree(pid_ents, num_nodes);
  
  return success;
}
