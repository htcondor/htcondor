#ifndef __SECMAN_H_
#define __SECMAN_H_

// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"
#include "condor_common.h"

// Note - condor_secman.h can't be included directly.  The following headers must
// be loaded first.  Sigh.
#include "condor_ipverify.h"
#include "sock.h"
#include "condor_secman.h"

#include "classad_wrapper.h"

struct SecManWrapper
{
public:
    SecManWrapper();

    void invalidateAllCache();
    boost::shared_ptr<ClassAdWrapper> ping(boost::python::object locate_obj, boost::python::object command_obj=boost::python::object("DC_NOP"));

    std::string getCommandString(int cmd);

    static boost::shared_ptr<SecManWrapper> enter(boost::shared_ptr<SecManWrapper> obj);
    bool exit(boost::python::object, boost::python::object, boost::python::object);

    static std::string getThreadLocalTag();
    static std::string getThreadLocalPoolPassword();
    static std::string getThreadLocalGSICred();
    static boost::shared_ptr<std::vector<std::pair<std::string, std::string> > > getThreadLocalConfigOverrides();

    void setTag(const std::string &tag);
    void setPoolPassword(const std::string &pool_pass);
    void setGSICredential(const std::string &cred);
    void setConfig(const std::string &key, const std::string &val);

private:
    SecMan m_secman;
    static pthread_key_t m_key;
    std::string m_tag;
    std::string m_pool_pass;
    std::string m_cred;
    boost::shared_ptr<std::vector<std::pair<std::string, std::string> > > m_config_overrides;
};

#endif  // __SECMAN_H_
