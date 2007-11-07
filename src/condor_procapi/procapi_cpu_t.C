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
#include "procapi_t.h"

/////////////////////////////test6/////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int cpuusage_test(bool verbose) {


	int status;
  int success = 1;

  piPTR pi_s = NULL;
  piPTR pi_r = NULL;

  pid_t child;

  if(verbose){
  printf ( "\n..................................\n" );
  printf ( "Here's the test of cpu usage monitoring over time.  I'll\n");
  printf ( "fork off a process.  The parent will then start and stop it\n");
  printf ( "(5s intervals) making sure cpuusage is doing the right thing.\n");
  printf ( "This test runs for one minute.\n");
  }

  child = fork();
  if ( !child ) { // in child
    // the child will run in an infinite loop using cpu time 
    // it will be SIGSTOPed, SIGCONTed, and SIGTERMed by the parent.
    kill(getpid(), SIGSTOP);
    while(1){
     
    }
      
    
  }
  // in parent
  if(child < 0){
    perror("Error forking");
  }
  
  
  if (ProcAPI::getProcInfo ( child, pi_s, status ) == PROCAPI_FAILURE){
    printf("Error:\n");
    printf("Unable to getProcInfo for process %d\n", child);
    success = -1;
  } 
 
  
  for(int i = 0; i < 3; i++){
    
    // start the child
    kill(child, SIGCONT);
    if(verbose){
      printf("Child is running\n");
    }
    
    // get the proc info after 5 seconds of running
    sleep(5);
    if (ProcAPI::getProcInfo ( child, pi_r, status ) == PROCAPI_FAILURE){
      printf("Error:\n");
      printf("Unable to getProcInfo for process %d\n", child);
      success = -1;
    } 
    if(verbose){
      printf ("\n");
      ProcAPI::printProcInfo ( pi_r );
    }
    if(pi_r->pid != child){
      printf("Error:\n");
      printf("process pid %d does not match that returned in procinfo %d\n", child, pi_r->pid);
      success = -1;
    }
    if(pi_r->ppid != getpid()){
      printf("Error:\n");
      printf("parent pid %d does not match that returned in procinfo %d\n", getpid(), pi_r->ppid);
      success = -1;
    }

    // stop the child process
    kill(child, SIGSTOP);
    if(verbose){
      printf("Child is stopped\n");
    }
    //get the proc info after five seconds of being stopped
    sleep(5);
    if (ProcAPI::getProcInfo ( child, pi_s, status ) == PROCAPI_FAILURE) {
      printf("Error:\n");
      printf("Unable to getProcInfo for process %d\n", child);
      success = -1;
    } 
    if(verbose){
      printf ("\n");
      ProcAPI::printProcInfo ( pi_s );
    }
    // do the pid's match
    if(pi_s->pid != child){
      printf("Error:\n");
      printf("process pid %d does not match that returned in procinfo %d\n", child, pi_s->pid);
      success = -1;
    }
    if(pi_s->ppid != getpid()){
      printf("Error:\n");
      printf("parent pid %d does not match that returned in procinfo %d\n", getpid(), pi_s->ppid);
      success = -1;
    }
    
    //now while child is still stopped do some checks on the info
    // the child should use more cpu time when running
    if(pi_s->cpuusage > pi_r->cpuusage){
      printf("Error:\n");
      printf("cpuusage of the process while stopped %f higher than while running %f\n", pi_s->cpuusage, pi_r->cpuusage);
      success = -1;
    }    
  }
  // end the short and relativly boring existance of the child process.
    // He'll be relieved trust me.  Would you like to just spin in a 
  // never ending loop.  Thought not.
  kill(child, SIGKILL);
  
  return success;
}
