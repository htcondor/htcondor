#ifndef _CONDOR_SINGULARITY_H
#define _CONDOR_SINGULARITY_H

#include <string>

// Fwd dec'ls
class CondorError;
namespace compat_classad {
	class ClassAd;
}
namespace classad {
	class ClassAd;
}
class ArgList;
class MyString;
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

  static bool job_enabled(compat_classad::ClassAd &machineAd,
                        compat_classad::ClassAd &jobAd);

  static result setup(compat_classad::ClassAd &machineAd,
			compat_classad::ClassAd &jobAd,
			MyString &exec,
			ArgList &args,
			const std::string &job_iwd,
			const std::string &execute_dir,
			Env &env
			);

	// To pass an environment variable FOO from host to container
	// it must be named SINGULARITYENV_FOO.  This method does that.
  static bool convertEnv(Env *job_env);

	// if SINGULARITY_TARGET_DIR is set, reset environment variables
	// for the scratch directory path as mounted inside the container
  static bool retargetEnvs(Env &job_env, const std::string &targetdir, const std::string &execute_dir);


private:
  static bool detect(CondorError &err);

  static bool m_enabled;
  static bool m_probed;
  static int m_default_timeout;
  static MyString m_singularity_version;
};

}

#endif  // _CONDOR_SINGULARITY_H
