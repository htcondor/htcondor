/***************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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
#include "shared_port_scm_rights.h"
#include "fdpass.h"

void
usage( char *cmd )
{
	fprintf(stderr,"Usage: %s [options] condor_* ....\n",cmd);
	fprintf(stderr,"Where options are:\n");
	fprintf(stderr,"    -help              Display options\n");
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
}

int main( int argc, char *argv[] )
{
	int i;
	std::string condor_prefix;

	set_priv_initialize(); // allow uid switching if root
	config();

	// Set prefix to be "condor_" 
	formatstr(condor_prefix,"%s_",myDistro->Get());

	for( i=1; i<argc; i++ ) {
		if(is_arg_prefix(argv[i],"-help")) {
			usage(argv[0]);
			exit(0);
		}
	}


	int uds = socket(AF_UNIX, SOCK_STREAM, 0);
	if (uds < 0) {
		fprintf(stderr, "Cannot create unix domain socket: errno %d\n", errno);
		exit(1);
	}

	struct sockaddr_un pipe_addr;
	memset(&pipe_addr, 0, sizeof(pipe_addr));
	pipe_addr.sun_family = AF_UNIX;
	unsigned pipe_addr_len;

	std::string pipeName = ".docker_sock";	

	strncpy(pipe_addr.sun_path, pipeName.c_str(), sizeof(pipe_addr.sun_path)-1);
	pipe_addr_len = SUN_LEN(&pipe_addr);

	int rc = connect(uds, (struct sockaddr *)&pipe_addr, pipe_addr_len);

	if (rc < 0) {
		dprintf(D_ALWAYS, "Cannot bind unix domain socket at %s for docker ssh_to_job: %d\n", pipeName.c_str(), errno);
		return -1;
	}
	fdpass_send(uds, 0);
	fdpass_send(uds, 1);
	fdpass_send(uds, 2);

	char buf[1];
	rc = read(uds, buf, 1); // wait until other side hangs up
	printf("read returned, exiting\n");

	
	return 0;
}

