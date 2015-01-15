#include "condor_common.h"

#include "env.h"
#include "my_popen.h"
#include "CondorError.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_daemon_core.h"

#include "docker-api.h"

//
// Because this executes docker run in detached mode, we don't actually
// care if the image is stored locally or not (except to the extent that
// remote image pull violates the principle of least astonishment).
//
int DockerAPI::run(
	const std::string & dockerName,
	const std::string & imageID,
	const std::string & command,
	const ArgList & args,
	const Env & /* env */,
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
	runArgs.AppendArg( "--detach" );
	runArgs.AppendArg( "--name" );
	runArgs.AppendArg( dockerName );

	// TODO: Set up the environment (in the container).

	// Map the external sanbox to the internal sandbox.
	runArgs.AppendArg( "-v" );
	runArgs.AppendArg( sandboxPath + ":/tmp" );

	// Start in the sandbox.
	runArgs.AppendArg( "--workdir" );
	runArgs.AppendArg( "/tmp" );

	// Run the command with its arguments in the image.
	runArgs.AppendArg( imageID );
	runArgs.AppendArg( command );
	runArgs.AppendArgsFromArgList( args );

	MyString displayString;
	runArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	// Read from Docker's combined output and error streams.  On success,
	// it returns a 64-digit hex number.
	FILE * dockerResults = my_popen( runArgs, "r", 1 );
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -7;
	}

	char buffer[1024];
	if( NULL == fgets( buffer, 1024, dockerResults ) ) {
		if( errno ) {
			dprintf( D_ALWAYS | D_FAILURE, "Failed to read results from '%s': '%s' (%d)\n", displayString.c_str(), strerror( errno ), errno );
		} else {
			dprintf( D_ALWAYS | D_FAILURE, "'%s' returned nothing.\n", displayString.c_str() );
		}
		my_pclose( dockerResults );
		return -2;
	}

	char rawContainerID[65];
	if( 1 != sscanf( buffer, "%64[A-Fa-f0-9]\n", rawContainerID ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "Docker printed something that doesn't look like a container ID.\n" );
		dprintf( D_ALWAYS | D_FAILURE, "%s", buffer );
		while( fgets( buffer, 1024, dockerResults ) != NULL ) {
			dprintf( D_ALWAYS | D_FAILURE, "%s", buffer );
		}
		my_pclose( dockerResults );
		return -3;
	}

	dprintf( D_FULLDEBUG, "Found raw countainer ID '%s'\n", rawContainerID );
	containerID = rawContainerID;
	my_pclose( dockerResults );

	// Use containerID to find the PID.
	ArgList inspectArgs;
	inspectArgs.AppendArg( docker );
	inspectArgs.AppendArg( "inspect" );
	inspectArgs.AppendArg( "--format" );
	StringList formatElements(	"ContainerId=\"{{.Id}}\" "
								"Pid={{.State.Pid}} "
								"ExitCode={{.State.ExitCode}} "
								"StartedAt=\"{{.State.StartedAt}}\" "
								"FinishedAt=\"{{.State.FinishedAt}}\" " );
	char * formatArg = formatElements.print_to_delimed_string( "\n" );
	inspectArgs.AppendArg( formatArg );
	free( formatArg );
	inspectArgs.AppendArg( containerID );

	displayString = "";
	inspectArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	dockerResults = my_popen( inspectArgs, "r", 1 );
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Unable to run '%s'.\n", displayString.c_str() );
		return -6;
	}

	// If the output isn't exactly formatElements.number() lines long,
	// something has gone wrong and we'll at least be able to print out
	// the error message(s).
	std::string correctOutput[ formatElements.number() ];
	for( int i = 0; i < formatElements.number(); ++i ) {
		if( fgets( buffer, 1024, dockerResults ) != NULL ) {
			correctOutput[i] = buffer;
		}
	}
	my_pclose( dockerResults );

	int attrCount = 0;
	ClassAd dockerAd;
	for( int i = 0; i < formatElements.number(); ++i ) {
		if( correctOutput[i].empty() || dockerAd.Insert( correctOutput[i].c_str() ) == FALSE ) {
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

	// If the PID is 0, the job either terminated really quickly or
	// hasn't started yet.
	if( ! dockerAd.LookupInteger( "Pid", pid ) ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to find pid in Docker output.\n" );
		return -5;
	}

	//
	// We use 'docker logs --follow' both as a signal/semaphore process (like
	// 'docker wait', it will terminate iff the container does), and to make
	// sure we catch all of the output.
	//
	ArgList waitArgs;
	waitArgs.AppendArg( docker );
//	waitArgs.AppendArg( "wait" );
	waitArgs.AppendArg( "logs" );
	waitArgs.AppendArg( "--follow" );
	waitArgs.AppendArg( containerID );

	//
	// It may be wiser to fork and run in attached mode, playing the
	// appropriate games with the childFDs, and passing the child's PID
	// as the proxy process ID, especially given the --rm option.
	//
	// That should eliminate the need to sleep before calling DockerAPI::rm()
	// in DockerProc::JobExit().
	//

	FamilyInfo fi;
	fi.max_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", 15 );
	int childPID = daemonCore->Create_Process( docker.c_str(), waitArgs,
		PRIV_UNKNOWN, 1, FALSE, FALSE, NULL, sandboxPath.c_str(),
		& fi, NULL, childFDs );

	if( childPID == FALSE ) {
		dprintf( D_ALWAYS | D_FAILURE, "Create_Process() failed.\n" );
		return -1;
	}
	pid = childPID;

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


//
// Random notes on Docker.
//

// To see currently-installed images, run 'docker images'.

// To obtain the full container ID, pass 'docker ps' the
// --no-trunc[ate] option.

// To map an external path to an internal path, pass 'docker run'
// '-v /path/to/sandbox:/inner-path/to/sandbox'.

// sudo docker inspect --format 'ContainerId = {{.Id}} ; Pid = {{.State.Pid}} ; ExitCode = {{.State.ExitCode}}' <container-name>

// docker run --rm will autoremove the container if it exited sucessfuly,
// which might be a nice back-up for us.
