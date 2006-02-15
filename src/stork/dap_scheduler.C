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
#include "condor_debug.h"
#include "dap_scheduler.h"
#include "dap_error.h"

/* ========================================================================
 * 
 * ======================================================================== */
void Scheduler::insert(char *dap_id, unsigned int pid)
{
  dprintf(D_ALWAYS, "Add request %s to the queue\n", dap_id);  

  Request *temp;

  temp = (Request *)malloc(1 * sizeof(Request));
  if (temp == NULL) {
    dprintf(D_ALWAYS, "** Error in malloc when adding request %s to the queue!\n", dap_id);
    return;
  }
  
  strcpy(temp->dap_id ,dap_id);
  temp->pid = pid;
  temp->next = NULL;

  if (head == NULL){
    head = temp;
    tail = temp;
  }
  else {
    tail->next = temp;
    tail = temp;
  }
  
  num_jobs ++;

  print();
}
/* ========================================================================
 * 
 * ======================================================================== */
int Scheduler::remove(char *dap_id)
{
  Request *p, *temp;

  //if dap_id is not found in queue, just decrease num_jobs (!??)
  if (!strcmp(dap_id, "0")){
    num_jobs --;
    print();
    return DAP_SUCCESS;
  }
  
  if (head == NULL){
    return DAP_ERROR;
  }
  
  dprintf (D_ALWAYS, "Remove request %s from queue\n",dap_id);

  p = head;

  if (!strcmp(dap_id, p->dap_id)){
    if (head == tail) {
      head = NULL;
      tail = NULL;
    }
    else{
      head = head->next;
    }
    
    free(p);
    num_jobs --;

    print();
    return DAP_SUCCESS;
  }

  while (p->next != NULL){
    if (!strcmp(dap_id, p->next->dap_id)){
      temp = p->next;
      p->next = p->next->next;
      if (p->next == NULL) tail = p;
      
      free(temp);
      num_jobs --;

      print();
      return DAP_SUCCESS;
    }
    p = p->next;
  }

  return DAP_ERROR;
}

/* ========================================================================
 *
 * ======================================================================== */
int Scheduler::get_dapid(unsigned int pid, char *dap_id)
{
  Request *p;

  if (head == NULL) {
    return DAP_ERROR;
  }
  
  p = head;

  while (p != NULL){
    if (pid == p->pid){
      strncpy(dap_id, p->dap_id, MAXSTR);

      return DAP_SUCCESS;
    }
    p = p->next;
  }

  return DAP_ERROR;
}
/* ========================================================================
 *
 * ======================================================================== */
int Scheduler::get_pid(unsigned int & pid, char *dap_id)
{
  Request *p;

  if (head == NULL) {
    return DAP_ERROR;
  }
  
  p = head;

  while (p != NULL){
    if ( !strcmp(dap_id, p->dap_id) ){
      pid = p->pid;

      return DAP_SUCCESS;
    }
    p = p->next;
  }

  return DAP_ERROR;
}

/* ========================================================================
 * 
 * ======================================================================== */
unsigned int Scheduler::get_numjobs()
{
  unsigned int num;

    num = num_jobs;

  return num;
}

/* ========================================================================
 * 
 * ======================================================================== */
void Scheduler::print()
{
  Request *p;

  dprintf(D_ALWAYS, "============================\n");
  dprintf(D_ALWAYS, "* Number of jobs running: %d\n", num_jobs);

  
  p = head;
  while (p != NULL){
    dprintf(D_ALWAYS, "->dap_id:%s, pid:%d\n", p->dap_id, p->pid);

      p = p->next;
  }

  dprintf(D_ALWAYS, "============================\n");
}
















