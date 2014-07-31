#ifndef _CONDOR_DOCKER_API_H
#define _CONDOR_DOCKER_API_H

#include <string>
#include "condor_arglist.h"


class DockerAPI {
	public:

		/**
		 * Runs command in the Docker identified by imageID.  The container
		 * will be named name.  The command will run in the given
		 * environment with the given arguments.  If directory is non-empty,
		 * it will be mapped into the container and the command run there
		 * [TODO].
		 *
		 * If run() succeeds, the containerID will be set to the Docker GUID,
		 * and the pid set to a process which will terminate when the instance
		 * does.  The error will be unchanged.
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
		 * @param containerID	...
		 * @param pid			...
		 * @param error			...
		 * @return 				0 on success, negative otherwise.
		 * @see
		 */
		static int run(	const std::string & name,
						const std::string & imageID,
						const std::string & command,
						const ArgList & arguments,
						const Env & environment,
						const std::string & directory,
						std::string & containerID,
						int & pid,
						CondorError & error );

		static int stop( const std::string & containerID, CondorError & err );

		static int suspend( const std::string & containerID, CondorError & err );

		static int getStatus( const std::string & containerID, bool isRunning, int & result, CondorError & err );
};

#endif /* _CONDOR_DOCKER_API_H */
