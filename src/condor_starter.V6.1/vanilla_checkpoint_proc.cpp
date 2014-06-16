#include "condor_common.h"
#include "vanilla_checkpoint_proc.h"
#include "my_popen.h"

#include "starter.h"
extern CStarter * Starter;

#include <sys/types.h>
#include <dirent.h>

#include "condor_vm_universe_types.h"

#define	INPUT_NAME	"_condor_sandbox.tar.gz"
#define	STATUS_NAME	"_condor_vm_classad"
#define	OUTPUT_NAME	"_condor_output_disk"
#define	EXIT_NAME	"_condor_exitcode"

VanillaCheckpointProc::VanillaCheckpointProc( ClassAd * jobAd ) : VanillaProc( jobAd ), vmProc( NULL ), shuttingDown( false ) {
}

VanillaCheckpointProc::~VanillaCheckpointProc() {
	if( vmProc != NULL ) { delete vmProc; }
}

bool VanillaCheckpointProc::CreateInitialDisks() {
	//
	// Create the input tarball "disk".  Note that we use gzip not for
	// efficiency (although it doesn't hurt), but because it doesn't care
	// about trailing zeros.
	//

	const char * tarBin = param( "TAR" );

	ArgList tarArgs;
	tarArgs.AppendArg( tarBin );
	tarArgs.AppendArg( "-c" );
	tarArgs.AppendArg( "-z" );
	tarArgs.AppendArg( "-f" );
	tarArgs.AppendArg( INPUT_NAME );

	const char * sandbox = Starter->jic->jobRemoteIWD();
	DIR * d = opendir( sandbox );
	if( d == NULL ) {
		dprintf( D_ALWAYS, "Unable to opendir(%s).\n", sandbox );
		return false;
	}

	errno = 0;
	struct dirent * de = NULL;
	while( (de = readdir( d )) != NULL ) {
		if( ( strcmp( de->d_name, "." ) == 0 ) || ( strcmp( de->d_name, ".." ) == 0 ) ) {
			continue;
		}
		tarArgs.AppendArg( de->d_name );
	}
	if( errno ) {
		dprintf( D_ALWAYS, "Unable to readdir(%s). (%d: %s)\n", sandbox, errno, strerror( errno ) );
		return false;
	}

	MyString displayString;
	tarArgs.GetArgsStringForDisplay( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to create input tarball with command '%s'\n", displayString.Value() );
	int rv = my_system( tarArgs );

	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable to create input tarball with command '%s' (%d).\n", displayString.Value(), rv );
		return false;
	}


	// KVM (correctly) truncates disk images to the nearest 512-byte sector.
	// Pad the tarball out so it isn't truncated, but use the the "advanced
	// format" sector size of 4K for future-proofing.
	struct stat tarStat;
	rv = stat( INPUT_NAME, & tarStat );
	if( rv == -1 ) {
		dprintf( D_ALWAYS, "Failed to stat() tarball (%d: %s)\n", errno, strerror( errno ) );
		return false;
	}

	unsigned long unpadded = tarStat.st_size % 4096;
	if( unpadded != 0 ) {
		int fd = safe_open_no_create( INPUT_NAME, O_WRONLY | O_APPEND );
		if( fd == -1 ) {
			dprintf( D_ALWAYS, "Unable to open() tarball (%d: %s)\n", errno, strerror( errno ) );
			return false;
		}

		unsigned char zeros[4096];
		memset( zeros, 0, 4096 );
		unsigned long length = 4096 - unpadded;
		while( length > 0 ) {
			rv = write( fd, zeros, length );
			if( rv == -1 ) {
				dprintf( D_ALWAYS, "Unable to write() to tarball (%d: %s)\n", errno, strerror( errno ) );
				return false;
			}
			length -= rv;
		}

		while( close( fd ) == EINTR ) { ; }
	}

	// There's an optimization opportunity here: we could have the internal
	// starter notify us when it's finished with the input tarball, and we
	// could delete it.  Another possibility is to unconditionally remove it
	// from the intermediate file transfer list (we'll clean it up before
	// transferring output), and add logic to recreate it on restart.  This
	// could be combined with the first optimization, although both should
	// be rare.  We could also refuse to take a checkpoint until input
	// transfer is done, which would eliminate this problem entirely.
	//
	// Of course, with a cooperative process on the inside, we could also
	// stream the tarball (via files or virtio serial), and save a lot of
	// peak disk-space usage.
	//
	// There's also an optimization to be made during a given instance's
	// first file-transfer (although that code is elsewhere):: there's no
	// reason to transfer the input files a second time, since we're keeping
	// the input disk.

	//
	// Pre-allocate the classad "disk".
	//
	int fd = safe_create_fail_if_exists( STATUS_NAME, O_WRONLY );
	if( fd == -1 ) {
		dprintf( D_ALWAYS, "Unable to open() monitor file (%d: %s)\n", errno, strerror( errno ) );
		return false;
	}

	while( (rv = posix_fallocate( fd, 0, 4096 * 4 )) == EINTR ) { ; }
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable to pre-allocate monitor file (%d: %s)\n", rv, strerror( rv ) );
		return false;
	}
	while( close( fd ) == EINTR ) { ; }

	// Note that we /want/ to maintain the STATUS_NAME file across checkpoints;
	// it's small (and therefore cheap), and it's easier than adding the logic
	// to recreate it, plus it means that we don't have to worry about partial
	// writes becoming inconsistent (partially zero).


	//
	// Pre-allocate the output "disk".
	//
	fd = safe_create_fail_if_exists( OUTPUT_NAME, O_WRONLY );
	if( fd == -1 ) {
		dprintf( D_ALWAYS, "Unable to open() output disk (%d: %s)\n", errno, strerror( errno ) );
		return false;
	}

	int outputSizeInKilobytes = 1024 * 1024;
	JobAd->EvaluateAttrInt( ATTR_REQUEST_DISK, outputSizeInKilobytes );
	unsigned fallocateBlocks = (outputSizeInKilobytes / 4) +
		( ((outputSizeInKilobytes % 4) == 0) ? 0 : 1 );
	while( (rv = posix_fallocate( fd, 0, 4096 * fallocateBlocks )) == EINTR ) { ; }
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "Unable to pre-allocate monitor file (%d: %s)\n", rv, strerror( rv ) );
		return false;
	}
	while( close( fd ) == EINTR ) { ; }


	//
	// Success!
	//
	return true;
}

int VanillaCheckpointProc::StartJob() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::StartJob()\n" );

	//
	// TO DO: The vanilla universe encourages users to supply resource
	// requirements (the Request* attributes), which we don't currently
	// forward to the VM.  In particular, it should be possible to enforce
	// memory and disk requests without using cgroups.  However, the VM
	// limits include overheads that don't exist for vanilla-universe jobs,
	// which must be accounted for during matchmaking if we're going to
	// enforce the limits.
	//


	//
	// At least in our initial implementation, where the VMs are under the
	// administrator's control, we don't want to give the user control over
	// the VM; therefore, use a ClassAd supplied by the administrator to
	// initialize the vmProc.
	//

	std::string vmClassAdPath;
	param( vmClassAdPath, "VM_CLASSAD_FILE" );
	if( vmClassAdPath.empty() ) {
		dprintf( D_ALWAYS, "VM_CLASSAD_FILE must be set to checkpoint vanilla universe jobs.\n" );
		return FALSE;
	}

	FILE * vmClassAdFile = safe_fopen_wrapper_follow( vmClassAdPath.c_str(), "r" );
	if( ! vmClassAdFile ) {
		dprintf( D_ALWAYS, "Failed to open VM_CLASSAD_FILE (%s), which is required to checkpoint vanilla universe jobs.\n", vmClassAdPath.c_str() );
		return FALSE;
	}

	// VMProc requires a classad allocated on the heap.
	int isEOF = 0, isError = 0, isEmpty = 0;
	ClassAd * vmClassAd = new ClassAd( vmClassAdFile, NULL, isEOF, isError, isEmpty );

	if( isError ) {
		dprintf( D_ALWAYS, "Failed to parse VM_CLASSAD_FILE (%s).\n", vmClassAdPath.c_str() );
		return FALSE;
	}

	if( isEmpty ) {
		dprintf( D_ALWAYS, "VM_CLASSAD_FILE (%s) is empty.\n", vmClassAdPath.c_str() );
		return FALSE;
	}

	if( ! isEOF ) {
		dprintf( D_ALWAYS, "VM_CLASSAD_FILE (%s) not read completely.\n", vmClassAdPath.c_str() );
		return FALSE;
	}

	ExprTree * expr = NULL;
	const char * name = NULL;
	vmClassAd->ResetExpr();
	while( vmClassAd->NextExpr( name, expr ) ) {
		dprintf( D_FULLDEBUG, "Found attribute '%s' in VM_CLASSAD_FILE.\n", name );
	}

	// Fail early on invalid input.
	std::string vmDiskString;
	if( ! vmClassAd->LookupString( VMPARAM_VM_DISK, vmDiskString ) ) {
		dprintf( D_ALWAYS, "VM_CLASSAD_FILE has no disk definition (%s).\n", VMPARAM_VM_DISK );
		return false;
	}

	if( vmDiskString.empty() ) {
		dprintf( D_ALWAYS, "VM_CLASSAD_FILE has empty disk definition.\n" );
		return false;
	}

	//
	// Don't adjust the transferred files if we're resuming from a checkpoint.
	//
	int lastCheckpointTime = 0;
	bool hasLastCkptTime = JobAd->EvaluateAttrInt( ATTR_LAST_CKPT_TIME, lastCheckpointTime );
	if( (! hasLastCkptTime) || lastCheckpointTime == 0 ) {
		if( ! CreateInitialDisks() ) { return FALSE; }
	}

	//
	// Adding the initial disks to the XML helps the VM GAHP rewrite the
	// saved state of the VM to reflect the new on-disk locations.
	//
	// Something along the way blithely ignores the actual device, so 'vdx'
	// could easily end up being '/dev/vdb'... *sigh*
	//
	formatstr( vmDiskString, "%s,%s:vdx:w:raw,%s:vdy:w:raw,%s:vdz:w:raw",
		vmDiskString.c_str(), INPUT_NAME, STATUS_NAME, OUTPUT_NAME );
	vmClassAd->InsertAttr( VMPARAM_VM_DISK, vmDiskString );

	//
	// The administrator's static job ad doesn't know certain things about
	// the dynamic ad that the VM universe requires.  Insert them here.
	//

	vmClassAd->CopyAttribute( ATTR_CLUSTER_ID, ATTR_CLUSTER_ID, JobAd );
	vmClassAd->CopyAttribute( ATTR_PROC_ID, ATTR_PROC_ID, JobAd );
	vmClassAd->CopyAttribute( ATTR_JOB_CMD, ATTR_JOB_CMD, JobAd );
	vmClassAd->CopyAttribute( ATTR_USER, ATTR_USER, JobAd );
	vmClassAd->CopyAttribute( ATTR_TRANSFER_INTERMEDIATE_FILES, ATTR_TRANSFER_INTERMEDIATE_FILES, JobAd );

	// Do we leak vmClassAd, or does vmProc become responsible for it?
	vmProc = new VMProc( vmClassAd );


	//
	// Finally, go ahead and actually start the vmProc.  We copy the JobPid
	// so that the reaper in baseStarter will actually fire (GetJobPid() is
	// not virtual, so we can't indirect there).
	//

	int result = vmProc->StartJob();
	JobPid = vmProc->GetJobPid();
	return result;
}

bool VanillaCheckpointProc::JobReaper( int pid, int status ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::JobReaper()\n" );

	//
	// Don't do any of this unless we're actually done with the job.  We
	// can't do it in JobExit(), because that's called /after/ file transfer.
	//
	if( shuttingDown ) {
		return vmProc->JobReaper( pid, status );
	}

	//
	// Clean up the sandbox.
	//

	// Remove the input "disk".  This will free up some space for extracting
	// files from the output "disk".
	unlink( INPUT_NAME );

	// Do we need to do one last check of the status "disk" here?
	unlink( STATUS_NAME );

	// Extract the output disk.
	const char * tarBin = param( "TAR" );

	ArgList tarArgs;
	tarArgs.AppendArg( tarBin );
	tarArgs.AppendArg( "-x" );
	tarArgs.AppendArg( "-z" );
	tarArgs.AppendArg( "-f" );
	tarArgs.AppendArg( OUTPUT_NAME );

	MyString displayString;
	tarArgs.GetArgsStringForDisplay( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to extract output tarball with command '%s'\n", displayString.Value() );
	int rv = my_system( tarArgs );

	if( rv != 0 ) {
		// FIXME: Do we consider this HTCondor's problem?  If so, will
		// returning FALSE here cause the job to re-run?  If so, do we
		// want to retry from the last checkpoint or what?
		dprintf( D_ALWAYS, "Unable to extract output tarball with command '%s' (%d).\n", displayString.Value(), rv );
		vmProc->JobReaper( pid, status );
		return FALSE;
	}

	// If the VM output includes '_condor_exitcode' file, use that as the
	// job's exit code.  (FIXME: when we get an internal/virtual starter,
	// this chore should be handled by the job ad issued by grid shell mode.)
	// Note that we don't presently handle signals, although could probably
	// work around that if we had to by extending exitString and playing
	// games in rc.local in the VM.
	int fd = safe_open_no_create( EXIT_NAME, O_RDONLY );
	if( fd != -1 ) {
		char exitString[6];
		ssize_t bytesRead = read( fd, exitString, 5 );
		if( bytesRead != -1 ) {
			exitString[bytesRead] = '\0';
			char * endptr = exitString;
			long exitCode = strtol( exitString, & endptr, 10 );
			if( endptr != exitString ) {
				dprintf( D_ALWAYS, "Job exit code %ld\n", exitCode );
				status = exitCode << 8;
			}
		}
		close( fd );

		unlink( EXIT_NAME );
	}

	unlink( OUTPUT_NAME );
	return vmProc->JobReaper( pid, status );
}

bool VanillaCheckpointProc::JobExit() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::JobExit()\n" );
	return vmProc->JobExit();
}

void VanillaCheckpointProc::Suspend() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Suspend()\n" );
	vmProc->Suspend();
}

void VanillaCheckpointProc::Continue() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Continue()\n" );
	vmProc->Continue();
}

bool VanillaCheckpointProc::Remove() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Remove()\n" );
	return vmProc->Remove();
}

bool VanillaCheckpointProc::Hold() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Hold()\n" );
	return vmProc->Hold();
}

bool VanillaCheckpointProc::Ckpt() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Ckpt()\n" );
	return vmProc->Ckpt();
}

void VanillaCheckpointProc::CkptDone( bool success ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::CkptDone()\n" );
	vmProc->CkptDone( success );
}

bool VanillaCheckpointProc::ShutdownGraceful() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::ShutdownGraceful()\n" );
	shuttingDown = true;
	return vmProc->ShutdownGraceful();
}

bool VanillaCheckpointProc::ShutdownFast() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::ShutdownFast()\n" );
	shuttingDown = true;
	return vmProc->ShutdownFast();
}

bool VanillaCheckpointProc::PublishUpdateAd( ClassAd * jobAd ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::PublishUpdateAd()\n" );
	return vmProc->PublishUpdateAd( jobAd );
}

void VanillaCheckpointProc::PublishToEnv( Env * env ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::PublishToEnv()\n" );
	vmProc->PublishToEnv( env );
}
