#ifndef _CONDOR_DOCKER_API_H
#define _CONDOR_DOCKER_API_H

#include <string>
#include "condor_arglist.h"


class DockerAPI {
	public:
		// For now, imageID is a GUID.
		static int run( const std::string & dockerName,
					const std::string & imageID,
					const std::string & command, const ArgList & args,
					const Env & e, const std::string & sandboxPath,
					std::string & containerID, int & pid, CondorError & err );

		static int stop( const std::string & containerID, CondorError & err );

		static int suspend( const std::string & containerID, CondorError & err );

		static int getStatus( const std::string & containerID, bool isRunning, int & result, CondorError & err );
};

#endif /* _CONDOR_DOCKER_API_H */
