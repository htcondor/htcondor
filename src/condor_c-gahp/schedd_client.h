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


#ifndef SCHEDD_CLIENT_H
#define SCHEDD_CLIENT_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "list.h"
#include "SchedDCommands.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "gahp_common.h"

//#include "basejob.h"

// Special value for a daemon-core timer id which indicates that there
// is no timer currently registered (for variables holding a timer id) or
// that no timer should be signalled when one normally would be (for
// functions that take as an argument a timer id to be signalled when
// something happens).
#define TIMER_UNSET -1

extern char *ScheddAddr;
extern char *ScheddPool;
extern char *ScheddJobConstraint;
extern char *GridmanagerScratchDir;
extern char *Owner;

extern int contactScheddTid;



#define GAHP_NULL_PARAM "NULL"


extern char *proxySubjectName;
extern int contact_schedd_interval;

// initialization
void Init();
void Register();

// maintainence
void Reconfig();

int get_int (const char *, int *);
int get_ulong (const char *, unsigned long *);
int get_job_id (const char *, int *, int *);
int get_class_ad (const char *, ClassAd **);
int enqueue_command (SchedDRequest *);
char * escape_string (const char *);

void doContactSchedd();
int request_pipe_handler(int);

int handle_gahp_command(char ** argv, int argc);
int parse_gahp_command (const char *, Gahp_Args *);


#endif
