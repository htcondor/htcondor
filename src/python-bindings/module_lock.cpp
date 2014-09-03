
#include <Python.h>

#include "module_lock.h"

#include "classad/classad.h"

using namespace condor;

pthread_mutex_t ModuleLock::m_mutex = PTHREAD_MUTEX_INITIALIZER;

ModuleLock::ModuleLock()
    : m_release_gil(!classad::ClassAdGetExpressionCaching()),
      m_owned(false), m_save(0)
{
    acquire();
}

void
ModuleLock::acquire()
{
    if (m_release_gil && !m_owned)
    {
        m_save = PyEval_SaveThread();
        pthread_mutex_lock(&m_mutex);
        m_owned = true;
    }
}

ModuleLock::~ModuleLock()
{
    release();
}

void
ModuleLock::release()
{
    if (m_release_gil && m_owned)
    {
        pthread_mutex_unlock(&m_mutex);
        PyEval_RestoreThread(m_save);
    }
}

