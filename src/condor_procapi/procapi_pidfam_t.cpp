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

/////////////////////////////test5/////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int getPidFamily_test(bool verbose) {
 
  int success = 1;
  int status;
  PidEnvID penvid;
 
  piPTR pi = NULL;

  pidenvid_init(&penvid);

  if(verbose){
  printf ( "\n..................................\n" );
  printf ( "This is a test of getPidFamily().  It forks off a tree of\n" );
  printf ( "processes and checks their pids against those returned by\n");
  printf ( "getPidFamily\n");
  }

  
  // this test used to just fork off 3 processes depth first now it uses
  // fork tree
  int num_nodes = get_num_nodes(PID_FAMILY_DEPTH, PID_FAMILY_BREADTH);

  PID_ENTRY* pids = fork_tree(PID_FAMILY_DEPTH, PID_FAMILY_BREADTH, 0, verbose);
     
  std::vector<pid_t> pidf;
  ProcAPI::getPidFamily( pids[0].pid, &penvid, pidf, status );
  
  if(verbose){
    printf ( "Result of getPidFamily...the descendants are:\n" );
    for ( int i=0 ; pidf[i] != 0 ; i++ ) {
      printf ( " %d ", pidf[i] );
    }
    
  
    printf("\n");
    printf ("Pid's as a result of forking\n");
    for(int i = 0; i < num_nodes; i++){
      printf(" %d ", pids[i].pid);
    }
    printf("\n");

  }
  
  
  for(int i = 0; i < num_nodes; i++){
    bool found = false;
    for(int j = 0; pidf[j] != 0; j++){
      if( pids[i].pid == pidf[j]){
	found = true;
	break;
      }
    }
    if(!found){
      printf("Error:\n");
      printf("Process %d was created but not found again with getPidFamily\n", pids[i].pid);
      success = -1;
    }
  }

  for(int i = 0; pidf[i] != 0 ; i++){
    bool found = false;
    for(int j = 0; j < num_nodes; j++){
      if( pids[j].pid == pidf[i]){
	found = true;
	break;
      }
    }
    if(!found){
      printf("Error:\n");
      printf("Process %d was not created but was found with getPidFamily\n", pidf[i]);
      success = -1;
    }
  }
 
  if(verbose){
    printf ( "\n" );  
  }
  end_tree(pids, num_nodes);
  
  return success;
}
