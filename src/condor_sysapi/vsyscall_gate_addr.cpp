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
#include "my_popen.h"

static char *_sysapi_vsyscall_gate_addr = NULL;

#define BUFFER_SIZE 2048

/* The memory returned from here must be freed */
static char* find_ckpt_probe(void);

static char* find_ckpt_probe(void)
{
	char *tmp = NULL;
	char *tmp2 = NULL;
	char path[BUFFER_SIZE];

	tmp = param("CKPT_PROBE");

	if (tmp != NULL) {
		return tmp;
	}

	tmp = param("LIBEXEC");

	if (tmp != NULL) {
		tmp2 = (char *) malloc(strlen(tmp) + strlen("/condor_ckpt_probe") + 1);
		if (tmp2 == NULL) {
			EXCEPT("Out of memory!");
		}

		/* build up the path to the probe process */
		strcpy(tmp2, tmp);
		strcat(tmp2, "/condor_ckpt_probe");
		free(tmp);

		/* now check to see if it exists */
		if (access(tmp2, X_OK) < 0) {
			/* Nope, it isn't there */
			free(tmp2);
			return NULL;
		}

		/* Ok it is executable, give it back to the caller */
		return tmp2;
	}

	tmp = param("RELEASE_DIR");

	if (tmp != NULL) {
		tmp2 = (char*) malloc(strlen(tmp) + strlen("/libexec/condor_ckpt_probe") + 1);
		if (tmp2 == NULL) {
			EXCEPT("Out of memory!");
		}

		/* build up the path to the probe process */
		strcpy(tmp2, tmp);
		strcat(tmp2, "/libexec/condor_ckpt_probe");
		free(tmp);

		/* now check to see if it exists */
		if (access(tmp2, X_OK) < 0) {
			/* Nope, it isn't there */
			free(tmp2);
			return NULL;
		}

		/* Ok it is executable, give it back to the caller */
		return tmp2;
	}

	return NULL;
}

/* the raw version */
/* Do not free the returned pointer */
const char *
sysapi_vsyscall_gate_addr_raw(void)
{
	char *tmp;
	char *cmd[3];
	FILE *fin;
	char buf[BUFFER_SIZE];
	char addr[BUFFER_SIZE];

	/* immediately set this up if it isn't already */
	if (_sysapi_vsyscall_gate_addr == NULL) {
		/* Set this up immediately for the rest of the algorithm */
		_sysapi_vsyscall_gate_addr = strdup("N/A");
	}

#if defined(LINUX)
	if (strcmp(_sysapi_vsyscall_gate_addr, "N/A") == MATCH) {

		/* get the probe process executable */
		tmp = find_ckpt_probe();
		if (tmp == NULL) {
			/* Can't find it at all, so bail */
			return _sysapi_vsyscall_gate_addr;
		}

		/* exec probe */
		cmd[0] = tmp;
		cmd[1] = "--vdso-addr";
		cmd[2] = NULL;
		fin = my_popenv(cmd, "r", TRUE);
		free(tmp);
		if (fin == NULL) {
			dprintf(D_ALWAYS, "my_popenv failed\n");
			return _sysapi_vsyscall_gate_addr;
		}
		if (fgets(buf, BUFFER_SIZE, fin) == NULL) {
			my_pclose(fin);
			dprintf(D_ALWAYS, "fgets failed\n");
			return _sysapi_vsyscall_gate_addr;
		}
		my_pclose(fin);

		if (sscanf(buf, "VDSO: %s\n", addr) != 1) {
			dprintf(D_ALWAYS, "sscanf didn't parse correctly\n");
			return _sysapi_vsyscall_gate_addr;
		}

	/* set up global with correct vdso address */
		if(_sysapi_vsyscall_gate_addr == NULL) {
			EXCEPT("Programmer error! _sysapi_vsyscall_gate_addr == NULL");
		}
		free(_sysapi_vsyscall_gate_addr);
		_sysapi_vsyscall_gate_addr = strdup(addr);
	}
#endif

	return _sysapi_vsyscall_gate_addr;
}

/* the cooked version */
/* Do not free the returned pointer */
const char *
sysapi_vsyscall_gate_addr(void)
{	
	sysapi_internal_reconfig();

	return sysapi_vsyscall_gate_addr_raw();
}


