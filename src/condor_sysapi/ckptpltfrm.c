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
	int size;

	/* compute the checkpointing signature of this machine, each thing
		you want to detect should be defined in some form for every platform. */
	opsys = sysapi_opsys();
	arch = sysapi_condor_arch();
	kernel_version = sysapi_kernel_version();
	memory_model = sysapi_kernel_memory_model();

	size = strlen(opsys) + 1 /*space*/ +
			strlen(arch) + 1 /*space*/ +
			strlen(kernel_version) + 1 /*space*/ +
			strlen(memory_model) + 1 /*nul*/;
	
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

