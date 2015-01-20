#include "condor_common.h"

#include "env.h"
#include "my_popen.h"
#include "CondorError.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_daemon_core.h"

#include "docker-api.h"

bool add_env_to_args_for_docker(ArgList &runArgs, const Env &env);

//
// Because we fork before calling docker, we don't actually
// care if the image is stored locally or not (except to the extent that
// remote image pull violates the principle of least astonishment).
//
int DockerAPI::run(
	const std::string & containerName,
	const std::string & imageID,
	const std::string & command,
	const ArgList & args,
	const Env & env,
	const std::string & sandboxPath,
	std::string & containerID,
	int & pid,
	int * childFDs,
	CondorError & /* err */ )
{
	//
	// We currently assume that the system has been configured so that
	// anyone (user) who can run an HTCondor job can also run docker.  It's
	// also apparently a security worry to run Docker as root, so let's not.
	//
	ArgList runArgs;
	std::string docker;
	if( ! param( docker, "DOCKER" ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "DOCKER is undefined.\n" );
		return -1;
	}
	runArgs.AppendArg( docker );
	runArgs.AppendArg( "run" );
	runArgs.AppendArg( "--tty" );

	// Write out a file with the container ID.
	// FIXME: The startd can check this to clean up after us.
	std::string cidFileName = sandboxPath + "/.cidfile";
	runArgs.AppendArg( "--cidfile=" + cidFileName );

	// FIXME: Configure resource limits.
	// runArgs.AppendArg( "--cpu-shares=<10x request_cpus>" );
	// runArgs.AppendArg( "--memory=<slot memory attribute>" );

	// FIXME: this is safer, but will cause spurious errors when we
	// try to remove the already-removed container.
	// Remove the container on a successful run.
	// runArgs.AppendArg( "--rm=true" );
	runArgs.AppendArg( "--name" );
	runArgs.AppendArg( containerName );

	if ( ! add_env_to_args_for_docker(runArgs, env)) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to pass enviroment to docker.\n" );
		return -8;
	}

	// Map the external sanbox to the internal sandbox.
	runArgs.AppendArg( "--volume" );
	runArgs.AppendArg( sandboxPath + ":" + sandboxPath );

	// Start in the sandbox.
	runArgs.AppendArg( "--workdir" );
	runArgs.AppendArg( sandboxPath );

	// Run the command with its arguments in the image.
	runArgs.AppendArg( imageID );
	runArgs.AppendArg( command );
	runArgs.AppendArgsFromArgList( args );

	MyString displayString;
	runArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	//
	// If we run Docker attached, we avoid a race condition where
	// 'docker logs --follow' returns before 'docker rm' knows that the
	// container is gone (and refuses to remove it).  Of course, we
	// can't block, so we have a proxy process run attached for us.
	//
	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );
	int childPID = daemonCore->Create_Process( docker.c_str(), runArgs,
		PRIV_UNKNOWN, 1, FALSE, FALSE, NULL, sandboxPath.c_str(),
		& fi, NULL, childFDs );

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS | D_FAILURE, "Create_Process() failed.\n" );
		return -1;
	}
	pid = childPID;

	//
	// We'll need the container ID to slurp usage information.
	//
	int cidFD = -1;
	for( int i = 0; i < 3 && cidFD < 0; ++i ) {
		sleep( i );
		cidFD = safe_open_wrapper_follow( cidFileName.c_str(), O_RDONLY, 0600 );
	}

	if( cidFD < 0 ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to open container ID file '%s'.\n", cidFileName.c_str() );
		return -7;
	}

	char buffer[1024];
	int size = read( cidFD, buffer, 1024 );
	close( cidFD );

	if( size == -1 ) {
		dprintf( D_ALWAYS, D_FAILURE, "Failed to read from container ID file '%s'.\n", cidFileName.c_str() );
		return -8;
	}

	buffer[ (1024 - 1) < size ? 1024 - 1 : size] = '\0';
	containerID = buffer;
	return 0;
}

int DockerAPI::rm( const std::string & containerID, CondorError & /* err */ ) {
	std::string docker;
	if( ! param( docker, "DOCKER" ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "DOCKER is undefined.\n" );
		return -1;
	}

	ArgList rmArgs;
	rmArgs.AppendArg( docker );
	rmArgs.AppendArg( "rm" );
	rmArgs.AppendArg( containerID.c_str() );

	MyString displayString;
	rmArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	// Read from Docker's combined output and error streams.
	FILE * dockerResults = my_popen( rmArgs, "r", 1 );
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
	return 0;
}

int DockerAPI::detect( CondorError & /* err */ ) {
	std::string docker;
	if( ! param( docker, "DOCKER" ) ) {
		dprintf( D_FULLDEBUG, "DOCKER is undefined.\n" );
		return -1;
	}

	ArgList infoArgs;
	infoArgs.AppendArg( docker );
	infoArgs.AppendArg( "info" );

	MyString displayString;
	infoArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

	FILE * dockerResults = my_popen( infoArgs, "r", 1 );
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	// Even if we don't care about the success output, the failure output
	// can be handy for debugging...
	char buffer[1024];
	std::vector< std::string > output;
	while( fgets( buffer, 1024, dockerResults ) != NULL ) {
		unsigned end = strlen(buffer) - 1;
		if( buffer[end] == '\n' ) { buffer[end] = '\0'; }
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

	return 0;
}

//
// FIXME: We have a lot of boilerplate code in this function and file.
//
int DockerAPI::version( std::string & version, CondorError & /* err */ ) {
	std::string docker;
	if( ! param( docker, "DOCKER" ) ) {
		dprintf( D_FULLDEBUG, "DOCKER is undefined.\n" );
		return -1;
	}

	ArgList versionArgs;
	versionArgs.AppendArg( docker );
	versionArgs.AppendArg( "-v" );

	MyString displayString;
	versionArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

	FILE * dockerResults = my_popen( versionArgs, "r", 1 );
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

	int exitCode = my_pclose( dockerResults );
	if( exitCode != 0 ) {
		dprintf( D_ALWAYS, "'%s' did not exit successfully (code %d); the first line of output was '%s'.\n", displayString.c_str(), exitCode, buffer );
		return -4;
	}

	unsigned end = strlen(buffer) - 1;
	if( buffer[end] == '\n' ) { buffer[end] = '\0'; }
	version = buffer;

	return 0;
}

int DockerAPI::inspect( const std::string & containerID, ClassAd * dockerAd, CondorError & /* err */ ) {
	std::string docker;
	if( ! param( docker, "DOCKER" ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "DOCKER is undefined.\n" );
		return -1;
	}

	if( dockerAd == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "dockerAd is NULL.\n" );
		return -2;
	}

	ArgList inspectArgs;
	inspectArgs.AppendArg( docker );
	inspectArgs.AppendArg( "inspect" );
	inspectArgs.AppendArg( "--format" );
	StringList formatElements(	"ContainerId=\"{{.Id}}\" "
								"Pid={{.State.Pid}} "
								"Name=\"{{.Name}}\" "
								"ExitCode={{.State.ExitCode}} "
								"StartedAt=\"{{.State.StartedAt}}\" "
								"FinishedAt=\"{{.State.FinishedAt}}\" " );
	char * formatArg = formatElements.print_to_delimed_string( "\n" );
	inspectArgs.AppendArg( formatArg );
	free( formatArg );
	inspectArgs.AppendArg( containerID );

	MyString displayString;
	inspectArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	FILE * dockerResults = my_popen( inspectArgs, "r", 1 );
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
		}
	}
	my_pclose( dockerResults );

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
			dprintf( D_ALWAYS | D_FAILURE, "%s", correctOutput[i].c_str() );
		}
		return -4;
	}

	dprintf( D_FULLDEBUG, "docker inspect printed:\n" );
	for( int i = 0; i < formatElements.number() && ! correctOutput[i].empty(); ++i ) {
		dprintf( D_FULLDEBUG, "\t%s", correctOutput[i].c_str() );
	}

	return 0;
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

