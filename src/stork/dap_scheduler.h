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
#ifndef _DAP_SCHEDULER_H_
#define _DAP_SCHEDULER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dap_constants.h"

typedef struct Request{
  char dap_id[MAXSTR];
  unsigned int pid;
  Request *next;
}Request;

class Scheduler{
	Request *head;
	Request *tail;
	unsigned int num_jobs;
public:
	Scheduler(){ head = NULL; tail = NULL; num_jobs = 0;}

	void insert(char *dap_id, unsigned int pid);
	int  remove(char *dap_id);
	int get_dapid(unsigned int pid, char *dap_id);
	int get_pid(unsigned int & pid, char *dap_id);
	unsigned int get_numjobs();
	
	void print();
};

#endif

