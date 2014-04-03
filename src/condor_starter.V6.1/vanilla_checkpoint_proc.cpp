#include "condor_common.h"
#include "vanilla_checkpoint_proc.h"

VanillaCheckpointProc::VanillaCheckpointProc( ClassAd * jobAd ) : VanillaProc( jobAd ), vmProc( NULL ) {
}

VanillaCheckpointProc::~VanillaCheckpointProc() {
}

int VanillaCheckpointProc::StartJob() {
	dprintf( D_FULLDEBUG, "Entering VanillaCheckpointProc::StartJob()\n" );
	return VanillaProc::StartJob();
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
