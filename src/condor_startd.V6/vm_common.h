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

#ifndef _VM_COMMON_H
#define _VM_COMMON_H

#include "condor_common.h"
#include "startd.h"

#define VM_SOCKET_TIMEOUT 5

extern int vm_register_interval; //seconds

bool vmapi_is_allowed_host_addr(char *addr);
bool vmapi_is_allowed_vm_addr(char *addr);
int vmapi_num_of_registered_vm(void);
int vmapi_register_cmd_handler(char *addr, int *permission);
bool vmapi_is_usable_for_condor(void);
bool vmapi_is_my_machine(char* );

void vmapi_request_host_classAd(void);
ClassAd* vmapi_get_host_classAd(void);

bool vmapi_sendCommand(char *addr, int cmd, void *data);
Daemon* vmapi_findDaemon( char *host_name, daemon_t real_dt);

#endif /* _VM_COMMON_H */
