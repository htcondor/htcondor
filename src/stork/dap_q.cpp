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
#include "dap_constants.h"
#include "dap_server_interface.h"
#include "dap_client_interface.h"
#include "condor_daemon_core.h"
#include "condor_config.h"

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"

#define USAGE \
"[stork_server]\n\
stork_server\t\t\tstork server (deprecated)"

int main(int argc, char **argv)
{
	struct stork_global_opts global_opts;
	config();	// read config file

	// Parse out stork global options.
	stork_parse_global_opts(argc, argv, USAGE, &global_opts, true);

	//check number of arguments
	switch (argc) {
		case 1:
			// do nothing
			break;
		case 2:
			// heritage positional parameter to specify remote server.
			if (argv[1][0] == '-') {
				// an invalid option argument
				fprintf(stderr, "unknown option: '%s'\n", argv[1]);
				stork_print_usage(stderr, argv[0], USAGE, true);
				exit(1);
			}
			global_opts.server = argv[1];
			break;
		default:
			fprintf(stderr, "too many arguments\n");
			stork_print_usage(stderr, argv[0], USAGE, true);
			exit(1);
	}


  char * error_reason = NULL;
  int result_size = 0;
  classad::ClassAd ** result = NULL;

  int rc =
	  stork_list ( global_opts.server,
				 result_size,
				   result,
				   error_reason);

  
  if (!rc) {
	  fprintf (stderr, "ERROR getting list of jobs (%s)\n", 
			   error_reason);
  } else {
	  fprintf(stdout, "===============\n");
	  fprintf(stdout, "job queue:\n");
	  fprintf(stdout, "===============\n");

	  classad::PrettyPrint unparser;
	  std::string buffer;

	  int i;
	  for (i=0; i<result_size; i++) {
		  buffer = "";	// Unparse will append to the buffer string
		  unparser.Unparse (buffer, result[i]);
		  fprintf (stdout, "\n%s\n", buffer.c_str());
	  }
	  fprintf(stdout, "===============\n");
	  delete [] result;
  }
  
  return (rc)?0:1;

}











