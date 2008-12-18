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
#include "condor_classad.h"
#include "VanillaToGrid.h"
#include "submit_job.h"
#include "classad_newold.h"
#include "condor_attributes.h"
#include "basename.h"
#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"

#include <libgen.h>

const char * MYID ="vanilla2grid";



void die(const char * s) {
	printf("FATAL ERROR: %s\n", s);
	exit(1);
}

MyString slurp_file(const char * filename) {
	int fd = safe_open_wrapper(filename, O_RDONLY);
	if(fd == -1) {
		die("failed to open input");
	}
	MyString s;
	char buf[1024];
	while(true) {
		int bytes = read(fd, buf, sizeof(buf));
		for(int i = 0; i < bytes; ++i) {
			s += buf[i];
		}
		if(bytes != sizeof(buf)) {
			break;
		}
	}
	return s;
}

void write_file(const char * filename, const char * data) {
	FILE * f = safe_fopen_wrapper(filename, "w");
	if( ! f ) {
		die("Failed to open output");
	}
	fprintf(f, "%s\n", data);
	fclose(f);
}



int vanilla2grid(int argc, char **argv)
{
	if(argc != 3) {
		fprintf(stderr, "Usage:\n"
			"  %s filename 'gridurl'\n"
			"     filename - filename of classad to process\n"
			"     girdurl - Unified grid URL\n", argv[0]);
		return 1;
	}


	// Load old classad string.
	MyString s_oldad = slurp_file(argv[1]);
	// Convert to old class ad.
	ClassAd oldad((char *)s_oldad.Value(),'\n');

	classad::ClassAd newad;
	if( ! old_to_new(oldad, newad) ) {
		die("Failed to parse class ad\n");
	}

	int orig_cluster;
	if( ! newad.EvaluateAttrInt(ATTR_CLUSTER_ID, orig_cluster) ) {
		dprintf(D_ALWAYS, "Vanilla job lacks a cluster\n");
		return 1;
	}
	int orig_proc;
	if( ! newad.EvaluateAttrInt(ATTR_PROC_ID, orig_proc) ) {
		dprintf(D_ALWAYS, "Vanilla job lacks a proc\n");
		return 1;
	}

	//====================================================================
	// Do something interesting:
	VanillaToGrid::vanillaToGrid(&newad, argv[2]);

	printf("Claiming job %d.%d\n", orig_cluster, orig_proc);
	MyString errors;
	switch(claim_job(newad, NULL, NULL, orig_cluster, orig_proc, &errors, MYID))
	{
		case CJR_OK:
			break;
		case CJR_BUSY:
			fprintf(stderr, "Unable to claim original job %d.%d because it's busy (%s)\n.  Continuing anyway.\n", orig_cluster, orig_proc, errors.Value());
			break;
		case CJR_ERROR:
			fprintf(stderr, "Unable to claim original job %d.%d because of an error: %s\n.  Continuing anyway.\n", orig_cluster, orig_proc, errors.Value());
			break;
	}

	int cluster,proc;
	if( ! submit_job( newad, 0, 0, &cluster, &proc ) ) {
		fprintf(stderr, "Failed to submit job\n");
	}
	printf("Successfully submitted %d.%d\n",cluster,proc);


	if(0) // Print the transformed add.
	{
		ClassAd oldadout;
		new_to_old(newad, oldadout); 
		// Convert to old classad string
		MyString out;
		oldadout.sPrint(out);
		printf("%s\n", out.Value());
	}


	return 0;
}

bool load_classad_from_old_file(const char * filename, classad::ClassAd & ad_new) {
	// Load old classad strings.
	MyString ad_string = slurp_file(filename);
	// Convert to old class ads.
	ClassAd ad_old((char *)ad_string.Value(),'\n');
	// Convert to new class ads
	return old_to_new(ad_old, ad_new);
}

int grid2vanilla(int argc, char **argv)
{
	if(argc != 3) {
		fprintf(stderr, "Usage:\n"
			"  %s vanillafile gridfile\n", argv[0]);
		return 1;
	}

	classad::ClassAd n_ad_van;
	if( ! load_classad_from_old_file(argv[1], n_ad_van) ) {
		die("Failed to parse vanilla class ad\n");
	}
	classad::ClassAd n_ad_grid;
	if( ! load_classad_from_old_file(argv[2], n_ad_grid) ) {
		die("Failed to parse grid class ad\n");
	}

	// Get old (vanilla) cluster.proc
	int vcluster;
	if( ! n_ad_van.EvaluateAttrInt(ATTR_CLUSTER_ID, vcluster) ) {
		dprintf(D_ALWAYS, "Vanilla job lacks a cluster\n");
		return 1;
	}
	int vproc;
	if( ! n_ad_van.EvaluateAttrInt(ATTR_PROC_ID, vproc) ) {
		dprintf(D_ALWAYS, "Vanilla job lacks a proc\n");
		return 1;
	}

	// Get new (grid) cluster.proc
	int cluster;
	if( ! n_ad_grid.EvaluateAttrInt(ATTR_CLUSTER_ID, cluster) ) {
		dprintf(D_ALWAYS, "Grid job lacks a cluster\n");
		return 1;
	}
	int proc;
	if( ! n_ad_grid.EvaluateAttrInt(ATTR_PROC_ID, proc) ) {
		dprintf(D_ALWAYS, "Grid job lacks a proc\n");
		return 1;
	}

	// Right. That was a silly amount of prep work.

	// Update the original ad
	n_ad_van.ClearAllDirtyFlags();
	if( ! update_job_status( n_ad_van, n_ad_grid) ) {
		dprintf(D_ALWAYS, "Unable to update ad\n");
		return 1;
	}

	// Push the updates!
	push_dirty_attributes(n_ad_van, 0, 0);

	// Put a fork in the grid job.
	bool b = finalize_job(n_ad_van,cluster,proc,0,0);
	printf("Finalize attempt on %d.%d %s\n", cluster,proc,b?"succeeded":"failed");
	if( ! b ) { return 1; }

	// Yield the original job
	MyString errors;
	b = yield_job(n_ad_van,0,0,true, vcluster, vproc, &errors, MYID);
	printf("Yield attempt on %d.%d %s\n", vcluster, vproc, b?"succeeded":"failed");
	return b?0:1;
}

int main(int argc, char **argv) 
{
	config(); // Initialize param.
	const char * progname = condor_basename(argv[0]);
	if(progname[0] == 'v') {
		return vanilla2grid(argc,argv);
	} else if(progname[0] == 'g') {
		return grid2vanilla(argc,argv);
	} else {
		fprintf(stderr, "Program must be named vanilla2grid or grid2vanilla\n");
		return 1;
	}

}


#include "daemon_core_stubs.h"
