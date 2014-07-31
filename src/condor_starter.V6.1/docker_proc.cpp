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



// move this to condor_attributes.h
#define ATTR_DOCKER_IMAGE_ID "DockerImageId"

extern CStarter *Starter;



	//
	// TO DO:  If the starter is running as root (maybe docker should be suid?)
	// we can't allow the normal ssh-to-job code to run.  We'd also probably
	// like ssh-to-job to connect to the docker/internal stuff anyway...
	//

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
	//
	// or instead docker ps -a --no-trunc for the full Docker ID.
	// This allows the docker to accumulate usage before we can look at
	// it, but there doesn't appear to be anything we can about that at
	// the moment.

	// The 'docker run' command hangs around for the duration of the
	// instance, which means we can probably hand its PID off to UserProc
	// and/or OSProc like we hand off the VM GAHP's PID.

	// We should be able to leverage the procd for usage-gathering if
	// we're clever -- assuming the (long) container ID suffices for
	// its cgroup magic.

	// We should run with '-v /path/to/sandbox:/inner-path/to/sandbox'.
	// .. and the IWD should probably be /inter-path/to/sandbox.  Use '-w'.

	// TODO for laterz: figure out this self-hosting magic.  (We want to
	// supply images via file transfer.)

	// TODO for laterz: can we configure Docker to drop its overlay filesystem
	// backing store(s) into the sandbox?  This seems like it might help to
	// ensure clean-up.
	//
	// TODO for laterz: Perhaps instead, we can steal an idea from the VM
	// universe and write the Docker ID out to disk (<sandbox>/../.dotfile)
	// so that the startd can execute the docker if the starter crashes and
	// again on startd start-up if necessary.

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

	// Warning: cargo cult approaching.
	ArgList args;
	args.SetArgV1SyntaxToCurrentPlatform();
	// My cult doesn't set argv[0] to the command.
	MyString argsError;
	if( ! args.AppendArgsFromClassAd( JobAd, & argsError ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to read job arguments from job ad: '%s'.\n", argsError.c_str() );
		return FALSE;
	}

	// We can't just use Starter->GetJobEnv() because we need to change e.g.,
	// _CONDOR_SCRATCH_DIR to be sensible for the chroot jail.
	Env e;

	std::string sandboxPath = Starter->jic->jobRemoteIWD();

	// The GlobalJobID is unsuitable by virtue its octothorpes.  This is
	// pretty good, but could be made even less likely to collide if it
	// had a timestamp.
	std::string dockerName;
	formatstr( dockerName, "%s_cluster%d_proc%d_starterPID%d",
		Starter->getMySlotName().c_str(),
		Starter->jic->jobCluster(),
		Starter->jic->jobProc(),
		getpid() );

	// We assume that DockerAPI::run() arranges for the reaper(s) to be called.
	CondorError err;
	int rv = DockerAPI::run( dockerName, imageID, command, args, e, sandboxPath, containerID, JobPid, err );
	if( rv < 0 ) {
		dprintf( D_ALWAYS | D_FAILURE, "DockerAPI::run( %s, %s, ... ) failed with return value %d\n", imageID.c_str(), command.c_str(), rv );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "DockerAPI::run() returned container ID '%s' and pid %d\n", containerID.c_str(), JobPid );

	// TO DO : don't bother with the FamilyInfo.
	// Take containerID and use it to initialize cgroup tracking.
	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);
	// FIXME: approximate?
	fi.cgroup = containerID.c_str();
	// FIXME: Necessary?
	// fi.isDocker = true;
	// Is this pid the right one?
/*
	// This method is private.  We'll have to do some DC surgery.
	if( ! daemonCore->Register_Family( JobPid, getpid(), fi.max_snapshot_interval,
							 NULL, NULL, NULL, fi.cgroup, NULL ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "Unable to register process family with the procd.\n" );
		return FALSE;
	}
*/

	// Immediately start our consumption metrics?

	return TRUE;
}

bool DockerProc::JobReaper( int pid, int status ) {
	dprintf( D_ALWAYS, "DockerProc::JobReaper()\n" );
	return VanillaProc::JobReaper( pid, status );
}

bool DockerProc::JobExit() {
	dprintf( D_ALWAYS, "DockerProc::JobExit()\n" );
	return VanillaProc::JobExit();
}

void DockerProc::Suspend() {
	dprintf( D_ALWAYS, "DockerProc::Suspend()\n" );
	VanillaProc::Suspend();
}

void DockerProc::Continue() {
	dprintf( D_ALWAYS, "DockerProc::Continue()\n" );
	VanillaProc::Continue();
}

bool DockerProc::Remove() {
	dprintf( D_ALWAYS, "DockerProc::Remove()\n" );
	return VanillaProc::Remove();
}

bool DockerProc::Hold() {
	dprintf( D_ALWAYS, "DockerProc::Hold()\n" );
	return VanillaProc::Hold();
}

bool DockerProc::ShutdownGraceful() {
	dprintf( D_ALWAYS, "DockerProc::ShutdownGraceful()\n" );
	return VanillaProc::ShutdownGraceful();
}

bool DockerProc::ShutdownFast() {
	dprintf( D_ALWAYS, "DockerProc::ShutdownFast()\n" );
	return VanillaProc::ShutdownFast();
}

bool DockerProc::PublishUpdateAd( ClassAd * jobAd ) {
	dprintf( D_ALWAYS, "DockerProc::PublishUpdateAd()\n" );
	// return VanillaProc::PublishUpdateAd( jobAd );
	return true;
}

void DockerProc::PublishToEnv( Env * env ) {
	dprintf( D_ALWAYS, "DockerProc::PublishToEnv()\n" );
	return;
}
