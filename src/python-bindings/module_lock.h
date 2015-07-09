
#ifndef __MODULE_LOCK_H_
#define __MODULE_LOCK_H_
#ifdef WIN32
  #include <windows.h> // for CRITICAL_SECTION
  #define MODULE_LOCK_MUTEX_TYPE CRITICAL_SECTION
  #define MODULE_LOCK_MUTEX_INITAILIZER {0}
  #define MODULE_LOCK_MUTEX_LOCK EnterCriticalSection
  #define MODULE_LOCK_MUTEX_UNLOCK LeaveCriticalSection
  #define MODULE_LOCK_MUTEX_INITIALIZE(mtx) InitializeCriticalSection(mtx)
#else
  #include <pthread.h>
  #define MODULE_LOCK_MUTEX_TYPE pthread_mutex_t
  #define MODULE_LOCK_MUTEX_INITAILIZER PTHREAD_MUTEX_INITIALIZER
  #define MODULE_LOCK_MUTEX_LOCK pthread_mutex_lock
  #define MODULE_LOCK_MUTEX_UNLOCK pthread_mutex_unlock
  #define MODULE_LOCK_MUTEX_INITIALIZE(mtx) (void)0
#endif

namespace condor {

class ModuleLock {

public:
    ModuleLock();
    ~ModuleLock();

    void acquire();
    void release();
    static void initialize();


private:

    bool m_release_gil;
    bool m_owned;
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
};

}

#endif // __MODULE_LOCK_H_
