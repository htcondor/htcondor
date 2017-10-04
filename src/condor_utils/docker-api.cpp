#include "condor_common.h"

#include "env.h"
#include "my_popen.h"
#include "CondorError.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_daemon_core.h"
#include "file_lock.h"
#include "condor_rw.h"

#include "docker-api.h"
#include <algorithm>

#if !defined(WIN32)
#include <sys/un.h>
#endif

static bool add_env_to_args_for_docker(ArgList &runArgs, const Env &env);
static bool add_docker_arg(ArgList &runArgs);
static int run_simple_docker_command(	const std::string &command,
					const std::string &container, int timeout,
					CondorError &e, bool ignore_output=false);
static int gc_image(const std::string &image);
static std::string makeHostname(ClassAd *machineAd, ClassAd *jobAd);
static int check_if_docker_offline(MyPopenTimer & pgmIn, const char * cmd_str, int original_error_code);

int DockerAPI::default_timeout = 120;

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
	const std::string & sandboxPath,
	const std::list<std::string> extraVolumes,
	int & pid,
	int * childFDs,
	CondorError & /* err */ )
{
	gc_image(imageID);
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
			
		// --no-new-privileges flag appears in docker 1.11
		if (DockerAPI::majorVersion > 1 ||
		    DockerAPI::minorVersion > 10) {
			runArgs.AppendArg("--no-new-privileges");
		}
	}

	// Give the container a useful name
	std::string hname = makeHostname(&machineAd, &jobAd);
	runArgs.AppendArg("--hostname");
	runArgs.AppendArg(hname.c_str());

		// Now the container name
	runArgs.AppendArg( "--name" );
	runArgs.AppendArg( containerName );

	if ( ! add_env_to_args_for_docker(runArgs, env)) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to pass enviroment to docker.\n" );
		return -8;
	}

	// Map the external sanbox to the internal sandbox.
	runArgs.AppendArg( "--volume" );
	runArgs.AppendArg( sandboxPath + ":" + sandboxPath );

	// Now any extra volumes
	for (std::list<std::string>::const_iterator it = extraVolumes.begin(); it != extraVolumes.end(); it++) {
		runArgs.AppendArg("--volume");
		std::string volume = *it;
		runArgs.AppendArg(volume);
	}
	
	// Start in the sandbox.
	runArgs.AppendArg( "--workdir" );
	runArgs.AppendArg( sandboxPath );

	// Run with the uid that condor selects for the user
	// either a slot user or submitting user or nobody
	uid_t uid = 0;
	uid_t gid = 0;

	// Docker doesn't actually run on Windows, but we compile
	// on Windows because...
#ifndef WIN32
	uid = get_user_uid();
	gid = get_user_gid();
#endif
	
	if ((uid == 0) || (gid == 0)) {
		dprintf(D_ALWAYS|D_FAILURE, "Failed to get userid to run docker job\n");
		return -9;
	}

	runArgs.AppendArg("--user");
	std::string uidgidarg;
	formatstr(uidgidarg, "%d:%d", uid, gid);
	runArgs.AppendArg(uidgidarg);

	// Run the command with its arguments in the image.
	runArgs.AppendArg( imageID );

	
	// If no command given, the default command in the image will run
	if (command.length() > 0) {
		runArgs.AppendArg( command );
	}

	runArgs.AppendArgsFromArgList( args );

	MyString displayString;
	runArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_ALWAYS, "Attempting to run: %s\n", displayString.c_str() );

	//
	// If we run Docker attached, we avoid a race condition where
	// 'docker logs --follow' returns before 'docker rm' knows that the
	// container is gone (and refuses to remove it).  Of course, we
	// can't block, so we have a proxy process run attached for us.
	//
	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );
	int childPID = daemonCore->Create_Process( runArgs.GetArg(0), runArgs,
		PRIV_CONDOR_FINAL, 1, FALSE, FALSE, NULL, "/",
		& fi, NULL, childFDs );

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS | D_FAILURE, "Create_Process() failed.\n" );
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

	MyString displayString;
	startArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_ALWAYS, "Runnning: %s\n", displayString.c_str() );

	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );
	int childPID = daemonCore->Create_Process( startArgs.GetArg(0), startArgs,
		PRIV_CONDOR_FINAL, 1, FALSE, FALSE, NULL, "/",
		& fi, NULL, childFDs );

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS | D_FAILURE, "Create_Process() failed.\n" );
		return -1;
	}
	pid = childPID;

	return 0;
}

static int check_if_docker_offline(MyPopenTimer & pgmIn, const char * cmd_str, int original_error_code)
{
	int rval = original_error_code;
	// this should not be called with a program that is still running.
	ASSERT(pgmIn.is_closed());

	MyString line;
	MyStringCharSource * src = NULL;
	if (pgmIn.output_size() > 0) {
		src = &pgmIn.output();
		src->rewind();
	}

	bool check_for_hung_docker = true; // if no output, we should check for hung docker.
	dprintf( D_ALWAYS | D_FAILURE, "%s failed, %s output.\n", cmd_str, src ? "printing first few lines of" : "no" );
	if (src) {
		check_for_hung_docker = false; // if we got output, assume docker is not hung.
		for (int ii = 0; ii < 10; ++ii) {
			if ( ! line.readLine(*src, false)) break;
			dprintf( D_ALWAYS | D_FAILURE, "%s\n", line.c_str() );

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
		add_docker_arg(infoArgs);
		infoArgs.AppendArg( "info" );
		MyString displayString;
		infoArgs.GetArgsStringForLogging( & displayString );

		MyPopenTimer pgm2;
		if (pgm2.start_program(infoArgs, true, NULL, false) < 0) {
			dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
			rval = DockerAPI::docker_hung;
		} else {
			int exitCode = 0;
			if ( ! pgm2.wait_for_exit(60, &exitCode) || pgm2.output_size() <= 0) {
				dprintf( D_ALWAYS | D_FAILURE, "Failed to get output from '%s' : %s.\n", displayString.c_str(), pgm2.error_str() );
				rval = DockerAPI::docker_hung;
			} else {
				while (line.readLine(pgm2.output(),false)) {
					line.chomp();
					dprintf( D_FULLDEBUG, "[Docker Info] %s\n", line.c_str() );
				}
			}
		}

		if (rval == DockerAPI::docker_hung) {
			dprintf( D_ALWAYS | D_FAILURE, "Docker is not responding. returning docker_hung error code.\n");
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

	MyString displayString;
	rmArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	// Read from Docker's combined output and error streams.
#if 1
	MyPopenTimer pgm;
	if (pgm.start_program( rmArgs, true, NULL, false ) < 0) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}
	const char * got_output = pgm.wait_and_close(default_timeout);

	// On a success, Docker writes the containerID back out.
	MyString line;
	if ( ! got_output || ! line.readLine(pgm.output(), false)) {
		int error = pgm.error_code();
		if( error ) {
			dprintf( D_ALWAYS | D_FAILURE, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), error );
			if (pgm.was_timeout()) {
				dprintf( D_ALWAYS | D_FAILURE, "Declaring a hung docker\n");
				return docker_hung;
			}
		} else {
			dprintf( D_ALWAYS | D_FAILURE, "'%s' returned nothing.\n", displayString.c_str() );
		}
		return -3;
	}

	line.chomp(); line.trim();
	if (line != containerID.c_str()) {
		// Didn't get back the result I expected, report the error and check to see if docker is hung.
		return check_if_docker_offline(pgm, "Docker remove", -4);
	}
#else
	FILE * dockerResults = my_popen( rmArgs, "r", 1 , 0, false);
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	// On a success, Docker writes the containerID back out.
	char buffer[1024];
	if( NULL == fgets( buffer, 1024, dockerResults ) ) {
		if( errno ) {
			dprintf( D_ALWAYS | D_FAILURE, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), strerror( errno ), errno );
		} else {
			dprintf( D_ALWAYS | D_FAILURE, "'%s' returned nothing.\n", displayString.c_str() );
		}
		my_pclose( dockerResults );
		return -3;
	}

	int length = strlen( buffer );
	if( length < 1 || strncmp( buffer, containerID.c_str(), length - 1 ) != 0 ) {
		dprintf( D_ALWAYS | D_FAILURE, "Docker remove failed, printing first few lines of output.\n" );
		dprintf( D_ALWAYS | D_FAILURE, "%s", buffer );
		while( NULL != fgets( buffer, 1024, dockerResults ) ) {
			dprintf( D_ALWAYS | D_FAILURE, "%s", buffer );
		}
		my_pclose( dockerResults );
		return -4;
	}

	my_pclose( dockerResults );
#endif
	return 0;
}

int
DockerAPI::rmi(const std::string &image, CondorError &err) {
		// First, try to remove the named image
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

	MyString displayString;
	args.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

#if 1
	MyPopenTimer pgm;
	if (pgm.start_program(args, true, NULL, false) < 0) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		MyString line;
		line.readLine(pgm.output(), false); line.chomp();
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		return -3;
	}

	return pgm.output_size() > 0;
#else
	FILE * dockerResults = my_popen( args, "r", 1 , 0, false);
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	char buffer[1024];
	std::vector< std::string > output;
	while( fgets( buffer, 1024, dockerResults ) != NULL ) {
		size_t end = strlen(buffer);
		if (end > 0 && buffer[end-1] == '\n') { buffer[end-1] = '\0'; }
		output.push_back( buffer );
	}

	int exitCode = my_pclose( dockerResults );
	if( exitCode != 0 ) {
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, output[0].c_str() );
		return -3;
	}

	if (output.size() == 0) {
		return 0;
	} else {
		return 1;
	}
#endif
}


int
DockerAPI::kill(const std::string &image, CondorError &err) {
	return run_simple_docker_command("kill", image, default_timeout, err);
}


int 
DockerAPI::pause( const std::string & container, CondorError & err ) {
	return run_simple_docker_command("pause", container, default_timeout, err);
}

int 
DockerAPI::unpause( const std::string & container, CondorError & err ) {
	return run_simple_docker_command("unpause", container, default_timeout, err);
}

#if 0
static uint64_t
convertUnits(uint64_t raw, char *unit) {
	switch (*unit) {
	case 'B':
		return raw;
		break;
	case 'K':
		return raw * 1024;
		break;
	case 'M':
		return raw * 1024 * 1024;
		break;
	case 'G':
		return raw * 1024 * 1024 * 1024;
		break;
	case 'T':
		return raw * 1024 * 1024 * 1024 * 1024;
	default:
		return -1;
		break;
	}
}
#endif


	/* Find usage stats on a running container by talking
 	 * directly to the server
 	 */
 	 
int
DockerAPI::stats(const std::string &container, uint64_t &memUsage, uint64_t &netIn, uint64_t &netOut, uint64_t &userCpu, uint64_t &sysCpu) {

#if defined(WIN32)
	return -1;
#else

	/*
 	 * Create a Unix domain socket 
 	 *
 	 * This is what the docker server listens on
 	 */

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

	char request[256];

	sprintf(request, "GET /containers/%s/stats?stream=0 HTTP/1.0\r\n\r\n", container.c_str());
	int ret = write(uds, request, strlen(request));
	if (ret < 0) {
		dprintf(D_ALWAYS, "Can't send request to docker server, no statistics will be available\n");
		close(uds);
		return -1;
	}

	std::string response;

	char buf[1024];
	int written;

	// read with 200 second timeout, no flags, nonblocking
	while ((written = condor_read("Docker Socket", uds, buf, 1, 5, 0, false)) > 0) {
		response.append(buf, written);
	} 

	dprintf(D_FULLDEBUG, "docker stats: %s\n", response.c_str());
	close(uds);

	// Response now contains an enormous JSON formatted response
	// Hackily extract the fields we are interested in
	
	size_t pos;
	memUsage = netIn = netOut = userCpu = sysCpu = 0;

		// Would really like a real JSON parser here...
	pos = response.find("\"rss\"");
	if (pos != std::string::npos) {
		sscanf(response.c_str()+pos, "\"rss\":%" SCNu64, &memUsage);
	}
	pos = response.find("\"tx_bytes\"");
	if (pos != std::string::npos) {
		sscanf(response.c_str()+pos, "\"tx_bytes\":%" SCNu64, &netOut);
	}
	pos = response.find("\"rx_bytes\"");
	if (pos != std::string::npos) {
		sscanf(response.c_str()+pos, "\"rx_bytes\":%" SCNu64, &netIn);
	}
	pos = response.find("\"usage_in_usermode\"");
	if (pos != std::string::npos) {
		sscanf(response.c_str()+pos, "\"usage_in_usermode\":%" SCNu64, &userCpu);
	}
	pos = response.find("\"usage_in_kernelmode\"");
	if (pos != std::string::npos) {
		sscanf(response.c_str()+pos, "\"usage_in_kernelmode\":%" SCNu64, &sysCpu);
	}
	dprintf(D_FULLDEBUG, "docker stats reports max_usage is %" PRIu64 " rx_bytes is %" PRIu64 " tx_bytes is %" PRIu64 " usage_in_usermode is %" PRIu64 " usage_in-sysmode is %" PRIu64 "\n", memUsage, netIn, netOut, userCpu, sysCpu);

	return 0;
#endif

} /*
 *  getting stats by running docker stats is now deprecated
 */

#if 0
int
DockerAPI::stats(const std::string &container, uint64_t &memUsage, uint64_t &netIn, uint64_t &netOut) {
newstats(container, memUsage, netIn, netOut);
	ArgList args;
	if ( ! add_docker_arg(args))
		return -1;
	args.AppendArg( "stats" );
	args.AppendArg( "--no-stream" );
	args.AppendArg( container.c_str() );

	MyString displayString;
	TemporaryPrivSentry sentry(PRIV_ROOT);

	args.GetArgsStringForLogging( & displayString );

	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

		// Read from Docker's combined output and error streams.
	FILE * dockerResults = my_popen( args, "r", 1 , 0, false);
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	const int statLineSize = 256;
	char header[statLineSize];
	char data[statLineSize];
	if( NULL == fgets( header, statLineSize, dockerResults ) ) {
		my_pclose(dockerResults);
		return -2;
	}

	if( NULL == fgets( data, statLineSize, dockerResults ) ) {
		my_pclose(dockerResults);
		return -2;
	}
	my_pclose(dockerResults);

	dprintf(D_FULLDEBUG, "Docker stats data is:\n%s\n", data);
		// condor stats output looks like:
		// Name	cpu%   mem usage/limit  mem%  net i/o   block i/o
		// container  0.00%  0 B / 0 B        0.00% 0 B / 0 B  0 B / 0 B
		//
	char memUsageUnit[2];
	char netInUnit[2];
	char netOutUnit[2];

	double memRaw, netInRaw, netOutRaw;
	int matches = sscanf(data, "%*s %*g%% %lg %s / %*g %*s %*g%% %lg %s / %lg %s", &memRaw, &memUsageUnit[0], &netInRaw, &netInUnit[0], &netOutRaw, &netOutUnit[0]);
	if (matches < 6) {
		return -2;
	}

	memUsage = convertUnits(memRaw, memUsageUnit);
	netIn    = convertUnits(netInRaw, netInUnit);
	netOut   = convertUnits(netOutRaw, netOutUnit);
  
	dprintf(D_FULLDEBUG, "memUsage is %g (%s), net In is %g (%s), net Out is %g (%s)\n", memRaw, memUsageUnit, netInRaw, netInUnit, netOutRaw, netOutUnit);
	return 0;
}
#endif

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

	MyString displayString;
	infoArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

#if 1
	MyPopenTimer pgm;
	if (pgm.start_program(infoArgs, true, NULL, false) < 0) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(default_timeout, &exitCode) || exitCode != 0) {
		pgm.close_program(1);
		MyString line;
		line.readLine(pgm.output(), false); line.chomp();
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str());
		return -3;
	}

	if (IsFulldebug(D_ALWAYS)) {
		MyString line;
		do {
			line.readLine(pgm.output(), false);
			line.chomp();
			dprintf( D_FULLDEBUG, "[docker info] %s\n", line.c_str() );
		} while (line.readLine(pgm.output(), false));
	}

#else
	FILE * dockerResults = my_popen( infoArgs, "r", 1 , 0, false);
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	// Even if we don't care about the success output, the failure output
	// can be handy for debugging...
	char buffer[1024];
	std::vector< std::string > output;
	while( fgets( buffer, 1024, dockerResults ) != NULL ) {
		size_t end = strlen(buffer);
		if (end > 0 && buffer[end-1] == '\n') { buffer[end-1] = '\0'; }
		output.push_back( buffer );
	}
	for( unsigned i = 0; i < output.size(); ++i ) {
		dprintf( D_FULLDEBUG, "[docker info] %s\n", output[i].c_str() );
	}

	int exitCode = my_pclose( dockerResults );
	if( exitCode != 0 ) {
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, output[0].c_str() );
		return -3;
	}
#endif
	return 0;
}

//
// FIXME: We have a lot of boilerplate code in this function and file.
//
int DockerAPI::version( std::string & version, CondorError & /* err */ ) {

	ArgList versionArgs;
	if ( ! add_docker_arg(versionArgs))
		return -1;
	versionArgs.AppendArg( "-v" );

	MyString displayString;
	versionArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

#if 1
	MyPopenTimer pgm;
	if (pgm.start_program(versionArgs, false, NULL, false) < 0) {
		// treat 'file not found' as not really error
		int d_level = (pgm.error_code() == ENOENT) ? D_FULLDEBUG : (D_ALWAYS | D_FAILURE);
		dprintf(d_level, "Failed to run '%s' errno=%d %s.\n", displayString.c_str(), pgm.error_code(), pgm.error_str() );
		return -2;
	}

	int exitCode;
	if ( ! pgm.wait_for_exit(default_timeout, &exitCode)) {
		pgm.close_program(1);
		dprintf( D_ALWAYS | D_FAILURE, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), pgm.error_code() );
		return -3;
	}

	if (pgm.output_size() <= 0) {
		dprintf( D_ALWAYS | D_FAILURE, "'%s' returned nothing.\n", displayString.c_str() );
		return -3;
	}

	MyStringSource * src = &pgm.output();
	MyString line;
	if (line.readLine(*src, false)) {
		line.chomp();
		bool jansens = strstr( line.c_str(), "Jansens" ) != NULL;
		bool bad_size = ! src->isEof() || line.size() > 1024 || line.size() < (int)sizeof("Docker version ");
		if (bad_size && ! jansens) {
			// check second line of output for the word Jansens also.
			MyString tmp; tmp.readLine(*src, false);
			jansens = strstr( tmp.c_str(), "Jansens" ) != NULL;
		}
		if (jansens) {
			dprintf( D_ALWAYS | D_FAILURE, "The DOCKER configuration setting appears to point to OpenBox's docker.  If you want to use Docker.IO, please set DOCKER appropriately in your configuration.\n" );
			return -5;
		} else if (bad_size) {
			dprintf( D_ALWAYS | D_FAILURE, "Read more than one line (or a very long line) from '%s', which we think means it's not Docker.  The (first line of the) trailing text was '%s'.\n", displayString.c_str(), line.c_str() );
			return -5;
		}
	}

	if( exitCode != 0 ) {
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, line.c_str() );
		return -4;
	}

	version = line.c_str();

#else
	FILE * dockerResults = my_popen( versionArgs, "r", 1 , 0, false);
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	char buffer[1024];
	if( NULL == fgets( buffer, 1024, dockerResults ) ) {
		if( errno ) {
			dprintf( D_ALWAYS | D_FAILURE, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), strerror( errno ), errno );
		} else {
			dprintf( D_ALWAYS | D_FAILURE, "'%s' returned nothing.\n", displayString.c_str() );
		}
		my_pclose( dockerResults );
		return -3;
	}

	if( NULL != fgets( buffer, 1024, dockerResults ) ) {
		if( strstr( buffer, "Jansens" ) != NULL ) {
			dprintf( D_ALWAYS | D_FAILURE, "The DOCKER configuration setting appears to point to OpenBox's docker.  If you want to use Docker.IO, please set DOCKER appropriately in your configuration.\n" );
		} else {
			dprintf( D_ALWAYS | D_FAILURE, "Read more than one line (or a very long line) from '%s', which we think means it's not Docker.  The (first line of the) trailing text was '%s'.\n", displayString.c_str(), buffer );
		}
		my_pclose( dockerResults );
		return -5;
	}

	int exitCode = my_pclose( dockerResults );
	if( exitCode != 0 ) {
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, buffer );
		return -4;
	}

	size_t end = strlen(buffer);
	if (end > 0 && buffer[end-1] == '\n') { buffer[end-1] = '\0'; }
	version = buffer;
#endif
	sscanf(version.c_str(), "Docker version %d.%d", &DockerAPI::majorVersion, &DockerAPI::minorVersion);
	return 0;
}
int DockerAPI::majorVersion = -1;
int DockerAPI::minorVersion = -1;

int DockerAPI::inspect( const std::string & containerID, ClassAd * dockerAd, CondorError & /* err */ ) {
	if( dockerAd == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "dockerAd is NULL.\n" );
		return -2;
	}

	ArgList inspectArgs;
	if ( ! add_docker_arg(inspectArgs))
		return -1;
	inspectArgs.AppendArg( "inspect" );
	inspectArgs.AppendArg( "--format" );
	StringList formatElements(	"ContainerId=\"{{.Id}}\" "
								"Pid={{.State.Pid}} "
								"Name=\"{{.Name}}\" "
								"Running={{.State.Running}} "
								"ExitCode={{.State.ExitCode}} "
								"StartedAt=\"{{.State.StartedAt}}\" "
								"FinishedAt=\"{{.State.FinishedAt}}\" "
								"DockerError=\"{{.State.Error}}\" "
								"OOMKilled=\"{{.State.OOMKilled}}\" " );
	char * formatArg = formatElements.print_to_delimed_string( "\n" );
	inspectArgs.AppendArg( formatArg );
	free( formatArg );
	inspectArgs.AppendArg( containerID );

	MyString displayString;
	inspectArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

#if 1
	MyPopenTimer pgm;
	if (pgm.start_program(inspectArgs, true, NULL, false) < 0) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -6;
	}

	MyStringSource * src = NULL;
	if (pgm.wait_and_close(default_timeout)) {
		src = &pgm.output();
	}

	int expected_rows = formatElements.number();
	dprintf( D_FULLDEBUG, "exit_status=%d, error=%d, %d bytes. expecting %d lines\n",
		pgm.exit_status(), pgm.error_code(), pgm.output_size(), expected_rows );

	// If the output isn't exactly formatElements.number() lines long,
	// something has gone wrong and we'll at least be able to print out
	// the error message(s).
	std::vector<std::string> correctOutput(expected_rows);
	if (src) {
		MyString line;
		int i=0;
		while (line.readLine(*src,false)) {
			line.chomp();
			if (line.find("=") == -1) {
				continue;
			}
			//dprintf( D_FULLDEBUG, "\t [%2d] %s\n", i, line.c_str() );
			if (i >= expected_rows) {
				if (line.empty()) continue;
				correctOutput.push_back(line.c_str());
			} else {
				correctOutput[i] = line.c_str();
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
#else
	FILE * dockerResults = my_popen( inspectArgs, "r", 1 , 0, false);
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Unable to run '%s'.\n", displayString.c_str() );
		return -6;
	}

	// If the output isn't exactly formatElements.number() lines long,
	// something has gone wrong and we'll at least be able to print out
	// the error message(s).
	char buffer[1024];
	std::vector<std::string> correctOutput(formatElements.number());
	for( int i = 0; i < formatElements.number(); ++i ) {
		if( fgets( buffer, 1024, dockerResults ) != NULL ) {
			correctOutput[i] = buffer;
			std::string::iterator first = 
				std::find(correctOutput[i].begin(),
					correctOutput[i].end(),
					'\"');
			if (first != correctOutput[i].end()) {
				std::replace(++first,
					-- --correctOutput[i].end(), '\"','\'');
			}
		}
	}
	my_pclose( dockerResults );
#endif

	int attrCount = 0;
	for( int i = 0; i < formatElements.number(); ++i ) {
		if( correctOutput[i].empty() || dockerAd->Insert( correctOutput[i].c_str() ) == FALSE ) {
			break;
		}
		++attrCount;
	}

	if( attrCount != formatElements.number() ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to create classad from Docker output (%d).  Printing up to the first %d (nonblank) lines.\n", attrCount, formatElements.number() );
		for( int i = 0; i < formatElements.number() && ! correctOutput[i].empty(); ++i ) {
			dprintf( D_ALWAYS | D_FAILURE, "%s\n", correctOutput[i].c_str() );
		}
		return -4;
	}

	dprintf( D_FULLDEBUG, "docker inspect printed:\n" );
	for( int i = 0; i < formatElements.number() && ! correctOutput[i].empty(); ++i ) {
		dprintf( D_FULLDEBUG, "\t%s\n", correctOutput[i].c_str() );
	}
	return 0;
}

// in most cases we can't invoke docker directly because of it will be priviledged
// instead, DOCKER will be defined as 'sudo docker' or 'sudo /path/to/docker' so 
// we need to recognise this as two arguments and do the right thing.
static bool add_docker_arg(ArgList &runArgs) {
	std::string docker;
	if( ! param( docker, "DOCKER" ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "DOCKER is undefined.\n" );
		return false;
	}
	const char * pdocker = docker.c_str();
	if (starts_with(docker, "sudo ")) {
		runArgs.AppendArg("/usr/bin/sudo");
		pdocker += 4;
		while (isspace(*pdocker)) ++pdocker;
		if ( ! *pdocker) {
			dprintf( D_ALWAYS | D_FAILURE, "DOCKER is defined as '%s' which is not valid.\n", docker.c_str() );
			return false;
		}
	}
	runArgs.AppendArg(pdocker);
	return true;
}

static bool docker_add_env_walker (void*pv, const MyString &var, const MyString &val) {
	ArgList* runArgs = (ArgList*)pv;
	MyString arg;
	arg.reserve_at_least(var.length() + val.length() + 2);
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
	dprintf(D_ALWAYS | D_VERBOSE, "adding %d environment vars to docker args\n", env.Count());
	env.Walk(
#if 1 // sigh
		docker_add_env_walker,
#else
		// use a lamda to walk the environment and add each entry as
		[](void*pv, const MyString &var, const MyString &val) -> bool {
			ArgList& runArgs = *(ArgList*)pv;
			runArgs.AppendArg("-e");
			MyString arg(var); arg += "="; arg += val;
			runArgs.AppendArg(arg);
			return true; // true to keep iterating
		},
#endif
		&runArgs
	);

	return true;
}

int
run_simple_docker_command(const std::string &command, const std::string &container, int timeout, CondorError &, bool ignore_output)
{
  ArgList args;
  if ( ! add_docker_arg(args))
    return -1;
  args.AppendArg( command );
  args.AppendArg( container.c_str() );

  MyString displayString;
  args.GetArgsStringForLogging( & displayString );
  dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

#if 1
	MyPopenTimer pgm;
	if (pgm.start_program( args, true, NULL, false ) < 0) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	if ( ! pgm.wait_and_close(timeout) || pgm.output_size() <= 0) {
		int error = pgm.error_code();
		if( error ) {
			dprintf( D_ALWAYS | D_FAILURE, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), pgm.error_str(), error );
			if (pgm.was_timeout()) {
				dprintf( D_ALWAYS | D_FAILURE, "Declaring a hung docker\n");
				return DockerAPI::docker_hung;
			}
		} else {
			dprintf( D_ALWAYS | D_FAILURE, "'%s' returned nothing.\n", displayString.c_str() );
		}
		return -3;
	}

	// On a success, Docker writes the containerID back out.
	MyString line;
	line.readLine(pgm.output());
	line.chomp(); line.trim();
	if (!ignore_output && line != container.c_str()) {
		// Didn't get back the result I expected, report the error and check to see if docker is hung.
		dprintf( D_ALWAYS | D_FAILURE, "Docker %s failed, printing first few lines of output.\n", command.c_str());
		for (int ii = 0; ii < 10; ++ii) {
			if ( ! line.readLine(pgm.output(), false)) break;
			dprintf( D_ALWAYS | D_FAILURE, "%s\n", line.c_str() );
		}
		return -4;
	}

#else
  // Read from Docker's combined output and error streams.
  FILE * dockerResults = my_popen( args, "r", 1 , 0, false);
  if( dockerResults == NULL ) {
    dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
    return -2;
  }

  // On a success, Docker writes the containerID back out.
  char buffer[1024];
  if( NULL == fgets( buffer, 1024, dockerResults ) ) {
    if( errno ) {
      dprintf( D_ALWAYS | D_FAILURE, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), strerror( errno ), errno );
    } else {
      dprintf( D_ALWAYS | D_FAILURE, "'%s' returned nothing.\n", displayString.c_str() );
    }
    my_pclose( dockerResults );
    return -3;
  }

  size_t length = strlen( buffer );
  if (!ignore_output) {
    if( length < 1 || strncmp( buffer, container.c_str(), length - 1 ) != 0 ) {
      dprintf( D_ALWAYS | D_FAILURE, "Docker %s failed, printing first few lines of output.\n", command.c_str() );
      dprintf( D_ALWAYS | D_FAILURE, "%s", buffer );
      while( NULL != fgets( buffer, 1024, dockerResults ) ) {
	dprintf( D_ALWAYS | D_FAILURE, "%s", buffer );
      }
      my_pclose( dockerResults );
      return -4;
    }
  }

  my_pclose( dockerResults );
#endif
  return 0;
}

static int 
gc_image(const std::string & image) {

  std::list<std::string> images;
  std::string imageFilename;
  
  int cache_size = param_integer("DOCKER_IMAGE_CACHE_SIZE", 20);
  cache_size--;
  if (cache_size < 0) cache_size = 0;

  if( ! param( imageFilename, "LOG" ) ) {
    dprintf(D_ALWAYS, "LOG not defined in param table, giving up\n");
    ASSERT(false);
  }

  
  TemporaryPrivSentry sentry(PRIV_ROOT);
  imageFilename += "/.startd_docker_images";

  int lockfd = safe_open_wrapper_follow(imageFilename.c_str(), O_WRONLY|O_CREAT, 0666);

  if (lockfd < 0) {
    dprintf(D_ALWAYS, "Can't open %s for locking: %s\n", imageFilename.c_str(), strerror(errno));
    ASSERT(false);
  }
  FileLock lock(lockfd, NULL, imageFilename.c_str());
  lock.obtain(WRITE_LOCK); // blocking

  FILE *f = safe_fopen_wrapper_follow(imageFilename.c_str(), "r");

  if (f) {
    char existingImage[1024];
    while ( fgets(existingImage, 1024, f)) {

      if (strlen(existingImage) > 1) {
	existingImage[strlen(existingImage) - 1] = '\0'; // remove newline
      }
      std::string tmp(existingImage);
      //
      // If we're reusing an image, we'll shuffle it to the end
      if (tmp != image) {
	images.push_back(tmp);
      }
    }
    fclose(f);
  }

  dprintf(D_ALWAYS, "Found %lu entries in docker image cache.\n", images.size());

   std::list<std::string>::iterator iter;
   int remove_count = (int)images.size() - cache_size;
   if (remove_count < 0) remove_count = 0;

   for (iter = images.begin(); iter != images.end(); iter++) {
    if (remove_count <= 0) break;
    std::string toRemove = *iter;

    CondorError err;
    int result = DockerAPI::rmi(toRemove, err);

    if (result == 0) {
      images.erase(iter);
      remove_count--;
    }
  }

  images.push_back(image); // our current image is the most recent one

  f = safe_fopen_wrapper_follow(imageFilename.c_str(), "w");
  if (f) {
    std::list<std::string>::iterator it;
    for (it = images.begin(); it != images.end(); it++) {
      fputs((*it).c_str(), f);
      fputs("\n", f);
    }
    fclose(f);
  } else {
    dprintf(D_ALWAYS, "Can't write to docker images file: %s\n", imageFilename.c_str());
    ASSERT(false);
  }

  lock.release();
  close(lockfd);

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

	return hostname;
}
