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

///////////////////////////////test7//////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int getMemInfo_test(bool verbose) {
  
  if(verbose){
  printf("\n\nThis is a quick test of getMemInfo.  It basically gets the \n");
  printf("info, allocates a portion of the free memory and then gets it \n");
  printf("again, then deallocates the memory and checks the info again\n\n");
  }

  pid_t child = fork();
  if(child == 0){ // in child

    int success = 1;
    int* expected_free_mem = new int[3];
    int* total_mem = new int[3];
    int* free_mem = new int[3];
    char* junk;
    
    int temp;
    
    // do this before the fork so the child has acess to the initial mem info
    if (ProcAPI::getMemInfo(total_mem[0], free_mem[0]) < 0){
      printf("Error getting memory information\n");
      success = -1;
    }
    if(verbose){
      printf("Total memory %d  Free memory %d\n", total_mem[0], free_mem[0]);
    }
    int total_free = free_mem[0];
    
    temp = (int)(total_free * MEM_INFO_ALLOC1);
    // allocate some significant portion of memory
    if(verbose){
      printf("allocating %d kbytes\n", temp);
    }
    junk = new char[temp*1024];
    
    if(junk == NULL){
      printf("Error unable to allocate available memory\n");
      success = -1;
    }
    memset(junk, 0, temp*1024);

    expected_free_mem[1] = total_free - temp;
    
    if (ProcAPI::getMemInfo(total_mem[1], free_mem[1]) < 0){
      printf("Error getting memory information\n");
      success = -1;
    }
    if(verbose){
      printf("Total memory %d  Free memory %d\n", total_mem[1], free_mem[1]);
    }
    if(verbose){
      printf("expected free mem %d\n",expected_free_mem[1]);
    }
  
    
    if(verbose){
      printf("freeing %d and allocating %d kbytes\n", temp,(int)(total_free * MEM_INFO_ALLOC2));
    }
    delete[] junk;
    temp = (int)(total_free * MEM_INFO_ALLOC2);
    
    junk = new char[temp*1024];
    if(junk == NULL){
      printf("Error unable to allocate available memory\n");
      success = -1;
    }
    
    memset(junk, 0, temp*1024);
    expected_free_mem[2] = total_free - temp;
    
    
    if (ProcAPI::getMemInfo(total_mem[2], free_mem[2]) < 0){
      printf("Error getting memory information\n");
      success = -1;
    }
    if(verbose){
      printf("Total memory %d  Free memory %d\n", total_mem[2], free_mem[2]);
    }

    delete[] junk;

    
    // Check the memory values
  
    // the values of totalmem should be the same throughout the tests
    if(total_mem[0] != total_mem[1] || total_mem[1] != total_mem[2]){
      printf("Error different values for total system memory\n");
      success = -1;
    }
    if(verbose){
      printf("expected free mem %d\n",expected_free_mem[2]);
    }
    
    // make sure the amount of free memory is about what it should be
    temp = expected_free_mem[1];
    if(free_mem[1] < temp - temp * MEM_INFO_ERR_MARGIN){
      printf("free mem 1 %d not close enough to expected value %d\n", free_mem[1], temp);
      success = -1;
    }
    if(free_mem[1] > temp + temp * MEM_INFO_ERR_MARGIN){
      printf("free mem 1 %d not close enough to expected value %d\n", free_mem[1], temp);
      success = -1;
    }
    
    temp = expected_free_mem[2];
    if(free_mem[2] < temp - temp * MEM_INFO_ERR_MARGIN){
      printf("free mem 2 %d not close enough to expected value %d\n", free_mem[2], temp);
      success = -1;
    }
    if(free_mem[2] > temp + temp * MEM_INFO_ERR_MARGIN){
      printf("free mem 2 %d not close enough to expected value %d\n", free_mem[2], temp);
      success = -1;
    }
    
    delete[] expected_free_mem;
    delete[] free_mem;
    delete[] total_mem;
     
    if(verbose){
      printf("exiting process %d\n", getpid());
    }

    if(success == -1){
      exit(1);
    }
    else{
      exit(0);
    }
  }

  // in parent
  int status = 10;
  wait(&status);
  if(status == 0){
    return 1;
  }
  return -1;
}
