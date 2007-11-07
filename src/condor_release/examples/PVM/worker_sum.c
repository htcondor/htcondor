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

#include "pvm3.h"
#include "sum.h"
#include <unistd.h>
#include <stdio.h>


/* worker first receives a round number, it does summation for this many 
 * rounds, and then exit. */
main()
{
     int tid, buf_id,i;
     int parent = pvm_parent();
     int n;
     int num;
     int result = 0;
     int round = 0;
     
     printf("Worker start running... \n");
     fflush(stdout);

     while (round != 1) 
     {
	  /* reset the result */
	  result = 0;

	  /* receive a round number */
	  buf_id = pvm_recv(parent, ROUND_TAG);
	  pvm_bufinfo(buf_id, (int *)0 , (int *)0, &tid);
	  pvm_upkint(&round, 1, 1);
          
	  printf("worker: round = %d\n", round);
	  fflush(stdout);

	  /* receive n,  the number of integers to add */
	  buf_id = pvm_recv(parent, NUM_NUM_TAG);
	  pvm_bufinfo(buf_id, (int *)0 , (int *)0, &tid);
	  pvm_upkint(&n, 1, 1);
	  /*
	  printf("worker: n = %d\n", n);
	  fflush(stdout);
	  */
	  /* receive num, the start nubmer */
	  buf_id = pvm_recv(parent, START_NUM_TAG);
	  pvm_bufinfo(buf_id, (int *)0 , (int *)0, &tid);
	  pvm_upkint(&num, 1, 1);
	  /*	  
	  printf("worker: start = %d\n", num);
	  fflush(stdout);
	  */
	  /* add num, ..., (num+n-1) */
	  for ( i = 0; i < n; i++) {
	    /*	       sleep(1); */
	       result += num + i;
	  }
	  /* sleep(1); */


	  /* send the result with the round number */
	  pvm_initsend(PvmDataDefault); 
	  pvm_pkint(&round, 1, 1);
	  pvm_pkint(&result, 1, 1);
	  pvm_send(parent, RESULT_TAG);
	  /*
	  printf("worker: result = %d\n",result);
	  fflush(stdout);
	  */
     }
     printf("worker before exit\n");
     /*     fflush(stdout); */
     pvm_exit();
     printf("worker after exit\n"); 
     exit(0);
}




