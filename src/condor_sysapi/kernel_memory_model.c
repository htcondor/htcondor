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

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

char *_sysapi_kernel_memory_model = NULL;

/* the raw version */
/* Do not free the returned pointer */
const char *
sysapi_kernel_memory_model_raw(void)
{
#if defined(LINUX)
	struct utsname buf;
#endif

	/* The output of this function is dependant upon the architecture
		and usually dictates some aspect of the memory model or 
		virtual memory layout of a process by the kernel */

#if defined(LINUX)
	if( uname(&buf) < 0 ) {
		_sysapi_kernel_memory_model = strdup("unknown");
		return _sysapi_kernel_memory_model;
	}

	if (strstr(buf.release, "hugemem") != NULL) {
		_sysapi_kernel_memory_model = strdup("hugemem");
	} else if (strstr(buf.release, "bigmem") != NULL) {
		_sysapi_kernel_memory_model = strdup("bigmem");
	} else {
		_sysapi_kernel_memory_model = strdup("normal");
	}

#elif defined(Solaris)
	/* stolen from stack_end_addr() */
#	if defined(X86)
		_sysapi_kernel_memory_model = strdup("0x8048000");
#	else
#		if defined(Solaris29)
			/* XXX educated guess */
			_sysapi_kernel_memory_model = strdup("0xFFC00000");
#		elif defined(Solaris27) || defined(Solaris28) 
			_sysapi_kernel_memory_model = strdup("0xFFBF0000");
#		elif defined(Solaris251) || defined(Solaris26)
			_sysapi_kernel_memory_model = strdup("0xF0000000");
#		else
#			error Please port sysapi_kernel_memory_model_raw() to this OS/ARCH!
#		endif
#	endif

#else

	/* The other OSes can just use this for now. */
	_sysapi_kernel_memory_model = strdup("normal");

#endif

	return _sysapi_kernel_memory_model;
}

/* the cooked version */
/* Do not free the returned pointer */
const char *
sysapi_kernel_memory_model(void)
{	
	sysapi_internal_reconfig();

	if( _sysapi_kernel_memory_model != NULL ) {
		return _sysapi_kernel_memory_model;
	} else {
		return sysapi_kernel_memory_model_raw();
	}
}


