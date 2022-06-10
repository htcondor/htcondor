/***************************************************************
 *
 * Copyright (C) 1990-2013, Condor Team, Computer Sciences Department,
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
#include "condor_version.h"
#include "condor_distribution.h"
#include "match_prefix.h"
#include "setenv.h"
#include "str_isxxx.h"

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] condor_* ....\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help              Display options\n");
	fprintf(stderr,"    -version           Display Condor version\n");
	fprintf(stderr,"    -debug             Display debugging info to console\n");
	fprintf(stderr,"    -timeoutmult <val> Multiply timeouts by given value\n");
	fprintf(stderr,"\nExample 1: %s condor_q\n",cmd);
	fprintf(stderr,"\nExample 2: %s -timeoutmult 5 condor_hold -a\n\n",cmd);
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

int main( int argc, char *argv[] )
{
	int i;
	const char* pcolon = nullptr;
	const char* timeout_multiplier = "1";
	std::string condor_prefix;

	set_priv_initialize(); // allow uid switching if root
	config();

	// Set prefix to be "condor_" 
	formatstr(condor_prefix,"%s_",myDistro->Get());

	for( i=1; i<argc; i++ ) {
		if(is_dash_arg_prefix(argv[i],"help")) {
			usage(argv[0]);
			exit(0);
		} else
		if (is_dash_arg_prefix(argv[i],"pool") || is_dash_arg_prefix(argv[i],"name"))
		{
			fprintf(stderr,
				"Neither -pool nor -name arguments are allowed, because"
				"%s will only communicate with HTCondor services on the local machine.\n\n"
				,argv[0]);
			usage(argv[0]);
			exit(1);
		} else
		if (is_dash_arg_prefix(argv[i],"version")) {
			version();
			exit(0);
		} else
		if (is_dash_arg_colon_prefix(argv[i],"debug",&pcolon)) {
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
		} else 
		if (is_dash_arg_prefix(argv[i],"timeoutmult")) {
			i++;
			if ( str_isint(argv[i]) ) {
				timeout_multiplier = argv[i];
			} else {
				fprintf(stderr,
					"Error: argument -timeoutmult requires an integer argument\n");
				exit(1);
			}
		} else
		if (match_prefix(argv[i],condor_prefix.c_str())) { 		
			// here we have a command line arg that matches condor_*
			int result, save_errno;
			// Set env to always use super port, and a nice long timeout
			SetEnv("_condor_USE_SUPER_PORT","true");
			SetEnv("_condor_TIMEOUT_MULTIPLIER",timeout_multiplier);
			// Exec the condor tool specified in the command line.
			// On Win32, execvp interacts strangly with interactive
			// console windows, so on Win32 we do a synchronous spwanvp instead.
#ifdef WIN32
			result = _spawnvp(_P_WAIT,argv[i],&argv[i]);
#else
			result = execvp(argv[i],&argv[i]);
#endif
			if (result == -1) {
				save_errno = errno;
				fprintf(stderr,"Failed to spawn %s: %s\n\n",argv[i],strerror(save_errno));
				exit(1);
			} else {
				// On Unix, if the exec succeeds we'll never make it here, but
				// on Win32, our spawnvp call will return the exit status,
				// so we need to propgate that to the user here.
				exit(result);
			}
		} else {
			fprintf(stderr,
					"Error: unrecognized argument %s\n",argv[i]);
			usage(argv[0]);
			exit(1);
		}
	}

	fprintf(stderr,"Error: no condor command specified\n\n");
	usage(argv[0]);
	exit(1);
}

