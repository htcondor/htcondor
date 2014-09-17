
#ifndef __MODULE_LOCK_H_
#define __MODULE_LOCK_H_

#include <pthread.h>

namespace condor {

class ModuleLock {

public:
    ModuleLock();
    ~ModuleLock();

    void acquire();
    void release();

private:

    bool m_release_gil;
    bool m_owned;
    static pthread_mutex_t m_mutex;
    PyThreadState *m_save;
};

}

#endif
