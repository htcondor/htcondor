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
#include "env.h"

void init_params(void);
void init_sig_mask();
void initial_bookeeping( int argc, char *argv[] );
void init_shadow_connections();
void init_logging();
void usage( char *my_name );
ReliSock* NewConnection( int id );
void support_job_wrapper(MyString &a_out_name, ArgList *args);
void setSlotEnv( Env* env_obj );

extern "C" int exception_cleanup(int,int,char*);

#endif /* CONDOR_STARTER_COMMON_H */
