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

#include "getPort.h"
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"



#define STDOUT_READBUF_SIZE 1024
#include <errno.h>


char *getPortPath(void) {
	char *path = param("QPID_PORT");
	if (path ==  NULL) {
	printf("is null ... \n");
		char *logPath = param("LOG");
		if (logPath) {
			printf("log path is also null ... \n");
			path = (char*)malloc(strlen(logPath) + 10);
			sprintf(path, "%s%cqpidPort", logPath, DIR_DELIM_CHAR);
			free(logPath);
		} else {
			printf("log path isn't  null ... \n");
			path = (char*)malloc(14);
			sprintf(path, "/tmp/qpidPort");
		}
	}
	printf("The qpid port path: %s \n", path);	
	printf("path: %s \n", path);
	char *condHost = param("CONDOR_HOST");
	if (condHost){
		printf("cond host: %s \n", condHost);
		free(condHost);
	}
	return path;
}

//internal function for deducting port number
char *getPort(bool def)
{
  dprintf(D_ALWAYS, "\n *********get port # called\n");//%s\n", str3);
	
  char *path;
  path = getPortPath();

  FILE *f = safe_fopen_wrapper(path, "r", 0644);
  if (!f)
  	return "";
  char *port = (char*)malloc(10);
  fread(port, 1, 10, f);
  fclose(f);
  port[strlen(port)-1] = '\0';
  if (!def)
  	free(path);
  return port;
}
