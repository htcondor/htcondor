
#ifndef __MODULE_LOCK_H_
#define __MODULE_LOCK_H_

#ifdef WIN32
  #include <windows.h> // for CRITICAL_SECTION
  #define MODULE_LOCK_MUTEX_TYPE CRITICAL_SECTION
  #define MODULE_LOCK_MUTEX_INITIALIZER {0}
  #define MODULE_LOCK_MUTEX_LOCK EnterCriticalSection
  #define MODULE_LOCK_MUTEX_UNLOCK LeaveCriticalSection
  #define MODULE_LOCK_MUTEX_INITIALIZE(mtx) InitializeCriticalSection(mtx)
  #define MODULE_LOCK_TLS_KEY unsigned int
  #define MODULE_LOCK_TLS_ALLOC(k) ((k = TlsAlloc()) == TLS_OUT_OF_INDEXES)
  #define MODULE_LOCK_TLS_FREE(k) TlsFree(k)
  #define MODULE_LOCK_TLS_GET(k) TlsGetValue(k)
  #define MODULE_LOCK_TLS_SET(k,p) TlsSetValue(k, p)
#else
  #include <pthread.h>
  #define MODULE_LOCK_MUTEX_TYPE pthread_mutex_t
  #define MODULE_LOCK_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
  #define MODULE_LOCK_MUTEX_LOCK pthread_mutex_lock
  #define MODULE_LOCK_MUTEX_UNLOCK pthread_mutex_unlock
  #define MODULE_LOCK_MUTEX_INITIALIZE(mtx) (void)0
  #define MODULE_LOCK_TLS_KEY pthread_key_t
  #define MODULE_LOCK_TLS_ALLOC(k) pthread_key_create(&k, NULL)
  #define MODULE_LOCK_TLS_FREE(k) pthread_key_delete(k)
  #define MODULE_LOCK_TLS_GET(k) pthread_getspecific(k)
  #define MODULE_LOCK_TLS_SET(k,p) pthread_setspecific(k,p)
#endif

// This class is used to hold param overrides and also to hold the
// previous values when an override is applied. 
typedef std::map<std::string, const char *, classad::CaseIgnLTStr> CONFIG_OVERRIDE_MAP;
class ConfigOverrides {
public:
	ConfigOverrides(bool owns_pointers) : auto_free(owns_pointers) {}
	~ConfigOverrides() { reset(); };
	void reset(); // remove all
	// set a value, makes a copy of value if auto_free is true, returns the old value if auto_free is false
	const char * set(const std::string & key, const char * value);
	void apply(ConfigOverrides * old);
private:
	CONFIG_OVERRIDE_MAP over;
	bool auto_free;
};

namespace condor {

class ModuleLock {

public:
    ModuleLock();
    ~ModuleLock();

    void acquire();
    void release();
    static void initialize();
    void useFamilySession(const std::string & sess);

private:

    bool m_release_gil;
    bool m_owned;
    bool m_restore_orig_proxy; // true when we have a m_proxy_orig to restore on release
    bool m_restore_orig_tag;   // true when we have a m_tag_orig to restore on release
    bool m_restore_orig_password; // true when we have a m_password_orig to restore on release
    bool m_restore_orig_token;
    static MODULE_LOCK_MUTEX_TYPE m_mutex;
#ifdef WIN32
    // We will initialize the mutex in a global constructor, but until it's initialized it can't be used
    // luckily, global constructors execute while the process is still single threaded, so it should
    // be ok to just skip the mutex locking until the mutex has been initialized.
    static bool m_mutex_intialized;
    static bool is_intialized() { return m_mutex_intialized; }
#else
    static bool is_intialized() { return true; }
#endif
    PyThreadState *m_save;

    // If there are any configuration overrides for this thread, this holds
    // the prior global values.
    ConfigOverrides m_config_orig;
    std::string m_tag_orig;
    std::string m_password_orig;
    char * m_proxy_orig;
    std::string m_token_orig;
};

}

#endif // __MODULE_LOCK_H_
