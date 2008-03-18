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


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

/* the raw version */
/* Do not free the returned pointer */
const char *
sysapi_ckptpltfrm_raw(void)
{
	const char *opsys;
	const char *arch;
	const char *kernel_version;
	const char *memory_model;
#ifndef WIN32
	const char *vsyscall_page;
#endif
	int size;

	/* compute the checkpointing signature of this machine, each thing
		you want to detect should be defined in some form for every platform. */
	opsys = sysapi_opsys();
	arch = sysapi_condor_arch();
	kernel_version = sysapi_kernel_version();
	memory_model = sysapi_kernel_memory_model();
#ifndef WIN32
	vsyscall_page = sysapi_vsyscall_gate_addr();
#endif

/* Currently, windows doesn't support condor_ckpt_probe, so don't put in
	the vsyscall page information. */
#ifndef WIN32
	size = strlen(opsys) + 1 /*space*/ +
			strlen(arch) + 1 /*space*/ +
			strlen(kernel_version) + 1 /*space*/ +
			strlen(memory_model) + 1 /*space*/ +
			strlen(vsyscall_page) + 1 /*nul*/;
#else
	size = strlen(opsys) + 1 /*space*/ +
			strlen(arch) + 1 /*space*/ +
			strlen(kernel_version) + 1 /*space*/ +
			strlen(memory_model) + 1; /*space*/
#endif
	
	/* this will always be NULL when this function is entered */
	_sysapi_ckptpltfrm = (char*)malloc(sizeof(char) * size);
	if (_sysapi_ckptpltfrm == NULL) {
		EXCEPT("Out of memory!");
	}

	strcpy(_sysapi_ckptpltfrm, opsys);
	strcat(_sysapi_ckptpltfrm, " ");
	strcat(_sysapi_ckptpltfrm, arch);
	strcat(_sysapi_ckptpltfrm, " ");
	strcat(_sysapi_ckptpltfrm, kernel_version);
	strcat(_sysapi_ckptpltfrm, " ");
	strcat(_sysapi_ckptpltfrm, memory_model);
#ifndef WIN32
	strcat(_sysapi_ckptpltfrm, " ");
	strcat(_sysapi_ckptpltfrm, vsyscall_page);
#endif

	return _sysapi_ckptpltfrm;
}

/* the cooked version */
/* Do not free the returned pointer */
const char *
sysapi_ckptpltfrm(void)
{	
	sysapi_internal_reconfig();

	if( _sysapi_ckptpltfrm != NULL ) {
		return _sysapi_ckptpltfrm;
	} else {
		return sysapi_ckptpltfrm_raw();
	}
}

