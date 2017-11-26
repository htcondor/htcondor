#ifndef _CONDOR_CHARLIECLOUD_H
#define _CONDOR_CHARLIECLOUD_H

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

class Charliecloud {
public:
  static bool enabled();
  static const char *version();

  static bool advertise(classad::ClassAd &daemonAd);

  enum result {
    SUCCESS,   // Charliecloud job has been setup.
    DISABLE,   // Charliecloud job was not requested.
    FAILURE,   // Charliecloud job was requested but setup failed.
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

private:
  static bool detect(CondorError &err);

  static bool m_enabled;
  static bool m_probed;
  static int m_default_timeout;
  static MyString m_charliecloud_version;
};

}

#endif  // _CONDOR_CHARLIECLOUD_H
