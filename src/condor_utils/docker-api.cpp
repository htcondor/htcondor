#include "condor_common.h"

#include "classad/jsonSource.h"
#include "condor_attributes.h"
#include "env.h"
#include "my_popen.h"
#include "CondorError.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_daemon_core.h"
#include "file_lock.h"
#include "condor_rw.h"
#include "basename.h"
#include "shortfile.h"
#include "nvidia_utils.h"

#include "docker-api.h"
#include <algorithm>
#include <sstream>
#include <string>

#include <charconv>

#if !defined(WIN32)
#include <sys/un.h>
#endif


static bool add_env_to_args_for_docker(ArgList &runArgs, const Env &env);
static bool add_docker_arg(ArgList &runArgs);
static int run_docker_command(const ArgList &args,
					const std::string &container, int timeout,
					CondorError &e, bool ignore_output=false);
static int run_simple_docker_command(const std::string &command,
					const std::string &container, int timeout,
					CondorError &e, bool ignore_output=false);
static int gc_image(const std::string &image);
static std::string makeHostname(ClassAd *machineAd, ClassAd *jobAd);
static int check_if_docker_offline(MyPopenTimer & pgmIn, const char * cmd_str, int original_error_code);
static void build_env_for_docker_cli(Env &env);

static std::string HTCondorLabel = "--label=org.htcondorproject=True";
int DockerAPI::default_timeout = 120;

int DockerAPI::pruneContainers() {
	ArgList args;
	if ( ! add_docker_arg(args))
		return -1;
	args.AppendArg( "container" );
	args.AppendArg( "prune");
	args.AppendArg( "-f");
	args.AppendArg( "--filter=label=org.htcondorproject=True"); // must match label in create

	std::string displayString;
	args.GetArgsStringForLogging( displayString );
	dprintf( D_ALWAYS, "Running: %s\n", displayString.c_str() );

	MyPopenTimer pgm;
	TemporaryPrivSentry sentry(PRIV_ROOT);
	if (pgm.start_program( args, true, NULL, false ) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	if ( ! pgm.wait_and_close(120) || pgm.output_size() <= 0) {
		int error = pgm.error_code();
		if( error ) {
			dprintf( D_ALWAYS, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), error );
			if (pgm.was_timeout()) {
				dprintf( D_ALWAYS, "Declaring a hung docker\n");
				return DockerAPI::docker_hung;
			}
		}
	}
	return 0;
}


//
// Because we fork before calling docker, we don't actually
// care if the image is stored locally or not (except to the extent that
// remote image pull violates the principle of least astonishment).
//
int DockerAPI::createContainer(
	ClassAd &machineAd,
	ClassAd &jobAd,
	const std::string & containerName,
	const std::string & imageID,
	const std::string & command,
	const ArgList & args,
	const Env & env,
	const std::string & /*outside_sandboxPath*/,
	const std::string & inside_directory,
	const std::list<std::string> extraVolumes,
	const std::string credentials_dir,
	int & pid,
	int * childFDs,
	bool & shouldAskForPorts,
	CondorError & /* err */,
	int * affinity_mask /*= NULL*/)
{
	//
	// We currently assume that the system has been configured so that
	// anyone (user) who can run an HTCondor job can also run docker.  It's
	// also apparently a security worry to run Docker as root, so let's not.
	//
	ArgList runArgs;
	if ( ! add_docker_arg(runArgs))
		return -1;
	runArgs.AppendArg( "create" );

	// Write out a file with the container ID.
	// FIXME: The startd can check this to clean up after us.
	// This needs to go into a directory that condor user
	// can write to.

/*
	std::string cidFileName = sandboxPath + "/.cidfile";
	runArgs.AppendArg( "--cidfile=" + cidFileName );
*/


	// Configure resource limits.

	if (affinity_mask) {
		std::stringstream cpuSetStr;
		cpuSetStr << "--cpuset-cpus=";
		for (int i = 1; i < affinity_mask[0]; i++) {
			if (i != 1) {
				cpuSetStr << ",";
			}
			cpuSetStr << affinity_mask[i];
		}
		runArgs.AppendArg(cpuSetStr.str());
	} else {
		// First cpus
		int  cpus;
		int cpuShare;

		if (machineAd.LookupInteger(ATTR_CPUS, cpus)) {
			cpuShare = 100 * cpus;
		} else {
			cpuShare = 100;
		}
		std::string cpuShareStr;
		formatstr(cpuShareStr, "--cpu-shares=%d", cpuShare);
		runArgs.AppendArg(cpuShareStr);
	}

	// Now memory
	int memory; // in Megabytes
	if (machineAd.LookupInteger(ATTR_MEMORY, memory)) {
		std::string mem;
		formatstr(mem, "--memory=%dm", memory);
		runArgs.AppendArg(mem);
	}

	// drop unneeded Linux capabilities
	if (param_boolean("DOCKER_DROP_ALL_CAPABILITIES", true /*default*/,
		true /*do_log*/, &machineAd, &jobAd)) {
		runArgs.AppendArg("--cap-drop=all");
		runArgs.AppendArg("--cap-add=SYS_PTRACE");

		// --no-new-privileges flag appears in docker 1.11
		runArgs.AppendArg("--security-opt");
		runArgs.AppendArg("no-new-privileges");
	}
	// Tell docker to run this with an init program
	if (param_boolean("DOCKER_RUN_UNDER_INIT", true)) {
		runArgs.AppendArg("--init");
	}
		// Now the container name
	runArgs.AppendArg( "--name" );
	runArgs.AppendArg( containerName );

		// Add a label to mark this container as htcondor-managed
	runArgs.AppendArg(HTCondorLabel);

	// We never need stdout/err to go to the docker logs, we will write our own stdout
	if (param_boolean("DOCKER_LOG_DRIVER_NONE", true)) {
		runArgs.AppendArg("--log-driver");
		runArgs.AppendArg("none");
	}

	if ( ! add_env_to_args_for_docker(runArgs, env)) {
		dprintf( D_ALWAYS, "Failed to pass enviroment to docker.\n" );
		return -8;
	}

#ifdef WIN32
	// TODO: extra volumes is used for /home/ when not doing file transfer, we can't support this on Windows
#else
	// Now any extra volumes
	for (std::list<std::string>::const_iterator it = extraVolumes.begin(); it != extraVolumes.end(); it++) {
		runArgs.AppendArg("--volume");
		std::string volume = *it;
		runArgs.AppendArg(volume);
	}
#endif

#ifndef LINUX
	// TODO: what do we do on Windows to set the gpu bind mounts?
#else
	// if the startd has assigned us a gpu, add in the
	// nvidia devices.  AssignedGPUS looks like CUDA0, CUDA1, etc.
	// when we don't use the "stable" uuids.
	// map these to /dev/nvidia0, /dev/nvidia1...
	// arguments to mount the nvidia devices
	//
	// Otherwise, with the "stable" uuids, they will
	// look like GPU-XXXXX.
	std::string assignedGpus;
	machineAd.LookupString("AssignedGPUs", assignedGpus);
	if  (assignedGpus.length() > 0) {

		// Always need to map these two devices
		runArgs.AppendArg("--device");
		runArgs.AppendArg("/dev/nvidiactl");

		runArgs.AppendArg("--device");
		runArgs.AppendArg("/dev/nvidia-uvm");

		// If the startd is configured for the "CUDA" style
		// GPU names...
		if (assignedGpus.starts_with("CUDA")) {
			size_t offset = 0;

			// For each CUDA substring in assignedGpus...
			while ((offset = assignedGpus.find("CUDA", offset)) != std::string::npos) {
				offset += 4; // strlen("CUDA")
				size_t comma = assignedGpus.find(",", offset);

				// ...append to command line args -device /dev/nvidiaXX
				std::string deviceName("/dev/nvidia");
				deviceName += assignedGpus.substr(offset, comma - offset);
				runArgs.AppendArg("--device");
				runArgs.AppendArg(deviceName);
			}
		} else {
			// Get a map of long-form uuid device minor.  Assume that device minor
			// matches the last digit of /dev/nvidiaX
			bool map_all_gpus = param_boolean("DOCKER_MAP_ALL_GPUS", false);
			auto uuid_dev_map = make_nvidia_uuid_to_dev_map();
			
			// For each of our short-named assigned GPU, find the device number
			// n-squared, but should be small.
			for (const auto &gpu: StringTokenIterator(assignedGpus)) {
				for (const auto &[uuid, dev]: uuid_dev_map) {
					if (map_all_gpus || uuid.starts_with(gpu)) {
						std::string deviceName;

						// and finally append it to our command line args
						formatstr(deviceName, "/dev/nvidia%u", minor(dev));
						runArgs.AppendArg("--device");
						runArgs.AppendArg(deviceName);
						break; // assume only one matches
					}
				}
			}
		}
	}
#endif

	// Start in the sandbox.
	runArgs.AppendArg( "--workdir" );
	runArgs.AppendArg( inside_directory );

#ifdef WIN32
	// TODO: what do we do on Windows to set the --user argument?
#else
	uid_t uid;
	uid_t gid;
	if (can_switch_ids()) {
		// We've got root, and InitUidIds has been called
		uid = get_user_uid();
		gid = get_user_gid();
	} else {
		// We're a personal condor, we'll run the job
		// as the same uid/gid as we are
		uid = getuid();
		gid = getgid();
	}

	// This just shouldn't happen
	if ((uid == 0) || (gid == 0)) {
		dprintf(D_ALWAYS, "Failed to get userid to run docker job\n");
		return -9;
	}

	// docker breaks horribly with uid or gids bigger than 31 bits
	if ((uid > INT_MAX) || (gid > INT_MAX)) {
		dprintf(D_ALWAYS, "Trying to run docker job with > 31 bit uid(%uld) or gid(%uld), which docker does not support\n", uid, gid);
		return -9;
	}


	// Swamp project really wanted this knob, but everyone should
	// compile with this off.
#ifdef DOCKER_ALLOW_RUN_AS_ROOT
	if (param_boolean("DOCKER_RUN_AS_ROOT", false)) {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		uid = gid = 0;
		// The scratch directory is 700, so we'll at least need 0755 to run
		chmod(".", 0755);
	}
#endif
	runArgs.AppendArg("--user");
	std::string uidgidarg;
	formatstr(uidgidarg, "%d:%d", uid, gid);
	runArgs.AppendArg(uidgidarg);

	// Now the supplemental groups, if any exist
	char *user_name = 0;

	if (pcache()->get_user_name(uid, user_name)) {
		TemporaryPrivSentry sentry(PRIV_ROOT);
		{ // These need to be run as root
		pcache()->cache_uid(user_name);
		pcache()->cache_groups(user_name);
		}

		int num = pcache()->num_groups(user_name);
		if (num > 0) {
			gid_t groups[num];
			if (pcache()->get_groups(user_name, num, groups)) {
				for (int i = 0; i < num; i++) {
					runArgs.AppendArg("--group-add");
					std::string suppGroup;
					formatstr(suppGroup, "%d", groups[i]);
					runArgs.AppendArg(suppGroup);
				}
			}
		}
		free(user_name);
	}
#endif

	std::string networkType;
	jobAd.LookupString(ATTR_DOCKER_NETWORK_TYPE, networkType);
	bool setHostname = true;
	if (networkType == "host") {
		runArgs.AppendArg("--network=host");
		setHostname = false;
	} else if (networkType == "none") {
		runArgs.AppendArg("--network=none");
	} else if (networkType == "nat") {
		// leave empty, as this is the default
	} else if (!networkType.empty()) {
		// We always allow network type "none", "host", or "nat"
		// as "none" is the safest, "nat" is the default, should be 
		// pretty safe, and "host" is like a vanilla job
		// But others must be allowed by the admin
		std::string docker_networks;
		machineAd.LookupString(ATTR_DOCKER_NETWORKS, docker_networks);

		if (contains(split(docker_networks), networkType)) {
			std::string networkArg = "--network=";
			networkArg += networkType;
			runArgs.AppendArg(networkArg);
		} else {
			dprintf(D_ALWAYS, "Docker job requested network %s not allowed by parameter DOCKER_NETWORKS with value %s, refusing to run job\n", networkType.c_str(), docker_networks.c_str());
			return -1;
		}
	} 

	// Give the container a useful name, but not if using host networking
	if (setHostname) {
		std::string hname = makeHostname(&machineAd, &jobAd);
		runArgs.AppendArg("--hostname");
		runArgs.AppendArg(hname.c_str());
	}


	// Handle port forwarding.
	std::string containerServiceNames;
	jobAd.LookupString(ATTR_CONTAINER_SERVICE_NAMES, containerServiceNames);
	if(! containerServiceNames.empty()) {
		for (const auto& service: StringTokenIterator(containerServiceNames)) {
			int portNo = -1;
			std::string attrName;
			formatstr( attrName, "%s%s", service.c_str(), ATTR_CONTAINER_PORT_SUFFIX );
			if( jobAd.LookupInteger( attrName, portNo ) ) {
				runArgs.AppendArg("-p");
				runArgs.AppendArg(std::to_string(portNo));
				shouldAskForPorts = true;
			} else {
				// FIXME: This should actually be a hold message.
				dprintf( D_ALWAYS, "Requested container service '%s' did "
					"not specify a port, or the specified port was not "
					"a number.\n", service.c_str() );
				return -1;
			}
		}
	}

	std::string pullPolicy;
	jobAd.LookupString(ATTR_DOCKER_PULL_POLICY, pullPolicy);
	
	// docker create supports --pull (always|missing|never) today.
	// missing is the default, never seems useless for condor
	// If they mis-type this, ignore but warn

	lower_case(pullPolicy);
	if (pullPolicy == "always") {
		runArgs.AppendArg("--pull");
		runArgs.AppendArg(pullPolicy);
	} else {
		if (!pullPolicy.empty()) {
			dprintf(D_ALWAYS, "Ignoring unknown docker pull policy: %s\n", pullPolicy.c_str());
		}
	}


	std::string args_error;
	char *tmp = param("DOCKER_EXTRA_ARGUMENTS");
	if(!runArgs.AppendArgsV1RawOrV2Quoted(tmp, args_error)) {
		dprintf(D_ALWAYS,"docker: failed to parse extra arguments: %s\n",
		args_error.c_str());
		free(tmp);
		return -1;
	}
	if (tmp) free(tmp);

	long long shm_size = 0;
	if(param_longlong("DOCKER_SHM_SIZE", shm_size, false, 0, true,
		(std::numeric_limits<long long>::min)(),
		(std::numeric_limits<long long>::max)(), &machineAd, &jobAd)) {
		runArgs.AppendArg("--shm-size");
		runArgs.AppendArg(std::to_string(shm_size));
	}

	
	// Control image entry point if the job has an executable
	// Docker create has two different behaviour depending on the image entrypoint:
	// - Without entrypoint, first argument is the executable and the rest are its arguments
	// - With entrypoint, all the arguments are used for it, nothing special happens with the first one.
	// It is not frequent, but sometimes it is necessary to change the entrypoint.
	if (command.length() > 0) {
		bool overrideEntrypoint = false;
		jobAd.LookupBool(ATTR_DOCKER_OVERRIDE_ENTRYPOINT, overrideEntrypoint);
		if (overrideEntrypoint) {
			// Entrypoint flag must be before the image
			runArgs.AppendArg( "--entrypoint" );
			runArgs.AppendArg( command );
			
			runArgs.AppendArg( imageID );
		} else {
			// Add the command as the first argument of the image
			runArgs.AppendArg( imageID );
			runArgs.AppendArg( command );
		}
	} else {
		runArgs.AppendArg( imageID );
	}

	runArgs.AppendArgsFromArgList( args );

	std::string displayString;
	runArgs.GetArgsStringForLogging( displayString );
	dprintf( D_ALWAYS, "Attempting to run: %s\n", displayString.c_str() );

	//
	// If we run Docker attached, we avoid a race condition where
	// 'docker logs --follow' returns before 'docker rm' knows that the
	// container is gone (and refuses to remove it).  Of course, we
	// can't block, so we have a proxy process run attached for us.
	//
	FamilyInfo fi;
	Env cliEnvironment;
	build_env_for_docker_cli(cliEnvironment);

	// Add in env var to point at credentials we might need to pull
	
	bool use_creds = false;
	jobAd.LookupBool(ATTR_DOCKER_SEND_CREDENTIALS, use_creds);
	if (use_creds) {
		// This is where dockerProc dropped the creds...
		cliEnvironment.SetEnv("DOCKER_CONFIG", credentials_dir);
	}

	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );

	int childPID = daemonCore->CreateProcessNew( runArgs.GetArg(0), runArgs,
		OptionalCreateProcessArgs().priv(PRIV_CONDOR_FINAL)
			.wantCommandPort(FALSE).wantUDPCommandPort(FALSE)
			.env(&cliEnvironment).cwd("/").familyInfo(& fi)
			.std(childFDs).jobOptMask(DCJOBOPT_NO_ENV_INHERIT) );

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS, "Create_Process() failed.\n" );
		return -1;
	}
	pid = childPID;

	return 0;
}

int DockerAPI::startContainer(
	const std::string &containerName,
	int & pid,
	int * childFDs,
	CondorError & /* err */ ) {

	ArgList startArgs;
	if ( ! add_docker_arg(startArgs))
		return -1;
	startArgs.AppendArg("start");
	startArgs.AppendArg("-a"); // start in Attached mode
	startArgs.AppendArg(containerName);

	std::string displayString;
	startArgs.GetArgsStringForLogging( displayString );
	dprintf( D_ALWAYS, "Runnning: %s\n", displayString.c_str() );

	FamilyInfo fi;
	Env cliEnvironment;
	build_env_for_docker_cli(cliEnvironment);
	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );
	int childPID = daemonCore->Create_Process( startArgs.GetArg(0), startArgs,
		PRIV_CONDOR_FINAL, 1, FALSE, FALSE, &cliEnvironment, "/",
		& fi, NULL, childFDs, NULL, 0, NULL, DCJOBOPT_NO_ENV_INHERIT);

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS, "Create_Process() failed.\n" );
		return -1;
	}
	pid = childPID;

	return 0;
}


int
DockerAPI::pullImage(const std::string &image_name, 
		const std::string &credentials_dir,
		const std::string &diag_dir,
		const ClassAd &jobAd,
		int reaperId,
		CondorError &/*error*/) {

	gc_image(image_name);

	ArgList pullArgs;
	if ( ! add_docker_arg(pullArgs)) {
		return -1;
	}
	pullArgs.AppendArg("pull");
	pullArgs.AppendArg("-q"); // be quiet
	pullArgs.AppendArg(image_name);

	std::string displayString;
	pullArgs.GetArgsStringForLogging( displayString );
	dprintf( D_ALWAYS, "Runnning: %s\n", displayString.c_str() );

	Env cliEnvironment;
	build_env_for_docker_cli(cliEnvironment);

	// Add in env var to point at credentials we might need to pull
	
	bool use_creds = false;
	if (jobAd.LookupBool(ATTR_DOCKER_SEND_CREDENTIALS, use_creds)) {
		// This is where dockerProc dropped the creds...
		cliEnvironment.SetEnv("DOCKER_CONFIG", credentials_dir);
	}

	int childFDs[3] = { 0, 0, 0 };
	{
	TemporaryPrivSentry sentry(PRIV_USER);
	std::string DockerOutputFile = diag_dir + "/docker_stdout";
	std::string DockerErrorFile  = diag_dir + "/docker_stderror";

	childFDs[1] = open(DockerOutputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	childFDs[2] = open(DockerErrorFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	}


	int childPID = daemonCore->Create_Process( pullArgs.GetArg(0), pullArgs,
		PRIV_CONDOR_FINAL, reaperId, FALSE, FALSE, &cliEnvironment, "/",
		nullptr, nullptr, childFDs, nullptr, 0, nullptr, DCJOBOPT_NO_ENV_INHERIT);

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS, "Create_Process() failed for docker pull.\n" );
		return -1;
	}

	return 0;
}

int
DockerAPI::execInContainer( const std::string &containerName,
			    const std::string &command,
			    const ArgList     &arguments,
			    const Env &environment,
			    int *childFDs,
			    int reaperid,
			    int &pid) {

	ArgList execArgs;
	if ( ! add_docker_arg(execArgs))
		return -1;
	execArgs.AppendArg("exec");
	execArgs.AppendArg("-ti");

	if ( ! add_env_to_args_for_docker(execArgs, environment)) {
		dprintf( D_ALWAYS, "Failed to pass enviroment to docker.\n" );
		return -8;
	}

	execArgs.AppendArg(containerName);
	execArgs.AppendArg(command);

	execArgs.AppendArgsFromArgList(arguments);


	std::string displayString;
	execArgs.GetArgsStringForLogging( displayString );
	dprintf( D_ALWAYS, "execing: %s\n", displayString.c_str() );

	FamilyInfo fi;
	Env cliEnvironment;
	build_env_for_docker_cli(cliEnvironment);
	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );
	int childPID = daemonCore->Create_Process( execArgs.GetArg(0), execArgs,
		PRIV_CONDOR_FINAL, reaperid, FALSE, FALSE, &cliEnvironment, "/",
		& fi, NULL, childFDs );

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS, "Create_Process() failed to condor exec.\n" );
		return -1;
	}
	pid = childPID;

	return 0;
}

/*static*/ /* docker cp SRC_PATH CONTAINER : CONTAINER_PATH */
int DockerAPI::copyToContainer(const std::string & srcPath, // path on local file system to copy file/folder from
	const std::string & container,       // container to copy into
	const std::string & containerPath,     // destination path in container
	const std::vector<std::string>& options)
{
	ArgList args;
	if (! add_docker_arg(args))
		return -1;
	args.AppendArg("cp");

	for (auto& opt: options) {
		args.AppendArg(opt);
	}

	args.AppendArg(srcPath);

	std::string dest(container);
	dest += ":";
	dest += containerPath;
	args.AppendArg(dest);

	std::string displayString;
	args.GetArgsStringForLogging(displayString);
	dprintf(D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str());

	MyPopenTimer pgm;
	if (pgm.start_program(args, true, NULL, false) < 0) {
		dprintf(D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str());
		return -2;
	}

	int exitCode;
	if (! pgm.wait_for_exit(default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		std::string line;
		readLine(line, pgm.output(), false); chomp(line);
		dprintf(D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n",
			displayString.c_str(), exitCode, line.c_str());
		return -3;
	}

	return pgm.output_size() > 0;
}

/*static*/ /* docker cp CONTAINER:CONTAINER_PATH DEST_PATH */
int DockerAPI::copyFromContainer(const std::string &container, // container to copy into
	const std::string & containerPath,             // source file or folder in container
	const std::string & destPath,                  // destination path on local file system
	const std::vector<std::string>& options)
{
	ArgList args;
	if (! add_docker_arg(args))
		return -1;
	args.AppendArg("cp");

	for (auto& opt: options) {
		args.AppendArg(opt);
	}

	std::string src(container);
	src += ":";
	src += containerPath;
	args.AppendArg(src);

	args.AppendArg(destPath);

	std::string displayString;
	args.GetArgsStringForLogging(displayString);
	dprintf(D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str());


	MyPopenTimer pgm;
	if (pgm.start_program(args, true, NULL, false) < 0) {
		dprintf(D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str());
		return -2;
	}

	int exitCode;
	if (! pgm.wait_for_exit(default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		std::string line;
		readLine(line, pgm.output(), false); chomp(line);
		dprintf(D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n",
			displayString.c_str(), exitCode, line.c_str());
		return -3;
	}

	return pgm.output_size() > 0;
}

static int check_if_docker_offline(MyPopenTimer & pgmIn, const char * cmd_str, int original_error_code)
{
	int rval = original_error_code;
	// this should not be called with a program that is still running.
	ASSERT(pgmIn.is_closed());

	std::string line;
	MyStringCharSource * src = NULL;
	if (pgmIn.output_size() > 0) {
		src = &pgmIn.output();
		src->rewind();
	}

	bool check_for_hung_docker = true; // if no output, we should check for hung docker.
	dprintf( D_ALWAYS, "%s failed, %s output.\n", cmd_str, src ? "printing first few lines of" : "no" );
	if (src) {
		check_for_hung_docker = false; // if we got output, assume docker is not hung.
		for (int ii = 0; ii < 10; ++ii) {
			if ( ! readLine(line, *src, false)) break;
			dprintf( D_ALWAYS, "%s\n", line.c_str() );

			// if we got something resembling "/var/run/docker.sock: resource temporarily unavaible" 
			// then we should check for a hung docker.
			const char * p = strstr(line.c_str(), ".sock: resource ");
			if (p && strstr(p, "unavailable")) {
				check_for_hung_docker = true;
			}
		}
	}

	if (check_for_hung_docker) {
		dprintf( D_ALWAYS, "Checking to see if Docker is offline\n");

		ArgList infoArgs;
		if (!add_docker_arg(infoArgs)) {
			dprintf( D_ALWAYS, "Cannot do Docker offline check, DOCKER is not properly set\n");
			return DockerAPI::docker_hung;
		}

		infoArgs.AppendArg( "info" );
		std::string displayString;
		infoArgs.GetArgsStringForLogging( displayString );

		MyPopenTimer pgm2;
		if (pgm2.start_program(infoArgs, true, NULL, false) < 0) {
			dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
			rval = DockerAPI::docker_hung;
		} else {
			int exitCode = 0;
			if ( ! pgm2.wait_for_exit(60, &exitCode) || pgm2.output_size() <= 0) {
				dprintf( D_ALWAYS, "Failed to get output from '%s' : %s.\n", displayString.c_str(), pgm2.error_str() );
				rval = DockerAPI::docker_hung;
			} else {
				while (readLine(line, pgm2.output(),false)) {
					chomp(line);
					dprintf( D_FULLDEBUG, "[Docker Info] %s\n", line.c_str() );
				}
			}
		}

		if (rval == DockerAPI::docker_hung) {
			dprintf( D_ALWAYS, "Docker is not responding. returning docker_hung error code.\n");
		}
	}

	return rval;
}

int DockerAPI::rm( const std::string & containerID, CondorError & /* err */ ) {

	ArgList rmArgs;
	if ( ! add_docker_arg(rmArgs))
		return -1;
	rmArgs.AppendArg( "rm" );
	rmArgs.AppendArg( "-f" );  // if for some reason still running, kill first
	rmArgs.AppendArg( "-v" );  // also remove the volume
	rmArgs.AppendArg( containerID.c_str() );

	std::string displayString;
	rmArgs.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	// Read from Docker's combined output and error streams.
	TemporaryPrivSentry sentry(PRIV_ROOT);
	MyPopenTimer pgm;
	if (pgm.start_program( rmArgs, true, NULL, false ) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}
	const char * got_output = pgm.wait_and_close(default_timeout);

	// On a success, Docker writes the containerID back out.
	std::string line;
	if ( ! got_output || ! readLine(line, pgm.output(), false)) {
		int error = pgm.error_code();
		if( error ) {
			dprintf( D_ALWAYS, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), error );
			if (pgm.was_timeout()) {
				dprintf( D_ALWAYS, "Declaring a hung docker\n");
				return docker_hung;
			}
		} else {
			dprintf( D_ALWAYS, "'%s' returned nothing.\n", displayString.c_str() );
		}
		return -3;
	}

	chomp(line); trim(line);
	if (line != containerID) {
		// Didn't get back the result I expected, report the error and check to see if docker is hung.
		return check_if_docker_offline(pgm, "Docker remove", -4);
	}
	return 0;
}

int
DockerAPI::rmi(const std::string &image, CondorError &err) {
		// First, try to remove the named image
	TemporaryPrivSentry sentry(PRIV_ROOT);
	run_simple_docker_command("rmi", image, default_timeout, err, true);

		// That may have succeed or failed.  It could have
		// failed if the image doesn't exist (anymore), or
		// if someone else deleted it outside of condor.
		// Check to see if the image still exists.  If it
		// has been removed, return 0.

	ArgList args;
	if ( ! add_docker_arg(args))
		return -1;
	args.AppendArg( "images" );
	args.AppendArg( "-q" );
	args.AppendArg( image );

	std::string displayString;
	args.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

	MyPopenTimer pgm;
	Env env;
	build_env_for_docker_cli(env);
	if (pgm.start_program(args, true, &env, false) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		std::string line;
		readLine(line, pgm.output(), false); chomp(line);
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		return -3;
	}

	return pgm.output_size() > 0;
}


int
DockerAPI::kill(const std::string &image, CondorError &err) {
	return run_simple_docker_command("kill", image, default_timeout, err);
}

int
DockerAPI::kill(const std::string &image, int signal, CondorError &err) {
    ArgList args;
    args.AppendArg( "kill" );
    args.AppendArg( "--signal" );
    args.AppendArg( std::to_string(signal) );
    return run_docker_command( args, image, default_timeout, err);
}


int
DockerAPI::pause( const std::string & container, CondorError & err ) {
	return run_simple_docker_command("pause", container, default_timeout, err);
}

int
DockerAPI::unpause( const std::string & container, CondorError & err ) {
	return run_simple_docker_command("unpause", container, default_timeout, err);
}

#if !defined(WIN32)
int
sendDockerAPIRequest( const std::string & request, std::string & response ) {
	int uds = socket(AF_UNIX, SOCK_STREAM, 0);
	if (uds < 0) {
		dprintf(D_ALWAYS, "Can't create unix domain socket, no docker statistics will be available\n");
		return -1;
	}

	struct sockaddr_un sa;
	memset(&sa, 0, sizeof(sa));

	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, "/var/run/docker.sock",sizeof(sa.sun_path) - 1);

	{
	TemporaryPrivSentry sentry(PRIV_ROOT);
	int cr = connect(uds, (struct sockaddr *) &sa, sizeof(sa));
	if (cr != 0) {
		dprintf(D_ALWAYS, "Can't connect to /var/run/docker.sock %s, no statistics will be available\n", strerror(errno));
		close(uds);
		return -1;
	}
	}

	int ret = write(uds, request.c_str(), request.length());
	if (ret < 0) {
		dprintf(D_ALWAYS, "Can't send request to docker server, no statistics will be available\n");
		close(uds);
		return -1;
	}

	char buf[1024];
	int written;

	// read with 200 second timeout, no flags, nonblocking
	while ((written = condor_read("Docker Socket", uds, buf, 1, 5, 0, false)) > 0) {
		response.append(buf, written);
	}

	dprintf(D_FULLDEBUG, "sendDockerAPIRequest(%s) = %s\n",
		request.c_str(), response.c_str());
	close(uds);
	return 0;
}
#endif /* WIN32 */


	/* Find usage stats on a running container by talking
	 * directly to the server
	 */

int
DockerAPI::stats(const std::string &container, uint64_t &memUsage, uint64_t &netIn, uint64_t &netOut, uint64_t &userCpu, uint64_t &sysCpu) {
#if defined(WIN32)
	return -1;
#else
	std::string request, response;
	formatstr( request, "GET /containers/%s/stats?stream=0 HTTP/1.0\r\n\r\n", container.c_str());

	int rv = sendDockerAPIRequest( request, response );
	if( rv < 0 ) { return rv; }

	// Response now contains an enormous JSON formatted response
	// Hackily extract the fields we are interested in

	size_t pos;
	memUsage = netIn = netOut = userCpu = sysCpu = 0;

		// Would really like a real JSON parser here...
	pos = response.find("\"rss\"");
	if (pos != std::string::npos) {
		uint64_t tmp;
		int count = sscanf(response.c_str()+pos, "\"rss\":%" SCNu64, &tmp);
		if (count > 0) {
			memUsage = tmp;
		}
	} else {
		// docker running on cgroup v2 systems does not advertise rss.
		// Check for "usage", which include cached memory, which is wrong,
		// but better than nothing.
		pos = response.find("\"usage\"");
		if (pos != std::string::npos) {
			uint64_t tmp;
			int count = sscanf(response.c_str()+pos, "\"usage\":%" SCNu64, &tmp);
			if (count > 0) {
				memUsage = tmp;
			}
		}
	}
	pos = response.find("\"tx_bytes\"");
	if (pos != std::string::npos) {
		uint64_t tmp;
		int count = sscanf(response.c_str()+pos, "\"tx_bytes\":%" SCNu64, &tmp);
		if (count > 0) {
			netOut = tmp;
		}
	}
	pos = response.find("\"rx_bytes\"");
	if (pos != std::string::npos) {
		uint64_t tmp;
		int count = sscanf(response.c_str()+pos, "\"rx_bytes\":%" SCNu64, &tmp);
		if (count > 0) {
			netIn = tmp;
		}
	}
	pos = response.find("\"usage_in_usermode\"");
	if (pos != std::string::npos) {
		uint64_t tmp;
		int count = sscanf(response.c_str()+pos, "\"usage_in_usermode\":%" SCNu64, &tmp);
		if (count > 0) {
			userCpu = tmp;
		}
	}
	pos = response.find("\"usage_in_kernelmode\"");
	if (pos != std::string::npos) {
		uint64_t tmp;
		int count = sscanf(response.c_str()+pos, "\"usage_in_kernelmode\":%" SCNu64, &tmp);
		if (count > 0) {
			sysCpu = tmp;
		}
	}
	dprintf(D_FULLDEBUG, "docker stats reports max_usage is %" PRIu64 " rx_bytes is %" PRIu64 " tx_bytes is %" PRIu64 " usage_in_usermode is %" PRIu64 " usage_in-sysmode is %" PRIu64 "\n", memUsage, netIn, netOut, userCpu, sysCpu);

	return 0;
#endif
}

int DockerAPI::detect( CondorError & err ) {
	// FIXME: Remove ::version() as a public API and return it from here,
	// because there's no point in doing this twice.
	std::string version;
	int rval = DockerAPI::version( version, err );
	if( rval  != 0 ) {
		dprintf(D_ALWAYS, "DockerAPI::detect() failed to detect the Docker version; assuming absent.\n" );
		return -4;
	}

	ArgList infoArgs;
	if ( ! add_docker_arg(infoArgs))
		return -1;
	infoArgs.AppendArg( "info" );

	std::string displayString;
	infoArgs.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

	MyPopenTimer pgm;
	if (pgm.start_program(infoArgs, true, NULL, false) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		std::string line;
		readLine(line, pgm.output(), false); chomp(line);
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		dprintf( D_ALWAYS, "  Try adding condor to the docker group in /etc/group\n");
		return -3;
	}

	if (IsFulldebug(D_ALWAYS)) {
		std::string line;
		do {
			readLine(line, pgm.output(), false);
			chomp(line);
			dprintf( D_FULLDEBUG, "[docker info] %s\n", line.c_str() );
		} while (readLine(line, pgm.output(), false));
	}

	return 0;
}

int
DockerAPI::testImageRuns(CondorError &err) {

#ifndef LINUX
	(void)err; // shut the compiler up
	return 0;
#else
	TemporaryPrivSentry sentry(PRIV_ROOT);

	bool run_test = param_boolean("DOCKER_PERFORM_TEST", true);
	if (!run_test) return 0;

	// First, get the path to the test image on the local fs
	std::string test_image_path;
	param(test_image_path, "DOCKER_TEST_IMAGE_PATH");
	if (test_image_path.empty()) return true;

	// and the name thereof
	std::string test_image_name;
	param(test_image_name, "DOCKER_TEST_IMAGE_NAME");
	if (test_image_name.empty()) return true;

	// First, load the image from file system into the local Docker cache
	// This will quietly succeed if image is already installed
	int r = 0;

	ArgList loadArgs;
	loadArgs.AppendArg( "load" );
	loadArgs.AppendArg( "-i" ); // input from following file
	r  = run_docker_command(loadArgs, test_image_path, 20, err, true);

	dprintf(D_FULLDEBUG, "Tried to load docker test image, result was %d\n", r);
	if (r != 0) {
		return r; // false
	}

	// Now let's run a container from that image
	// Note that we can't use DockerAPI::createContainer, as that uses daemoncore
	// which isn't initialized when the starter runs this
	ArgList runArgs;
	runArgs.AppendArg("docker");
	runArgs.AppendArg("run");
	runArgs.AppendArg("--rm=true");
	runArgs.AppendArg(test_image_name);
	runArgs.AppendArg("/exit_37");

	MyPopenTimer pgm;
	pgm.start_program(
			runArgs,
			false,
			nullptr, // env
			false);  // false == don't drop privs -- docker needs priv to run

	int exit_status = -1;
	pgm.wait_for_exit(20, &exit_status);

	exit_status = WEXITSTATUS(exit_status);

	bool dockerWorks = false;
	if (exit_status == 37) {
		dprintf(D_ALWAYS, "Docker test container ran correctly!  Docker works!\n");
		dockerWorks = true;
	} else {
		dprintf(D_ALWAYS, "Docker test container ran incorrectly, returned %d unexpectedly\n", exit_status);
	}

	// we changed the local docker image cache by loading an image.  Let's
	// return to state by removing what we created
	ArgList rmiArgs;
	rmiArgs.AppendArg("rmi");
	r  = run_docker_command(rmiArgs, test_image_name, 20, err, true);
	dprintf(D_FULLDEBUG, "Tried to remove docker test image, result was %d\n", r);

	if (dockerWorks) {
		return 0;
	} else {
		return 1;
	}
#endif
}

//
// FIXME: We have a lot of boilerplate code in this function and file.
//
int DockerAPI::version( std::string & version, CondorError & /* err */ ) {

	ArgList versionArgs;
	if ( ! add_docker_arg(versionArgs))
		return -1;
	versionArgs.AppendArg( "-v" );

	std::string displayString;
	versionArgs.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

	MyPopenTimer pgm;
	if (pgm.start_program(versionArgs, false, NULL, false) < 0) {
		// treat 'file not found' as not really error
		int d_level = (pgm.error_code() == ENOENT) ? D_FULLDEBUG : D_ALWAYS;
		dprintf(d_level, "Failed to run '%s' errno=%d %s.\n", displayString.c_str(), pgm.error_code(), pgm.error_str() );
		return -2;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(default_timeout, &exitCode)) {
		pgm.close_program(1);
		dprintf( D_ALWAYS, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), pgm.error_code() );
		return -3;
	}

	if (pgm.output_size() <= 0) {
		dprintf( D_ALWAYS, "'%s' returned nothing.\n", displayString.c_str() );
		return -3;
	}

	MyStringSource * src = &pgm.output();
	std::string line;
	if (readLine(line, *src, false)) {
		chomp(line);
		bool jansens = strstr( line.c_str(), "Jansens" ) != NULL;
		bool bad_size = ! src->isEof() || line.size() > 1024 || line.size() < (int)sizeof("Docker version ");
		if (bad_size && ! jansens) {
			// check second line of output for the word Jansens also.
			std::string tmp; readLine(tmp, *src, false);
			jansens = strstr( tmp.c_str(), "Jansens" ) != NULL;
		}
		if (jansens) {
			dprintf( D_ALWAYS, "The DOCKER configuration setting appears to point to OpenBox's docker.  If you want to use Docker.IO, please set DOCKER appropriately in your configuration.\n" );
			return -5;
		} else if (bad_size) {
			dprintf( D_ALWAYS, "Read more than one line (or a very long line) from '%s', which we think means it's not Docker.  The (first line of the) trailing text was '%s'.\n", displayString.c_str(), line.c_str() );
			return -5;
		}
	}

	if( exitCode != 0 ) {
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str() );
		return -4;
	}

	version = line;

	int count = sscanf(version.c_str(), "Docker version %d.%d", &DockerAPI::majorVersion, &DockerAPI::minorVersion);
	if (count != 2) {
		dprintf(D_ALWAYS, "Could not parse docker version string %s\n", version.c_str());
	}
	return 0;
}
int DockerAPI::majorVersion = -1;
int DockerAPI::minorVersion = -1;

int DockerAPI::inspect( const std::string & containerID, ClassAd * dockerAd, CondorError & /* err */ ) {
	if( dockerAd == NULL ) {
		dprintf( D_ALWAYS, "dockerAd is NULL.\n" );
		return -2;
	}

	ArgList inspectArgs;
	if ( ! add_docker_arg(inspectArgs))
		return -1;
	inspectArgs.AppendArg( "inspect" );
	inspectArgs.AppendArg( "--format" );

	// Only DockerError is json formated, as that's the only one that
	// a string outside of our control.  Json formatting means no embedded
	// newlines or double quotes to worry about
	const std::string formatArg("ContainerId=\"{{.Id}}\"\n"
	                            "Pid={{.State.Pid}}\n"
	                            "Name=\"{{.Name}}\"\n"
	                            "Running={{.State.Running}}\n"
	                            "ExitCode={{.State.ExitCode}}\n"
	                            "StartedAt=\"{{.State.StartedAt}}\"\n"
	                            "FinishedAt=\"{{.State.FinishedAt}}\"\n"
	                            "DockerError={{json .State.Error}}\n"
	                            "OOMKilled=\"{{.State.OOMKilled}}\"");
	int formatCnt = std::ranges::count(formatArg, '\n') + 1;
	inspectArgs.AppendArg( formatArg );
	inspectArgs.AppendArg( containerID );

	std::string displayString;
	inspectArgs.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	MyPopenTimer pgm;
	if (pgm.start_program(inspectArgs, true, NULL, false) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -6;
	}

	MyStringSource * src = NULL;
	if (pgm.wait_and_close(default_timeout)) {
		src = &pgm.output();
	}

	int expected_rows = formatCnt;
	dprintf( D_FULLDEBUG, "exit_status=%d, error=%d, %d bytes. expecting %d lines\n",
		pgm.exit_status(), pgm.error_code(), pgm.output_size(), expected_rows );

	// If the output isn't exactly formatCnt lines long,
	// something has gone wrong and we'll at least be able to print out
	// the error message(s).
	std::vector<std::string> correctOutput(expected_rows);
	if (src) {
		std::string line;
		int i=0;
		while (readLine(line, *src, false)) {
			chomp(line);
			if (line.find('=') == std::string::npos) {
				continue;
			}
			//dprintf( D_FULLDEBUG, "\t [%2d] %s\n", i, line.c_str() );
			if (i >= expected_rows) {
				if (line.empty()) continue;
				correctOutput.push_back(line);
			} else {
				correctOutput[i] = line;
			}
			std::string::iterator first =
				std::find(correctOutput[i].begin(),
					correctOutput[i].end(),
					'\"');
			if (first != correctOutput[i].end()) {
				first++;
				if (first != correctOutput[i].end()) {
					std::replace(first,
						--correctOutput[i].end(), '\"','\'');
				}
			}
			++i;
		}
	}

	int attrCount = 0;
	for( int i = 0; i < formatCnt; ++i ) {
		if( correctOutput[i].empty() || dockerAd->Insert( correctOutput[i].c_str() ) == FALSE ) {
			break;
		}
		++attrCount;
	}

	if( attrCount != formatCnt ) {
		dprintf( D_ALWAYS, "Failed to create classad from Docker output (%d).  Printing up to the first %d (nonblank) lines.\n", attrCount, formatCnt );
		for( int i = 0; i < formatCnt && ! correctOutput[i].empty(); ++i ) {
			dprintf( D_ALWAYS, "%s\n", correctOutput[i].c_str() );
		}
		return -4;
	}

	dprintf( D_FULLDEBUG, "docker inspect printed:\n" );
	for( int i = 0; i < formatCnt && ! correctOutput[i].empty(); ++i ) {
		dprintf( D_FULLDEBUG, "\t%s\n", correctOutput[i].c_str() );
	}
	return 0;
}

int 
DockerAPI::tag(const std::string & src, const std::string &dst) {
	ArgList tagArgs;
	if ( ! add_docker_arg(tagArgs))
		return -1;
	tagArgs.AppendArg("tag");
	tagArgs.AppendArg(src);
	tagArgs.AppendArg(dst);

	std::string displayString;
	tagArgs.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	TemporaryPrivSentry sentry(PRIV_ROOT);
	MyPopenTimer pgm;
	if (pgm.start_program(tagArgs, true, nullptr, false) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -6;
	}

	pgm.wait_and_close(default_timeout);

	dprintf( D_FULLDEBUG, "exit_status=%d, error=%d, %d bytes\n",
		pgm.exit_status(), pgm.error_code(), pgm.output_size());

	return pgm.exit_status();
}

// in most cases we can't invoke docker directly because of it will be priviledged
// instead, DOCKER will be defined as 'sudo docker' or 'sudo /path/to/docker' so 
// we need to recognise this as two arguments and do the right thing.
static bool add_docker_arg(ArgList &runArgs) {
	std::string docker;
	if( ! param( docker, "DOCKER" ) ) {
		dprintf( D_ALWAYS, "DOCKER is undefined.\n" );
		return false;
	}

	const char * pdocker = docker.c_str();
	if (starts_with(docker, "sudo ")) {
		runArgs.AppendArg("/usr/bin/sudo");
		pdocker += 4;
		while (isspace(*pdocker)) ++pdocker;
		if ( ! *pdocker) {
			dprintf( D_ALWAYS, "DOCKER is defined as '%s' which is not valid.\n", docker.c_str() );
			return false;
		}
	}

	// Avoid startd log spam by checking if the docker binary
	// exists, and not try to run docker info, etc. if the binary
	// isn't there.
	struct stat buf;
	int r = stat(pdocker, &buf);
	if ((r < 0) && (errno == ENOENT)) {
		return false;
	}

	runArgs.AppendArg(pdocker);
	return true;
}

static bool docker_add_env_walker (void*pv, const std::string &var, const std::string &val) {
	ArgList* runArgs = (ArgList*)pv;
	std::string arg;
	arg.reserve(var.length() + val.length() + 2);
	arg = var;
	arg += "=";
	arg += val;
	runArgs->AppendArg("-e");
	runArgs->AppendArg(arg);
	return true; // true to keep iterating
}

// helper function, convert Env into arguments for docker run.
// essentially this means adding each as an argument of the form -e name=value
bool add_env_to_args_for_docker(ArgList &runArgs, const Env &env)
{
	dprintf(D_ALWAYS | D_VERBOSE, "adding %zu environment vars to docker args\n", env.Count());
	env.Walk(
#if 1 // sigh
		docker_add_env_walker,
#else
		// use a lamda to walk the environment and add each entry as
		[](void*pv, const std::string &var, const std::string &val) -> bool {
			ArgList& runArgs = *(ArgList*)pv;
			runArgs.AppendArg("-e");
			std::string arg(var); arg += "="; arg += val;
			runArgs.AppendArg(arg);
			return true; // true to keep iterating
		},
#endif
		&runArgs
	);

	return true;
}

int
run_docker_command(const ArgList & a, const std::string &container, int timeout, CondorError &, bool ignore_output)
{
  ArgList args;
  if ( ! add_docker_arg(args))
    return -1;
  args.AppendArgsFromArgList( a );
  args.AppendArg( container.c_str() );

  std::string displayString;
  args.GetArgsStringForLogging( displayString );
  dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	MyPopenTimer pgm;
	if (pgm.start_program( args, true, NULL, false ) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	if ( ! pgm.wait_and_close(timeout) || pgm.output_size() <= 0) {
		int error = pgm.error_code();
		if( error ) {
			dprintf( D_ALWAYS, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), error );
			if (pgm.was_timeout()) {
				dprintf( D_ALWAYS, "Declaring a hung docker\n");
				return DockerAPI::docker_hung;
			}
		} else {
			dprintf( D_ALWAYS, "'%s' returned nothing.\n", displayString.c_str() );
		}
		return -3;
	}

	// On a success, Docker writes the containerID back out.
	std::string line;
	readLine(line, pgm.output());
	chomp(line); trim(line);
	if (!ignore_output && line != container) {
		// Didn't get back the result I expected, report the error and check to see if docker is hung.
		std::string argString;
		args.GetArgsStringForDisplay(argString);
		dprintf( D_ALWAYS, "Docker invocation '%s' failed, printing first few lines of output.\n", argString.c_str());
		for (int ii = 0; ii < 10; ++ii) {
			if ( ! readLine(line, pgm.output(), false)) break;
			dprintf( D_ALWAYS, "%s\n", line.c_str() );
		}
		return -4;
	}
  return 0;
}

int
run_simple_docker_command(const std::string &command, const std::string &container, int timeout, CondorError & err, bool ignore_output)
{
    ArgList args; args.AppendArg(command);
    return run_docker_command(args, container, timeout, err, ignore_output);
}

static int
gc_image(const std::string & image) {

	int max_cache_size = param_integer("DOCKER_IMAGE_CACHE_SIZE", 8);
	if (max_cache_size < 0) max_cache_size = 0;

	std::vector<DockerAPI::ImageInfo> imageInfos = DockerAPI::getImageInfos();

	// If this image is already in the cache skip the gc step
	for (const auto &imageInfo: imageInfos) {
		if (imageInfo.imageName == image) {
			dprintf(D_FULLDEBUG, "Docker image cache already contains %s, skipping gc\n", image.c_str());
			return 0;
		}
	}

	auto notOurs = [](const std::string &name) -> bool {
		return ! name.starts_with("htcondor.org/");
	};

	{
		const auto [first, last] = std::ranges::remove_if(imageInfos, notOurs, &DockerAPI::ImageInfo::imageName);
		imageInfos.erase(first, last);
	}

	{ // Need to remove duplicate sha entries, they only consume one image
		std::ranges::sort(imageInfos, std::equal_to{}, &DockerAPI::ImageInfo::sha256);
		const auto [first, last] = std::ranges::unique(imageInfos, std::equal_to{}, &DockerAPI::ImageInfo::sha256);
		imageInfos.erase(first, last);
	}

	dprintf(D_FULLDEBUG, "Found %ld htcondor unique images, limit is %d\n", imageInfos.size(), max_cache_size);

	max_cache_size--; // As we are about to add one for the next job start

	// If fewer than our limit, our work here is done
	size_t needed_to_remove = imageInfos.size() - max_cache_size;
	if (needed_to_remove <= 0) {
		return true;
	}

	// Sort imageInfo by last access time
	// lastTagTime is ISO date, so string comparison works
	std::ranges::sort(imageInfos, std::less{}, &DockerAPI::ImageInfo::lastTagTime);

	// In lastTagTime order, remove as many images as we need to get under our limit
	for (const auto &imageInfo: imageInfos) {
		// If we've removed our limit, exit
		if (needed_to_remove <= 0) break;

		CondorError err;
		int result = DockerAPI::rmi(imageInfo.imageName, err);

		// Success at removing the annotated tag name
		if (result == 0) {
			std::string realImageName = DockerAPI::fromAnnotatedImageName(imageInfo.imageName);
			if (!realImageName.empty()) {
				int result = DockerAPI::rmi(realImageName, err);
				if (result != 0) {
					// Arg.  Can't remove real image.  Retag original
					DockerAPI::tag(realImageName, imageInfo.imageName);
				} else {
					dprintf(D_ALWAYS, "Removed image %s from docker image cache to stay under DOCkER_IMAGE_CACHE_SIZE\n", realImageName.c_str());
					needed_to_remove--;
				}
			}
		}
	} 

	return 0;
}

std::string
makeHostname(ClassAd *machineAd, ClassAd *jobAd) {
	std::string hostname;

	std::string owner("unknown");
	jobAd->LookupString(ATTR_OWNER, owner);

	hostname += owner;

	int cluster = 1;
	int proc = 1;
	jobAd->LookupInteger(ATTR_CLUSTER_ID, cluster);
	jobAd->LookupInteger(ATTR_PROC_ID, proc);
	formatstr_cat(hostname, "-%d.%d-", cluster, proc);

	std::string machine("host");
	machineAd->LookupString(ATTR_MACHINE, machine);

	hostname += machine;

	// Linux allows hostnames up to 256 characters,
	// but docker throws an error if they are greater then 63
	if (hostname.length() > 63) {
		hostname = hostname.substr(0,63);
	}

	return hostname;
}

// Return number of bytes used by our images in the image cache.
int64_t 
DockerAPI::imageCacheUsed() {
	int64_t total = 0;

	// get all the images
	std::vector<ImageInfo> imageInfos = getImageInfos();

	// htcondor images have tags that begin with htcondor.org/
	// remove anything else from the vector.
	
	auto notOurs = [](const std::string &name) -> bool {
		return ! name.starts_with("htcondor.org/");
	};

	{
		const auto [first, last] = std::ranges::remove_if(imageInfos, notOurs, &ImageInfo::imageName);
		imageInfos.erase(first, last);
	}

	{ // Need to remove duplicate sha entries, they only consume one image
		std::ranges::sort(imageInfos, std::equal_to{}, &ImageInfo::sha256);
		const auto [first, last] = std::ranges::unique(imageInfos, std::equal_to{}, &ImageInfo::sha256);
		imageInfos.erase(first, last);
	}

	// Sum up all the .size_in_bytes fields
	total = std::accumulate(imageInfos.begin(), imageInfos.end(), total, 
		[](int64_t accum, const ImageInfo &ii) {return accum + ii.size_in_bytes;});

	return total;
}

bool
DockerAPI::imageArchIsCompatible(const std::string &imageArch) {
	if (param_boolean("DOCKER_SKIP_IMAGE_ARCH_CHECK", false)) {
		return true;
	}
#ifdef X86_64
	// If we can't tell, be optimistic..
	if (imageArch.empty()) {
		dprintf(D_FULLDEBUG, "Docker image architecture was indeterminate, assuming it is compatible.\n");
		return true;
	}

	// docker-speak for x86-64 is "amd64"
	return (imageArch == "amd64");
#else
// Cowardly ignoring the problem on non-x86-64 architectures.  e.g arm Can
// be arm, aarch64, armv8, arm64, etc., and I'm not sure what's compatible with
// what
   dprintf(D_FULLDEBUG, "Ignoring docker image architecture check on non-x886 platform, arch was %s\n", imageArch.c_str());
   return true;
#endif
}

int
DockerAPI::getImageArch(const std::string &image_name, std::string &arch) {
	ArgList archArgs;
	if ( ! add_docker_arg(archArgs)) {
		return -1;
	}
	archArgs.AppendArg("inspect");
	archArgs.AppendArg("--format");
	archArgs.AppendArg("{{.Architecture}}");
	archArgs.AppendArg(image_name);

	std::string displayString;
	archArgs.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	TemporaryPrivSentry sentry(PRIV_ROOT);
	MyPopenTimer pgm;
	if (pgm.start_program( archArgs, true, nullptr, false ) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}
	const char * got_output = pgm.wait_and_close(default_timeout);

	// On a success, Docker writes the arch back out.
	std::string line;
	if ( ! got_output || ! readLine(line, pgm.output(), false)) {
		int error = pgm.error_code();
		if( error ) {
			dprintf( D_ALWAYS, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), error );
			if (pgm.was_timeout()) {
				dprintf( D_ALWAYS, "Declaring a hung docker\n");
				return docker_hung;
			}
		} else {
			dprintf( D_ALWAYS, "'%s' returned nothing.\n", displayString.c_str() );
		}
		return -3;
	}

	chomp(line); trim(line);
	arch = line;
	return 0;
}

int
DockerAPI::getServicePorts( const std::string & container,
  const ClassAd & jobAd, ClassAd & serviceAd ) {
#if defined(WIN32)
	return -1;
#else
	std::string request, response;
	formatstr( request, "GET /containers/%s/json HTTP/1.0\r\n\r\n",
		container.c_str() );
	int rv = sendDockerAPIRequest( request, response );
	if( rv < 0 ) { return rv; }

	// Strip the HTTP headers out of response.
	auto separator = response.find( "\r\n\r\n" );
	if( separator != std::string::npos ) {
		response = response.substr( separator + 4 );
	}

	// Convert the JSON blob to a ClassAd so we can deal with it.
	classad::ClassAd result;
	classad::ClassAdJsonParser cajp;
	if(! cajp.ParseClassAd( response, result, true )) {
		return -1;
	}

	//
    // When result goes out scope, it will free all the pointers returned here.
	//

	// This is clumsier than it seems like it ought to be.
	ExprTree * ns = result.Lookup( "NetworkSettings" );
	if( ns == NULL ) { return -2; }
	classad::ClassAd * nsCA = dynamic_cast< classad::ClassAd * >(ns);
	if( nsCA == NULL ) { return -2; }

	ExprTree * p = nsCA->Lookup( "Ports" );
	if( p == NULL ) { return -1; }
	classad::ClassAd * pCA = dynamic_cast< classad::ClassAd * >(p);
	if( pCA == NULL ) { return -1; }

    std::map< int, int > containerPortToHostPortMap;
	for( auto i = pCA->begin(); i != pCA->end(); ++i ) {
		ExprTree * hi = pCA->Lookup( i->first );
		classad::ExprList * hl = dynamic_cast< classad::ExprList * >(hi);
		if( hl == NULL ) { return -1; }

		std::vector< ExprTree * > pairs;
		hl->GetComponents( pairs );

		for( const auto j : pairs ) {
			classad::ClassAd * hostCA = dynamic_cast<classad::ClassAd *>(j);
			if( hostCA == NULL ) { return -1; }

			std::string hp;
			if(! hostCA->EvaluateAttrString( "HostPort", hp )) { return -1; }

			unsigned long containerPort = std::stoul( i->first );
			unsigned long hostPort = std::stoul( hp );
			containerPortToHostPortMap[containerPort] = hostPort;
			dprintf( D_FULLDEBUG, "DockerAPI::getServicePorts() - container port %lu <- host port %lu\n", containerPort, hostPort );
		}
	}

	std::string containerServiceNames;
	jobAd.LookupString(ATTR_CONTAINER_SERVICE_NAMES, containerServiceNames);
	if(! containerServiceNames.empty()) {
		for (const auto& service: StringTokenIterator(containerServiceNames)) {
		    int portNo = -1;
			std::string attrName;
			formatstr( attrName, "%s%s", service.c_str(), ATTR_CONTAINER_PORT_SUFFIX );
			if( jobAd.LookupInteger( attrName, portNo ) ) {
				if( containerPortToHostPortMap.count(portNo) ) {
					formatstr( attrName, "%s_%s", service.c_str(), "HostPort" );
					serviceAd.InsertAttr( attrName, containerPortToHostPortMap[portNo] );
				}
			}
		}

		dprintf( D_FULLDEBUG, "DockerAPI::getServicePorts() - service to host map:\n" );
		dPrintAd( D_FULLDEBUG, serviceAd );
	}

	return 0;
#endif /* defined(WIN32) */
}

// When we run the docker cli, it looks at environment variables to make decisions.
// Frequently, if condor is started by hand, HOME will be set to the user who started it, 
// which is usually different than the user we run the cli as.  If condor is started as root
// we run the cli as the condor user.  docker will look at the HOME environment variable, which 
// won't match the euid, and report confusing errors.  However, there are some environment variables
// like DOCKER_HOST, we which probably do want to obey.  So, we will pass to the cli (not the job inside
// the cli, mind you, but to the cli) a copy of the starter's environment with HOME removed.
//
void build_env_for_docker_cli(Env &env) {
			env.Clear();
			env.Import();
#ifdef LINUX
			env.DeleteEnv("HOME");
			struct passwd *pw = getpwuid(get_condor_uid());
			if (pw) {
				env.SetEnv("HOME", pw->pw_dir);
			}
#endif
}
std::string 
DockerAPI::toAnnotatedImageName(const std::string &rawImageName, const ClassAd &job) {
	std::string user;
	job.LookupString(ATTR_USER, user);

	if (user.empty()) {
		return "";
	}

	// tags cannot have @ in them (dots are ok, though)
	replace_str(user, "@", "_at_");

	return std::string("htcondor.org/" + user + "/" + rawImageName);
}

std::string 
DockerAPI::fromAnnotatedImageName(const std::string &annotatedName) {
	if (!annotatedName.starts_with("htcondor.org/")) {
		return "";
	}

	size_t firstSlash = annotatedName.find('/');
	size_t secondSlash = annotatedName.find('/', firstSlash + 1);
	std::string raw_name = annotatedName.substr(secondSlash + 1);;
	return raw_name;
}
static
size_t convert_number_with_suffix(std::string size) {
	size_t result = 0;
	std::from_chars(&size[0], &size[size.size() - 1], result);
	char unit = size.size() > 2 ? size[size.size() - 2] : '?';
	switch (unit) {
		case 'K':
		case 'k':
			result *= 1024ll;
			break;
		case 'M':
		case 'm':
			result *= (1024ll * 1024);
			break;
		case 'G':
		case 'g':
			result *= (1024ll * 1024 * 1024);
			break;
		case 'T': // Unlikely...
		case 't':
			result *= (1024ll * 1024 * 1024 * 1024);
			break;
		default:
			dprintf(D_ALWAYS, "Warning: unknown unit suffix %c in number %sn", unit, size.c_str());
	}
	return result;
}

std::vector<DockerAPI::ImageInfo>
DockerAPI::getImageInfos() {

#ifdef LINUX
	std::vector<DockerAPI::ImageInfo> result;

	TemporaryPrivSentry sentry(PRIV_ROOT);

	// First run docker images --format '{{.Repository}}:{{.Tag}} {{.ID}}
	// to get list of images by name and by their short id

	ArgList args;
	if ( ! add_docker_arg(args))
		return {};
	args.AppendArg( "images" );
	args.AppendArg( "--format");
	args.AppendArg( "{{.Repository}}:{{.Tag}} {{.ID}} {{.Size}}"); // repo/name:tag shortsha size_with_suffix

	std::string displayString;
	args.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	MyPopenTimer pgm;
	if (pgm.start_program(args, true, nullptr, false) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return {};
	}

	MyStringSource * src = nullptr;
	if (pgm.wait_and_close(default_timeout)) {
		src = &pgm.output();
	}

	dprintf( D_FULLDEBUG, "exit_status=%d, error=%d, %d bytes.\n",
		pgm.exit_status(), pgm.error_code(), pgm.output_size());

	// Loop over all the output, building up our vector of image names and shas
	if (src) {
		std::string line;
		while (readLine(line, *src, false)) {
			chomp(line);
			size_t first_space = line.find(' ');
			size_t second_space = line.find(' ', first_space + 1);
			if (first_space != std::string::npos) {
				std::string imageName = line.substr(0, first_space);
				if (imageName == "<none>") {
					// How does this happen?  Don't know, but it does
					continue;
				}

				std::string sha       = line.substr(first_space + 1, second_space - first_space - 1);
				std::string size      = line.substr(second_space + 1);
				size_t size_in_bytes = convert_number_with_suffix(size);
				result.emplace_back(imageName, sha, "", size_in_bytes);
			}
		}
	}

	// Unfortunately, we can't get the last tag name from docker images, 
	// we need "docker inspect" for that

	ArgList inspectArgs;
	if ( ! add_docker_arg(inspectArgs))
		return result;
	inspectArgs.AppendArg( "inspect" );
	inspectArgs.AppendArg( "--format");
	inspectArgs.AppendArg( "{{.Id}} {{.Metadata.LastTagTime}}");

	std::string allImages;
	for (const auto &imageInfo: result) {
		inspectArgs.AppendArg(imageInfo.sha256);
	}

	displayString.clear();
	inspectArgs.GetArgsStringForLogging( displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	MyPopenTimer inspectPgm;
	if (inspectPgm.start_program(inspectArgs, true, nullptr, false) < 0) {
		dprintf( D_ALWAYS, "Failed to run '%s'.\n", displayString.c_str() );
		return result;
	}

	if (inspectPgm.wait_and_close(default_timeout)) {
		src = &inspectPgm.output();
	}

	dprintf( D_FULLDEBUG, "exit_status=%d, error=%d, %d bytes.\n",
		inspectPgm.exit_status(), inspectPgm.error_code(), inspectPgm.output_size());

	// Loop over all the output, joining the last tag time to the image
	if (src) {
		std::string line;
		while (readLine(line, *src, false)) {
			chomp(line);
			size_t space = line.find(' ');
			if (space != std::string::npos) {
				// output is sha256:fullshaid lastTagtime
				size_t colon = line.find(':');
				std::string shaName     = line.substr(colon + 1, space - colon - 1);
				std::string lastTagTime = line.substr(space + 1);

				// Find the image with this sha, and add the lastTagTime
				for (auto &imageInfo: result) {
					//imageInfo.sha256 is the short sha, shaName is the long sha
					if (shaName.starts_with(imageInfo.sha256)) {
						imageInfo.lastTagTime = lastTagTime;
					}
				}
			}
		}
	}

	return result;
#else
	return {};
#endif
}

// We used to maintain the list of images pulled by HTCondor in a separate file,
// $(LOG)/.startd_docker_images. Sadly, this file was often removed by external
// sources, and as a result, we'd leak images.  The new way is to keep a special
// docker tag in the image cache, which we hope is more robust.
//
// When the startd boots, it should call this method, which transitions from the
// old way to the new way, by removing all images from the cache that are in the file
int 
DockerAPI::removeImagesInImageFile() {
	std::string imageFilename;
	if( ! param( imageFilename, "LOG" ) ) {
		dprintf(D_ALWAYS, "LOG not defined in param table, giving up\n");
		ASSERT(false);
	}

	imageFilename += "/.startd_docker_images";

	FILE *f = safe_fopen_wrapper_follow(imageFilename.c_str(), "r");

	if (f) {
		dprintf(D_ALWAYS, "Old %s file exists, about to docker rmi all cached images therein\n", imageFilename.c_str());
		char image_name[1024];
		while ( fgets(image_name, 1024, f)) {

			if (strlen(image_name) > 1) {
				image_name[strlen(image_name) - 1] = '\0'; // remove newline
			} else {
				continue; // zero length image name, skip
			}
			// and remove the image from the cache
			CondorError err;
			int r = DockerAPI::rmi(image_name, err);
			if (r < 0) {
				dprintf(D_ALWAYS, "Unable to docker rmi %s\n", image_name);
			}
		}
		fclose(f);
		std::ignore = remove(imageFilename.c_str());
		std::string lockFilename = imageFilename + ".lock";
		std::ignore = remove(lockFilename.c_str());
		return 0;
	}
	return 0; // nothing to do
}
