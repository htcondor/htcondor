
// Note - python_bindings_common.h must be included first so it can manage macro definition conflicts
// between python and condor.
#include "python_bindings_common.h"
#include <Python.h>

#include "module_lock.h"
#include "secman.h"

#include "condor_config.h"

#include "classad/classad.h"

using namespace condor;

MODULE_LOCK_MUTEX_TYPE ModuleLock::m_mutex = MODULE_LOCK_MUTEX_INITAILIZER;
#ifdef WIN32
bool ModuleLock::m_mutex_intialized = false;
#endif

#ifdef WIN32
// define and declare a singlton global object so that the constructor will
// call initialize the module lock critical section at load time.
class ModuleLockInitializer {
public:
	ModuleLockInitializer() {
		ModuleLock::initialize();
	}
} g_ModuleLockInitializerSingleton;
#endif


ModuleLock::ModuleLock()
    : m_release_gil(ModuleLock::is_intialized() && !classad::ClassAdGetExpressionCaching()),
      m_owned(false), m_save(0)
{
    acquire();
}

void
ModuleLock::initialize()
{
#ifdef WIN32
    if (ModuleLock::m_mutex_intialized) return;
    #pragma warning(suppress: 28125) // function must be called from within a try/except block... (except that's really optional)
#endif
    MODULE_LOCK_MUTEX_INITIALIZE(&m_mutex);
#ifdef WIN32
    ModuleLock::m_mutex_intialized = true;
#endif
}

void
ModuleLock::acquire()
{
    if (m_release_gil && !m_owned)
    {
        m_save = PyEval_SaveThread();
        MODULE_LOCK_MUTEX_LOCK(&m_mutex);
        m_owned = true;
    }
    // Apply "thread-local" settings - not actually thread-local, but we change them while we
    // have an exclusive lock on HTCondor routines.
    boost::shared_ptr<std::vector<std::pair<std::string,std::string> > > temp_param = SecManWrapper::getThreadLocalConfigOverrides();
    if (temp_param.get())
    {
        m_config_orig.clear();
        m_config_orig.reserve(temp_param->size());
        std::vector<std::pair<std::string,std::string> >::const_iterator iter = temp_param->begin();
        for (; iter != temp_param->end(); iter++)
        {
            MyString name_used;
            const char *pdef_value;
            const MACRO_META *pmeta;
            const char * result_str = param_get_info(iter->first.c_str(), NULL, NULL, name_used, &pdef_value, &pmeta);
            m_config_orig.push_back(std::make_pair(iter->first, result_str ? result_str : ""));
            param_insert(iter->first.c_str(), iter->second.c_str());
        }
    }

    m_tag_orig = SecMan::getTag();
    SecMan::setTag(SecManWrapper::getThreadLocalTag());

    m_password_orig = SecMan::getPoolPassword();
    SecMan::setPoolPassword(SecManWrapper::getThreadLocalPoolPassword());

    m_proxy_orig = getenv("X509_USER_PROXY");
    setenv("X509_USER_PROXY", SecManWrapper::getThreadLocalGSICred().c_str(), 1);
}

ModuleLock::~ModuleLock()
{
    release();
}

void
ModuleLock::release()
{
    setenv("X509_USER_PROXY", m_proxy_orig, 1);
    m_proxy_orig = NULL;

    SecMan::setPoolPassword(m_password_orig);
    m_password_orig = "";

    SecMan::setTag(m_tag_orig);
    m_tag_orig = "";

    std::vector<std::pair<std::string,std::string> >::const_iterator it = m_config_orig.begin();
    for (; it != m_config_orig.end(); it++)
    {
        param_insert(it->first.c_str(), it->second.c_str());
    }
    m_config_orig.clear();
    if (m_release_gil && m_owned)
    {
        MODULE_LOCK_MUTEX_UNLOCK(&m_mutex);
        PyEval_RestoreThread(m_save);
        m_owned = false;
    }
}

