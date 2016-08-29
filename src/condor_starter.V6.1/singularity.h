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

namespace htcondor {

class Singularity {
public:
  static bool enabled();
  static bool version(std::string &);

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
			const std::string &execute_dir
			);

private:
  static bool detect(CondorError &err);

  static bool m_enabled;
  static bool m_probed;
  static std::string m_singularity_version;
};

}

#endif  // _CONDOR_SINGULARITY_H
