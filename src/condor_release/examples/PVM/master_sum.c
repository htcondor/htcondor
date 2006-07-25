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

/* 
 * master_sum.c
 * 
 * master program to perform parallel addition - takes a number n as input
 * and returns the result of the sum 0..(n-1).  Addition is performed parallely
 * by k tasks, where k is also taken as input.  The numbers 0..(n-1) are 
 * stored in an array, and each worker task adds a portion of the array, and
 * returns the sum to the master.  The Master adds these sums and prints final
 * sum.  
 *
 * To make the program fault-tolerant, the master has to monitor the
 * tasks that exited without sending the result back.  The master
 * creates some new tasks to do the work of those tasks that have exited. 
 */

/* 
 * input = in_sum     - stdin
 * output = out_sum   - stdout
 * error = err_sum    - stderr
 */

#include <sys/types.h> /* for getpid() */
#include <unistd.h>    /* for getpid() */

#include <stdio.h>
#include "pvm3.h"
#include "sum.h"

#define NOTIFY_NUM 5  /* number of items to notify */

#define HOSTDELETE 12
#define HOSTSUSPEND 13
#define HOSTRESUME 14
#define TASKEXIT 15
#define HOSTADD 16

#define MAX_TASKS 100

/* send the pertask and start number to the worker task i */
void send_data_to_worker(int i, int *tid, int *num, int pertask, 
			FILE *fp, int round)
{
     int status;
     int start_val;
     
     /* send the round number */
     pvm_initsend(PvmDataDefault); /* XDR format */
     pvm_pkint(&round, 1, 1);    /* number of numbers to add */
     status = pvm_send(tid[i], ROUND_TAG);

     pvm_initsend(PvmDataDefault); /* XDR format */
     pvm_pkint(&pertask, 1, 1);    /* number of numbers to add */
     status = pvm_send(tid[i], NUM_NUM_TAG);

     pvm_initsend(PvmDataDefault); /* XDR format */
     start_val = i * pertask;   /* initial number for this task */
     pvm_pkint(&start_val, 1, 1);     /* the initial number */
     status = pvm_send(tid[i], START_NUM_TAG);   

     pvm_initsend(PvmDataDefault); /* XDR format */
     pvm_send(tid[i], 1000);
}

/* 
 * to see if more hosts are needed 
 * 1 = yes; 0 = no 
 */
int need_more_hosts(int i)
{
     int nhost, narch;
     char *hosts="0";  /* any host in arch class 0 */
     struct pvmhostinfo *hostp = (struct pvmhostinfo *) 
	  calloc (1, sizeof(struct pvmhostinfo));

     /* get the current configuration */
     pvm_config(&nhost, &narch, &hostp);
     
     if (nhost > i)
	  return 0;
     else 
	  return 1;
}

/* 
 * Add a new host until success, assuming that request for AddHost
 * notification has already been sent 
 */
void add_a_host(FILE *fp)
{
     int done = 0;
     int buf_id;
     int success = 0;
     int tid;
     int msg_len, msg_tag, msg_src;
     char *hosts="0";  /* any host in arch class 0 */
     int infos[1];

     while (done != 1) {
	  /* 
	   * add one host - no specific machine named 
	   * add host will asynchronously, so we need
	   * to receive the notification before go on.
	   */
	  pvm_addhosts(&hosts,1 , infos);
	  
	  /* receive notification from anyone, with HostAdd tag */
	  buf_id = pvm_recv(-1, HOSTADD);
	  
	  if (buf_id < 0) {
	       done = 0;
	       continue;
	  }
	  done = 1;
     
	  pvm_bufinfo(buf_id, &msg_len , &msg_tag, &msg_src);
	  pvm_upkint(&tid, 1, 1);

	  pvm_notify(PvmHostDelete, HOSTDELETE, 1, &tid);

	  fprintf(fp, "Received HOSTADD: ");
	  fprintf(fp, "Host %x added from %x\n", tid, msg_src);
	  fflush(fp);
     }
}

/* 
 * Spawn a worker task until success.  
 * Return its tid, and the tid of its host. 
 */
void spawn_a_worker(int i, int* tid, int * host_tid, FILE *fp)
{
     int numt = 0;
     int status;

     while (numt == 0){
	  /* spawn a worker on a host belonging to arch class 0 */
	  numt = pvm_spawn ("worker_sum", NULL, PvmTaskArch, "0" ,1, &tid[i]);
	  fprintf(fp, "DEBUG : master spawned %d task tid[%d] = %x\n" , 
		  numt, i , tid[i]);
	  fflush(fp);
     
	  /* if the spawn is successful */
	  if (numt == 1)
	  {
	       /* notify when the task exits */
	       status = pvm_notify(PvmTaskExit, TASKEXIT, 1, &tid[i]);
	       
	       if (pvm_pstat(tid[i]) != PvmOk)
		    numt = 0;
	  }
	  
	  if (numt != 1)
	  {
	       fprintf(fp, "!! Failed to spawn task[%d]\n", i);
	       
	       if (tid[i] ==  PvmBadParam)
		    fprintf(fp, " PvmBadParam\n");
	       else if (tid[i] == PvmNoHost)
		    fprintf(fp, " PvmNoHost\n");
	       else if ( tid[i] == PvmNoFile)
		    fprintf(fp, " PvmNoFile\n");
	       else if ( tid[i] == PvmNoMem)
		    fprintf(fp, " PvmNoMem\n");
	       else if ( tid[i] == PvmSysErr)
		    fprintf(fp, " PvmSysErr\n");
	       else if ( tid[i] == PvmOutOfRes)
		    fprintf(fp, " PvmOutOfRes\n");
	       else
		     fprintf(fp, "Reason unknown %d\n", tid[i]);
	       
	       fflush(fp);
	       /* 
		* currently condor-pvm allows only one task running on 
		* a host
		*/
	       if (tid[i] == PvmNoHost) {
		    while (need_more_hosts(i) == 1)
			 add_a_host(fp);
	       }
	       else {
		/* avoid error-prone catchout cleanup on pvm_exit() */
			pvm_catchout(0);
		    pvm_exit();
		    exit(-1);
	       }
	  }
     }
}

main()
{
     int n;        /* will add <n> numbers n .. n-1 */
     int ntasks;   /* need <ntask> workers to do the addition. */
     int pertask;  /* numbers to add per task */

     int tid[MAX_TASKS];     
     int deltid[MAX_TASKS];  /* monitor these tids for deletion */
     
     int sum[MAX_TASKS];     /* hold the reported sum */
     int num[MAX_TASKS];     /* the initail numbers the workers should add */

     int host_tid[MAX_TASKS];  /* the tids of the host that the tasks *
				 * <0..ntasks> are running on          */
     
     int i, numt, nhost, narch, status;
     int result;
     int mytid;    /* task id of master */
     int mypid;    /* process id of master */
     int buf_id;   /* id of recv buffer */
     int msg_leg, msg_tag, msg_src, msg_len;
     int int_val;  

     int infos[MAX_TASKS];
     char * hosts[MAX_TASKS];
     struct pvmhostinfo *hostp =
		 (struct pvmhostinfo *) calloc (MAX_TASKS, sizeof(struct pvmhostinfo));

     FILE *fp;
     char outfile_name[100];

     char *codes[NOTIFY_NUM] = {"HostDelete", "HostSuspend", "HostResume", 
		       "TaskExit", "HostAdd"};
     
     int count;   /* the number of times that while loops */
     int round_val;
     int correct = 0;
     int wrong = 0;

     mypid = getpid();

     sprintf(outfile_name, "out_sum.%d", mypid);
     fp = fopen(outfile_name, "w"); 

     /* redirect all children tasks' stdout to fp */
     pvm_catchout(stderr);  

     /* will add <n> numbers 0..(n-1) */
     fprintf(fp, "How many numbers? ");
     fflush(fp);
     scanf("%d", &n);
     fprintf(fp, "%d\n", n);
     fflush(fp);

     /* will spawn ntasks workers to perform addition */
     fprintf(fp, "How many tasks? ");
     fflush(fp);
     scanf("%d", &ntasks);
     fprintf(fp, "%d\n", ntasks);
     fflush(fp);

     /* will iterate count loops */
     fprintf(fp, "How many loops? ");
     fflush(fp);
     scanf("%d", &count);
     fprintf(fp, "%d\n", count);
     fflush(fp);

     /* set the hosts to be in arch class 0 */
     for (i = 0; i< ntasks; i++) 
	  hosts[i] = "0";

     /* numbers to be added by each worker */
     pertask = n/ntasks;

     /* get the master's TID */
     mytid = pvm_mytid();
     fprintf(fp, "mytid = %x; mypid = %d\n", mytid, mypid);

     /* get the current configuration */
     pvm_config(&nhost, &narch, &hostp);

     fprintf(fp, "current number of hosts = %d\n", nhost);
     fflush(fp);

     /* 
      * notify request for host addition, with tag HOSTADD, 
      * no tids to monitor.  
      *
      * -1 turns the notification request on;
      * 0 turns it off;
      * a positive integer n will generate at most n notifications.
      */     
     pvm_notify(PvmHostAdd, HOSTADD, -1, NULL);

     /* add more hosts - no specific machine named */
     i = ntasks - nhost;
     if (i > 0) {
	  status = pvm_addhosts(hosts, i , infos);
	  
	  fprintf(fp, "master: addhost status = %d\n", status);
	  fflush(fp);
     }
     
     /* if more tasks than hosts, loop and call pvm_addhosts repeatedly */
     for (i = nhost; i < ntasks; i++)
     {
	  /* receive notification from anyone, with HostAdd tag */
	  buf_id = pvm_recv(-1, HOSTADD);

	  pvm_bufinfo(buf_id, &msg_len , &msg_tag, &msg_src);
	  if (msg_tag==HOSTADD)
	  {
	       pvm_upkint(&int_val, 1, 1);

	       fprintf(fp, "Received HOSTADD: ");
	       fprintf(fp, "Host %x added from %x\n", int_val, msg_src);
	       fflush(fp);
	  }
	  else
	       fprintf(fp, "Received unexpected message with tag: %d\n", 
		       msg_tag);

     }

     /* print current configuration */
     pvm_config(&nhost, &narch, &hostp);
     fprintf(fp, "Config info : \n");
     fprintf(fp, "nhost = %d\n", nhost);

     /* number of different data formats used */
     fprintf(fp, "narch = %d\n", narch); 
     
     fprintf(fp, "PVM host ID            Hostname                   arch         relative speed\n");
     for (i = 0; i < nhost; i++)
     {
	  fprintf(fp, "%x            %s             %s               %d\n", 
		  hostp[i].hi_tid, hostp[i].hi_name, hostp[i].hi_arch, 
		  hostp[i].hi_speed);
	  
	  /* hosts to be monitored for deletion */
	  deltid[i] = hostp[i].hi_tid;
     }
     fflush(fp);
     
     /* notify all exceptional conditions about the hosts*/
     status = pvm_notify(PvmHostDelete, HOSTDELETE, ntasks, deltid);
     
     status = pvm_notify(PvmHostSuspend, HOSTSUSPEND, ntasks, deltid);
     
     status = pvm_notify(PvmHostResume, HOSTRESUME, ntasks, deltid);

     /* spawn <ntasks> */
     for (i = 0; i < ntasks ; i++) 
     {
	  /* spawn the i-th task, with notifications. */
	  spawn_a_worker(i, tid, host_tid, fp);
     }

     /* add the result <count> times */
     while (count > 0) 
     {
	  /* 
	   * if array length was not perfectly divisible by ntasks, 
	   *	some numbers are remaining. Add these yourself 
	   */
	  result = 0;
	  for ( i = ntasks * pertask ; i < n ; i++)
	       result += i;
	 
	  /* initialize the sum array with -1 */
	  for (i = 0; i< ntasks; i++) 
	    sum[i] = -1;
 
	  /* send array partitions to each task */
	  for (i = 0; i < ntasks ; i++) 
	  {
	       send_data_to_worker(i, tid, num, pertask, fp, count);
	  }

	  
	  /* 
	   * Wait for results.  If a task exited without 
	   * sending back the result, start another task to do
	   * its job. 
	   */
	  
	  for (i = 0; i< ntasks; )
	  {	  
	       buf_id = pvm_recv(-1, -1);
	       pvm_bufinfo(buf_id, &msg_len , &msg_tag, &msg_src);
	       fprintf(fp, "Receive: task %x returns mesg tag %d, buf_id = %d\n", 
		       msg_src, msg_tag, buf_id);
	       fflush(fp);
	       
	       /* is a result returned by a worker */
	       if(msg_tag == RESULT_TAG)  
	       {
		    int j;
		    
		    pvm_upkint(&round_val, 1, 1);
		    fprintf(fp, "  round_val = %d\n", round_val);
		    fflush(fp);
		 
		    if (round_val != count)
			 continue;

		    pvm_upkint(&int_val, 1, 1);
		    for (j=0; (j<ntasks) && (tid[j] != msg_src); j++)
			 ;
		    fprintf(fp, "  Data from task %d, tid = %x : %d\n", 
			    j, msg_src, int_val);
		    fflush(fp);
		    
		    if (sum[j] == -1) {
			 sum[j] = int_val; /* store the sum */
			 i++;
		    }
	       }
	       /* A task has exited. */
	       else if (msg_tag == TASKEXIT) 
	       {
		    /* Find out which task has exited. */ 
		    int which_tid, j;	       
		    pvm_upkint(&which_tid, 1, 1);
		    for (j=0; (j<ntasks) && (tid[j] != which_tid); j++)
			 ;
		    fprintf(fp, "  from tid %x : task %d, tid =  %x, exited.\n", 
			    msg_src, j, which_tid);
		    fflush(fp);
		    /* 
		     * If a task exited before sending back the message,
		     * create another task to do the same job.
		     */
		    if (j < ntasks && sum[j] == -1) 
		    {
			 /* spawn the j-th task */
			 spawn_a_worker(j, tid, host_tid, fp);
			 
			 /* send the unfinished work to the new task */
			 send_data_to_worker(j, tid, num, pertask, fp, count);
		    }
	       }
	       
	       /* 
		* If a host has been deleted, check to see if 
		* the tasks running on it has been finished.  If not, should
		* create  new worker tasks to do the work on some other  hosts.
		*/
	       else if (msg_tag == HOSTDELETE)
	       {
		    int which_tid, j;
		    
		    /* get which host has been suspended/deleted */
		    pvm_upkint(&which_tid, 1, 1);
		    
		    fprintf(fp, "  from tid %x : %x %s\n", msg_src, which_tid, 
			    codes[msg_tag - HOSTDELETE]);
		    fflush(fp);
		    
		    /* 
		     * If the task on that host has not finished its work,
		     * then create new task to do the work.
		     */
		    for (j = 0; j < ntasks; j++)
			 
			 if (host_tid[j] == which_tid && sum[j] == -1)
			 {
			      fprintf(fp, "host_tid[%d] = %x, need new task\n",
				      j, host_tid[j]);
			      fflush(fp);
			      
			      /* spawn the i-th task, with notifications. */
			      spawn_a_worker(j, tid, host_tid, fp);
			      
			      /* send the unfinished work to the new task */
			      send_data_to_worker(j, tid, num, pertask, fp, count);
			      
			 }
	       }
	       
	       /* print out some other notifications or messages */
	       else 
	       {
		    int which_tid;
		    pvm_upkint(&which_tid, 1, 1);
		    
		    fprintf(fp, "  from tid %x : %x %s\n", msg_src, which_tid, 
			    codes[msg_tag - HOSTDELETE]);
		    fflush(fp);
	       }
	  }	       
	  
	  /* add up the sum */
	  for (i=0; i<ntasks; i++)
	       result += sum[i];
	  
	  fprintf(fp, "Sum from  0 to %d is %d\n", n-1 , result);
	  fflush(fp);
	  
	  /* check correctness */
	  if (result == (n-1)*n/2)
	  {
	       correct++;
	       fprintf(fp, "*** Result Correct! ***\n");
	  }
	  else {
	       wrong++;
	       fprintf(fp, "*** Result WRONG! ***\n");
	  }

	  fflush(fp);
	  
	  count--;
     }

     
     fprintf(fp, "correct = %d; wrong = %d\n", correct, wrong);
     fflush(fp);
     
	 /* avoid error-prone catchout cleanup on pvm_exit() */
	 pvm_catchout(0);
     pvm_exit();

     fprintf(fp, "After pvm_exit()\n");
     fflush(fp);
     fclose(fp);

	 if (wrong == 0) {
		 printf("success!\n");
	 }

     exit(0);
}
