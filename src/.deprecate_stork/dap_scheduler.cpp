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
	return;		// Disabling this function because of severe performance
				// penalties
#if 0
  Request *p;

  dprintf(D_ALWAYS, "============================\n");
  dprintf(D_ALWAYS, "* Number of jobs running: %d\n", num_jobs);

  
  p = head;
  while (p != NULL){
    dprintf(D_ALWAYS, "->dap_id:%s, pid:%d\n", p->dap_id, p->pid);

      p = p->next;
  }

  dprintf(D_ALWAYS, "============================\n");
#endif
}
















