#include "docker_proc.h"

DockerProc::DockerProc( ClassAd * jobAd ) : VanillaProc( jobAd ) {
	// Extract the name of the desired Docker image from jobAd
	// Ask Docker which cgroup it's going to use?
	// Steal code from os_proc::StartJob() to do the stdio mangling
	// ...

	// We can run 'docker pull' to verify that it can find the image
	// before we run 'docker run' to actually start it.  In fact, if
	// we feel like being clever, we could have the startd use config
	// to do the pull before we run a docker job at all, and advertise
	// which ones we will know will work...

	// To see currently-installed images, run 'docker images'.

	// If we supply a name to 'docker run', we can use 'docker ps' to
	// link the name to the (short) container ID.  We can then pass that
	// to 'docker inspect' to get the (long) container ID, among other
	// interesting bits.  The (long) container ID will let use grovel
	// around in /sys/fs/cgroup if we need to (for usage information?).

	// The 'docker run' command hangs around for the duration of the
	// instance, which means we can probably hand its PID off to UserProc
	// and/or OSProc like we hand off the VM GAHP's PID.

	// We should be able to leverage the procd for usage-gathering if
	// we're clever -- assuming the (long) container ID suffices for
	// its cgroup magic.

	// We should run with '-v /path/to/sandbox:/inner-path/to/sandbox'.
	// .. and the IWD should probably be /inter-path/to/sandbox.  Use '-w'.

	// TODO for laterz: figure out this self-hosting magic.
}

DockerProc::~DockerProc() { }

int DockerProc::StartJob() {
	return VanillaProc::StartJob();
}

bool DockerProc::JobReaper( int pid, int status ) {
	return VanillaProc::JobReaper( pid, status );
}

bool DockerProc::JobExit() {
	return VanillaProc::JobExit();
}

void DockerProc::Suspend() {
	VanillaProc::Suspend();
}

void DockerProc::Continue() {
	VanillaProc::Continue();
}

bool DockerProc::Remove() {
	return VanillaProc::Remove();
}

bool DockerProc::Hold() {
	return VanillaProc::Hold();
}

bool DockerProc::ShutdownGraceful() {
	return VanillaProc::ShutdownGraceful();
}

bool DockerProc::ShutdownFast() {
	return VanillaProc::ShutdownFast();
}

bool DockerProc::PublishUpdateAd( ClassAd * jobAd ) {
	return VanillaProc::PublishUpdateAd( jobAd );
}

void DockerProc::PublishToEnv( Env * env ) {
	return VanillaProc::PublishToEnv( env );
}
