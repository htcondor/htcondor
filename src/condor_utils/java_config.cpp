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

int java_config( std::string &cmd, ArgList *args, StringList *extra_classpath )
{
	char *tmp;
	char separator;
	std::string arg_buf;

	tmp = param("JAVA");
	if(!tmp) return 0;
	cmd = tmp;
	free(tmp);
	
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
	args->AppendArg(arg_buf);

	MyString args_error;

	tmp = param("JAVA_EXTRA_ARGUMENTS");
	if(!args->AppendArgsV1RawOrV2Quoted(tmp,&args_error)) {
		dprintf(D_ALWAYS,"java_config: failed to parse extra arguments: %s\n",
				args_error.c_str());
		free(tmp);
		return 0;
	}
	free(tmp);

	return 1;
}


