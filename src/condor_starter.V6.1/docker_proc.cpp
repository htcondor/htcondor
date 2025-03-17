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
#include "directory.h"
#include "docker_proc.h"
#include "starter.h"
#include "condor_holdcodes.h"
#include "docker-api.h"
#include "condor_daemon_client.h"
#include "condor_daemon_core.h"
#include "classad_helpers.h"
#include "ToE.h"
#include "NTsenders.h"
#include "shortfile.h"

#ifdef HAVE_SCM_RIGHTS_PASSFD
#include "shared_port_scm_rights.h"
#include "fdpass.h"
#endif

extern class Starter *starter;

static void buildExtraVolumes(std::list<std::string> &extras, ClassAd &machAd, ClassAd &jobAd);

static bool handleFTL(int error) {
	if (error != 0) {
		// dprintf( D_ALWAYS, "Failed to launch Docker universe job (%s).\n", error );
	}

	//
	// If we failed to launch the job (as opposed to aborted the takeoff
	// because there was something wrong with the payload), we also need
	// to force the startd to advertise this fact so other jobs can avoid
	// this machine.
	//
	const char * nullString = nullptr;
	DCStartd startd( nullString);
	if( ! startd.locate() ) {
		dprintf( D_ERROR, "Unable to locate startd: %s\n", startd.error() );
		return false;
	}

	//
	// The startd will update the list 'OfflineUniverses' for us.
	//
	ClassAd update;
	if (error) {
		update.Assign( ATTR_HAS_DOCKER, false );
		update.Assign( "DockerOfflineReason", error );
	} else {
		update.Assign( ATTR_HAS_DOCKER, true );
	}

	ClassAd reply;
	if( ! startd.updateMachineAd( &update, &reply ) ) {
		dprintf( D_ERROR, "Unable to update machine ad: %s\n", startd.error() );
		return false;
	}

	return true;
}


//
// TODO: Allow the use of HTCondor file-transfer to provide the image.
// May require understanding "self-hosting".
//

//
// TODO: Leverage the procd to gather usage information; Docker uses
// the full container ID as (part of) the cgroup identifier(s).
//

DockerProc::DockerProc( ClassAd * jobAd ) : VanillaProc( jobAd ), updateTid(-1), memUsage(0), max_memUsage(0), netIn(0), netOut(0), userCpu(0), sysCpu(0), waitForCreate(false), execReaperId(-1), shouldAskForServicePorts(false) { }

DockerProc::~DockerProc() { 
	if ( daemonCore && daemonCore->SocketIsRegistered(&listener)) {
		daemonCore->Cancel_Socket(&listener);
	}
}

static
std::string credentials_dir() {
	std::string dir {"/tmp/docker_config."};
	dir += std::to_string(getpid());
	return dir;
}

int 
DockerProc::StartJob() {
	std::string imageID;

	// Not really ready for ssh-to-job'ing until we start the container
	starter->SetJobEnvironmentReady(false);

	if( ! JobAd->LookupString( ATTR_DOCKER_IMAGE, imageID ) ) {
		if (! JobAd->LookupString(ATTR_CONTAINER_IMAGE, imageID)) {
			dprintf( D_ERROR, "Neither %s nor %s defined in job ad, unable to start job.\n", ATTR_DOCKER_IMAGE, ATTR_CONTAINER_IMAGE);
			return FALSE;
		}
	}

	if (starts_with(imageID, "docker://")) {
		imageID = imageID.substr(9);
	}

	imageName = imageID;

	// The GlobalJobID is unsuitable by virtue its octothorpes.  This
	// construction is informative, but could be made even less likely
	// to collide if it had a timestamp.
	formatstr( containerName, "HTCJob%d_%d_%s_PID%d",
		starter->jic->jobCluster(),
		starter->jic->jobProc(),
		starter->getMySlotName().c_str(), // note: this can be "" for single slot machines.
		getpid() );


	// Grab credentials from shadow, store them in a file,
	// as docker pull requires them in a file named "config.json"
	// in a directory pointed at by DOCKER_CONFIG
	bool use_creds = false;
	JobAd->LookupBool(ATTR_DOCKER_SEND_CREDENTIALS, use_creds)
	if (use_creds) {
		
		// Grab the creds from the shadow
		// Now write out just the auths as condor, which runs docker container create
		ClassAd query; // not used now
		ClassAd creds; // creds from the shadow

		int r = starter->jic->fetch_docker_creds(query, creds);

		std::string creds_error;
		creds.LookupString("HTCondorError", creds_error);

		if ((r == 0) && (creds_error.empty())) {
			creds_error = "Cannot connect to shadow to fetch docker creds";
		}

		if (!creds_error.empty()) {
			dprintf(D_ALWAYS, "Error fetching docker credentials: %s\n", creds_error.c_str());
			starter->jic->holdJob(creds_error.c_str(), CONDOR_HOLD_CODE::InvalidDockerImage, 0);
			return FALSE;
		}
		std::string tmp_dir = credentials_dir();

		// mkdir as user condor...
		int ret = mkdir(tmp_dir.c_str(), 0700);
		if (ret != 0) {
			dprintf(D_ALWAYS, "Cannot make directory for docker creds: %s\n", strerror(errno));
			return FALSE;
		}

		// and a file named "config.json" in that directory
		std::string config_output_filename;
		config_output_filename = tmp_dir + "/config.json";

		std::string auths_str;
		classad::ClassAdJsonUnParser cajup;
		cajup.Unparse(auths_str, &creds);

		bool good = htcondor::writeShortFile(config_output_filename, auths_str);
		if (!good) {
			// This is an error on the EP side, do not hold the job
			dprintf(D_ALWAYS, "Cannot write docker creds: %s\n", strerror(errno));
			return FALSE;
		}
	}
	// Get all our cached docker images
	std::vector<DockerAPI::ImageInfo> imageInfos = DockerAPI::getImageInfos();
	std::string annotatedImageName = DockerAPI::toAnnotatedImageName(imageName, *JobAd);
	if (annotatedImageName.empty()) {
		dprintf(D_ALWAYS, "DockerProc::Start job, cannot convert %s to a annotated image name\n", imageName.c_str());
		return FALSE;
	}

	// See if we've already got this image annotation
	auto it = std::ranges::find(imageInfos, annotatedImageName, &DockerAPI::ImageInfo::imageName);
	if (imageInfos.end() ==  it) {
		return PullImage();
	} else {
		// And do the real work of creating the container and launching it
		return LaunchContainer();
	}
}

int 
DockerProc::LaunchContainer() {

	std::string command;
	JobAd->LookupString(ATTR_JOB_CMD, command);
	dprintf(D_FULLDEBUG, "%s: '%s'\n", ATTR_JOB_CMD, command.c_str());

	std::string sandboxPath = starter->jic->jobRemoteIWD();

#ifdef WIN32
	#if 1
	// TODO: make this configurable? and the same on Windows/Linux
	// TODO: make it settable by the job so that we can support Windows containers on Windows?
	std::string innerdir("/test/execute/");
	starter->SetInnerWorkingDir(innerdir.c_str());
	#else
	const char * outerdir = starter->GetWorkingDir(false);
	std::string innerdir(outerdir);
	const char * tmp = strstr(outerdir, "\\execute\\");
	if (tmp) {
		innerdir = tmp;
		std::replace(innerdir.begin(), innerdir.end(), '\\', '/');
		starter->SetInnerWorkingDir(innerdir.c_str());
	}
	#endif
#else
	// TODO: make this work on Linux also
	std::string innerdir = starter->jic->jobRemoteIWD();
#endif

	bool transferExecutable = true;
	JobAd->LookupBool( ATTR_TRANSFER_EXECUTABLE, transferExecutable );
	if( starter->jic->usingFileTransfer() && transferExecutable ) {
		std::string old_cmd = command;
		formatstr(command, "./%s", condor_basename(old_cmd.c_str()));
	}

	ArgList args;
	args.SetArgV1SyntaxToCurrentPlatform();
	std::string argsError;
	if( ! args.AppendArgsFromClassAd( JobAd, argsError ) ) {
		dprintf( D_ERROR, "Failed to read job arguments from job ad: '%s'.\n", argsError.c_str() );
		return FALSE;
	}
	//
	// temporary hack to allow condor_submit -i to work better
	// with docker universe
	bool isInteractive = false;
	JobAd->LookupBool("InteractiveJob", isInteractive);	
	if (isInteractive) {
		args.AppendArg("86400");
	}

	Env job_env;
	std::string env_errors;
	if( !starter->GetJobEnv(JobAd,&job_env, env_errors) ) {
		dprintf( D_ALWAYS, "Aborting DockerProc::StartJob: %s\n", env_errors.c_str());
		return 0;
	}

	ClassAd recoveryAd;
	recoveryAd.Assign("DockerContainerName", containerName);
	starter->WriteRecoveryFile(&recoveryAd);

	int childFDs[3] = { 0, 0, 0 };
	{
	TemporaryPrivSentry sentry(PRIV_USER);
	std::string workingDir = starter->GetWorkingDir(0);
	//std::string DockerOutputFile = workingDir + "/docker_stdout";
	std::string DockerErrorFile  = workingDir + "/docker_stderror";

	//childFDs[1] = open(DockerOutputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	childFDs[2] = open(DockerErrorFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	}

	  // Ulog the execute event
	starter->jic->notifyJobPreSpawn();

	CondorError err;
	// DockerAPI::createContainer() returns a PID from daemonCore->Create_Process(), which
	// makes it suitable for passing up into VanillaProc.  This combination
	// will trigger the reaper(s) when the container terminates.
	
	ClassAd *machineAd = starter->jic->machClassAd();

	std::list<std::string> extras;
	std::string scratchDir = starter->GetWorkingDir(0);

	// map the scratch dir inside the container
	extras.push_back(scratchDir + ":" + scratchDir);

	// if file xfer is off, also map the iwd
	std::string iwd = starter->jic->jobRemoteIWD();
	if (iwd != scratchDir) {
		extras.push_back(iwd + ":" + iwd);
	}

	buildExtraVolumes(extras, *machineAd, *JobAd);

	int *affinity_mask = makeCpuAffinityMask(starter->getMySlotNumber());

	// The following line is for condor_who to parse
	dprintf( D_ALWAYS, "About to exec docker:%s\n", command.c_str());
	int rv = DockerAPI::createContainer( *machineAd, *JobAd,
			containerName, imageName,
			command, args, job_env,
			sandboxPath, innerdir,
			extras, credentials_dir(), JobPid, childFDs,
			shouldAskForServicePorts, err, affinity_mask);

	if( rv < 0 ) {
		dprintf( D_ERROR, "DockerAPI::createContainer( %s, %s, ... ) failed with return value %d\n", imageName.c_str(), command.c_str(), rv );
		handleFTL(rv);
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "DockerAPI::createContainer() returned pid %d\n", JobPid );
	// The following line is for condor_who to parse
	dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid);
	// If we manage to start the Docker job, clear the offline state for docker universe
	handleFTL(0);

	free(affinity_mask);
	waitForCreate = true;  // Tell the reaper to run start container on exit
	return TRUE;
}

int
DockerProc::PullImage() {
	CondorError err;
	int pullReaperId = daemonCore->Register_Reaper("PullReaper", (ReaperHandlercpp)&DockerProc::PullReaper,
			"PullReaper", this);
	int r = DockerAPI::pullImage(imageName, credentials_dir(), starter->GetWorkingDir(0), *JobAd, pullReaperId, err);
	if (r == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

int
DockerProc::PullReaper(int pid, int status) {
	dprintf(D_FULLDEBUG, "DockerProc::pullReaper fired for pid %d with status %d\n", pid, status);
	if (status == 0) {
		return LaunchContainer();
	} else {
		std::string message;
		formatstr(message, "Cannot pull image %s", imageName.c_str());
		dprintf(D_ALWAYS, "%s\n", message.c_str());
		starter->jic->holdJob(message.c_str(), CONDOR_HOLD_CODE::InvalidDockerImage, 0);
		return 0;
	}
	return 0;
}

int execPid;
ReliSock *ns;

int
DockerProc::ExecReaper(int pid, int status) {
	dprintf( D_FULLDEBUG, "DockerProc::ExecReaper() pid is %d with status %d\n", pid, status);
	if (pid == execPid) {
		dprintf(D_ALWAYS, "docker exec pid %d exited\n", pid);
		delete ns;
		return 0;
	} else {
		dprintf(D_ALWAYS, "docker exec unknown pid %d exited\n", pid);
	}
	return 1;
}

bool DockerProc::JobReaper( int pid, int status ) {
	dprintf( D_FULLDEBUG, "DockerProc::JobReaper() pid is %d status is %d wait_for_Create is %d\n", pid, status, waitForCreate);

	if (waitForCreate) {
		// Cleanup the credentials we no longer need
		// Should be done as user Condor
		std::string creds_dir = credentials_dir();
		std::string creds_file = creds_dir + "/config.json";

		{
			TemporaryPrivSentry sentry(PRIV_CONDOR);
			std::ignore = remove(creds_file.c_str());
			std::ignore = rmdir(creds_dir.c_str());
		}

		// When we get here, the docker create container has exited

		if (status != 0) {
			// Error creating container.  Perhaps invalid image
			std::string message;

			char buf[512];
			buf[0] = '\0';

			{
			TemporaryPrivSentry sentry(PRIV_USER);
			std::string fileName = starter->GetWorkingDir(0);
			fileName += "/docker_stderror";
			int fd = open(fileName.c_str(), O_RDONLY, 0000);
			if (fd >= 0) {
				int r = read(fd, buf, 511);
				if (r < 0) {
					dprintf(D_ALWAYS, "Cannot read docker error file on docker create container. Errno %d\n", errno);
					buf[0] = '\0';
				} else {
					buf[r] = '\0';
					int buflen = strlen(buf);
					for (int i = 0; i < buflen; i++) {
						if (buf[i] == '\n') buf[i] = ' ';
					}
				}
				close(fd);
			} else {
				dprintf(D_ALWAYS, "Cannot open docker_stderror\n");
			}
			}
			message = buf;
			starter->jic->holdJob(message.c_str(), CONDOR_HOLD_CODE::InvalidDockerImage, 0);
			{
			TemporaryPrivSentry sentry(PRIV_USER);
			unlink("docker_stderror");
			}
			return VanillaProc::JobReaper( pid, status );
		}

		// When we get here docker create has just succeeded
		waitForCreate = false;
		dprintf(D_FULLDEBUG, "DockerProc::JobReaper docker create (pid %d) exited with status %d\n", pid, status);

		// Now tag the image we just downloaded (or re-used) with a tag annotation. 
		// This let's us know that this image is one of ours.  If the tag already
		// exists, this is not an error, but the LastTagModified field is updated.
		//
		std::string annotatedImage = DockerAPI::toAnnotatedImageName(imageName, *JobAd);
		if (!annotatedImage.empty()) {
			DockerAPI::tag(imageName, annotatedImage);
		} else {
			dprintf(D_ALWAYS, "DockerProc:: cannot create annotated image from image names %s\n", imageName.c_str());
		}

		// It would be nice if we could find out what port Docker selected
		// for search service (if any) before we actually started the job,
		// but (understandably) Docker doesn't do that.

	#ifdef WIN32 
		#ifdef COPY_INPUT_SANDBOX
		// copy the input sandbox into the container
		{
			std::string workingDir = starter->GetWorkingDir(0);
			std::string innerPath = starter->GetWorkingDir(true);
			std::vector<std::string> opts{"-a"};

			//TODO: figure out if we need to do this, or to switch to  PRIV_USER
			//TemporaryPrivSentry sentry(PRIV_ROOT);

			int rv = DockerAPI::copyToContainer(workingDir, containerName, innerPath, &opts);
			if (rv < 0) {
				dprintf(D_ERROR, "DockerAPI::copyToContainer( %s, %s, %s ) failed with return value %d\n",
					workingDir.c_str(), containerName.c_str(), innerPath.c_str(), rv);
				return FALSE;
			}
		}
		#endif
	#endif

		// It seems like this should be done _after_ we call start Container().
		starter->SetJobEnvironmentReady(true);


		CondorError err;

		//
		// Do I/O redirection (includes streaming).
		//

		int childFDs[3] = { -2, -2, -2 };
		{
		TemporaryPrivSentry sentry(PRIV_USER);
		// getStdFile() returns -1 on error.

		if( -1 == (childFDs[0] = openStdFile( SFT_IN, NULL, true, "Input file" )) ) {
			dprintf( D_ERROR, "DockerProc::StartJob(): failed to open stdin.\n" );
			return FALSE;
		}

		if( -1 == (childFDs[1] = openStdFile( SFT_OUT, NULL, true, "Output file" )) ) {

			dprintf( D_ERROR, "DockerProc::StartJob(): failed to open stdout.\n" );
			daemonCore->Close_FD( childFDs[0] );
			return FALSE;
		}
		if( -1 == (childFDs[2] = openStdFile( SFT_ERR, NULL, true, "Error file" )) ) {
			dprintf( D_ERROR, "DockerProc::StartJob(): failed to open stderr.\n" );
			daemonCore->Close_FD( childFDs[0] );
			daemonCore->Close_FD( childFDs[1] );
			return FALSE;
		}
		}

		{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		std::string arch;
		DockerAPI::getImageArch(imageName, arch);

		if (!DockerAPI::imageArchIsCompatible(arch)) {
			std::string message;
			formatstr(message, "DockerProc::StartJob(): Image Architecture %s not compatible with this machine.", arch.c_str());
			dprintf(D_ALWAYS, "%s\n", message.c_str());
			starter->jic->holdJob(message.c_str(), CONDOR_HOLD_CODE::InvalidDockerImage, 0);
			return false;
		}

		DockerAPI::startContainer( containerName, JobPid, childFDs, err );
		}
		condor_gettimestamp( job_start_time );
	
		// Start a timer to poll for job usage updates.
		int polling_interval = param_integer("POLLING_INTERVAL",5);
		updateTid = daemonCore->Register_Timer(2,
				polling_interval, (TimerHandlercpp)&DockerProc::getStats, 
					"DockerProc::getStats",this);

		bool ssh_enabled = param_boolean("ENABLE_SSH_TO_JOB",true,true,starter->jic->machClassAd(),JobAd);
		if( ssh_enabled ) {
			SetupDockerSsh();
		}

		++num_pids; // Used by OsProc::PublishUpdateAd().
		return false; // don't exit
	}


	//
	// This should mean that the container has terminated.
	//
	if( pid == JobPid ) {
		daemonCore->Cancel_Timer(updateTid);

		// Once the container has terminated, I don't care what Docker
		// thinks; nobody's going to be answering the phone.
		std::string containerServiceNames;
		JobAd->LookupString(ATTR_CONTAINER_SERVICE_NAMES, containerServiceNames);
		if(! containerServiceNames.empty()) {
			for (const auto &service : StringTokenIterator(containerServiceNames)) {
				std::string attrName;
				formatstr( attrName, "%s_%s", service.c_str(), "HostPort" );
				serviceAd.Insert( attrName, classad::Literal::MakeUndefined() );
			}
		}

		TemporaryPrivSentry sentry(PRIV_ROOT);

		//
		// Even running Docker in attached mode, we have a race condition
		// is exiting when the container exits, not when the docker daemon
		// notices that the container has exited.
		//

		int rv = -1;
		bool running = false;
		ClassAd dockerAd;
		CondorError error;
		// Years of careful research.
		for( int i = 0; i < 20; ++i ) {
			rv = DockerAPI::inspect( containerName, & dockerAd, error );
			if( rv < 0 ) {
				dprintf( D_FULLDEBUG, "Failed to inspect (for removal) container '%s'; sleeping a second (%d already slept) to give Docker a chance to catch up.\n", containerName.c_str(), i );
				sleep( 1 );
				continue;
			}

			if( ! dockerAd.LookupBool( "Running", running ) ) {
				dprintf( D_FULLDEBUG, "Inspection of container '%s' failed to reveal its running state; sleeping a second (%d already slept) to give Docker a chance to catch up.\n", containerName.c_str(), i );
				sleep( 1 );
				continue;
			}

			if( running ) {
				dprintf( D_FULLDEBUG, "Inspection reveals that container '%s' is still running; sleeping a second (%d already slept) to give Docker a chance to catch up.\n", containerName.c_str(), i );
				sleep( 1 );
				continue;
			}

			break;
		}

// FIXME: Move all this shared conditional-checking into a function.

		if( rv < 0 ) {
			dprintf( D_ERROR, "Failed to inspect (for removal) container '%s'.\n", containerName.c_str() );
			// We're in a tight spot.  But we've seen this happen in CHTC where we can create the
			// container successfully, but not inspect it.  In this case, try to remove the container 
			// so we don't leak it.
			rv = DockerAPI::rm(containerName, error);
			if (rv == 0) {
				dprintf(D_ERROR,"    Nevertheless, container %s was successfully removed.\n", containerName.c_str());
			} else {
				dprintf(D_ERROR,"    Container %s remove failed -- does it even exist? If so, startd will remove on next boot.\n", containerName.c_str());
			}
			EXCEPT("Cannot inspect exited container");
		}

		if( ! dockerAd.LookupBool( "Running", running ) ) {
			dprintf( D_ERROR, "Inspection of container '%s' failed to reveal its running state.\n", containerName.c_str() );
			return VanillaProc::JobReaper( pid, status );
		}

		if( running ) {
			dprintf( D_ERROR, "Inspection reveals that container '%s' is still running.\n", containerName.c_str() );
			return VanillaProc::JobReaper( pid, status );
		}

		// FIXME: Rethink returning a classad.  Having to check for missing
		// attributes blows.

		// TODO: Set status appropriately (as if it were from waitpid()).
		std::string oomkilled;
		if (! dockerAd.LookupString( "OOMKilled", oomkilled)) {
			dprintf( D_ERROR, "Inspection of container '%s' failed to reveal whether it was OOM killed. Assuming it was not.\n", containerName.c_str() );
		}

		if (oomkilled.find("true") == 0) {
			ClassAd *machineAd = starter->jic->machClassAd();
			int memory;
			machineAd->LookupInteger(ATTR_MEMORY, memory);
			std::string message;
			formatstr(message, "Docker job has gone over memory limit of %d Mb", memory);
			dprintf(D_ALWAYS, "%s, going on hold\n", message.c_str());


			starter->jic->holdJob(message.c_str(), CONDOR_HOLD_CODE::JobOutOfResources, 0);
			DockerAPI::rm( containerName, error );

			if ( starter->Hold( ) ) {
				starter->allJobsDone();
				this->JobExit();
			}

			starter->ShutdownFast();
			return 0;
		}

			// See if docker could not run the job
			// most likely invalid executable
		std::string dockerError;
		if (! dockerAd.LookupString( "DockerError", dockerError)) {
			dprintf( D_ERROR, "Inspection of container '%s' failed to reveal whether there was an internal docker error.\n", containerName.c_str() );
		}

		if (dockerError.length() > 0) {
			std::string message;
			formatstr(message, "Error running docker job: %s", dockerError.c_str());
			dprintf(D_ALWAYS, "%s, going on hold\n", message.c_str());


			starter->jic->holdJob(message.c_str(), CONDOR_HOLD_CODE::FailedToCreateProcess, 0);
			DockerAPI::rm( containerName, error );

			if ( starter->Hold( ) ) {
				starter->allJobsDone();
				this->JobExit();
			}

			starter->ShutdownFast();
			return 0;
		}

		int dockerStatus;
		if( ! dockerAd.LookupInteger( "ExitCode", dockerStatus ) ) {
			dprintf( D_ERROR, "Inspection of container '%s' failed to reveal its exit code.\n", containerName.c_str() );
			return VanillaProc::JobReaper( pid, status );
		}
		status = dockerStatus > 128 ? (dockerStatus - 128) : (dockerStatus << 8);
		dprintf( D_FULLDEBUG, "Setting status of Docker job to %d.\n", status );

		// TODO: Record final job usage.

		// We don't have to do any process clean-up, because container.
		// We'll do the disk clean-up after we've transferred files.

		// This helps to make ssh-to-job more plausible.
		return VanillaProc::JobReaper( pid, status );
	}

	return 0;
}

void
DockerProc::SetupDockerSsh() {
#ifdef LINUX
	static bool first_time = true;
	if (first_time) {
		first_time = false;
	} else {
		return;
	}

	// First, create a unix domain socket that we can listen on
	int uds = socket(AF_UNIX, SOCK_STREAM, 0);
	if (uds < 0) {
		dprintf(D_ALWAYS, "Cannot create unix domain socket for docker ssh_to_job\n");
		return;
	}

	// stuff the unix domain socket into a reli sock
	listener.close();
	listener.assignDomainSocket(uds);

	// and bind it to a filename in the scratch directory
	struct sockaddr_un pipe_addr;
	memset(&pipe_addr, 0, sizeof(pipe_addr));
	pipe_addr.sun_family = AF_UNIX;
	unsigned pipe_addr_len;

	std::string workingDir = starter->GetWorkingDir(0);
	std::string pipeName = workingDir + "/.docker_sock";	

	strncpy(pipe_addr.sun_path, pipeName.c_str(), sizeof(pipe_addr.sun_path)-1);
	pipe_addr_len = SUN_LEN(&pipe_addr);

	{
	TemporaryPrivSentry sentry(PRIV_USER);
	int rc = bind(uds, (struct sockaddr *)&pipe_addr, pipe_addr_len);
	if (rc < 0) {
		dprintf(D_ALWAYS, "Cannot bind unix domain socket at %s for docker ssh_to_job: %d\n", pipeName.c_str(), errno);
		return;
	}
	}

	listen(uds, 50);
	listener._state = Sock::sock_special;
	listener._special_state = ReliSock::relisock_listen;

	// now register this socket so we get called when connected to
	int rc;
	rc = daemonCore->Register_Socket(
		&listener,
		"Docker sshd listener",
		(SocketHandlercpp)&DockerProc::AcceptSSHClient,
		"DockerProc::AcceptSSHClient",
		this);
	ASSERT( rc >= 0 );

	// and a reaper 
	execReaperId = daemonCore->Register_Reaper("ExecReaper", (ReaperHandlercpp)&DockerProc::ExecReaper,
		"ExecReaper", this);
#else
	// Shut the compiler up
	(void)execReaperId;
#endif
	return;
}


int
DockerProc::AcceptSSHClient(Stream *stream) {
#ifdef LINUX
	int fds[3];
	ns = ((ReliSock*)stream)->accept();

	dprintf(D_ALWAYS, "Accepted new connection from ssh client for docker job\n");
	fds[0] = fdpass_recv(ns->get_file_desc());
	fds[1] = fdpass_recv(ns->get_file_desc());
	fds[2] = fdpass_recv(ns->get_file_desc());

	ArgList args;
	args.AppendArg("-i");

	Env env;
	std::string env_errors;
	if( !starter->GetJobEnv(JobAd,&env, env_errors) ) {
		dprintf( D_ALWAYS, "Aborting DockerProc::exec: %s\n", env_errors.c_str());
		return 0;
	}

	int rc;
	{
	TemporaryPrivSentry sentry(PRIV_ROOT);

	rc = DockerAPI::execInContainer(
	 containerName, "/bin/bash", args, env,fds,execReaperId,execPid);
	}

	dprintf(D_ALWAYS, "docker exec returned %d for pid %d\n", rc, execPid);

#else
	// Shut the compiler up
	(void)stream;
#endif
	return KEEP_STREAM;
}

//
// JobExit() is called after file transfer.
//
bool DockerProc::JobExit() {
	dprintf( D_ALWAYS, "DockerProc::JobExit() container '%s'\n", containerName.c_str() );

	{
	TemporaryPrivSentry sentry(PRIV_ROOT);
	ClassAd dockerAd;
	CondorError error;
	int rv = DockerAPI::inspect( containerName, & dockerAd, error );
	if( rv < 0 ) {
		dprintf( D_ERROR, "Failed to inspect (for removal) container '%s'.\n", containerName.c_str() );
		return VanillaProc::JobExit();
	}

	bool running;
	if( ! dockerAd.LookupBool( "Running", running ) ) {
		dprintf( D_ERROR, "Inspection of container '%s' failed to reveal its running state.\n", containerName.c_str() );
		return VanillaProc::JobExit();
	}
	if( running ) {
		dprintf( D_ERROR, "Inspection reveals that container '%s' is still running.\n", containerName.c_str() );
		return VanillaProc::JobExit();
	}

	rv = DockerAPI::rm( containerName, error );
	if( rv < 0 ) {
		dprintf( D_ERROR, "Failed to remove container '%s'.\n", containerName.c_str() );
	}
	}

	return VanillaProc::JobExit();
}

void DockerProc::Suspend() {
	dprintf( D_ALWAYS, "DockerProc::Suspend() container '%s'\n", containerName.c_str() );
	int rv = 0;

	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		CondorError error;
		rv = DockerAPI::pause( containerName, error );
	}
	TemporaryPrivSentry sentry(PRIV_ROOT);
	if( rv < 0 ) {
		dprintf( D_ERROR, "Failed to suspend container '%s'.\n", containerName.c_str() );
	}

	is_suspended = true;
}


void DockerProc::Continue() {
	dprintf( D_ALWAYS, "DockerProc::Continue() container '%s'\n", containerName.c_str() );
	int rv = 0;	

	if( is_suspended ) {
		{
			TemporaryPrivSentry sentry(PRIV_ROOT);
			CondorError error;
			rv = DockerAPI::unpause( containerName, error );
		}
		if( rv < 0 ) {
			dprintf( D_ERROR, "Failed to unpause container '%s'.\n", containerName.c_str() );
		}


		is_suspended = false;
	}
}

//
// Setting requested_exit allows OsProc::JobExit() to handle telling the
// user why the job exited.
//

bool DockerProc::Remove() {
	dprintf( D_ALWAYS, "DockerProc::Remove() container '%s'\n", containerName.c_str() );

	if( is_suspended ) { Continue(); }
	requested_exit = true;

	CondorError err;
	TemporaryPrivSentry sentry(PRIV_ROOT);
	DockerAPI::kill( containerName, rm_kill_sig, err );

	// Do NOT send any signals to the waiting process.  It should only
	// react when the container does.

	// If rm_kill_sig is not SIGKILL, the process may linger.  Returning
	// false indicates that shutdown is pending.
	return false;
}


bool DockerProc::Hold() {
	dprintf( D_ALWAYS, "DockerProc::Hold() container '%s'\n", containerName.c_str() );

	if( is_suspended ) { Continue(); }
	requested_exit = true;

	CondorError err;
	TemporaryPrivSentry sentry(PRIV_ROOT);
	DockerAPI::kill( containerName, hold_kill_sig, err );

	// Do NOT send any signals to the waiting process.  It should only
	// react when the container does.

	// If hold_kill_sig is not SIGKILL, the process may linger.  Returning
	// false indicates that shutdown is pending.
	return false;
}


bool DockerProc::ShutdownGraceful() {
	dprintf( D_ALWAYS, "DockerProc::ShutdownGraceful() container '%s'\n", containerName.c_str() );

	if( containerName.empty() ) {
		// We haven't started a Docker yet, probably because we're still
		// doing file transfer.  Since we're all done, just return true;
		// the FileTransfer object will clean itself up.
		return true;
	}

	if( is_suspended ) { Continue(); }
	requested_exit = true;

	CondorError err;
	TemporaryPrivSentry sentry(PRIV_ROOT);
	if( findRmKillSig(JobAd) != -1 ) {
		DockerAPI::kill( containerName, rm_kill_sig, err );
	} else {
		DockerAPI::kill( containerName, soft_kill_sig, err );
	}

	// If the signal was not SIGKILL, the process may linger.  Returning
	// false indicates that shutdown is pending.
	return false;
}


bool DockerProc::ShutdownFast() {
	dprintf( D_ALWAYS, "DockerProc::ShutdownFast() container '%s'\n", containerName.c_str() );

	if( containerName.empty() ) {
		// We haven't started a Docker yet, probably because we're still
		// doing file transfer.  Since we're all done, just return true;
		// the FileTransfer object will clean itself up.
		return true;
	}

	// There's no point unpausing the container (and possibly swapping
	// it all back in again) if we're just going to be sending it a SIGKILL,
	// so don't bother to Continue() the process if it's been suspended.
	requested_exit = true;

	CondorError err;
	TemporaryPrivSentry sentry(PRIV_ROOT);
	DockerAPI::kill( containerName, SIGKILL, err );

	// Based on the other comments, you'd expect this to return true.
	// It could, but it's simpler to just to let the usual routines
	// handle the job clean-up than to duplicate them all here.
	return false;
}


void
DockerProc::getStats( int /* timerID */ ) {
	if( shouldAskForServicePorts ) {
		if( DockerAPI::getServicePorts( containerName, * JobAd, serviceAd ) == 0 ) {
			shouldAskForServicePorts = false;

			// Append serviceAd to the sandbox's copy of the job ad.
			std::string jobAdFileName;
			formatstr( jobAdFileName, "%s/.job.ad", starter->GetWorkingDir(0) );
			{
				TemporaryPrivSentry sentry(PRIV_ROOT);
				// ... sigh ...
				ToE::writeTag( & serviceAd, jobAdFileName );
			}
		}
	}
	DockerAPI::stats(containerName, memUsage, netIn, netOut, userCpu, sysCpu);

	// Since we typically will poll the Docker daemon for memory usage
	// at a higher rate than we update the shadow, keep the maximum observed
	// memory usage here in the starter (and use max_memUsage to update the shadow).
	if (memUsage > max_memUsage) {
		max_memUsage = memUsage;
	}

	return;
}

bool DockerProc::PublishUpdateAd( ClassAd * ad ) {
	dprintf( D_FULLDEBUG, "DockerProc::PublishUpdateAd() container '%s'\n", containerName.c_str() );

	// Copy the service-to-port map into the update ad.
	ad->Update( serviceAd );

	//
	// If we want to use the existing reporting code (probably a good
	// idea), we'll need to make sure that m_proc_exited, is_checkpointed,
	// is_suspended, num_pids, and dumped_core are set for OsProc; and that
	// job_start_time, job_exit_time, and exit_status are set.
	//
	// We set is_suspended and num_pids already, except for the TODO above.
	// DockerProc::JobReaper() already sets m_proc_exited, exit_code, and
	// dumped_core (indirectly, via OsProc::JobReaper()).
	//
	// We will need to set is_checkpointed appropriately when we support it.
	//
	// TODO: We could approximate job_start_time and job_exit_time internally,
	// or set them during our status polling.
	//
	
	if (max_memUsage > 0) {
		// Set RSS, Memory and ImageSize to same values, best we have
		ad->Assign(ATTR_RESIDENT_SET_SIZE, int(max_memUsage / 1024));
		ad->Assign(ATTR_MEMORY_USAGE, int(max_memUsage / (1024 * 1024)));
		ad->Assign(ATTR_IMAGE_SIZE, int(max_memUsage / (1024 * 1024)));
		ad->Assign(ATTR_NETWORK_IN, double(netIn) / (1000 * 1000));
		ad->Assign(ATTR_NETWORK_OUT, double(netOut) / (1000 * 1000));
		ad->Assign(ATTR_JOB_REMOTE_USER_CPU, (int) (userCpu / (1000l * 1000l * 1000l)));
		ad->Assign(ATTR_JOB_REMOTE_SYS_CPU, (int) (sysCpu / (1000l * 1000l * 1000l)));
	}
	return OsProc::PublishUpdateAd( ad );
}


// TODO: Implement.
void DockerProc::PublishToEnv( Env * /* env */ ) {
	dprintf( D_FULLDEBUG, "DockerProc::PublishToEnv()\n" );
	return;
}


bool DockerProc::Detect() {
	dprintf( D_FULLDEBUG, "DockerProc::Detect()\n" );

	//
	// To turn off Docker, unset DOCKER.  DockerAPI::detect() will fail
	// but not complain to the log (unless D_FULLDEBUG) if so.
	//

	CondorError err;
	bool hasDocker = DockerAPI::detect( err ) == 0;

	if (hasDocker) {
		int r  = DockerAPI::testImageRuns(err);
		if (r == 0) {
			hasDocker = true;
		} else {
			hasDocker = false;
		}
	}

	return hasDocker;
}

bool DockerProc::Version( std::string & version ) {
	dprintf( D_FULLDEBUG, "DockerProc::Version()\n" );

	//
	// To turn off Docker, unset DOCKER.  DockerAPI::version() will fail
	// but not complain to the log (unless D_FULLDEBUG) if so.
	//

	CondorError err;
	bool foundVersion = DockerAPI::version( version, err ) == 0;
	if (foundVersion) {
		dprintf( D_ALWAYS, "DockerProc::Version() found version '%s'\n", version.c_str() );
	}

	return foundVersion;
}
void 
DockerProc::restartCheckpointedJob() {
	TemporaryPrivSentry sentry(PRIV_ROOT);
	CondorError error;
	int rv = DockerAPI::rm( containerName, error );
	if( rv < 0 ) {
		dprintf( D_ERROR, "Failed to remove container '%s' after checkpoint exit.\n", containerName.c_str() );
		// Will fail later when we try to restart if it still exists.  If it doesn't :shrug: all good!
	}

	VanillaProc::restartCheckpointedJob();
}

// Generate a list of strings that are suitable arguments to
// docker run --volume
static void buildExtraVolumes(std::list<std::string> &extras, ClassAd &machAd, ClassAd &jobAd) {
	// Bind mount items from MOUNT_UNDER_SCRATCH into working directory
	std::string scratchNames;
	if (!param_eval_string(scratchNames, "MOUNT_UNDER_SCRATCH", "", &jobAd)) {
		// If not an expression, maybe a literal
		param(scratchNames, "MOUNT_UNDER_SCRATCH");
	} 

	// Now add in scratch mounts requested by the job.
	std::string job_mount_under_scratch;
	jobAd.LookupString(ATTR_JOB_MOUNT_UNDER_SCRATCH, job_mount_under_scratch);
	if (job_mount_under_scratch.length() > 0) {
		if (scratchNames.length() > 0) {
			scratchNames += ' ';
		}
		scratchNames += job_mount_under_scratch;
	}

#ifdef DOCKER_ALLOW_RUN_AS_ROOT
		// If docker is allowing the user to be root, don't mount anything
		// so that we can't create rootly files in shared places.
	if (param_boolean("DOCKER_RUN_AS_ROOT", false)) {
		return;
	}
#endif

	if (scratchNames.length() > 0) {
		std::string workingDir = starter->GetWorkingDir(0);
			// Foreach scratch name...
		for (const auto &scratchName: StringTokenIterator(scratchNames)) {
			std::string hostdirbuf;
			const char * hostDir = dirscat(workingDir.c_str(), scratchName.c_str(), hostdirbuf);
			std::string volumePath;
			volumePath.append(hostDir).append(":").append(scratchName);
			
			mode_t old_mask = umask(000);
			if (mkdir_and_parents_if_needed( hostDir, 01777, PRIV_USER )) {
				extras.push_back(volumePath);
				dprintf(D_ALWAYS, "Adding %s as a docker volume to mount under scratch\n", volumePath.c_str());
			} else {
				dprintf(D_ALWAYS, "Failed to create scratch directory %s\n", hostDir);
			}
			umask(old_mask);
		}
	}

	// These are the ones the administrator wants unconditionally mounted
	char *volumeNames = param("DOCKER_MOUNT_VOLUMES");
	if (!volumeNames) {
		// Nothing extra to mount, give up
		return;
	}

		// Foreach volume name...
	for (const auto &volumeName: StringTokenIterator(volumeNames)) {
		std::string paramName("DOCKER_VOLUME_DIR_");
		paramName += volumeName;
		char *volumePath = param(paramName.c_str());
		if (volumePath) {
			if (strchr(volumePath, ':') == 0) {
				// Must have a colon.  If none, assume we meant
				// source:source
				char *volumePath2 = (char *)malloc(1 + 2 * strlen(volumePath));
				strcpy(volumePath2, volumePath);
				strcat(volumePath2, ":");
				strcat(volumePath2, volumePath);
				free(volumePath);
				volumePath = volumePath2;
			}

			// If there's a DOCKER_VOLUME_DIR_XXX_MOUNT_IF
			// param, and it evals to true, add it to the list
			// if there's no param, still add it
			// If the param exists, but doesn't eval to true, don't
			std::string paramNameConst = paramName + "_MOUNT_IF";
			char *mountIfValue = param(paramNameConst.c_str());

			if (mountIfValue == NULL) {
				// there's no MOUNT_IF, assume true
				extras.push_back(volumePath);
				dprintf(D_ALWAYS, "Adding %s as a docker volume to mount\n", volumePath);
			} else if (param_boolean(paramNameConst.c_str(), false /*deflt*/,
				false /*do_log*/, &machAd, &jobAd)) {

				// There is a MOUNT_IF, and it is true
				extras.push_back(volumePath);
				dprintf(D_ALWAYS, "Adding %s as a docker volume to mount\n", volumePath);
			} else {
				// There is a MOUNT_IF, and it is false/undef
				dprintf(D_ALWAYS, "Not adding %s as a docker volume to mount -- ...MOUNT_IF evaled to false.\n", volumePath);
			}
			free(volumePath);
			free(mountIfValue);
		} else {
			dprintf(D_ALWAYS, "WARNING: DOCKER_VOLUME_DIR_%s is missing in config file.\n", volumeName.c_str());
		}
	}
}
