
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>
# if defined(__APPLE__)
# undef HAVE_SSIZE_T
# include <pyport.h>
# endif

#include "condor_common.h"

#include <boost/python.hpp>
#include <boost/python/overloads.hpp>

// Note - condor_secman.h can't be included directly.  The following headers must
// be loaded first.  Sigh.
#include "condor_ipverify.h"
#include "sock.h"
#include "condor_secman.h"

#include "condor_attributes.h"
#include "command_strings.h"
#include "daemon.h"

#include "old_boost.h"
#include "classad_wrapper.h"

using namespace boost::python;

static
int getCommand(object command)
{
    int res = -1;

    extract<std::string> extract_string(command);
    if (extract_string.check())
    {
        std::string cmdstring = extract_string();
        if (-1 != (res = getPermissionFromString(cmdstring.c_str())))
        {
            return getSampleCommand(res);
        }
        if (-1 != (res = getCommandNum(cmdstring.c_str())))
        {
            return res;
        }
    }
    extract<int> extract_int(command);
    if (extract_int.check())
    {
        return extract_int();
    }
    THROW_EX(ValueError, "Unable to determine DaemonCore command value")
    return 0;
}

struct SecManWrapper
{
public:
    SecManWrapper() : m_secman() {}

    void
    invalidateAllCache()
    {
        m_secman.invalidateAllCache();
    }

    boost::shared_ptr<ClassAdWrapper>
    ping(object locate_obj, object command_obj=object("DC_NOP"))
    {
        int num = getCommand(command_obj);
        extract<ClassAdWrapper&> ad_extract(locate_obj);
        std::string addr;
        if (ad_extract.check())
        {
            ClassAdWrapper& ad = ad_extract();
            if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, addr))
            {
                THROW_EX(ValueError, "Daemon address not specified.");
            }
        }
        else
        {
            addr = extract<std::string>(locate_obj);
        }
        Daemon daemon(DT_ANY, addr.c_str(), NULL);
        if (!daemon.locate())
        {
            THROW_EX(RuntimeError, "Unable to find daemon.");
        }

        CondorError errstack;
        boost::shared_ptr<ClassAdWrapper> authz_ad(new ClassAdWrapper());
        ReliSock *sock = NULL;

        if (!(sock = (ReliSock*) daemon.makeConnectedSocket( Stream::reli_sock, 0, 0, &errstack )))
        {
            THROW_EX(RuntimeError, "Unable to connect to daemon.");
        }
        if (!(daemon.startSubCommand(DC_SEC_QUERY, num, sock, 0, &errstack)))
        {
            THROW_EX(RuntimeError, "Unable to send security query to daemon.");
        }
        sock->decode();
        if (!getClassAd(sock, *authz_ad.get()) || !sock->end_of_message())
        {
            THROW_EX(RuntimeError, "Failed to get security session information from remote daemon.");
        }

        MyString cmd_map_ent;
        cmd_map_ent.formatstr ("{%s,<%i>}", addr.c_str(), num);

        MyString session_id;
        KeyCacheEntry *k = NULL;
        ClassAd *policy = NULL;

        // IMPORTANT: this hashtable returns 0 on success!
        if ((SecMan::command_map)->lookup(cmd_map_ent, session_id))
        {
            THROW_EX(RuntimeError, "No valid entry in command map hash table!");
        }
        // IMPORTANT: this hashtable returns 1 on success!
        if (!(SecMan::session_cache)->lookup(session_id.Value(), k))
        {
            THROW_EX(RuntimeError, "No valid entry in session map hash table!");
        }
        policy = k->policy();
        authz_ad->Update(*policy);

        return authz_ad;
    }

    std::string
    getCommandString(int cmd)
    {
        return ::getCommandString(cmd);
    }

private:
    SecMan m_secman;
};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ping_overloads, SecManWrapper::ping, 1, 2);

void
export_secman()
{
    class_<SecManWrapper>("SecMan", "Access to the internal security state information.")
        .def("invalidateAllSessions", &SecManWrapper::invalidateAllCache, "Invalidate all security sessions.")
        .def("ping", &SecManWrapper::ping, ping_overloads("Ping a remote daemon."
            ":param ad: ClassAd describing daemon location or sinful string.\n"
            ":param command: HTCondor command to query.\n"
            ":return: ClassAd containing authorization information for the current security session."))
        .def("getCommandString", &SecManWrapper::getCommandString, "Return the string for a given integer command.");
}
