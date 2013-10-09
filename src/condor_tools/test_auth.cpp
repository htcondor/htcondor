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
#include "condor_string.h"
#include "get_daemon_name.h"
#include "daemon.h"
#include "dc_collector.h"
#include "daemon_types.h"
#include "internet.h"
#include "condor_distribution.h"
#include "string_list.h"
#include "simplelist.h"
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
	
	set_mySubSystem( "DAEMON-TOOL", SUBSYSTEM_TYPE_TOOL );

	MyName = argv[0];
	myDistro->Init( argc, argv );

	FILE *input_fp = stdin;

	for( i=1; i<argc; i++ ) {
		if( match_prefix( argv[i], "-daemontype" ) ) {
			if( argv[i + 1] ) {
				get_mySubSystem()->setName( argv[++i] );
				get_mySubSystem()->setTypeFromName( );
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-debug" ) ) {
				// dprintf to console
			dprintf_set_tool_debug("DAEMON-TOOL", 0);
			set_debug_flags(NULL, D_FULLDEBUG|D_SECURITY);
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

	config_ex( 0, false );

	IpVerify ipverify;

	MyString line;
	while( line.readLine(input_fp) ) {
		line.chomp();
		if( line.IsEmpty() || line[0] == '#' ) {
			printf("%s\n",line.Value());
			continue;
		}

		StringList fields(line.Value()," ");
		fields.rewind();

		char const *perm_str = fields.next();
		char const *fqu = fields.next();
		char const *ip = fields.next();
		char const *expected = fields.next();

		MyString sin_str = generate_sinful(ip, 0);

		condor_sockaddr addr;
		if( !addr.from_sinful(sin_str) ) {
			fprintf(stderr,"Invalid ip address: %s\n",ip);
			exit(1);
		}

		DCpermission perm = StringToDCpermission(perm_str);
		if( perm == LAST_PERM ) {
			fprintf(stderr,"Invalid permission level: %s\n",perm_str);
			exit(1);
		}

		if( strcmp(fqu,"*") == 0 ) {
			fqu = "";
		}

		char const *result;
		MyString reason;
		if( ipverify.Verify(perm,addr,fqu,&reason,&reason) != USER_AUTH_SUCCESS ) {
			result = "DENIED";
		}
		else {
			result = "ALLOWED";
		}

		if( expected && strcasecmp(expected,result) != 0 ) {
			printf("Got wrong result '%s' for '%s': reason: %s!\n",
				   result,line.Value(),reason.Value());
			printf("Aborting.\n");
			exit(1);
		}
		if( expected ) {
			printf("%s\n",line.Value());
		}
		else {
			printf("%s %s\n",line.Value(),result);
		}
	}
}
