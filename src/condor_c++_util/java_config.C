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
#include "string_list.h"
#include "condor_config.h"
#include "../condor_sysapi/sysapi.h"
#include "java_config.h"

/*
Extract the java configuration from the local config files.
The name of the java executable gets put in 'cmd', and the necessary
arguments get put in 'args'.  If you have other dirs or jarfiles
that should be placed in the classpath, provide them in 'extra_classpath'.
*/

int java_config( char *cmd, ArgList *args, StringList *extra_classpath )
{
	char *tmp;
	char separator;
	MyString arg_buf;

	tmp = param("JAVA");
	if(!tmp) return 0;
	strcpy(cmd,tmp);
	free(tmp);

	tmp = param("JAVA_MAXHEAP_ARGUMENT");
	if(tmp) {
		arg_buf.sprintf("%s%dm",tmp,sysapi_phys_memory());
		args->AppendArg(arg_buf.Value());
		free(tmp);
	}
	
	tmp = param("JAVA_CLASSPATH_ARGUMENT");
	if(!tmp) tmp = strdup("-classpath");
	if(!tmp) return 0;
	args->AppendArg(tmp);
	free(tmp);

	tmp = param("JAVA_CLASSPATH_SEPARATOR");
	if(tmp) {
		separator = tmp[0];
		free(tmp);
	} else {
		separator = PATH_DELIM_CHAR;
	}

	tmp = param("JAVA_CLASSPATH_DEFAULT");
	if(!tmp) tmp = strdup(".");
	if(!tmp) return 0;
	StringList classpath_list(tmp);
	free(tmp);

	int first = 1;

	classpath_list.rewind();
	arg_buf = "";
	while((tmp=classpath_list.next())) {
		if(!first) {
			arg_buf += separator;
		} else {
			first = 0;
		}
		arg_buf += tmp;
	}

	if(extra_classpath) {
		extra_classpath->rewind();
		while((tmp=extra_classpath->next())) {
			if(!first) {
				arg_buf += separator;
			}
			else {
				first = 0;
			}
			arg_buf += tmp;
		}
	}
	args->AppendArg(arg_buf.Value());

	MyString args_error;

	tmp = param("JAVA_EXTRA_ARGUMENTS");
	if(!args->AppendArgsV1RawOrV2Quoted(tmp,&args_error)) {
		dprintf(D_ALWAYS,"java_config: failed to parse extra arguments: %s\n",
				args_error.Value());
		free(tmp);
		return 0;
	}
	free(tmp);

	return 1;
}


