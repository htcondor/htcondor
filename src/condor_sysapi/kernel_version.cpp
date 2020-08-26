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

char *_sysapi_kernel_version = NULL;

/* the raw version */
/* Do not free the returned pointer */
const char *
sysapi_kernel_version_raw(void)
{
#if !defined(WIN32)
	struct utsname buf;

	if( uname(&buf) < 0 ) {
		_sysapi_kernel_version = strdup("N/A");
		return _sysapi_kernel_version;
	}
#endif

#if defined(LINUX)
	if (strncmp(buf.release, "2.2.", 4) == MATCH) {
		_sysapi_kernel_version = strdup("2.2.x");
	} else if (strncmp(buf.release, "2.3.", 4) == MATCH) {
		_sysapi_kernel_version = strdup("2.3.x");
	} else if (strncmp(buf.release, "2.4.", 4) == MATCH) {
		_sysapi_kernel_version = strdup("2.4.x");
	} else if (strncmp(buf.release, "2.5.", 4) == MATCH) {
		_sysapi_kernel_version = strdup("2.5.x");
	} else if (strncmp(buf.release, "2.6.", 4) == MATCH) {
		_sysapi_kernel_version = strdup("2.6.x");
	} else if (strncmp(buf.release, "2.7.", 4) == MATCH) {
		_sysapi_kernel_version = strdup("2.7.x");
	} else if (strncmp(buf.release, "2.8.", 4) == MATCH) {
		_sysapi_kernel_version = strdup("2.8.x");
	} else {
		_sysapi_kernel_version = strdup(buf.release);
	}
#elif defined(Darwin) || defined(CONDOR_FREEBSD)
	_sysapi_kernel_version = strdup(buf.release);
#elif defined(WIN32)
	_sysapi_kernel_version = strdup("Unknown");
#else
#	error Please port sysapi_kernel_version_raw() to this platform!
#endif
	
	return _sysapi_kernel_version;
}

/* the cooked version */
/* Do not free the returned pointer */
const char *
sysapi_kernel_version(void)
{	
	sysapi_internal_reconfig();

	if( _sysapi_kernel_version != NULL ) {
		return _sysapi_kernel_version;
	} else {
		return sysapi_kernel_version_raw();
	}
}

/* Is the Linux kernel release number equal to or greater than
 * a given version? Parameter version_num_to_check should be a
 * null-terminated string that looks like "2.6.29".  Returns
 * true if kernel is Linux with a kernel as recent or newer
 * than version_num_to_check, false otherwise.
 */
bool
sysapi_is_linux_version_atleast(const char *version_num_to_check)
{
#ifdef LINUX
	struct utsname ubuf;
	const char *kversion = "0.0.0-";
	if( uname(&ubuf) == 0 ) {
		// ubuf.release will look like "2.6.32-358.el6.x86_64"
		kversion = ubuf.release;
	}
	char *ver = strdup(kversion);
	char *p=strchr(ver,'-');
	if (p) {
		*p = '\0';
	}
	int inputscalar,kernelscalar,fld,MajorVer,MinorVer,SubMinorVer;
	// set a scalar representing the kernel version
	fld = sscanf(ver,"%d.%d.%d",&MajorVer,&MinorVer,&SubMinorVer);
	free(ver);
	kernelscalar=0;
	if (fld==3) {
		kernelscalar = MajorVer * 1000000 + MinorVer * 1000 + SubMinorVer;
	}
	// set a scalar prepresenting the input version
	fld = sscanf(version_num_to_check,"%d.%d.%d",&MajorVer,&MinorVer,
			&SubMinorVer);
	inputscalar =0;
	if (fld==3) {
		inputscalar  = MajorVer * 1000000 + MinorVer * 1000 + SubMinorVer;
	}
	// compare
	if ( kernelscalar >= inputscalar ) {
		return true;
	}
#else
	(void) version_num_to_check;
#endif

	return false;
}
