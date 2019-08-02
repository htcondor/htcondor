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
#include "match_prefix.h"
#include "param_info.h" // access to default params
#include "param_info_tables.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "classad_collection.h"
#include "dprintf_internal.h"
#include <stdlib.h>

void PREFAST_NORETURN
my_exit( int status )
{
	fflush( stdout );
	fflush( stderr );

	if ( ! status ) {
		clear_global_config_table();
	}

	exit( status );
}

// consume the next command line argument, otherwise return an error
// note that this function MAY change the value of i
//
#if 0 // future use
static const char * use_next_arg(const char * arg, const char * argv[], int & i)
{
	if (argv[i+1]) {
		return argv[++i];
	}

	fprintf(stderr, "-%s requires an argument\n", arg);
	//usage();
	my_exit(1);
	return NULL;
}
#endif

static int validate_queue(const char * filename, int max_logs, bool verbose);

bool dash_verbose = false;
const char * queue_filename = NULL;
int queue_rotations = 1;


int main(int argc, const char ** argv)
{
	bool show_usage = true;
	bool fix_errors = false;

	int ix = 1;
	while (ix < argc) {
		if (is_dash_arg_prefix(argv[ix], "help", -1)) {
			show_usage = true;
			break;
		} else if (is_dash_arg_prefix(argv[ix], "verbose", 1)) {
			dash_verbose = true;
		} else if (is_dash_arg_prefix(argv[ix], "fix", 1)) {
			fix_errors = true;
		} else if (argv[ix][0] == '-' || argv[ix][0] == '?') {
			fprintf(stderr, "unknown argument: %s\n", argv[ix]);
			my_exit(1);
		} else {
			if ( ! queue_filename) {
				queue_filename = argv[ix];
				show_usage = false;
			} else {
				fprintf(stderr, "queue filename already specified - '%s' is not valid\n", argv[ix]);
				my_exit(1);
			}
		}
		++ix;
	}

	if (show_usage) {
		fprintf(stderr, "Usage: %s [-help | -verbose] [<classad_log_filename>]\n", argv[0]);
		my_exit(0);
	}

	// tell the dprintf code to write to stderr
	dprintf_output_settings my_output;
	my_output.choice = (1<<D_ALWAYS) | (1<<D_ERROR);
	my_output.accepts_all = true;
	my_output.logPath = "2>";
	my_output.HeaderOpts = D_NOHEADER;
	my_output.VerboseCats = 0;
	dprintf_set_outputs(&my_output, 1);

	// the queue_rotations option also controls the code that auto-creates a new log if the current one is corrupt
	// positive values of queue_rotations will "fix" the log, negative values will not.
	if (fix_errors) {
		if (queue_rotations < 0) queue_rotations = -queue_rotations;
	} else {
		if (queue_rotations > 0) queue_rotations = -queue_rotations;
	}

	if (queue_filename) {
		return validate_queue(queue_filename, queue_rotations, dash_verbose);
	}

	return 0;
}


static int validate_queue(const char * filename, int max_logs, bool verbose)
{
	ClassAdCollection * JobQueue = new ClassAdCollection(NULL, filename, max_logs);

	ClassAd * ad;
	if ( ! JobQueue->LookupClassAd("0.0", ad)) {
		fprintf(stderr, "Header (0.0) classad is missing!\n");
		return 1;
	} else {
		int next_cluster_num = 0;
		if (ad->LookupInteger(ATTR_NEXT_CLUSTER_NUM, next_cluster_num)) {
			if (verbose) {
				fprintf(stderr, "NextClusterNum is %d\n", next_cluster_num); 
			}
		}
	}

	std::string key;
	JobQueue->StartIterateAllClassAds();
	while (JobQueue->IterateAllClassAds(ad,key)) {
		const char *tmp = key.c_str();
		if ( tmp[0] == '0' ) continue;	// skip cluster & header ads
	}

	return 0;
}
