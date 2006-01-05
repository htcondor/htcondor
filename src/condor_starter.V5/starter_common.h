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

#ifndef CONDOR_STARTER_COMMON_H
#define CONDOR_STARTER_COMMON_H

#include "condor_uid.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_syscall_mode.h"
#include "my_hostname.h"
#include "exit.h"
#include "condor_arglist.h"

void init_params(void);
void init_sig_mask();
void initial_bookeeping( int argc, char *argv[] );
void init_shadow_connections();
void init_logging();
void usage( char *my_name );
ReliSock* NewConnection( int id );
void support_job_wrapper(char *a_out_name,ArgList *args);

extern "C" int exception_cleanup(int,int,char*);

#endif /* CONDOR_STARTER_COMMON_H */
