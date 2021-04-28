#ifndef _CONDOR_DOCKER_API_H
#define _CONDOR_DOCKER_API_H

#include <string>
#include "condor_arglist.h"


class DockerAPI {
	public:
		static int default_timeout;
		static const int docker_hung = -9; // this error code is returned when we timed out a command to docker.

		static int pruneContainers();

		/**
		 * Create a new container, identified by imageID but don't
		 * start it.  The container
		 * will be named name.  The command will run in the given
		 * environment with the given arguments.  If directory is non-empty,
		 * it will be mapped into the container and the command run there
		 * [TODO].
		 *
		 * If run() succeeds, the pid will be that of a process which will
		 * terminate when the instance does.  The error will be unchanged.
		 *
		 * If run() fails, it will return a negative number [TODO: and set
		 * error to ....]
		 *
		 * @param name 			If empty, Docker will generate a random name.  [FIXME]
		 * @param imageID		For now, must be the GUID.
		 * @param command		...
		 * @param arguments		...
		 * @param environment	...
		 * @param directory		...
		 * @param pid			On success, will be set to the PID of a process which will terminate when the container does.  Otherwise, unchanged.
		 * @param childFDs		The redirected std[in|out|err] FDs.
		 * @param askForPorts	If you should ask for the redirected ports.
		 * @param error			On success, unchanged.  Otherwise, [TODO].
		 * @return 				0 on success, negative otherwise.
		 */
		static int createContainer(	ClassAd &machineAd,
						ClassAd &jobAd,
						const std::string & name,
						const std::string & imageID,
						const std::string & command,
						const ArgList & arguments,
						const Env & environment,
						const std::string & outside_directory,
						const std::string & inside_directory,
						const std::list<std::string> extraVolumes,
						int & pid,
						int * childFDs,
						bool & shouldAskForPorts,
						CondorError & error,
						int * affinity_mask = NULL);

		static int startContainer(const std::string &name,
						int &pid,
						int *childFDs,
						CondorError &error);

		static int execInContainer( const std::string &containerName,
					    const std::string &command,
					    const ArgList &arguments,
					    const Env &environment,
					    int *childFDs,
					    int reaperid,
					    int &pid);

		/**
		 * copy files/folders from srcPath to the given path in the container
		 *   invokes
		 *      docker cp SRC_PATH CONTAINER:CONTAINER_PATH
		 */
		static int copyToContainer(const std::string & srcPath, // path on local file system to copy file/folder from
						const std::string &container,       // container to copy into
						const std::string & containerPath,  // destination path in container
						StringList * options);
		/**
		 * copy files/folders from given path in the container to destPath
		 *   invokes
		 *      docker cp CONTAINER:CONTAINER_PATH DEST_PATH
		 */
		static int copyFromContainer(const std::string &container, // container to copy into
						const std::string & containerPath,             // source file or folder in container
						const std::string & destPath,                 // destination path on local file system
						StringList * options);

		/**
		 * Releases the disk space (but not the image) associated with
		 * the given container.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int rm( const std::string & container, CondorError & err );

		/**
		 * Releases the named image
		 *
		 * @param image		The Docker image name or GUUID.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int rmi( const std::string & image, CondorError & err );

		/**
		 * Sends a signal to the first process in the named container
		 *
		 * @param image		The Docker image name or GUUID.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */

		static int kill( const std::string & image, CondorError & err );

		/**
		 * Sends the given signal to the specified container's primary process.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param signal		The signal to send.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int kill( const std::string & container, int signal, CondorError & err );

		// Only available in Docker 1.1 or later.
		static int pause( const std::string & container, CondorError & err );

		// Only available in Docker 1.1 or later.
		static int unpause( const std::string & container, CondorError & err );

		static int stats(const std::string &container, uint64_t &memUsage, uint64_t &netIn, uint64_t &netOut, uint64_t &userCpu, uint64_t &sysCpu);

		/**
		 * Obtains the docker-inspect values State.Running and State.ExitCode.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param isRunning		On success, will be set to State.Running.  Otherwise, unchanged.
		 * @param exitCode		On success, will be set to State.ExitCode.  Otherwise, unchanged.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int getStatus( const std::string & container, bool isRunning, int & result, CondorError & err );

		static int getServicePorts( const std::string & container,
			const ClassAd & jobAd, ClassAd & serviceAd );

		/**
		 * Attempts to detect the presence of a working Docker installation.
		 *
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int detect( CondorError & err );

		/*
		 *  Load a pre-defined docker test image and exec it to make sure that docker fully works
		 */
		static int testImageRuns( CondorError &err );
		/**
		 * Obtains the configured DOCKER's version string.
		 *
		 * @param version		On success, will be set to the version string.  Otherwise, unchanged.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int version( std::string & version, CondorError & err );
		static int majorVersion;
		static int minorVersion;

		/**
		 * Returns a ClassAd corresponding to a subset of the output of
		 * 'docker inspect'.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param inspectionAd	Populated on success, unchanged otherwise.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int inspect( const std::string & container, ClassAd * inspectionAd, CondorError & err );
};

#endif /* _CONDOR_DOCKER_API_H */
