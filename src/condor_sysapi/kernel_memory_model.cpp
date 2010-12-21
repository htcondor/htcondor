/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
	_sysapi_kernel_memory_model = NULL;

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
#  if defined(Solaris10) || defined(Solaris11)
	// We don't do standard universe on Solaris anymore anyway,
	// so do nothing (falls through to "normal", below)
	
	/* stolen from stack_end_addr() */
#  elif defined(X86)
	_sysapi_kernel_memory_model = strdup("0x8048000");
#  elif defined(Solaris29)
	/* XXX educated guess */
	_sysapi_kernel_memory_model = strdup("0xFFC00000");
#  elif defined(Solaris27) || defined(Solaris28) 
	_sysapi_kernel_memory_model = strdup("0xFFBF0000");
#  elif defined(Solaris26)
	_sysapi_kernel_memory_model = strdup("0xF0000000");
#  else
#	error Please port sysapi_kernel_memory_model_raw() to this OS/ARCH!
#  endif
#endif

	/* The other OSes can just use this for now. */
	if ( NULL ==_sysapi_kernel_memory_model ) {
		_sysapi_kernel_memory_model = strdup("normal");
	}

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


