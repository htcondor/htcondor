#include "docker_proc.h"

DockerProc::DockerProc( ClassAd * jobAd ) : VanillaProc( jobAd ) {
	// Extract the name of the desired Docker image from jobAd
	// Ask Docker which cgroup it's going to use?
	// Steal code from os_proc::StartJob() to do the stdio mangling
	// ...
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
