/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "procapi_t.h"

/////////////////////////////test5/////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int getPidFamily_test(bool verbose) {
 
  int success = 1;
 
  piPTR pi = NULL;

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
     
  pid_t *pidf = new pid_t[512];
  ProcAPI::getPidFamily( pids[0].pid, pidf );
  
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
 
  delete [] pidf;
  if(verbose){
    printf ( "\n" );  
  }
  end_tree(pids, num_nodes);
  
  return success;
}
