
#include "secman.h"

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
#include "module_lock.h"

using namespace boost::python;

pthread_key_t SecManWrapper::m_key;

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

SecManWrapper::SecManWrapper() : m_secman() {}

void
SecManWrapper::invalidateAllCache()
{
    m_secman.invalidateAllCache();
}

boost::shared_ptr<ClassAdWrapper>
SecManWrapper::ping(object locate_obj, object command_obj)
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

        condor::ModuleLock ml;
        if (!(sock = (ReliSock*) daemon.makeConnectedSocket( Stream::reli_sock, 0, 0, &errstack )))
        {
            ml.release();
            THROW_EX(RuntimeError, "Unable to connect to daemon.");
        }
        if (!(daemon.startSubCommand(DC_SEC_QUERY, num, sock, 0, &errstack)))
        {
            ml.release();
            THROW_EX(RuntimeError, "Unable to send security query to daemon.");
        }
        sock->decode();
        if (!getClassAd(sock, *authz_ad.get()) || !sock->end_of_message())
        {
            ml.release();
            THROW_EX(RuntimeError, "Failed to get security session information from remote daemon.");
        }
        ml.release();

        MyString cmd_map_ent;
        const std::string &tag = SecMan::getTag();
        if (tag.size()) {
                cmd_map_ent.formatstr ("{%s,%s,<%i>}", tag.c_str(), addr.c_str(), num);
        } else {
                cmd_map_ent.formatstr ("{%s,<%i>}", addr.c_str(), num);
        }

        MyString session_id;
        KeyCacheEntry *k = NULL;
        ClassAd *policy = NULL;

        // IMPORTANT: this hashtable returns 0 on success!
        if ((SecMan::command_map).lookup(cmd_map_ent, session_id))
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
SecManWrapper::getCommandString(int cmd)
{
        return ::getCommandString(cmd);
}

boost::shared_ptr<SecManWrapper>
SecManWrapper::enter(boost::shared_ptr<SecManWrapper> obj)
{
    pthread_setspecific(m_key, obj.get());
    return obj;
}

bool
SecManWrapper::exit(boost::python::object obj1, boost::python::object /*obj2*/, boost::python::object /*obj3*/)
{
    pthread_setspecific(m_key, NULL);
    m_tag = "";
    m_pool_pass = "";
    m_cred = "";
    m_config_overrides.reset();
    return (obj1.ptr() == Py_None);
}


std::string
SecManWrapper::getThreadLocalTag()
{
    SecManWrapper *man = static_cast<SecManWrapper*>(pthread_getspecific(m_key));
    return man ? man->m_tag : "";
}


std::string
SecManWrapper::getThreadLocalPoolPassword()
{
        SecManWrapper *man = static_cast<SecManWrapper*>(pthread_getspecific(m_key));
        return man ? man->m_pool_pass : "";
}


std::string
SecManWrapper::getThreadLocalGSICred()
{
        SecManWrapper *man = static_cast<SecManWrapper*>(pthread_getspecific(m_key));
        return man ? man->m_cred : "";
}


boost::shared_ptr<std::vector<std::pair<std::string, std::string> > >
SecManWrapper::getThreadLocalConfigOverrides()
{
        SecManWrapper *man = static_cast<SecManWrapper*>(pthread_getspecific(m_key));
        if (man) {return man->m_config_overrides;}
        boost::shared_ptr<std::vector<std::pair<std::string, std::string> > > empty;
        return empty;
}

void
SecManWrapper::setTag(const std::string &tag)
{
        m_tag = tag;
}

void
SecManWrapper::setPoolPassword(const std::string &pool_pass)
{
        m_pool_pass = pool_pass;
}


void
SecManWrapper::setGSICredential(const std::string &cred)
{
        m_cred = cred;
}

void
SecManWrapper::setConfig(const std::string &key, const std::string &val)
{
    if (!m_config_overrides.get())
    {
             m_config_overrides.reset(new std::vector<std::pair<std::string, std::string> >());
    }
    m_config_overrides->push_back(std::make_pair(key, val));
}


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
        .def("getCommandString", &SecManWrapper::getCommandString, "Return the string for a given integer command.")
        .def("__exit__", &SecManWrapper::exit, "Exit the context manager.")
        .def("__enter__", &SecManWrapper::enter, "Enter the context manager.")
        .def("setTag", &SecManWrapper::setTag, "Set the auth context tag")
        .def("setPoolPassword", &SecManWrapper::setPoolPassword, "Set the pool password")
        .def("setGSICredential", &SecManWrapper::setGSICredential, "Set the GSI credential")
        .def("setConfig", &SecManWrapper::setConfig, "Set a temporary configuration variable.");
}
