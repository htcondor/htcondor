#include "condor_common.h"
#include "vanilla_checkpoint_proc.h"

VanillaCheckpointProc::VanillaCheckpointProc( ClassAd * jobAd ) : VanillaProc( jobAd ), vmProc( NULL ) {
}

VanillaCheckpointProc::~VanillaCheckpointProc() {
	if( vmProc != NULL ) { delete vmProc; }
}

int VanillaCheckpointProc::StartJob() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::StartJob()\n" );

	//
	// At least in our initial implementation, where the VMs are under the
	// administrator's control, we don't want to give the user control over
	// the VM; therefore, use a ClassAd supplied by the administrator to
	// initialize the vmProc.
	//

	//
	// TO DO: The vanilla universe encourages users to supply resource
	// requirements (the Request* attributes), which we don't currently
	// forward to the VM.  In particular, it should be possible to enforce
	// memory and disk requests without using cgroups.  However, the VM
	// limits include overheads that don't exist for vanilla-universe jobs,
	// which must be accounted for during matchmaking if we're going to
	// enforce the limits.
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

	//
	// The administrator's static job ad doesn't know certain things about
	// the dynamic ad that the VM universe requires.  Insert them here.
	//

	vmClassAd->CopyAttribute( ATTR_CLUSTER_ID, ATTR_CLUSTER_ID, JobAd );
	vmClassAd->CopyAttribute( ATTR_PROC_ID, ATTR_PROC_ID, JobAd );
	vmClassAd->CopyAttribute( ATTR_JOB_CMD, ATTR_JOB_CMD, JobAd );
	vmClassAd->CopyAttribute( ATTR_USER, ATTR_USER, JobAd );
	vmClassAd->CopyAttribute( ATTR_TRANSFER_INTERMEDIATE_FILES, ATTR_TRANSFER_INTERMEDIATE_FILES, JobAd );
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
	// return VanillaProc::JobReaper( pid, status );
	return vmProc->JobReaper( pid, status );
}

bool VanillaCheckpointProc::JobExit() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::JobExit()\n" );
	// return VanillaProc::JobExit();
	return vmProc->JobExit();
}

void VanillaCheckpointProc::Suspend() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Suspend()\n" );
	// VanillaProc::Suspend();
	vmProc->Suspend();
}

void VanillaCheckpointProc::Continue() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Continue()\n" );
	// VanillaProc::Continue();
	vmProc->Continue();
}

bool VanillaCheckpointProc::Remove() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Remove()\n" );
	// return VanillaProc::Remove();
	return vmProc->Remove();
}

bool VanillaCheckpointProc::Hold() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Hold()\n" );
	// return VanillaProc::Hold();
	return vmProc->Hold();
}

bool VanillaCheckpointProc::Ckpt() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Ckpt()\n" );

	//
	// If we're only supporting the self-checkpointing model of user-level
	// checkpoints, we don't need to do anything here -- we just require that
	// the application never partially-overwrites its own checkpoints.
	//

	//
	// On the other hand, this only provides a benefit if you're worried
	// about the machine going away, rather than the being preempted.  If
	// that's your only concern, ON_EXIT_OR_EVICT will work just as well.
	//

	//
	// Therefore, we instead support sending the process its soft_kill_sig.
	//
	// FIXME: do this look-up once in the constructor and save the result.
	// FIXME: We don't currently have a mechanism for the application to
	// signal the starter that it finished taking its checkpoint.  While
	// this would undoubtedly be useful, the practical effect is just going
	// to be that the checkpoint on the submitter is always one checkpoint
	// behind.
	//
	int userLevelCheckpoint = 0;
	JobAd->LookupBool( "UserLevelCheckpoint", userLevelCheckpoint );
	if( userLevelCheckpoint ) {
		// The UserProc class sets this via findSoftKillSig(), which uses
		// the job attribute 'kill_sig', as defined in the manual.
		// TO DO: would it make more sense to define different soft-kill
		// and checkpointing signals?
		daemonCore->Send_Signal( JobPid, soft_kill_sig );
		return true;
	}

	return vmProc->Ckpt();
}

void VanillaCheckpointProc::CkptDone( bool success ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::CkptDone()\n" );

	int userLevelCheckpoint = 0;
	JobAd->LookupBool( "UserLevelCheckpoint", userLevelCheckpoint );
	if( userLevelCheckpoint ) {
		// This presently does nothing, but in case that changes...
		VanillaProc::CkptDone( success );
		return;
	}

	vmProc->CkptDone( success );
}

bool VanillaCheckpointProc::ShutdownGraceful() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::ShutdownGraceful()\n" );
	// return VanillaProc::ShutdownGraceful();
	return vmProc->ShutdownGraceful();
}

bool VanillaCheckpointProc::ShutdownFast() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::ShutdownFast()\n" );
	// return VanillaProc::ShutdownFast();
	return vmProc->ShutdownFast();
}

bool VanillaCheckpointProc::PublishUpdateAd( ClassAd * jobAd ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::PublishUpdateAd()\n" );
	// return VanillaProc::PublishUpdateAd( jobAd );
	return vmProc->PublishUpdateAd( jobAd );
}

void VanillaCheckpointProc::PublishToEnv( Env * env ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::PublishToEnv()\n" );
	// VanillaProc::PublishToEnv( env );
	vmProc->PublishToEnv( env );
}
