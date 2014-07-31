/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "condor_config.h"
#include "docker_proc.h"
#include "starter.h"
#include "docker-api.h"


// TODO: move this to condor_attributes.h
#define ATTR_DOCKER_IMAGE_ID "DockerImageId"

extern CStarter *Starter;

	//
	// TODO: If the starter is running as root (maybe docker should be suid?)
	// we can't allow the normal ssh-to-job code to run.  We'd also probably
	// like ssh-to-job to connect to the docker/internal stuff anyway...
	//

	//
	// TODO: Allow the use of HTCondor file-transfer to provide the image.
	// May require understanding "self-hosting".
	//

	//
	// TODO: Leverage the procd to gather usage information; Docker uses
	// the full container ID as (part of) the cgroup identifier(s).
	//

	//
	// TODO: Can we configure Docker to store its overlay filesystem in
	// the sandbox?  Would that be useful for cleanup, or to help admins
	// allocate disk space?
	//

	//
	// TODO: Write the container ID out to disk (<sandbox>../../.dotfile)
	// like the VM universe so that the startd can clean up if the starter
	// crashes and again, if necessary, on start-up.
	//

DockerProc::DockerProc( ClassAd * jobAd ) : VanillaProc( jobAd ) { }

DockerProc::~DockerProc() { }

int DockerProc::StartJob() {
	std::string imageID;
	if( ! JobAd->LookupString( ATTR_DOCKER_IMAGE_ID, imageID ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "%s not defined in job ad, unable to start job.\n", ATTR_DOCKER_IMAGE_ID );
		return FALSE;
	}

	std::string command;
	JobAd->LookupString( ATTR_JOB_CMD, command );
	dprintf( D_FULLDEBUG, "%s: '%s'\n", ATTR_JOB_CMD, command.c_str() );

	ArgList args;
	args.SetArgV1SyntaxToCurrentPlatform();
	MyString argsError;
	if( ! args.AppendArgsFromClassAd( JobAd, & argsError ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to read job arguments from job ad: '%s'.\n", argsError.c_str() );
		return FALSE;
	}

	// We can't just use Starter->GetJobEnv() because we need to change e.g.,
	// _CONDOR_SCRATCH_DIR to be sensible for the chroot jail.
	// TODO: pass requested job environment to Docker.
	Env e;

	std::string sandboxPath = Starter->jic->jobRemoteIWD();

	// The GlobalJobID is unsuitable by virtue its octothorpes.  This
	// construction is informative, but could be made even less likely
	// to collide if it had a timestamp.
	std::string dockerName;
	formatstr( dockerName, "%s_cluster%d_proc%d_starterPID%d",
		Starter->getMySlotName().c_str(),
		Starter->jic->jobCluster(),
		Starter->jic->jobProc(),
		getpid() );

	CondorError err;
	// DockerAPI::run() returns a PID from daemonCore->Create_Process(), which
	// makes it suitable for passing up into VanillaProc.  This combination
	// will trigger the reaper(s) when the container terminates.
	int rv = DockerAPI::run( dockerName, imageID, command, args, e, sandboxPath, containerID, JobPid, err );
	if( rv < 0 ) {
		dprintf( D_ALWAYS | D_FAILURE, "DockerAPI::run( %s, %s, ... ) failed with return value %d\n", imageID.c_str(), command.c_str(), rv );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "DockerAPI::run() returned container ID '%s' and pid %d\n", containerID.c_str(), JobPid );

	return TRUE;
}

// TODO: Implement.
bool DockerProc::JobReaper( int pid, int status ) {
	dprintf( D_ALWAYS, "DockerProc::JobReaper()\n" );
	return VanillaProc::JobReaper( pid, status );
}

// TODO: Implement.
bool DockerProc::JobExit() {
	dprintf( D_ALWAYS, "DockerProc::JobExit()\n" );
	return VanillaProc::JobExit();
}

// TODO: Implement.
void DockerProc::Suspend() {
	dprintf( D_ALWAYS, "DockerProc::Suspend()\n" );
	VanillaProc::Suspend();
}

// TODO: Implement.
void DockerProc::Continue() {
	dprintf( D_ALWAYS, "DockerProc::Continue()\n" );
	VanillaProc::Continue();
}

// TODO: Implement.
bool DockerProc::Remove() {
	dprintf( D_ALWAYS, "DockerProc::Remove()\n" );
	return VanillaProc::Remove();
}

// TODO: Implement.
bool DockerProc::Hold() {
	dprintf( D_ALWAYS, "DockerProc::Hold()\n" );
	return VanillaProc::Hold();
}

// TODO: Implement.
bool DockerProc::ShutdownGraceful() {
	dprintf( D_ALWAYS, "DockerProc::ShutdownGraceful()\n" );
	return VanillaProc::ShutdownGraceful();
}

// TODO: Implement.
bool DockerProc::ShutdownFast() {
	dprintf( D_ALWAYS, "DockerProc::ShutdownFast()\n" );
	return VanillaProc::ShutdownFast();
}

// TODO: Implement.  Do NOT call VanillaProc::PublishUpdateAd(), because it
// will report on the wrong process (the one we're using to generate a job-
// termination event).
bool DockerProc::PublishUpdateAd( ClassAd * jobAd ) {
	dprintf( D_ALWAYS, "DockerProc::PublishUpdateAd()\n" );
	return true;
}

// TODO: Implement.
void DockerProc::PublishToEnv( Env * env ) {
	dprintf( D_ALWAYS, "DockerProc::PublishToEnv()\n" );
	return;
}
