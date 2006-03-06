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
