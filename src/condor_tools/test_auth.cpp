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


 

/***********************************************************************
*
* Test authorization policy.
*
***********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "match_prefix.h"
#include "string_list.h"
#include "daemon.h"
#include "dc_collector.h"
#include "daemon_types.h"
#include "internet.h"
#include "condor_distribution.h"
#include "subsystem_info.h"

char	*MyName;

void
usage()
{
	fprintf( stderr, "Usage: %s [options]\n", MyName );
	fprintf( stderr, "\n" );
	fprintf( stderr, "Test authorization policy.  Each test is entered into\n"
	                 "this program, one per line as follows:\n");
	fprintf( stderr, "\n" );
	fprintf( stderr, "perm user ip [expected result]\n" );
	fprintf( stderr, "Example: WRITE badguy@some.domain 192.168.4.3 DENIED\n");
	fprintf( stderr, "\n" );
	fprintf( stderr, "Use '*' for the user to represent an unauthenticated user.\n");
	fprintf( stderr, "\n   Valid options are:\n" );
	fprintf( stderr, "   -daemontype name\t(schedd, startd, ...)\n" );
	fprintf( stderr, "   -debug\n" );
	exit( 2 );
}

DCpermission
StringToDCpermission(char const *str) {
	DCpermission perm;

	for(perm = FIRST_PERM;perm!=LAST_PERM;perm=NEXT_PERM(perm)) {
		if(str && !strcasecmp(str,PermString(perm)) ) {
			return perm;
		}
	}
	return LAST_PERM;
}

int
main( int argc, char* argv[] )
{
	int		i;
	const char* pcolon;
	
	set_mySubSystem( "DAEMON-TOOL", false, SUBSYSTEM_TYPE_TOOL );

	MyName = argv[0];

	FILE *input_fp = stdin;

	for( i=1; i<argc; i++ ) {
		if( is_dash_arg_prefix( argv[i], "daemontype", 2 ) ) {
			if( argv[i + 1] ) {
				get_mySubSystem()->setName( argv[++i] );
				get_mySubSystem()->setTypeFromName( );
			} else {
				usage();
			}
		} else if( is_dash_arg_colon_prefix( argv[i], "debug", &pcolon ) ) {
				// dprintf to console
			dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
			if ( ! pcolon) { set_debug_flags(NULL, D_FULLDEBUG | D_SECURITY); }
		} else if( match_prefix( argv[i], "-" ) ) {
			usage();
		} else {
			usage();
		}
	}

	// If we didn't get told what subsystem we should use, set it
	// to "TOOL".

	if( !get_mySubSystem()->isNameValid() ) {
		get_mySubSystem()->setName( "DAEMON-TOOL" );
	}

	set_priv_initialize(); // allow uid switching if root
	config_ex( CONFIG_OPT_WANT_META | CONFIG_OPT_NO_EXIT );

	int failures = 0;
	IpVerify ipverify;

	std::string line;
	while( readLine(line, input_fp) ) {
		chomp(line);
		if( line.empty() || line[0] == '#' ) {
			printf("%s\n",line.c_str());
			continue;
		}

		StringTokenIterator fields(line.c_str()," ");

		const std::string perm_str = *fields++;
		      std::string fqu = *fields++;
		const std::string ip = *fields++;
		const std::string expected = *fields++;

		std::string sin_str = generate_sinful(ip.c_str(), 0);

		condor_sockaddr addr;
		if( !addr.from_sinful(sin_str) ) {
			fprintf(stderr,"Invalid ip address: %s\n",ip.c_str());
			failures++;
			continue;
		}

		DCpermission perm = StringToDCpermission(perm_str.c_str());
		if( perm == LAST_PERM ) {
			fprintf(stderr,"Invalid permission level: %s\n",perm_str.c_str());
			failures++;
			continue;
		}

		if (fqu == "*") {
			fqu = "";
		}

		char const *result;
		std::string reason;
		if( ipverify.Verify(perm,addr,fqu.c_str(), reason, reason) != USER_AUTH_SUCCESS ) {
			result = "DENIED";
		}
		else {
			result = "ALLOWED";
		}

		if( strcasecmp(expected.c_str(),result) != 0 ) {
			printf("Got wrong result '%s' for '%s': reason: %s!\n",
				   result,line.c_str(),reason.c_str());
			failures++;
			continue;
		}
		if(!expected.empty() ) {
			printf("%s\n",line.c_str());
		}
		else {
			printf("%s %s\n",line.c_str(),result);
		}
	}

	if (failures) {
		printf("Aborting because of %d failures\n", failures);
	}

	exit(failures);
}
