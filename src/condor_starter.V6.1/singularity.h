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

  static bool advertise(classad::ClassAd &daemonAd);

  enum result {
    SUCCESS,   // Singularity job has been setup.
    DISABLE,   // Singularity job was not requested.
    FAILURE,   // Singularity job was requested but setup failed.
  };

  static bool job_enabled(classad::ClassAd &machineAd,
                        classad::ClassAd &jobAd);

  static result setup(classad::ClassAd &machineAd,
			classad::ClassAd &jobAd,
			std::string &exec,
			ArgList &args,
			const std::string &job_iwd,
			const std::string &execute_dir,
			Env &env
			);

	// To pass an environment variable FOO from host to container
	// it must be named SINGULARITYENV_FOO.  This method does that.
  static bool convertEnv(Env *job_env);

  static bool hasTargetDir(const classad::ClassAd &jobAd, std::string &target_dir);

	// if SINGULARITY_TARGET_DIR is set, reset environment variables
	// for the scratch directory path as mounted inside the container
  static bool retargetEnvs(Env &job_env, const std::string &targetdir, const std::string &execute_dir);
  static bool runTest(const std::string &JobName, const ArgList &args, int orig_args_len, const Env &env, std::string &errorMessage);

  static bool canRunSandbox();
  static bool canRunSIF();
  static bool canRun(const std::string &image);


private:
  static bool detect(CondorError &err);
  static std::string environmentPrefix();

  static bool m_enabled;
  static bool m_probed;
  static bool m_apptainer;
  static int m_default_timeout;
  static std::string m_singularity_version;
};

}

#endif  // _CONDOR_SINGULARITY_H
