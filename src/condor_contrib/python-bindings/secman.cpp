
#include "condor_common.h"

#include <boost/python.hpp>

// Note - condor_secman.h can't be included directly.  The following headers must
// be loaded first.  Sigh.
#include "condor_ipverify.h"
#include "sock.h"

#include "condor_secman.h"

using namespace boost::python;

struct SecManWrapper
{
public:
    SecManWrapper() : m_secman() {}

    void
    invalidateAllCache()
    {
        m_secman.invalidateAllCache();
    }

private:
    SecMan m_secman;
};

void
export_secman()
{
    class_<SecManWrapper>("SecMan", "Access to the internal security state information.")
        .def("invalidateAllSessions", &SecManWrapper::invalidateAllCache, "Invalidate all security sessions.");
}
