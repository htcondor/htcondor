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

