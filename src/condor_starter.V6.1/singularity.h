#ifndef _CONDOR_SINGULARITY_H
#define _CONDOR_SINGULARITY_H

#include <string>
#include "condor_uid.h"

// Fwd dec'ls
class CondorError;
namespace classad {
	class ClassAd;
}
class ArgList;
class Env;

namespace htcondor {

class Singularity {
public:
  static bool enabled();
  static const char *version();

  enum result {
    SUCCESS,   // Singularity job has been setup.
    DISABLE,   // Singularity job was not requested.
    FAILURE,   // Singularity job was requested but setup failed.
  };

  static bool job_enabled(classad::ClassAd &machineAd,
                        classad::ClassAd &jobAd);

  enum UseLauncher {
	  USE_LAUNCHER,
	  NO_LAUNCHER
  };

  // Is singularity/apptainer setuid, using user namespace, or we don't know
  enum IsSetuid {
	  SingSetuid,
	  SingUserNamespaces,
	  SingSetuidUnknown
  };

  static result setup(classad::ClassAd &machineAd,
			classad::ClassAd &jobAd,
			std::string &exec,
			ArgList &args,
			const std::string &job_iwd,
			const std::string &execute_dir,
			Env &env,
			UseLauncher launcher
			);

	// To pass an environment variable FOO from host to container
	// it must be named SINGULARITYENV_FOO.  This method does that.
  static bool convertEnv(Env *job_env);

  static bool hasTargetDir(const classad::ClassAd &jobAd, std::string &target_dir);

	// if SINGULARITY_TARGET_DIR is set, reset environment variables
	// for the scratch directory path as mounted inside the container
  static bool retargetEnvs(Env &job_env, const std::string &targetdir, const std::string &execute_dir);
  static bool runTest(const std::string &JobName, const ArgList &args, int orig_args_len, Env &env, std::string &errorMessage);

  static bool canRunSandbox(bool &can_use_pidnamespaces);
  static bool canRunSIF();
  static bool canRun(const std::string &image, const std::string &command, std::string &firstLine, int timeout = m_default_timeout);
  static IsSetuid usesUserNamespaces();
  static std::string m_lastSingularityErrorLine;

private:
  static bool detect(CondorError &err);
  static std::string environmentPrefix();
  static void add_containment_args(ArgList & sing_args);

  static bool m_enabled;
  static bool m_probed;
  static bool m_apptainer;
  static int m_default_timeout;
  static std::string m_singularity_version;
  static bool m_use_pid_namespaces;
};

}

#endif  // _CONDOR_SINGULARITY_H
