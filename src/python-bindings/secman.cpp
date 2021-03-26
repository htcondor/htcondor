// Note - python_bindings_common.h must be included first so it can manage macro definition conflicts
// between python and condor.
#include "python_bindings_common.h"
#include <Python.h>

// Note - condor_secman.h can't be included directly.  The following headers must
// be loaded first.  Sigh.
#include "condor_ipverify.h"
#include "sock.h"
#include "condor_secman.h"
#include "token_utils.h"

#include "classad_wrapper.h"
#include "module_lock.h"
#include "secman.h"

#include <boost/python/overloads.hpp>

#include "condor_attributes.h"
#include "command_strings.h"
#include "daemon.h"
#include "condor_claimid_parser.h"

#include "old_boost.h"
#include "htcondor.h"

using namespace boost::python;

class Token {
public:
    Token(const std::string &value) : m_value(value) {}

    const std::string &get() const {return m_value;}

    void write(boost::python::object tokenfile_obj) const {
        std::string tokenfile = "python_generated_tokens";
        if (tokenfile_obj.ptr() != Py_None) {
            boost::python::str tokenfile_str(tokenfile_obj);
            tokenfile = boost::python::extract<std::string>(tokenfile_str);
        }
        htcondor::write_out_token(tokenfile, m_value, "");
    }

private:
    const std::string m_value;
};


struct TokenPickle : boost::python::pickle_suite
{
    static boost::python::tuple
    getinitargs(const Token& token)
    {
        return boost::python::make_tuple(token.get());
    }
};


class TokenRequest {
public:
    TokenRequest(boost::python::object identity,
        boost::python::object bounding_set,
        int lifetime)
        : m_lifetime(lifetime)
    {
        boost::python::str identity_str(identity);
        m_identity = boost::python::extract<std::string>(identity_str);

        if (bounding_set.ptr() != Py_None) {
            boost::python::object iter = bounding_set.attr("__iter__")();
            while (true) {
                PyObject *pyobj = PyIter_Next(iter.ptr());
                if (!pyobj) break;
                if (PyErr_Occurred()) {
                    boost::python::throw_error_already_set();
                }
                boost::python::object obj = boost::python::object(boost::python::handle<>(pyobj));
                boost::python::str obj_str(obj);

                m_bounding_set.push_back(boost::python::extract<std::string>(obj_str));
            }
        }
    }

    std::string request_id() const {
        if (m_reqid.empty()) {
            THROW_EX(HTCondorIOError, "Request ID requested prior to submitting request!");
        }
        return m_reqid;
    }

    void submit(boost::python::object ad_obj) {
        if (m_daemon) {
            THROW_EX(HTCondorIOError, "Token request already submitted.");
        }

        if (ad_obj.ptr() == Py_None) {
            m_daemon = new Daemon(DT_COLLECTOR, nullptr);
        } else {
            ClassAdWrapper &ad = boost::python::extract<ClassAdWrapper&>(ad_obj);
            std::string ad_type_str;
            if (!ad.EvaluateAttrString(ATTR_MY_TYPE, ad_type_str))
            {
                THROW_EX(HTCondorValueError, "Daemon type not available in location ClassAd.");
            }
            int ad_type = AdTypeFromString(ad_type_str.c_str());
            if (ad_type == NO_AD)
            {
                THROW_EX(HTCondorEnumError, "Unknown ad type.");
            }
            daemon_t d_type;
            switch (ad_type) {
                case MASTER_AD: d_type = DT_MASTER; break;
                case STARTD_AD: d_type = DT_STARTD; break;
                case SCHEDD_AD: d_type = DT_SCHEDD; break;
                case NEGOTIATOR_AD: d_type = DT_NEGOTIATOR; break;
                case COLLECTOR_AD: d_type = DT_COLLECTOR; break;
                default:
                    d_type = DT_NONE;
                    THROW_EX(HTCondorEnumError, "Unknown daemon type.");
            }

            ClassAd ad_copy; ad_copy.CopyFrom(ad);
            m_daemon = new Daemon(&ad_copy, d_type, nullptr);
        }

        m_client_id = htcondor::generate_client_id();

        CondorError err;
        if (!m_daemon->startTokenRequest(m_identity, m_bounding_set, m_lifetime,
            m_client_id, m_token, m_reqid, &err))
        {
            m_client_id = "";
            THROW_EX(HTCondorIOError, err.getFullText().c_str());
        }
    }

    bool done() {
        if (!m_token.empty()) {
            return true;
        }
        CondorError err;
        if (!m_daemon->finishTokenRequest(m_client_id, m_reqid, m_token, &err)) {
            THROW_EX(HTCondorIOError, err.getFullText().c_str());
        }
        return !m_token.empty();
    }

    Token result(time_t timeout)
    {
        if (m_client_id.empty()) {
            THROW_EX(HTCondorIOError, "Request has not been submitted to a remote daemon");
        }
        bool infinite_loop = timeout == 0;
        bool last_iteration = false;
        while (true) {
            if (done()) {
                return Token(m_token);
            }
            Py_BEGIN_ALLOW_THREADS;
            int sleep_count = 5;
            if (!infinite_loop) {
                timeout -= sleep_count;
                if (timeout < 0) {
                    sleep_count += timeout;
                }
                if (timeout <= 0) {
                    last_iteration = true;
                }
            }
            sleep(5);
            Py_END_ALLOW_THREADS;
            if (PyErr_CheckSignals()) {
                boost::python::throw_error_already_set();
            }
            if (last_iteration) {
                if (done()) {
                    return Token(m_token);
                } else {
                    THROW_EX(HTCondorIOError, "Timed out waiting for token approval");
                }
            }
        }
    }

private:
    Daemon *m_daemon{nullptr};
    std::string m_reqid;
    std::string m_identity;
    std::vector<std::string> m_bounding_set;
    std::string m_token;
    std::string m_client_id;
    int m_lifetime{-1};
};

MODULE_LOCK_TLS_KEY SecManWrapper::m_key;
bool SecManWrapper::m_key_allocated = false;

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
    THROW_EX(HTCondorEnumError, "Unable to determine DaemonCore command value")
    return 0;
}

SecManWrapper::SecManWrapper()
	: m_secman()
	, m_config_overrides(true)
	, m_tag_set(false), m_pool_pass_set(false), m_cred_set(false)
{}

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
                THROW_EX(HTCondorValueError, "Daemon address not specified.");
            }
        }
        else
        {
            addr = extract<std::string>(locate_obj);
        }
        Daemon daemon(DT_ANY, addr.c_str(), NULL);
        if (!daemon.locate())
        {
            THROW_EX(HTCondorLocateError, "Unable to find daemon.");
        }

        CondorError errstack;
        boost::shared_ptr<ClassAdWrapper> authz_ad(new ClassAdWrapper());
        ReliSock *sock = NULL;

        condor::ModuleLock ml;
        if (!(sock = (ReliSock*) daemon.makeConnectedSocket( Stream::reli_sock, 0, 0, &errstack )))
        {
            ml.release();
            THROW_EX(HTCondorIOError, "Unable to connect to daemon.");
        }
        if (!(daemon.startSubCommand(DC_SEC_QUERY, num, sock, 0, &errstack)))
        {
            ml.release();
            THROW_EX(HTCondorIOError, "Unable to send security query to daemon.");
        }
        sock->decode();
        if (!getClassAd(sock, *authz_ad.get()) || !sock->end_of_message())
        {
            ml.release();
            THROW_EX(HTCondorIOError, "Failed to get security session information from remote daemon.");
        }
        // Replace addr with the sinful string that ReliSock has associated with the socket,
        // since this is what the SecMan object will do, and we need to do the same thing
        // for the SecMan::command_map lookups to succeed below.  Note that
        // get_connect_addr() may return a different sinful string than was used to
        // create the socket, due to processing of things like private network interfaces.
        addr = sock->get_connect_addr();

        // Get the policy stored in the socket.
        ClassAd sock_policy;
        sock->getPolicyAd(sock_policy);

        // Don't leak sock!
        delete sock;
        sock = NULL;
        ml.release();

        std::string cmd_map_ent;
        const std::string &tag = m_tag_set ? m_tag : SecMan::getTag();
        if (tag.size()) {
                formatstr(cmd_map_ent, "{%s,%s,<%i>}", tag.c_str(), addr.c_str(), num);
        } else {
                formatstr(cmd_map_ent, "{%s,<%i>}", addr.c_str(), num);
        }

        std::string session_id;
        KeyCacheEntry *k = NULL;
        ClassAd *policy = NULL;

        // IMPORTANT: this hashtable returns 0 on success!
        if ((SecMan::command_map).lookup(cmd_map_ent, session_id))
        {
            THROW_EX(HTCondorValueError, "No valid entry in command map hash table!");
        }
        // Session cache lookup is tag-dependent; hence, we may need to temporarily override
        std::string origTag = SecMan::getTag();
        if (m_tag_set) {SecMan::setTag(tag);}
        // IMPORTANT: this hashtable returns 1 on success!
        if (!(SecMan::session_cache)->lookup(session_id.c_str(), k))
        {
            if (m_tag_set) {SecMan::setTag(origTag);}
            THROW_EX(HTCondorValueError, "No valid entry in session map hash table!");
        }
        if (m_tag_set) {SecMan::setTag(origTag);}
        policy = k->policy();
        authz_ad->Update(*policy);
	authz_ad->Update(sock_policy);

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
    if ( ! m_key_allocated) {
        m_key_allocated = 0 == MODULE_LOCK_TLS_ALLOC(m_key);
    }
    MODULE_LOCK_TLS_SET(m_key, obj.get());
    return obj;
}

bool
SecManWrapper::exit(boost::python::object obj1, boost::python::object /*obj2*/, boost::python::object /*obj3*/)
{
    MODULE_LOCK_TLS_SET(m_key, NULL);
	//PRAGMA_REMIND("should m_cred_set, etc be cleared here?")
    m_tag = "";
    m_pool_pass = "";
    m_token = "";
    m_token_set = false;
    m_cred = "";
    m_config_overrides.reset();
    return (obj1.ptr() == Py_None);
}


const char *
SecManWrapper::getThreadLocalTag()
{
    if ( ! m_key_allocated) return NULL;
    SecManWrapper *man = static_cast<SecManWrapper*>(MODULE_LOCK_TLS_GET(m_key));
    return (man && man->m_tag_set) ? man->m_tag.c_str() : NULL;
}


const char *
SecManWrapper::getThreadLocalPoolPassword()
{
    if ( ! m_key_allocated) return NULL;
        SecManWrapper *man = static_cast<SecManWrapper*>(MODULE_LOCK_TLS_GET(m_key));
        return (man && man->m_pool_pass_set) ? man->m_pool_pass.c_str() : NULL;
}


const char *
SecManWrapper::getThreadLocalGSICred()
{
    if ( ! m_key_allocated) return NULL;
        SecManWrapper *man = static_cast<SecManWrapper*>(MODULE_LOCK_TLS_GET(m_key));
        return (man && man->m_cred_set) ? man->m_cred.c_str() : NULL;
}


bool SecManWrapper::applyThreadLocalConfigOverrides(ConfigOverrides & old)
{
    if ( ! m_key_allocated) return false;
        SecManWrapper *man = static_cast<SecManWrapper*>(MODULE_LOCK_TLS_GET(m_key));
        if (man) { man->m_config_overrides.apply(&old); return true; }
        return false;
}

void SecManWrapper::setFamilySession(const std::string & sess)
{
    if ( ! m_key_allocated) return;

    SecManWrapper *man = static_cast<SecManWrapper*>(MODULE_LOCK_TLS_GET(m_key));
    if (man) { 
        ClaimIdParser claimid(sess.c_str());
        man->m_secman.CreateNonNegotiatedSecuritySession(
                DAEMON,
                claimid.secSessionId(),
                claimid.secSessionKey(),
                claimid.secSessionInfo(),
                "FAMILY",
                "condor@family",
                NULL,
                0,
                nullptr);
    }
}


const char *
SecManWrapper::getThreadLocalToken()
{
    if (!m_key_allocated) return NULL;
        SecManWrapper *man = static_cast<SecManWrapper*>(MODULE_LOCK_TLS_GET(m_key));
        return (man && man->m_token_set) ? man->m_token.c_str() : NULL;
}

void
SecManWrapper::setTag(const std::string &tag)
{
        m_tag = tag;
        m_tag_set = true;
}

void
SecManWrapper::setPoolPassword(const std::string &pool_pass)
{
        m_pool_pass = pool_pass;
        m_pool_pass_set = true;
}

void
SecManWrapper::setToken(const Token &token)
{
        m_token = token.get();
        m_token_set = true;
}

void
SecManWrapper::setGSICredential(const std::string &cred)
{
        m_cred = cred;
        m_cred_set = true;
}

void
SecManWrapper::setConfig(const std::string &key, const std::string &val)
{
    m_config_overrides.set(key, val.c_str());
}


BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(ping_overloads, SecManWrapper::ping, 1, 2);

void
export_secman()
{
    class_<Token>("Token",
            R"C0ND0R(
            A class representing a generated HTCondor authentication token.

            :param contents: The contents of the token.
            :type contents: str
            )C0ND0R",
            boost::python::init<std::string>((boost::python::arg("self"), boost::python::arg("contents"))))
    .def("write", &Token::write,
            R"C0ND0R(
            Write the contents of the token into the appropriate token directory on disk.

            :param tokenfile: Filename inside the user token directory where the token will be written.
            :type ad: str
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("tokenfile")=boost::python::object()))
    .def_pickle(TokenPickle());

    class_<TokenRequest>("TokenRequest",
            R"C0ND0R(
            A class representing a request for a HTCondor authentication token.

            :param identity: Requested identity from the remote daemon (the empty string implies condor user).
            :type identity: str
            :param bounding_set: A list of authorizations that the token is restricted to.
            :type bounding_set: list[str]
            :param lifetime: Requested lifetime, in seconds, that the token will be valid for.
            :type lifetime: int
            )C0ND0R",
            boost::python::init<boost::python::object, boost::python::object, int>(
                (boost::python::arg("self"),
                 boost::python::arg("identity")=boost::python::str(),
                 boost::python::arg("bounding_set")=boost::python::object(),
                 boost::python::arg("lifetime")=-1)))
        .add_property(
            "request_id",
            &TokenRequest::request_id,
            "The ID of the request at the remote daemon.")
        .def("submit", &TokenRequest::submit,
            R"C0ND0R(
            Submit the token request to a remote daemon.

            :param ad: ClassAd describing the location of the remote daemon.
            :type ad: :class:`~classad.ClassAd`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("ad") = boost::python::object()))
        .def("done", &TokenRequest::done,
            R"C0ND0R(
            Check to see if the token request has completed.

            :return: ``True`` if the request is complete; ``False`` otherwise.
                May throw an exception.
            :rtype: bool
            )C0ND0R",
            (boost::python::arg("self")))
        .def("result", &TokenRequest::result,
            R"C0ND0R(
            Return the result of the token request.  Will block until the token request is approved
            or the timeout is hit (a timeoute of 0, the default, indicates this method may block
            indefinitely).

            :return: The token resulting from this request.
            :rtype: :class:`Token`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("timeout")=0));

    class_<SecManWrapper>("SecMan",
            R"C0ND0R(
            A class that represents the internal HTCondor security state.

            If a security session becomes invalid, for example, because the remote daemon restarts,
            reuses the same port, and the client continues to use the session, then all future
            commands will fail with strange connection errors. This is the only mechanism to
            invalidate in-memory sessions.

            The :class:`SecMan` can also behave as a context manager; when created, the object can
            be used to set temporary security configurations that only last during the lifetime
            of the security object.
            )C0ND0R")
        .def(boost::python::init<>(boost::python::args("self")))
        .def("invalidateAllSessions", &SecManWrapper::invalidateAllCache,
            R"C0ND0R(
            Invalidate all security sessions. Any future connections to a daemon will
            cause a new security session to be created.
            )C0ND0R",
            boost::python::args("self"))
        .def("ping", &SecManWrapper::ping, ping_overloads(
            R"C0ND0R(
            Perform a test authorization against a remote daemon for a given command.

            :param ad: The ClassAd of the daemon as returned by :meth:`Collector.locate`;
                alternately, the sinful string can be given directly as the first parameter.
            :type ad: str or :class:`~classad.ClassAd`
            :param command: The DaemonCore command to try; if not given, ``'DC_NOP'`` will be used.
            :return: An ad describing the results of the test security negotiation.
            :rtype: :class:`~classad.ClassAd`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("ad"), boost::python::arg("command")="DC_NOP")))
        .def("getCommandString", &SecManWrapper::getCommandString,
            R"C0ND0R(
            Return the string name corresponding to a given integer command.

            :param int command_int: The integer command to get the string name of.
            )C0ND0R",
            boost::python::args("self", "command_int"))
        .def("__exit__", &SecManWrapper::exit, "Exit the context manager.")
        .def("__enter__", &SecManWrapper::enter, "Enter the context manager.")
        .def("setTag", &SecManWrapper::setTag,
            R"C0ND0R(
            Set the authentication context tag for the current thread.

            All security sessions negotiated with the same tag will only
            be utilized when that tag is active.

            For example, if thread A has a tag set to ``'Joe'`` and thread B
            has a tag set to ``'Jane'``, then all security sessions negotiated
            for thread A will not be used for thread B.

            :param str tag: New tag to set.
            )C0ND0R",
            boost::python::args("self", "tag"))
        .def("setPoolPassword", &SecManWrapper::setPoolPassword,
            R"C0ND0R(
            Set the pool password.

            :param str new_pass: Updated pool password to use for new
                security negotiations.
            )C0ND0R",
            boost::python::args("self", "new_pass"))
        .def("setGSICredential", &SecManWrapper::setGSICredential,
            R"C0ND0R(
            Set the GSI credential to be used for security negotiation.

            :param str filename: File name of the GSI credential.
            )C0ND0R",
            boost::python::args("self", "filename"))
        .def("setToken", &SecManWrapper::setToken,
            R"C0ND0R(
            Set the token used for auth.

            :param token: The object representing the token contents
            :type token: :class:`Token`
            )C0ND0R",
            boost::python::args("self", "token"))
        .def("setConfig", &SecManWrapper::setConfig,
            R"C0ND0R(
            Set a temporary configuration variable; this will be kept for all security
            sessions in this thread for as long as the :class:`SecMan` object is alive.

            :param str key: Configuration key to set.
            :param str value: Temporary value to set.
            )C0ND0R",
            boost::python::args("self", "key", "value"))
        ;
}
