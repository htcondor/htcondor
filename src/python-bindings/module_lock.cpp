
// Note - python_bindings_common.h must be included first so it can manage macro definition conflicts
// between python and condor.
#include "python_bindings_common.h"
#include <Python.h>

// Note - condor_secman.h can't be included directly.  The following headers must
// be loaded first.  Sigh.
#include "condor_ipverify.h"
#include "sock.h"
#include "condor_secman.h"

#include "classad_wrapper.h"
#include "module_lock.h"
#include "secman.h" // python bindings secman wrapper.

#include "condor_config.h" // so we can do param mutation (ick)


void ConfigOverrides::reset()
{
    for (CONFIG_OVERRIDE_MAP::iterator it = over.begin(); it != over.end(); /*advance in loop*/) {
        CONFIG_OVERRIDE_MAP::iterator jt = it++;
        if (auto_free && jt->second) free(const_cast<char*>(jt->second));
        over.erase(jt);
    }
}

// set a value, makes a copy of the input value if auto_free is true
// returns the old value if auto_free is false
const char * ConfigOverrides::set(const std::string & key, const char * value)
{
    const char * oldval = NULL;
    if (auto_free) { value = strdup(value); }
    CONFIG_OVERRIDE_MAP::iterator found = over.find(key);
    if (found != over.end()) { oldval = found->second; }
    over[key] = value;
    if (auto_free && oldval) { free(const_cast<char*>(oldval)); oldval = NULL; }
    return oldval;
}

void ConfigOverrides::apply(ConfigOverrides * old)
{
    if (old) { ASSERT(!old->auto_free); old->reset(); }
    for (CONFIG_OVERRIDE_MAP::iterator it = over.begin(); it != over.end(); ++it) {
        const char * p = set_live_param_value(it->first.c_str(), it->second);
        if (old) { old->set(it->first.c_str(), p); }
    }
}


using namespace condor;

MODULE_LOCK_MUTEX_TYPE ModuleLock::m_mutex = MODULE_LOCK_MUTEX_INITIALIZER;
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
      m_owned(false), m_restore_orig_proxy(false), m_save(0)
    , m_config_orig(false) // backup config doesn't own it's pointers.
    , m_proxy_orig(NULL)
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
    m_config_orig.reset();
    SecManWrapper::applyThreadLocalConfigOverrides(m_config_orig);

    const char * p = SecManWrapper::getThreadLocalTag();
    m_restore_orig_tag = p!=NULL;
    if (m_restore_orig_tag) {
        m_tag_orig = SecMan::getTag();
        SecMan::setTag(p);
    }

    p = SecManWrapper::getThreadLocalPoolPassword();
    m_restore_orig_password = p!=NULL;
    if (m_restore_orig_password) {
        m_password_orig = SecMan::getPoolPassword();
        SecMan::setPoolPassword(p);
    }

    p = SecManWrapper::getThreadLocalToken();
    m_restore_orig_token = p;
    if (m_restore_orig_token) {
        m_token_orig = SecMan::getToken();
        SecMan::setToken(p);
    }

    p = SecManWrapper::getThreadLocalGSICred();
    m_restore_orig_proxy = p!=NULL;
    if (m_restore_orig_proxy) {
        m_proxy_orig = getenv("X509_USER_PROXY");
        if (m_proxy_orig) { m_proxy_orig = strdup(m_proxy_orig); }
        #ifdef WIN32
        SetEnvironmentVariable("X509_USER_PROXY", p);
        #else
        setenv("X509_USER_PROXY", p, 1);
        #endif
    }
}

ModuleLock::~ModuleLock()
{
    release();
}

void ModuleLock::useFamilySession(const std::string & sess)
{
	if (! sess.empty()) {
		SecManWrapper::setFamilySession(sess);
	}
}

void
ModuleLock::release()
{
    if (m_restore_orig_proxy) {
        #ifdef WIN32
        // setting NULL deletes the environment variable
        SetEnvironmentVariable("X509_USER_PROXY", m_proxy_orig);
        #else
        if (m_proxy_orig) {
            setenv("X509_USER_PROXY", m_proxy_orig, 1);
        } else {
            unsetenv("X509_USER_PROXY");
        }
        #endif
    }
    m_restore_orig_proxy = false;
    if (m_proxy_orig) free(m_proxy_orig);
    m_proxy_orig = NULL;

    if (m_restore_orig_password) {
        SecMan::setPoolPassword(m_password_orig);
    }
    m_restore_orig_password = false;
    m_password_orig = "";

    if (m_restore_orig_token) {
        SecMan::setToken(m_token_orig);
    }
    m_restore_orig_token = false;
    m_token_orig = "";

    if (m_restore_orig_tag) {
        SecMan::setTag(m_tag_orig);
    }
    m_restore_orig_tag = false;
    m_tag_orig = "";

    // put original config values back
    m_config_orig.apply(NULL);
    m_config_orig.reset();

    if (m_release_gil && m_owned)
    {
        m_owned = false;
        MODULE_LOCK_MUTEX_UNLOCK(&m_mutex);
        PyEval_RestoreThread(m_save);
    }
}

