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
#include "network_adapter.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_distribution.h"
#include "MyString.h"
#include "simple_arg.h"
#include <stdio.h>

static const char *	VERSION = "0.1";

struct Options
{
	int				 m_verbosity;
	const char		*m_address;
};

bool
CheckArgs(int argc, const char **argv, Options &opts);

int
main(int argc, const char **argv)
{
	DebugFlags = D_ALWAYS;

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("TEST_NETWORK_ADAPTER");

	const char	*tmp;
	int			 result = 0;
	Options		 opts;

	if ( CheckArgs(argc, argv, opts) ) {
		exit( 1 );
	}

	MyString	sinful;
	sinful.sprintf( "<%s:1234>", opts.m_address );
	NetworkAdapterBase	*net =
		NetworkAdapterBase::createNetworkAdapter( sinful.GetCStr() );

	// Initialize it
	if ( !net->getInitStatus() ) {
		printf( "Initialization of adaptor with address %s failed\n",
				opts.m_address );
		exit(1);
	}

	// And, check for it's existence
	if ( !net->exists() ) {
		printf( "Adaptor with address %s not found\n",
				opts.m_address );
		exit(1);
	}

	// Now, extract information from it
	tmp = net->hardwareAddress();
	if ( !tmp || !strlen(tmp) ) tmp = "<NONE>";
	printf( "hardware address: %s\n", tmp );

	tmp = net->subnet();
	if ( !tmp || !strlen(tmp) ) tmp = "<NONE>";
	printf( "subnet: %s\n", tmp );

	printf( "wakable: %s\n", net->isWakeable() ? "YES" : "NO" );

	MyString	tmpstr;
	net->wakeSupportedString( tmpstr );
	printf( "wake support flags: %s\n", tmpstr.GetCStr() );

	net->wakeEnabledString( tmpstr );
	printf( "wake enable flags: %s\n", tmpstr.GetCStr() );

	ClassAd	ad;
	net->publish( ad );
	ad.fPrint( stdout );

	if ( result != 0 && opts.m_verbosity >= 1 ) {
		fprintf(stderr, "test_network_adapter FAILED\n");
	}

	return result;
}

bool
CheckArgs(int argc, const char **argv, Options &opts)
{
	const char *	usage =
		"Usage: test_network_adapter [options] <IP address>\n"
		"  -d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n";

	opts.m_address = "127.0.0.1";
	opts.m_verbosity = 1;

	for ( int index = 1; index < argc; ++index ) {
		SimpleArg	arg( argv, argc, index );

		if ( arg.Error() ) {
			printf("%s", usage);
			return true;
		}

		if ( arg.Match( 'd', "debug") ) {
			if ( arg.HasOpt() ) {
				set_debug_flags( const_cast<char *>(arg.Opt()) );
				index = arg.ConsumeOpt( );
			} else {
				fprintf(stderr, "Value needed for --debug argument\n");
				printf("%s", usage);
				return true;
			}

		} else if ( ( arg.Match("usage") )		||
					( arg.Match('h') )			||
					( arg.Match("help") )  )	{
			printf("%s", usage);
			return true;

		} else if ( arg.Match('v') ) {
			opts.m_verbosity++;

		} else if ( arg.Match("verbosity") ) {
			if ( arg.OptIsNumber() ) {
				opts.m_verbosity = atoi(argv[index]);
				index = arg.ConsumeOpt( );
			} else {
				fprintf(stderr, "Value needed for --verbosity argument\n");
				printf("%s", usage);
				return true;
			}

		} else if ( arg.Match("version") ) {
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
			return true;

		} else if ( !arg.ArgIsOpt() ) {
			opts.m_address = arg.ArgStr();

		} else {
			fprintf(stderr, "Unrecognized argument: <%s>\n", arg.ArgStr() );
			printf("%s", usage);
			return true;
		}
	}

	return false;
}
