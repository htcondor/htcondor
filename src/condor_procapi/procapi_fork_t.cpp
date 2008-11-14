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

#include "procapi_t.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
//#include <errno.h>
// forks off a binary tree of processes of a specified depth 
// Future - at some point the pids should be piped up the tree and stored in 
// a pid map returned from the function  

// fork_tree and recursive_fork are different functions to allow some easy 
// change this way it is a simple matter to make the head node either the 
// calling process or a new process forked form the calling process 

int TOTAL_DEPTH;
int TREE_BREADTH_CHANGE;

// any process calling do_work will allocate some memory access it and spin 
// for a while
void do_work(int k, bool verbose){
  char** memory;

  // allocate k kilobytes
  memory = new char*[k];
  for(int i = 0; i < k; i++){
    memory[i] = new char[1024];
  }
  if(verbose){
    printf("Process %d done allocating %d k of memory\n", getpid(), k);
  }
  //access it all 
  for (int i = 0; i < k; i++){
    memory[i][0] = 'x';
  }
  if(verbose){
    printf("Process %d done touching all pages\n", getpid());
  }
  
  for(int i = 0; i < BUSY_WORK_TIME; i++)
    ;
  if(verbose){
    printf("Process %d done doing busy work\n", getpid());
  }
  // wait a while for the main process to get information on me 
  //sleep(TREE_SLEEP_TIME);

}

PID_ENTRY* recursive_fork(int depth, int breadth, bool verbose){
  
  if(depth <= 0){
    // if this node is a leaf return just its own information
    PID_ENTRY* arr = new PID_ENTRY[1];
    arr[0].pid = getpid();
    arr[0].ppid = getppid();
    arr[0].depth = 1;
    arr[0].breadth = breadth;
    return arr;
  }

  
  // create the file descriptor arrays for the pipe
  // we'll need one pipe for each child
  int** fds;
  fds = new int*[breadth];
  for(int i = 0; i < breadth; i++){
    fds[i] = new int[2]; 
  }
 
  // we are really at the depth of the calling process hence depth + 1
  PID_ENTRY* pid_arr = new PID_ENTRY[get_num_nodes(depth + 1 , breadth)];

  for (int i = 0; i < breadth; i++){
    // create the pipe
    
    pipe(fds[i]);
    pid_t child = fork();
    if(child == 0){// in child
      
      
      
      if(verbose){
	printf("Process %d is the #%d child of %d at depth %d\n", getpid(), i, 
	       getppid(),  TOTAL_DEPTH - depth);
      }
      
      // recurse 
      PID_ENTRY* child_arr = recursive_fork(depth - 1, breadth + TREE_BREADTH_CHANGE, verbose);

      // do the work before the passing our array to the parent so when the 
      // array gets all the way back to the calling function all the work is
      // done
      do_work(MEMFACTOR*1024, verbose);

      // write the array of all my descendent info back to my parent 
      int num_nodes = get_num_nodes(depth , breadth);
      child_arr[0].pid = getpid();
      child_arr[0].ppid = getppid();
      child_arr[0].depth = TOTAL_DEPTH - depth;
      child_arr[0].subtree_depth = depth;
      child_arr[0].breadth = i;
      
      // close read end of pipe
      close(fds[i][0]);
      write(fds[i][1], child_arr, num_nodes * sizeof(PID_ENTRY));
      delete[] child_arr;

      
      // stop myself, parent will restart me in time
      kill(getpid(), SIGSTOP);
      // I'll wait for my children to exit before I do
      while(wait(0) != -1)
	;
      if(verbose){
	printf("exiting process %d\n", getpid());
      }
      exit(0);
    }
    //in parent
    if(child == -1){
      perror("Error forking child");
      // Seperate errors?
      if(errno == EAGAIN){
	printf("EAGAIN\n");
      }
      else if(errno == ENOMEM){
	printf("ENOMEM\n");
      }     
    }
    
    // in parent
  }

  // leave space at the front of the array for myself
  PID_ENTRY p;
  p.pid = getpid();
  p.ppid = getppid();
  
  
  pid_arr[0] = p;
  
  // add the information for all of my descendents to the array
  for (int i = 0; i < breadth; i++){
    //we are really at the depth of the calling process
    int num_nodes = (get_num_nodes(depth + 1 , breadth)-1) / breadth;
    
    PID_ENTRY temp_arr[num_nodes ];
    close(fds[i][1]);
    read(fds[i][0], temp_arr, num_nodes * sizeof(PID_ENTRY));
    
    int start = 1 + i*num_nodes;
    int k = 0;

    // copy the information over 
    for(int j = start; j < start + num_nodes; j++){
      pid_arr[j] = temp_arr[k];
      k++;
    } 
    
  }
  
  return pid_arr;
}

// Not worth using any more
/*
PID_ENTRY* fork_tree (int depth, int breadth, int tree_breadth_change, bool verbose){
 
  TREE_BREADTH_CHANGE = tree_breadth_change;
  TOTAL_DEPTH = depth;
  if(verbose){
    printf("Process %d is the head node at depth 0\n", getpid());
  }
  PID_ENTRY* p = recursive_fork(depth - 1, breadth, verbose); 
  return p;
 
}
*/



PID_ENTRY* fork_tree(int depth, int breadth, int tree_breadth_change, bool verbose){
  TREE_BREADTH_CHANGE = tree_breadth_change;
  TOTAL_DEPTH = depth;

  int fds[2];

  pipe(fds);
  pid_t child = fork();
  if(child == 0){//in child
    if(verbose){
      printf("Process %d is the head node at depth 0\n", getpid());
    }

    int num_nodes = get_num_nodes(depth, breadth); 
    PID_ENTRY* p = recursive_fork(depth - 1, breadth, verbose);

    //make this process a little less boring
    do_work(MEMFACTOR*1024, verbose);
    p[0].pid = getpid();
    p[0].ppid = getppid();
    p[0].depth = TOTAL_DEPTH - depth;
    p[0].subtree_depth = depth;
    p[0].breadth = 0;
    close(fds[0]);
    write(fds[1], p, num_nodes*(sizeof(PID_ENTRY)));
    delete[] p;

    // stop myself before exiting
    kill(getpid(), SIGSTOP);

    // I'll wait for my children to exit before I do
    while(wait(0) != -1)
      ;
    if(verbose){
      printf("exiting process %d\n", getpid());
    }
    exit(0);
  }
  // in parent
  int num_nodes = get_num_nodes(depth, breadth);
  PID_ENTRY* arr = new PID_ENTRY[num_nodes];
  close(fds[1]);
  read(fds[0], arr, num_nodes*(sizeof(PID_ENTRY)));
  
  for(int i = 0; i < num_nodes; i++){
    printf("pid %d ppid %d depth %d breadth %d subtree_depth %d\n", arr[i].pid, arr[i].ppid, arr[i].depth, arr[i].breadth, arr[i].subtree_depth);
  }

  return arr;
}

void end_tree(PID_ENTRY* pids, int num_pids){

  sleep(10);  /* Ensure all children must be SIGSTOPped before we SIGCONT them */

  for(int i = 0; i < num_pids; i++){
    kill(pids[i].pid, SIGCONT);
  }
  //while(wait(0) != -1)
  //  ;
  wait(0);
  delete[] pids;

}


int get_approx_mem(int depth, int breadth){

  if (depth <= 0 || breadth <= 0){
    return 0;
  }
  int mem;
  // head node
  mem = 0;
  // the amount of memory allocated in a group of breadth nodes
  int mem_per_group = 0;

  for(int i = 1; i <= breadth; i++){
    mem_per_group += MEMFACTOR;
  }
  
  //header node
  mem = MEMFACTOR;
  for (int i = 1; i < depth; i++){
    int num_nodes = (int)pow((double)breadth, (double)i);
    mem += (num_nodes / breadth) * mem_per_group;
  }
  
  return mem;

}

int get_num_nodes(int depth, int breadth){

  if (depth <= 0 || breadth <= 0){
    return 0;
  }
  int num_nodes = 0;
  for(int i = 0; i < depth; i++){
    num_nodes += (int)pow((double)breadth, (double)i);
  }
  return num_nodes;
}
