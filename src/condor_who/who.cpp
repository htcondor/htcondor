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
#include "condor_config.h"
#include "condor_attributes.h"
#include "get_daemon_name.h"
#include "sig_install.h"
#include "daemon.h"
#include "dc_collector.h"
#include "MyString.h"
#include "match_prefix.h"    // is_arg_colon_prefix
#include "condor_api.h"
#include "status_types.h"

#include "condor_distribution.h"
#include "condor_version.h"

#include <vector>
#include <sstream>
#include <iostream>

using std::vector;
using std::string;
using std::stringstream;

char	*param();
char	*MyName;


	// Tell folks how to use this program
void
usage(bool and_exit)
{
	fprintf( stderr, "Usage: %s [{+|-}priority ] [-p priority] ", MyName );
	fprintf( stderr, "[ -a ] [-n schedd_name] [ -pool pool_name ] [user | cluster | cluster.proc] ...\n");
	if (and_exit)
		exit( 1 );
}

int
main( int argc, char *argv[] )
{
#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	myDistro->Init( argc, argv );
	MyName = argv[0];
	config();

	if( argc < 1 ) {
		usage(true);
	}

	bool diagnose = false;
	AttrListPrintMask print_mask;
	// parse command line arguments here.

	CondorQuery *query = new CondorQuery(STARTD_AD);
	if ( ! query) {
		fprintf (stderr, "Error:  Out of memory\n");
		exit (1);
	}

	//query->setDesiredAttrs(attr_list);

	// if diagnose was requested, just print the query ad
	if (diagnose) {

		// print diagnostic information about inferred internal state
		printf ("----------\n");

		ClassAd queryAd;
		QueryResult qr = query->getQueryAd (queryAd);
		queryAd.fPrint (stdout);

		printf ("----------\n");
		fprintf (stderr, "Result of making query ad was:  %d\n", qr);
		exit (1);
	}

	const char * direct = "localhost";
	const char * addr = NULL; // pool ? pool->addr() : NULL;
	Daemon *dae = new Daemon( DT_STARTD, direct, addr );
	if ( ! dae) {
		fprintf (stderr, "Error:  Could not create Daemon object for %s\n", direct);
		exit (1);
	}

	if( dae->locate() ) {
		addr = dae->addr();
	}

	ClassAdList result;
	if (addr) {
		CondorError errstack;
		QueryResult qr = query->fetchAds (result, addr, &errstack);
		if (Q_OK != qr) {
			fprintf( stderr, "Error: %s\n", getStrQueryResult(qr) );
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
			exit(1);
		}
	}

	// extern int mySortFunc(AttrList*,AttrList*,void*);
	// result.Sort((SortFunctionType)mySortFunc);

	// prettyPrint (result, &totals);
	result.Open();
	while (ClassAd	*ad = result.Next()) {
		print_mask.display (stdout, ad);
	}
	result.Close();

	delete query;

	return 0;
}

