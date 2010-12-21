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

