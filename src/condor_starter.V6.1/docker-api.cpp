#include "condor_common.h"

#include "env.h"
#include "my_popen.h"
#include "CondorError.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_daemon_core.h"
#include "file_lock.h"

#include "docker-api.h"
#include <algorithm>

static bool add_env_to_args_for_docker(ArgList &runArgs, const Env &env);
static bool add_docker_arg(ArgList &runArgs);
static int run_simple_docker_command(	const std::string &command,
					const std::string &container,
					CondorError &e, bool ignore_output=false);
static int gc_image(const std::string &image);

//
// Because we fork before calling docker, we don't actually
// care if the image is stored locally or not (except to the extent that
// remote image pull violates the principle of least astonishment).
//
int DockerAPI::run(
	const ClassAd &machineAd,
	const std::string & containerName,
	const std::string & imageID,
	const std::string & command,
	const ArgList & args,
	const Env & env,
	const std::string & sandboxPath,
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
	runArgs.AppendArg( "run" );

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
		cpuShare = 10 * cpus;
	} else {
		cpuShare = 10;
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

	// Start in the sandbox.
	runArgs.AppendArg( "--workdir" );
	runArgs.AppendArg( sandboxPath );

	// Run with the uid that condor selects for the user
	// either a slot user or submitting user or nobody
	uid_t uid = 0;

	// Docker doesn't actually run on Windows, but we compile
	// on Windows because...
#ifndef WIN32
	uid = get_user_uid();
#endif
	
	if (uid == 0) {
		dprintf(D_ALWAYS|D_FAILURE, "Failed to get userid to run docker job\n");
		return -9;
	}
	runArgs.AppendArg("--user");
	runArgs.AppendArg(uid);

	// Run the command with its arguments in the image.
	runArgs.AppendArg( imageID );

	
	// If no command given, the default command in the image will run
	if (command.length() > 0) {
		runArgs.AppendArg( command );
	}

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

int DockerAPI::rm( const std::string & containerID, CondorError & /* err */ ) {

	ArgList rmArgs;
	if ( ! add_docker_arg(rmArgs))
		return -1;
	rmArgs.AppendArg( "rm" );
	rmArgs.AppendArg( "-v" );  // also remove the volume
	rmArgs.AppendArg( containerID.c_str() );

	MyString displayString;
	rmArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

	// Read from Docker's combined output and error streams.
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
	return 0;
}

int
DockerAPI::rmi(const std::string &image, CondorError &err) {
		// First, try to remove the named image
	run_simple_docker_command("rmi", image, err, true);
		
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

	FILE * dockerResults = my_popen( args, "r", 1 , 0, false);
	if( dockerResults == NULL ) {
		dprintf( D_ALWAYS | D_FAILURE, "Failed to run '%s'.\n", displayString.c_str() );
		return -2;
	}

	char buffer[1024];
	std::vector< std::string > output;
	while( fgets( buffer, 1024, dockerResults ) != NULL ) {
		unsigned end = strlen(buffer) - 1;
		if( buffer[end] == '\n' ) { buffer[end] = '\0'; }
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
}


int
DockerAPI::kill(const std::string &image, CondorError &err) {
	return run_simple_docker_command("kill", image, err);
}


int 
DockerAPI::pause( const std::string & container, CondorError & err ) {
	return run_simple_docker_command("pause", container, err);
}

int 
DockerAPI::unpause( const std::string & container, CondorError & err ) {
	return run_simple_docker_command("unpause", container, err);
}

int DockerAPI::detect( CondorError & err ) {
	// FIXME: Remove ::version() as a public API and return it from here,
	// because there's no point in doing this twice.
	std::string version;
	if( DockerAPI::version( version, err ) != 0 ) {
		dprintf( D_ALWAYS | D_FAILURE, "DockerAPI::detect() failed to detect the Docker version; assuming absent.\n" );
		return -4;
	}

	ArgList infoArgs;
	if ( ! add_docker_arg(infoArgs))
		return -1;
	infoArgs.AppendArg( "info" );

	MyString displayString;
	infoArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

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

	ArgList versionArgs;
	if ( ! add_docker_arg(versionArgs))
		return -1;
	versionArgs.AppendArg( "-v" );

	MyString displayString;
	versionArgs.GetArgsStringForLogging( & displayString );
	dprintf( D_FULLDEBUG, "Attempting to run: '%s'.\n", displayString.c_str() );

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

	unsigned end = strlen(buffer) - 1;
	if( buffer[end] == '\n' ) { buffer[end] = '\0'; }
	version = buffer;

	return 0;
}

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
run_simple_docker_command(const std::string &command, const std::string &container, CondorError &, bool ignore_output)
{
  ArgList args;
  if ( ! add_docker_arg(args))
    return -1;
  args.AppendArg( command );
  args.AppendArg( container.c_str() );

  MyString displayString;
  args.GetArgsStringForLogging( & displayString );
  dprintf( D_FULLDEBUG, "Attempting to run: %s\n", displayString.c_str() );

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

  int length = strlen( buffer );
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

  FileLock lock(imageFilename.c_str());
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
   int remove_count = images.size() - cache_size;
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

  return 0;
}
