#include "condor_common.h"
#include "condor_new_classads.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_universe.h"
#include "VanillaToGrid.h"
#define WANT_NAMESPACES
#include "classad_distribution.h"


bool VanillaToGrid::vanillaToGrid(classad::ClassAd * ad, const char * gridresource)
{
	ASSERT(ad);
	ASSERT(gridresource);

	/* TODO:
		- If job fails to specify transfer_input_files but has some (relying on
		  a shared filesystem), we're doomed.  Probably
		  if(should_transfer_files is NO, we should just fail.
		  (We'll do the right thing if grid_type=condor, but not other
		  grid types.)

		- Job may be relying on Condor automatically transfer output files.  If
		  transfer_output_files is set, we're probably good (again, assuming no
		  shared filesystem).  If it's not set, we can't tell the difference
		  between no output files and "everything"
		  (We'll do the right thing if grid_type=condor, but not other
		  grid types.)

		- I'm leaving WhenToTransferFiles alone.  It should work fine.

		- Should I leave FilesystemDomain in?  May be useful, but
		simultaneously will likely do the Wrong Thing on the remote
		side
	*/

	MyString remoteattr;
	remoteattr = "Remote_";
	remoteattr += ATTR_JOB_UNIVERSE;

	// Things we don't need or want
	ad->Delete(ATTR_CLUSTER_ID); // Definate no-no
	ad->Delete(ATTR_PROC_ID);

	ad->Delete(ATTR_JOB_STATUS); // Better be Idle?

	ad->Delete(ATTR_BUFFER_BLOCK_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete(ATTR_BUFFER_SIZE);
	ad->Delete(ATTR_JOB_COMMITTED_TIME);
	ad->Delete(ATTR_COMPLETION_DATE);
	ad->Delete("CondorPlatform"); // TODO: Find #define
	ad->Delete("CondorVersion");  // TODO: Find #define
	ad->Delete(ATTR_CORE_SIZE);
	ad->Delete(ATTR_CUMULATIVE_SUSPENSION_TIME);
	ad->Delete(ATTR_CURRENT_HOSTS);
	ad->Delete(ATTR_ON_EXIT_BY_SIGNAL);
	ad->Delete(ATTR_JOB_EXIT_STATUS);
	ad->Delete(ATTR_GLOBAL_JOB_ID); // Store in different ATTR here?
	ad->Delete(ATTR_LAST_SUSPENSION_TIME);
	ad->Delete(ATTR_JOB_LOCAL_SYS_CPU);
	ad->Delete(ATTR_JOB_LOCAL_USER_CPU);
	ad->Delete(ATTR_NUM_CKPTS);
	ad->Delete(ATTR_NUM_RESTARTS);
	ad->Delete(ATTR_NUM_SYSTEM_HOLDS);
	ad->Delete(ATTR_OWNER); // How does schedd filter? 
	ad->Delete(ATTR_Q_DATE);
	ad->Delete(ATTR_JOB_REMOTE_SYS_CPU);
	ad->Delete(ATTR_JOB_REMOTE_USER_CPU);
	ad->Delete(ATTR_JOB_REMOTE_WALL_CLOCK);
	ad->Delete(ATTR_SERVER_TIME);
	ad->Delete(ATTR_TOTAL_SUSPENSIONS);

	ad->Delete(ATTR_MY_TYPE); // Shoul
	ad->Delete(ATTR_TARGET_TYPE); // Should be implied.
	// Set the grid resource
	ad->InsertAttr(ATTR_GRID_RESOURCE, gridresource);

	// Remap the universe
	classad::ExprTree * tmp;
	tmp = ad->Lookup(ATTR_JOB_UNIVERSE);
	if( ! tmp ) {
		EXCEPT("VanillaToGrid: job ad lacks universe");
	}
	classad::ExprTree * olduniv = tmp->Copy();
	if( ! olduniv) {
		EXCEPT("Unable to copy old universe");
	}

	ad->InsertAttr(ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GRID);
	ad->Insert(remoteattr.Value(), olduniv);
		// olduniv is now controlled by ClassAd

	return true;
}





void die(const char * s) {
	printf("%s\n", s);
	exit(1);
}

MyString slurp_file(const char * filename) {
	int fd = open("classadin", O_RDONLY);
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
	FILE * f = fopen(filename, "w");
	if( ! f ) {
		die("Failed to open output");
	}
	fprintf(f, "%s\n", data);
	fclose(f);
}

int main(int argc, char **argv) 
{

	// How hard can it be to take a file containing an old classad (in string
	// form) and turn it into a new classad object?

	// Load old classad string.
	MyString s_oldad = slurp_file("classadin");
	// Convert to old class ad.
	ClassAd oldad((char *)s_oldad.Value(),'\n');

	// Convert to new classad string
	NewClassAdUnparser oldtonew;
	oldtonew.SetOutputType(true);
	oldtonew.SetOutputTargetType(true);
	MyString s_newad;
	oldtonew.Unparse(&oldad, s_newad);

	// Convert to new classad
	classad::ClassAdParser parser;
	classad::ClassAd newad;
	if( ! parser.ParseClassAd(s_newad.Value(), newad, true) )
	{
		die("Failed to parse class ad\n");
	}


	// Finally, a new classad.


	//====================================================================
	// Do something interesting:
	VanillaToGrid::vanillaToGrid(&newad, "condor adesmet@puffin.cs.wisc.edu puffin.cs.wisc.edu");
	//====================================================================


	// Convert back to new classad string
	classad::ClassAdUnParser unparser;
	std::string s_newadout;
	unparser.Unparse(s_newadout, &newad);

	// Convert to old classad
	NewClassAdParser newtoold;
	ClassAd * newad2 = newtoold.ParseClassAd(s_newadout.c_str());

	// Convert to old classad string
	MyString out;
	newad2->sPrint(out);

	delete newad2;

	printf("%s\n", out.Value());

	return 0;
}
