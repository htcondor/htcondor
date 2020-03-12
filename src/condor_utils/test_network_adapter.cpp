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
#include "stl_string_utils.h"
#include "simple_arg.h"
#include <stdio.h>

static const char *	VERSION = "0.1";

struct Options
{
	int				 m_verbosity;

	const char		*m_if_name;
	const char		*m_address;
	bool			 m_is_primary;
};

bool
CheckArgs(int argc, const char **argv, Options &opts);

int
main(int argc, const char **argv)
{
	set_debug_flags(NULL, D_ALWAYS);

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	dprintf_set_tool_debug("TEST_NETWORK_ADAPTOR", 0);

	const char	*tmp;
	int			 result = 0;
	Options		 opts;

	if ( CheckArgs(argc, argv, opts) ) {
		exit( 1 );
	}

	NetworkAdapterBase	*net = NULL;

	if ( opts.m_if_name ) {
		printf( "Creating network adapter object for name %s\n",
				opts.m_if_name );
		net = NetworkAdapterBase::createNetworkAdapter( opts.m_if_name );
	}
	else {
		std::string sinful;
		formatstr( sinful, "<%s:1234>", opts.m_address );
		printf( "Creating network adapter object for %s\n", sinful.c_str() );
		net = NetworkAdapterBase::createNetworkAdapter( sinful.c_str() );
	}
	if ( !net ) {
		printf( "Error creating adapter\n" );
		exit(1);
	}

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

	tmp = net->subnetMask();
	if ( !tmp || !strlen(tmp) ) tmp = "<NONE>";
	printf( "subnet: %s\n", tmp );

	printf( "wakable: %s\n", net->isWakeable() ? "YES" : "NO" );

	std::string tmpstr;
	net->wakeSupportedString( tmpstr );
	printf( "wake support flags: %s\n", tmpstr.c_str() );

	net->wakeEnabledString( tmpstr );
	printf( "wake enable flags: %s\n", tmpstr.c_str() );

	ClassAd	ad;
	net->publish( ad );
	fPrintAd( stdout, ad );

	delete net;

	if ( result != 0 && opts.m_verbosity >= 1 ) {
		fprintf(stderr, "test_network_adapter FAILED\n");
	}

	return result;
}

bool
CheckArgs(int argc, const char **argv, Options &opts)
{
	const char *	usage =
		"Usage: test_network_adapter [options] <IP address|IF name>\n"
		"  -d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n";

	opts.m_verbosity = 1;

	opts.m_if_name = NULL;
	opts.m_address = "127.0.0.1";
	opts.m_is_primary = false;


	int		optno = 0;
	for ( int index = 1; index < argc; ++index ) {
		SimpleArg	arg( argv, argc, index );

		if ( arg.Error() ) {
			printf("%s", usage);
			return true;
		}

		if ( arg.Match( 'd', "debug") ) {
			if ( arg.hasOpt() ) {
				set_debug_flags( arg.getOpt(), 0 );
				index = arg.ConsumeOpt( );
			} else {
				fprintf(stderr, "Value needed for %s argument\n", arg.Arg() );
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
			if ( arg.getOpt(opts.m_verbosity) ) {
				fprintf(stderr, "Value needed for %s argument\n", arg.Arg() );
				printf("%s", usage);
				return true;
			}

		} else if ( arg.Match("version") ) {
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
			return true;

		} else if ( !arg.ArgIsOpt()  &&  (optno == 0)  &&  arg.isOptInt() ) {
			opts.m_address = arg.getOpt();
			optno++;

		} else if ( !arg.ArgIsOpt()  &&  (optno == 0) ) {
			opts.m_if_name = arg.getOpt();
			optno++;

		} else if ( !arg.ArgIsOpt()  &&  (optno == 1)  &&  arg.isOptBool() ) {
			arg.getOpt(opts.m_is_primary, false);
			optno++;

		} else {
			fprintf(stderr, "Unrecognized argument: <%s>\n", arg.Arg() );
			printf("%s", usage);
			return true;
		}
	}

	return false;
}
