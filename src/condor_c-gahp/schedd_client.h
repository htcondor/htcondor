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

#ifndef SCHEDD_CLIENT_H
#define SCHEDD_CLIENT_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "classad_hashtable.h"
#include "list.h"
#include "SchedDCommands.h"
#include "daemon.h"
#include "dc_schedd.h"

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



extern bool useXMLClassads;

// initialization
void Init();
void Register();

// maintainence
void Reconfig();

void enqueue_result (int req_id, const char ** results, const int argc) ;
int get_int (const char *, int *);
int get_ulong (const char *, unsigned long *);
int get_job_id (const char *, int *, int *);
int get_class_ad (const char *, ClassAd **);
int enqueue_command (SchedDRequest *);
char * escape_string (const char *);

int doContactSchedd();
int request_pipe_handler();

int handle_gahp_command(char ** argv, int argc);
int parse_gahp_command (const char *, char ***, int *);


#endif
