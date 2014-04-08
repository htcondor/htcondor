#include "condor_common.h"
#include "vanilla_checkpoint_proc.h"

VanillaCheckpointProc::VanillaCheckpointProc( ClassAd * jobAd ) : VanillaProc( jobAd ), vmProc( NULL ) {
	//
	// At least in our initial implementation, where the VMs are under the
	// administrator's control, we don't want to give the user control over
	// the VM; therefore, use a ClassAd supplied by the administrator to
	// initialize the vmProc.
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
		// FIXME: abort?  reportErrorToStartd()?  return and throw the
		// error in StartJob()?
	}

	FILE * vmClassAdFile = safe_fopen_wrapper_follow( vmClassAdPath.c_str(), "r" );
	if( ! vmClassAdFile ) {
		dprintf( D_ALWAYS, "Failed to open VM_CLASSAD_FILE (%s), which is required to checkpoint vanilla universe jobs.\n", vmClassAdPath.c_str() );
		// FIXME: see above.
	}

	// VMProc requires a classad allocated on the heap.
	int isEOF = 0, isError = 0, isEmpty = 0;
	ClassAd * vmClassAd = new ClassAd( vmClassAdFile, NULL, isEOF, isError, isEmpty );

	if( isError ) {
		dprintf( D_ALWAYS, "Failed to parse VM_CLASSAD_FILE (%s).\n", vmClassAdPath.c_str() );
		// FIXME: see above.
	}

	if( isEmpty ) {
		dprintf( D_ALWAYS, "VM_CLASSAD_FILE (%s) is empty.\n", vmClassAdPath.c_str() );
		// FIXME: see above.
	}

	if( ! isEOF ) {
		dprintf( D_ALWAYS, "VM_CLASSAD_FILE (%s) not read completely.\n", vmClassAdPath.c_str() );
		// FIXME: see above.
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

	vmClassAd->CopyAttribute( ATTR_CLUSTER_ID, ATTR_CLUSTER_ID, jobAd );
	vmClassAd->CopyAttribute( ATTR_PROC_ID, ATTR_PROC_ID, jobAd );
	vmClassAd->CopyAttribute( ATTR_USER, ATTR_USER, jobAd );
	vmProc = new VMProc( vmClassAd );
}

VanillaCheckpointProc::~VanillaCheckpointProc() {
}

int VanillaCheckpointProc::StartJob() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::StartJob()\n" );
	// return VanillaProc::StartJob();
	return vmProc->StartJob();
}

bool VanillaCheckpointProc::JobReaper( int pid, int status ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::JobReaper()\n" );
	return VanillaProc::JobReaper( pid, status );
}

bool VanillaCheckpointProc::JobExit() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::JobExit()\n" );
	return VanillaProc::JobExit();
}

void VanillaCheckpointProc::Suspend() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Suspend()\n" );
	VanillaProc::Suspend();
}

void VanillaCheckpointProc::Continue() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Continue()\n" );
	VanillaProc::Continue();
}

bool VanillaCheckpointProc::Remove() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Remove()\n" );
	return VanillaProc::Remove();
}

bool VanillaCheckpointProc::Hold() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::Hold()\n" );
	return VanillaProc::Hold();
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
	}

	return true;
}

void VanillaCheckpointProc::CkptDone( bool success ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::CkptDone()\n" );
	VanillaProc::CkptDone( success );
}

bool VanillaCheckpointProc::ShutdownGraceful() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::ShutdownGraceful()\n" );
	return VanillaProc::ShutdownGraceful();
}

bool VanillaCheckpointProc::ShutdownFast() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::ShutdownFast()\n" );
	return VanillaProc::ShutdownFast();
}

bool VanillaCheckpointProc::PublishUpdateAd( ClassAd * jobAd ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::PublishUpdateAd()\n" );
	return VanillaProc::PublishUpdateAd( jobAd );
}

void VanillaCheckpointProc::PublishToEnv( Env * env ) {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::PublishToEnv()\n" );
	VanillaProc::PublishToEnv( env );
}
