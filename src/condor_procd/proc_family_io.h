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

#ifndef _PROC_FAMILY_IO_H
#define _PROC_FAMILY_IO_H

#include "condor_common.h"

// command identifiers for communicatoin with the procd"
//
enum proc_family_command_t {
	PROC_FAMILY_REGISTER_SUBFAMILY,
	PROC_FAMILY_SIGNAL_PROCESS,
	PROC_FAMILY_SUSPEND_FAMILY,
	PROC_FAMILY_CONTINUE_FAMILY,
	PROC_FAMILY_KILL_FAMILY,
	PROC_FAMILY_GET_USAGE,
	PROC_FAMILY_UNREGISTER_FAMILY,
	PROC_FAMILY_TAKE_SNAPSHOT,
	PROC_FAMILY_DUMP,
	PROC_FAMILY_QUIT
};

struct ProcFamilyUsage {
	long          user_cpu_time;
	long          sys_cpu_time;
	double        percent_cpu;
	unsigned long max_image_size;
	unsigned long total_image_size;
	int           num_procs;
};

#endif
