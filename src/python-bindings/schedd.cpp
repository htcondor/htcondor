#include "python_bindings_common.h"
#include "condor_common.h"
#include "globus_utils.h"

#include "condor_attributes.h"
#include "condor_universe.h"
#include "condor_q.h"
#include "condor_qmgr.h"
#include "daemon.h"
#include "daemon_types.h"
#include "enum_utils.h"
#include "dc_schedd.h"
#include "classad_helpers.h"
#include "condor_config.h"
#include "condor_holdcodes.h"
#include "basename.h"
#include "selector.h"
#include "my_username.h"
#include "condor_version.h"

#include <classad/operators.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/version.hpp>

#include "old_boost.h"
#include "classad_wrapper.h"
#include "exprtree_wrapper.h"
#include "module_lock.h"
#include "query_iterator.h"
#include "submit_utils.h"

using namespace boost::python;

#define DO_ACTION(action_name) \
    reason_str = extract<std::string>(reason); \
    { \
    condor::ModuleLock ml; \
    if (use_ids) \
        result = schedd. action_name (&ids, reason_str.c_str(), NULL, AR_TOTALS); \
    else \
        result = schedd. action_name (constraint.c_str(), reason_str.c_str(), NULL, AR_TOTALS); \
    }

#define ADD_REQUIREMENT(parm, value) \
    if (boost::ifind_first(req_str, ATTR_ ##parm).begin() == req_str.end()) \
    { \
        classad::ExprTree * new_expr; \
        parser.ParseExpression(value, new_expr); \
        if (result.get()) \
        { \
            result.reset(classad::Operation::MakeOperation(classad::Operation::LOGICAL_AND_OP, result.release(), new_expr)); \
        } \
        else \
        { \
            result.reset(new_expr); \
        } \
        if (!result.get() || !new_expr) \
        { \
            PyErr_SetString(PyExc_RuntimeError, "Unable to add " #parm " requirements."); \
            throw_error_already_set(); \
        } \
    }

#define ADD_PARAM(parm) \
    { \
        const char *new_param = param(#parm); \
        if (!new_param) \
        { \
            PyErr_SetString(PyExc_RuntimeError, "Unable to determine " #parm " param value."); \
            throw_error_already_set(); \
        } \
        std::stringstream ss; \
        ss << "TARGET." #parm " == \"" << new_param << "\""; \
        ADD_REQUIREMENT(parm, ss.str()) \
    }


void
process_submit_errstack(CondorError *errstack)
{
    if (!errstack) {return;}
    while (1)
    {
        int code = errstack->code();
        std::string message = errstack->message();
        if (message.size() && message[message.size()-1] == '\n') {message.erase(message.size()-1);}
        bool realStack = errstack->pop();
        if (!realStack) {return;}
        if (code)
        {
            THROW_EX(RuntimeError, message.c_str())
        }
        else
        {
#if (PY_MINOR_VERSION >= 6) || (PY_MAJOR_VERSION > 2)
            PyErr_WarnEx(PyExc_UserWarning, message.c_str(), 0);
#endif
        }
    }
}

void
make_spool_remap(classad::ClassAd& ad, const std::string &attr, const std::string &stream_attr, const std::string &working_name)
{
    bool stream_stdout = false;
    ad.EvaluateAttrBool(stream_attr, stream_stdout);
    std::string output;
    if (ad.EvaluateAttrString(attr, output) && strcmp(output.c_str(),"/dev/null") != 0
        && output.c_str() != condor_basename(output.c_str()) && !stream_stdout)
    {
        boost::algorithm::erase_all(output, "\\");
        boost::algorithm::erase_all(output, ";");
        boost::algorithm::erase_all(output, "=");
        if (!ad.InsertAttr(attr, working_name))
            THROW_EX(RuntimeError, "Unable to add file to remap.");
        std::string output_remaps;
        ad.EvaluateAttrString(ATTR_TRANSFER_OUTPUT_REMAPS, output_remaps);
        if (output_remaps.size())
            output_remaps += ";";
        output_remaps += working_name;
        output_remaps += "=";
        output_remaps += output;
        if (!ad.InsertAttr(ATTR_TRANSFER_OUTPUT_REMAPS, output_remaps))
            THROW_EX(RuntimeError, "Unable to rewrite remaps.");
    }
}

void
make_spool(classad::ClassAd& ad)
{
    if (!ad.InsertAttr(ATTR_JOB_STATUS, HELD))
        THROW_EX(RuntimeError, "Unable to set job to hold.");
    if (!ad.InsertAttr(ATTR_HOLD_REASON, "Spooling input data files"))
        THROW_EX(RuntimeError, "Unable to set job hold reason.")
    if (!ad.InsertAttr(ATTR_HOLD_REASON_CODE, CONDOR_HOLD_CODE_SpoolingInput))
        THROW_EX(RuntimeError, "Unable to set job hold code.")
    std::stringstream ss;
    ss << ATTR_JOB_STATUS << " == " << COMPLETED << " && ( ";
    ss << ATTR_COMPLETION_DATE << "=?= UNDDEFINED || " << ATTR_COMPLETION_DATE << " == 0 || ";
    ss << "((time() - " << ATTR_COMPLETION_DATE << ") < " << 60 * 60 * 24 * 10 << "))";
    classad::ClassAdParser parser;
    classad::ExprTree * new_expr;
    parser.ParseExpression(ss.str(), new_expr);
    if (!new_expr || !ad.Insert(ATTR_JOB_LEAVE_IN_QUEUE, new_expr))
        THROW_EX(RuntimeError, "Unable to set " ATTR_JOB_LEAVE_IN_QUEUE);
    make_spool_remap(ad, ATTR_JOB_OUTPUT, ATTR_STREAM_OUTPUT, "_condor_stdout");
    make_spool_remap(ad, ATTR_JOB_ERROR, ATTR_STREAM_ERROR, "_condor_stderr");
}

std::unique_ptr<ExprTree>
make_requirements(ExprTree *reqs, ShouldTransferFiles_t stf)
{
    // Copied ideas from condor_submit.  Pretty lame.
    classad::ClassAdUnParser printer;
    classad::ClassAdParser parser;
    std::string req_str;
    printer.Unparse(req_str, reqs);
    ExprTree *reqs_copy = NULL;
    if (reqs)
        reqs_copy = reqs->Copy();
    if (!reqs_copy)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to create copy of requirements expression.");
        throw_error_already_set();
    }
    std::unique_ptr<ExprTree> result(reqs_copy);
    ADD_PARAM(OPSYS);
    ADD_PARAM(ARCH);
    switch (stf)
    {
    case STF_NO:
        ADD_REQUIREMENT(FILE_SYSTEM_DOMAIN, "TARGET." ATTR_FILE_SYSTEM_DOMAIN " == MY." ATTR_FILE_SYSTEM_DOMAIN);
        break;
    case STF_YES:
        ADD_REQUIREMENT(HAS_FILE_TRANSFER, "TARGET." ATTR_HAS_FILE_TRANSFER);
        break;
    case STF_IF_NEEDED:
        ADD_REQUIREMENT(HAS_FILE_TRANSFER, "(TARGET." ATTR_HAS_FILE_TRANSFER " || (TARGET." ATTR_FILE_SYSTEM_DOMAIN " == MY." ATTR_FILE_SYSTEM_DOMAIN"))");
        break;
    }
    ADD_REQUIREMENT(REQUEST_DISK, "TARGET.Disk >= " ATTR_REQUEST_DISK);
    ADD_REQUIREMENT(REQUEST_MEMORY, "TARGET.Memory >= " ATTR_REQUEST_MEMORY);
    return result;
}

bool
putClassAdAndEOM(Sock & sock, classad::ClassAd &ad)
{
        if (sock.type() != Stream::reli_sock)
	{
		return putClassAd(&sock, ad) && sock.end_of_message();
	}
	ReliSock & rsock = static_cast<ReliSock&>(sock);

	Selector selector;
	selector.add_fd(sock.get_file_desc(), Selector::IO_WRITE);
	int timeout = sock.timeout(0); sock.timeout(timeout);
	timeout = timeout ? timeout : 20;
	selector.set_timeout(timeout);
	if (!putClassAd(&sock, ad, PUT_CLASSAD_NON_BLOCKING))
	{
		return false;
	}
	int retval = rsock.end_of_message_nonblocking();
	while (true) {
		if (rsock.clear_backlog_flag()) {
			Py_BEGIN_ALLOW_THREADS
			selector.execute();
			Py_END_ALLOW_THREADS
			if (selector.timed_out()) {THROW_EX(RuntimeError, "Timeout when trying to write to remote host");}
		} else if (retval == 1) {
			return true;
		} else if (!retval) {
			return false;
		}
		retval = rsock.finish_end_of_message();
	}
}

bool
getClassAdWithoutGIL(Sock &sock, classad::ClassAd &ad)
{
	Selector selector;
	selector.add_fd(sock.get_file_desc(), Selector::IO_READ);
	int timeout = sock.timeout(0); sock.timeout(timeout);
	timeout = timeout ? timeout : 20;
	selector.set_timeout(timeout);
	int idx = 0;
	while (!sock.msgReady())
	{
		Py_BEGIN_ALLOW_THREADS
		selector.execute();
		Py_END_ALLOW_THREADS
		if (selector.timed_out()) {THROW_EX(RuntimeError, "Timeout when waiting for remote host");}
		if (idx++ == 50) break;
	}
	return getClassAd(&sock, ad);
}

struct HistoryIterator
{
    HistoryIterator(boost::shared_ptr<Sock> sock)
      : m_count(0), m_sock(sock)
    {}

    inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

    boost::shared_ptr<ClassAdWrapper> next()
    {
        if (m_count < 0) THROW_EX(StopIteration, "All ads processed");

        boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
        if (!getClassAdWithoutGIL(*m_sock.get(), *ad.get())) THROW_EX(RuntimeError, "Failed to receive remote ad.");
        long long intVal;
        if (ad->EvaluateAttrInt(ATTR_OWNER, intVal) && (intVal == 0))
        { // Last ad.
            if (!m_sock->end_of_message()) THROW_EX(RuntimeError, "Unable to close remote socket");
            m_sock->close();
            std::string errorMsg;
            if (ad->EvaluateAttrInt(ATTR_ERROR_CODE, intVal) && intVal && ad->EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
            {
                THROW_EX(RuntimeError, errorMsg.c_str());
            }
            if (ad->EvaluateAttrInt("MalformedAds", intVal) && intVal) THROW_EX(ValueError, "Remote side had parse errors on history file")
            if (!ad->EvaluateAttrInt(ATTR_NUM_MATCHES, intVal) || (intVal != m_count)) THROW_EX(ValueError, "Incorrect number of ads returned");

            // Everything checks out!
            m_count = -1;
            THROW_EX(StopIteration, "All ads processed");
        }
        m_count++;
        return ad;
    }

private:
    int m_count;
    boost::shared_ptr<Sock> m_sock;
};


struct RequestIterator;

struct ScheddNegotiate
{

friend struct Schedd;

public:

    static boost::shared_ptr<ScheddNegotiate> enter(boost::shared_ptr<ScheddNegotiate> obj) {return obj;}
    static bool exit(boost::shared_ptr<ScheddNegotiate> mgr, boost::python::object obj1, boost::python::object /*obj2*/, boost::python::object /*obj3*/);

    bool negotiating() {return m_negotiating;}

    void disconnect();

    void sendClaim(boost::python::object claim, boost::python::object offer_obj, boost::python::object request_obj);

    boost::shared_ptr<RequestIterator> getRequests();

    ~ScheddNegotiate();

private:

    ScheddNegotiate(const std::string & addr, const std::string & owner, const classad::ClassAd &ad);

    bool m_negotiating;
    boost::shared_ptr<Sock> m_sock;
    boost::shared_ptr<RequestIterator> m_request_iter;
};


struct RequestIterator
{
    friend struct ScheddNegotiate;

    inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

    void getNextRequest()
    {
         if (!m_parent->negotiating()) {THROW_EX(RuntimeError, "Tried to continue negotiation after disconnect.");}

        condor::ModuleLock ml;

        m_sock->encode();
        if (m_use_rrc)
        {
            if (!m_sock->put(SEND_RESOURCE_REQUEST_LIST) || !m_sock->put(m_num_to_fetch) || !m_sock->end_of_message()) {THROW_EX(RuntimeError, "Failed to request resource requests from remote schedd.");}
        }
        else
        {
            if (!m_sock->put(SEND_JOB_INFO) || !m_sock->end_of_message()) {THROW_EX(RuntimeError, "Failed to request job information from remote schedd.");}
        }

        m_sock->decode();

        for (unsigned idx=0; idx<m_num_to_fetch; idx++)
        {
            int reply;
            if (!m_sock->get(reply)) {THROW_EX(RuntimeError, "Failed to get reply from schedd.");}
            if (reply == NO_MORE_JOBS)
            {
                if (!m_sock->end_of_message()) {THROW_EX(RuntimeError, "Failed to get EOM from schedd.");}
                m_done = true;
                return;
            }
            else if (reply != JOB_INFO) {THROW_EX(RuntimeError, "Unexpected response from schedd.");}

            m_got_job_info = true;
            boost::shared_ptr<ClassAdWrapper> request_ad(new ClassAdWrapper());
            if (!getClassAdWithoutGIL(*m_sock.get(), *request_ad.get()) || !m_sock->end_of_message()) THROW_EX(RuntimeError, "Failed to receive remote ad.");
            m_requests.push_back(request_ad);
        }
    }

    boost::shared_ptr<ClassAdWrapper> next()
    {
        if (m_requests.empty())
        {
                if (m_done) {THROW_EX(StopIteration, "All requests processed");}

                getNextRequest();
                if (m_requests.empty())
                {
                    THROW_EX(StopIteration, "All requests processed");
                }
        }
        boost::shared_ptr<ClassAdWrapper> result = m_requests.front();
        m_requests.pop_front();
        return result;
    }

private:

    RequestIterator(boost::shared_ptr<Sock> sock, ScheddNegotiate *parent)
        :
          m_done(false)
        , m_use_rrc(false)
        , m_got_job_info(false)
        , m_num_to_fetch(1)
        , m_parent(parent)
        , m_sock(sock)
    {
        CondorVersionInfo vinfo;
        if (m_sock.get() && m_sock->get_peer_version())
        {
            m_use_rrc = m_sock->get_peer_version()->built_since_version(8,3,0);
        }
        if (m_use_rrc) {m_num_to_fetch = param_integer("NEGOTIATOR_RESOURCE_REQUEST_LIST_SIZE");}
    }

    bool needs_end_negotiate()
    {
        if (!m_done) {return true;}
        return m_use_rrc ? m_got_job_info : false;
    }

    bool m_done;
    bool m_use_rrc;
    bool m_got_job_info;
    unsigned m_num_to_fetch;
    ScheddNegotiate *m_parent;
    boost::shared_ptr<Sock> m_sock;
    std::deque<boost::shared_ptr<ClassAdWrapper> > m_requests;
};


struct Schedd;
struct ConnectionSentry
{

public:

    ConnectionSentry(Schedd &schedd, bool transaction=false, SetAttributeFlags_t flags=0, bool continue_txn=false);
    ~ConnectionSentry();

    void abort();
    void disconnect();
    bool connected() const {return m_connected;}
    bool transaction() const {return m_transaction;}
    int  clusterId() const {return m_cluster_id;}
    int  newCluster();
    void reschedule();
    std::string owner() const;
    std::string schedd_version();

    static boost::shared_ptr<ConnectionSentry> enter(boost::shared_ptr<ConnectionSentry> obj);
    static bool exit(boost::shared_ptr<ConnectionSentry> mgr, boost::python::object obj1, boost::python::object obj2, boost::python::object obj3);

private:
    bool m_connected;
    bool m_transaction;
    int  m_cluster_id; // non-zero when there is a submit transaction and newCluster has been called already
    SetAttributeFlags_t m_flags;
    Schedd& m_schedd;
};


bool
ScheddNegotiate::exit(boost::shared_ptr<ScheddNegotiate> mgr, boost::python::object obj1, boost::python::object /*obj2*/, boost::python::object /*obj3*/)
{
    mgr->disconnect();
    return (obj1.ptr() == Py_None);
}


void
ScheddNegotiate::disconnect()
{
    if (!m_negotiating) {return;}
    m_negotiating = false;
    /* No way to tell the remote schedd that this was a failure or a success.  Just finish the negotiation */
    bool needs_end_negotiate = m_request_iter.get() ? m_request_iter->needs_end_negotiate() : true;
    m_sock->encode();
    if (needs_end_negotiate && (!m_sock->put(END_NEGOTIATE) || !m_sock->end_of_message()))
    {
        if (!PyErr_Occurred())
        {
            THROW_EX(RuntimeError, "Could not send END_NEGOTIATE to remote schedd.");
        }
    }
}


void
ScheddNegotiate::sendClaim(boost::python::object claim, boost::python::object offer_obj, boost::python::object request_obj)
{
    if (!m_negotiating) {THROW_EX(RuntimeError, "Not currently negotiating with schedd");}
    if (!m_sock.get()) {THROW_EX(RuntimeError, "Unable to connect to schedd for negotiation");}

    std::string claim_id = boost::python::extract<std::string>(claim);
    ClassAdWrapper offer_ad = boost::python::extract<ClassAdWrapper>(offer_obj);
    ClassAdWrapper request_ad = boost::python::extract<ClassAdWrapper>(request_obj);

    compat_classad::ClassAd::CopyAttribute(ATTR_REMOTE_GROUP, offer_ad, ATTR_SUBMITTER_GROUP, request_ad);
    compat_classad::ClassAd::CopyAttribute(ATTR_REMOTE_NEGOTIATING_GROUP, offer_ad, ATTR_SUBMITTER_NEGOTIATING_GROUP, request_ad);
    compat_classad::ClassAd::CopyAttribute(ATTR_REMOTE_AUTOREGROUP, offer_ad, ATTR_SUBMITTER_AUTOREGROUP, request_ad);
    compat_classad::ClassAd::CopyAttribute(ATTR_RESOURCE_REQUEST_CLUSTER, offer_ad, ATTR_CLUSTER_ID, request_ad);
    compat_classad::ClassAd::CopyAttribute(ATTR_RESOURCE_REQUEST_PROC, offer_ad, ATTR_PROC_ID, request_ad);

    m_sock->encode();
    m_sock->put(PERMISSION_AND_AD);
    m_sock->put_secret(claim_id);
    putClassAd(m_sock.get(), offer_ad);
    m_sock->end_of_message();
}


boost::shared_ptr<RequestIterator>
ScheddNegotiate::getRequests()
{
    if (!m_negotiating) {THROW_EX(RuntimeError, "Not currently negotiating with schedd");}
    if (m_request_iter.get()) {THROW_EX(RuntimeError, "Already started negotiation for this session.");}

    boost::shared_ptr<RequestIterator> requests(new RequestIterator(m_sock, this));
    m_request_iter = requests;
    return requests;
}


ScheddNegotiate::~ScheddNegotiate()
{
    try
    {
        disconnect();
    }
    catch (boost::python::error_already_set) {}
}


ScheddNegotiate::ScheddNegotiate(const std::string & addr, const std::string & owner, const classad::ClassAd &ad)
        : m_negotiating(false)
{
    int timeout = param_integer("NEGOTIATOR_TIMEOUT",30);
    DCSchedd schedd(addr.c_str());
    m_sock.reset(schedd.reliSock(timeout));
    if (!m_sock.get()) {THROW_EX(RuntimeError, "Failed to create socket to remote schedd.");}
    bool result;
    {
        condor::ModuleLock ml;
        result = schedd.startCommand(NEGOTIATE, m_sock.get(), timeout);
    }
    if (!result) {THROW_EX(RuntimeError, "Failed to start negotiation with remote schedd.");}

    classad::ClassAd neg_ad;
    neg_ad.Update(ad);
    neg_ad.InsertAttr(ATTR_OWNER, owner);
    if (neg_ad.find(ATTR_SUBMITTER_TAG) == neg_ad.end())
    {
        neg_ad.InsertAttr(ATTR_SUBMITTER_TAG, "");
    }
    if (neg_ad.find(ATTR_AUTO_CLUSTER_ATTRS) == neg_ad.end())
    {
        neg_ad.InsertAttr(ATTR_AUTO_CLUSTER_ATTRS, "");
    }
    if (!putClassAdAndEOM(*m_sock.get(), neg_ad))
    {
        THROW_EX(RuntimeError, "Failed to send negotiation header to remote schedd.");
    }
    m_negotiating = true;
}


QueryIterator::QueryIterator(boost::shared_ptr<Sock> sock, const std::string &tag)
  : m_count(0), m_sock(sock), m_tag(tag)
{}


boost::python::object
QueryIterator::next(BlockingMode mode)
{
    if (m_count < 0) THROW_EX(StopIteration, "All ads processed");

    boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
    if (mode == Blocking)
    {
        if (!getClassAdWithoutGIL(*m_sock.get(), *ad.get())) THROW_EX(RuntimeError, "Failed to receive remote ad.");
    }
    else if (m_sock->msgReady())
    {
        if (!getClassAd(m_sock.get(), *ad)) {THROW_EX(RuntimeError, "Failed to receive remote ad.");}
    }
    else
    {
        return boost::python::object();
    }
    if (!m_sock->end_of_message()) THROW_EX(RuntimeError, "Failed to get EOM after ad.");
    long long intVal;
    if (ad->EvaluateAttrInt(ATTR_OWNER, intVal) && (intVal == 0))
    { // Last ad.
        m_sock->close();
        std::string errorMsg;
        if (ad->EvaluateAttrInt(ATTR_ERROR_CODE, intVal) && intVal && ad->EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
        {
            THROW_EX(RuntimeError, errorMsg.c_str());
        }
        if (ad->EvaluateAttrInt("MalformedAds", intVal) && intVal) THROW_EX(ValueError, "Remote side had parse errors on history file")
        //if (!ad->EvaluateAttrInt(ATTR_LIMIT_RESULTS, intVal) || (intVal != m_count)) THROW_EX(ValueError, "Incorrect number of ads returned");

        // Everything checks out!
        m_count = -1;
        if (mode == Blocking)
        {
            THROW_EX(StopIteration, "All ads processed");
        }
        else
        {
            return boost::python::object();
        }
    }
    m_count++;
    boost::python::object result(ad);
    return result;
}


boost::python::list
QueryIterator::nextAds()
{
    boost::python::list results;
    while (true)
    {
        try
        {
            boost::python::object nextobj = next(NonBlocking);
            if (nextobj == boost::python::object())
            {
                break;
            }
            results.append(nextobj);
        }
        catch (boost::python::error_already_set)
        {
            if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
                PyErr_Clear();
                break;
            }
            throw;
        }
    }
    return results;
}


int
QueryIterator::watch()
{
    return m_sock->get_file_desc();
}


struct query_process_helper
{
    object callable;
    list output_list;
    condor::ModuleLock *ml;
};

bool
query_process_callback(void * data, ClassAd* ad)
{
    query_process_helper *helper = static_cast<query_process_helper *>(data);
    helper->ml->release();
    if (PyErr_Occurred())
    {
        helper->ml->acquire();
        return true;
    }

    try
    {
        boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
        wrapper->CopyFrom(*ad);
        object wrapper_obj = object(wrapper);
        object result = (helper->callable == object()) ? wrapper_obj : helper->callable(wrapper);
        if (result != object())
        {
            helper->output_list.append(wrapper);
        }
    }
    catch (error_already_set)
    {
        // Suppress the C++ exception.  HTCondor sure can't deal with it.
        // However, PyErr_Occurred will be set and we will no longer invoke the callback.
    }
    catch (...)
    {
        PyErr_SetString(PyExc_RuntimeError, "Uncaught C++ exception encountered.");
    }
    helper->ml->acquire();
    return true;
}

struct Schedd {

    friend struct ConnectionSentry;

    Schedd()
     : m_connection(NULL)
    {
        Daemon schedd( DT_SCHEDD, 0, 0 );

        if (schedd.locate())
        {
            if (schedd.addr())
            {
                m_addr = schedd.addr();
            }
            else
            {
                PyErr_SetString(PyExc_RuntimeError, "Unable to locate schedd address.");
                throw_error_already_set();
            }
            m_name = schedd.name() ? schedd.name() : "Unknown";
            m_version = schedd.version() ? schedd.version() : "";
        }
        else
        {
            PyErr_SetString(PyExc_RuntimeError, "Unable to locate local daemon");
            boost::python::throw_error_already_set();
        }
    }

    Schedd(const ClassAdWrapper &ad)
      : m_connection(NULL), m_addr(), m_name("Unknown"), m_version("")
    {
        if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, m_addr))
        {
            PyErr_SetString(PyExc_ValueError, "Schedd address not specified.");
            throw_error_already_set();
        }
        ad.EvaluateAttrString(ATTR_NAME, m_name);
        ad.EvaluateAttrString(ATTR_VERSION, m_version);
    }

    ~Schedd()
    {
        if (m_connection) { m_connection->abort(); }
    }

    boost::shared_ptr<ScheddNegotiate> negotiate(const std::string &owner, boost::python::object ad_obj)
    {
        ClassAdWrapper ad = boost::python::extract<ClassAdWrapper>(ad_obj);
        boost::shared_ptr<ScheddNegotiate> negotiator(new ScheddNegotiate(m_addr, owner, ad));
        return negotiator;
    }

    object query(boost::python::object constraint_obj=boost::python::object(""), list attrs=list(), object callback=object(), int match_limit=-1, CondorQ::QueryFetchOpts fetch_opts=CondorQ::fetch_Jobs)
    {
        std::string constraint;
        extract<std::string> constraint_extract(constraint_obj);
        if (constraint_extract.check())
        {
            constraint = constraint_extract();
        }
        else
        {
            classad::ClassAdUnParser printer;
            classad_shared_ptr<classad::ExprTree> expr(convert_python_to_exprtree(constraint_obj));
            printer.Unparse(constraint, expr.get());
        }

        CondorQ q;

        if (constraint.size())
            q.addAND(constraint.c_str());

        StringList attrs_list(NULL, "\n");
        // Must keep strings alive; note StringList DOES create an internal copy
        int len_attrs = py_len(attrs);
        for (int i=0; i<len_attrs; i++)
        {
            std::string attrName = extract<std::string>(attrs[i]);
            attrs_list.append(attrName.c_str()); // note append() does strdup
        }

        list retval;
        int fetchResult;
        CondorError errstack;
        {
        query_process_helper helper;
        helper.callable = callback;
        helper.output_list = retval;
        void *helper_ptr = static_cast<void *>(&helper);
        ClassAd * summary_ad = NULL; // points to a final summary ad when we query an actual schedd.
        ClassAd ** p_summary_ad = NULL;
        if ( fetch_opts == CondorQ::fetch_SummaryOnly ) {  // only get the summary ad if option says so
            p_summary_ad = &summary_ad;
        }


        {
            condor::ModuleLock ml;
            helper.ml = &ml;
            fetchResult = q.fetchQueueFromHostAndProcess(m_addr.c_str(), attrs_list, fetch_opts, match_limit, query_process_callback, helper_ptr, 2, &errstack, p_summary_ad);
			if (summary_ad) {
				query_process_callback(helper_ptr,summary_ad);
				delete summary_ad;
				summary_ad = NULL;
			}
        }
        }

        if (PyErr_Occurred())
        {
            throw_error_already_set();
        }

        switch (fetchResult)
        {
        case Q_OK:
            break;
        case Q_PARSE_ERROR:
        case Q_INVALID_CATEGORY:
            PyErr_SetString(PyExc_RuntimeError, "Parse error in constraint.");
            throw_error_already_set();
            break;
        case Q_UNSUPPORTED_OPTION_ERROR:
            PyErr_SetString(PyExc_RuntimeError, "Query fetch option unsupported by this schedd.");
            throw_error_already_set();
			break;
        default:
			std::string errmsg = "Failed to fetch ads from schedd, errmsg=" + errstack.getFullText();
			PyErr_SetString(PyExc_IOError, errmsg.c_str());
            throw_error_already_set();
            break;
        }

        return retval;
    }

    void reschedule()
    {
        DCSchedd schedd(m_addr.c_str());
        Stream::stream_type st = schedd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
        bool result;
        {
        condor::ModuleLock ml;
        result = !schedd.sendCommand(RESCHEDULE, st, 0);
        }
        if (result) {
            dprintf(D_ALWAYS, "Can't send RESCHEDULE command to schedd.\n" );
        }
    }


    std::string
    owner() const
    {
        std::string result;
        if (owner_from_sock(result)) {return result;}

        char *owner = my_username();
        if (!owner)
        {
            result = "unknown";
        }
        else
        {
            result = owner;
            free(owner);
        }
        return result;
    }


    bool
    owner_from_sock(std::string &result) const
    {
        MyString cmd_map_ent;
        cmd_map_ent.formatstr ("{%s,<%i>}", m_addr.c_str(), QMGMT_WRITE_CMD); 

        MyString session_id;
        KeyCacheEntry *k = NULL;
        int ret = 0;

        // IMPORTANT: this hashtable returns 0 on success!
        ret = (SecMan::command_map).lookup(cmd_map_ent, session_id);
        if (ret)
        {
            return false;
        }

        // IMPORTANT: this hashtable returns 1 on success!
        ret = (SecMan::session_cache)->lookup(session_id.Value(), k);
        if (!ret)
        {
            return false;
        }

	ClassAd *policy = k->policy();

        if (!policy->EvaluateAttrString(ATTR_SEC_MY_REMOTE_USER_NAME, result))
        {
            return false;
        }
        std::size_t pos = result.find("@");
        if (pos != std::string::npos)
        {
            result = result.substr(0, result.find("@"));
        }
        return true;
    }

    object actOnJobs(JobAction action, object job_spec, object reason=object())
    {
        if (reason == object())
        {
            reason = object("Python-initiated action");
        }
        StringList ids;
        std::vector<std::string> ids_list;
        std::string constraint, reason_str, reason_code;
        bool use_ids = false;
        extract<std::string> constraint_extract(job_spec);
        if (constraint_extract.check())
        {
            constraint = constraint_extract();
        }
        else
        {
            int id_len = py_len(job_spec);
            ids_list.reserve(id_len);
            for (int i=0; i<id_len; i++)
            {
                std::string str = extract<std::string>(job_spec[i]);
                ids_list.push_back(str);
                ids.append(ids_list[i].c_str());
            }
            use_ids = true;
        }
        DCSchedd schedd(m_addr.c_str());
        ClassAd *result = NULL;
        VacateType vacate_type;
        tuple reason_tuple;
        const char *reason_char, *reason_code_char = NULL;
        extract<tuple> try_extract_tuple(reason);
        switch (action)
        {
        case JA_HOLD_JOBS:
            if (try_extract_tuple.check())
            {
                reason_tuple = extract<tuple>(reason);
                if (py_len(reason_tuple) != 2)
                {
                    PyErr_SetString(PyExc_ValueError, "Hold action requires (hold string, hold code) tuple as the reason.");
                    throw_error_already_set();
                }
                reason_str = extract<std::string>(reason_tuple[0]); reason_char = reason_str.c_str();
                reason_code = extract<std::string>(reason_tuple[1]); reason_code_char = reason_code.c_str();
            }
            else
            {
                reason_str = extract<std::string>(reason);
                reason_char = reason_str.c_str();
            }
            if (use_ids)
            {
                condor::ModuleLock ml;
                result = schedd.holdJobs(&ids, reason_char, reason_code_char, NULL, AR_TOTALS);
            }
            else
            {
                condor::ModuleLock ml;
                result = schedd.holdJobs(constraint.c_str(), reason_char, reason_code_char, NULL, AR_TOTALS);
            }
            break;
        case JA_RELEASE_JOBS:
            DO_ACTION(releaseJobs)
            break;
        case JA_REMOVE_JOBS:
            DO_ACTION(removeJobs)
            break;
        case JA_REMOVE_X_JOBS:
            DO_ACTION(removeXJobs)
            break;
        case JA_VACATE_JOBS:
        case JA_VACATE_FAST_JOBS:
            vacate_type = action == JA_VACATE_JOBS ? VACATE_GRACEFUL : VACATE_FAST;
            if (use_ids)
            {
                condor::ModuleLock ml;
                result = schedd.vacateJobs(&ids, vacate_type, NULL, AR_TOTALS);
            }
            else
            {
                condor::ModuleLock ml;
                result = schedd.vacateJobs(constraint.c_str(), vacate_type, NULL, AR_TOTALS);
            }
            break;
        case JA_SUSPEND_JOBS:
            DO_ACTION(suspendJobs)
            break;
        case JA_CONTINUE_JOBS:
            DO_ACTION(continueJobs)
            break;
        default:
            PyErr_SetString(PyExc_NotImplementedError, "Job action not implemented.");
            throw_error_already_set();
        }
        if (!result)
        {
            PyErr_SetString(PyExc_RuntimeError, "Error when performing action on the schedd.");
            throw_error_already_set();
        }

        boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
        wrapper->CopyFrom(*result);
        object wrapper_obj(wrapper);

        boost::shared_ptr<ClassAdWrapper> result_ptr(new ClassAdWrapper());
        object result_obj(result_ptr);

        result_obj["TotalError"] = wrapper_obj["result_total_0"];
        result_obj["TotalSuccess"] = wrapper_obj["result_total_1"];
        result_obj["TotalNotFound"] = wrapper_obj["result_total_2"];
        result_obj["TotalBadStatus"] = wrapper_obj["result_total_3"];
        result_obj["TotalAlreadyDone"] = wrapper_obj["result_total_4"];
        result_obj["TotalPermissionDenied"] = wrapper_obj["result_total_5"];
        result_obj["TotalJobAds"] = wrapper_obj["TotalJobAds"];
        result_obj["TotalChangedAds"] = wrapper_obj["ActionResult"];
        return result_obj;
    }

    object actOnJobs2(JobAction action, object job_spec)
    {
        return actOnJobs(action, job_spec, object("Python-initiated action."));
    }

    int submitMany(const ClassAdWrapper &wrapper, boost::python::object proc_ads, bool spool, boost::python::object ad_results=object())
    {
        PyObject *py_iter = PyObject_GetIter(proc_ads.ptr());
        if (!py_iter)
        {
            THROW_EX(ValueError, "Proc ads must be iterator of 2-tuples.");
        }

        ConnectionSentry sentry(*this); // Automatically connects / disconnects.

        classad::ClassAd cluster_ad;
        cluster_ad.CopyFrom(wrapper);
        int cluster = submit_cluster_internal(cluster_ad, spool);

        boost::python::object iter = boost::python::object(boost::python::handle<>(py_iter));
        PyObject *obj;
        while ((obj = PyIter_Next(iter.ptr())))
        {
            boost::python::object boost_obj = boost::python::object(boost::python::handle<>(obj));
            ClassAdWrapper proc_ad = boost::python::extract<ClassAdWrapper>(boost_obj[0]);
            int count = boost::python::extract<int>(boost_obj[1]);
            try
            {
                proc_ad.ChainToAd(const_cast<classad::ClassAd*>(&cluster_ad));
                submit_proc_internal(cluster, proc_ad, count, spool, ad_results);
            }
            catch (...)
            {
                proc_ad.ChainToAd(NULL);
                throw;
            }
        }

        if (param_boolean("SUBMIT_SEND_RESCHEDULE",true))
        {
            reschedule();
        }
        return cluster;
    }

    int submit(const ClassAdWrapper &wrapper, int count=1, bool spool=false, object ad_results=object())
    {
        boost::python::list proc_entry;
        boost::shared_ptr<ClassAdWrapper> proc_ad(new ClassAdWrapper());
        proc_entry.append(proc_ad);
        proc_entry.append(count);
        boost::python::list proc_ads;
        proc_ads.append(proc_entry);
        return submitMany(wrapper, proc_ads, spool, ad_results);
    }

    int submit_cluster_internal(classad::ClassAd &orig_cluster_ad, bool spool)
    {
        int cluster;
        {
            condor::ModuleLock ml;
            cluster = NewCluster();
        }
        if (cluster < 0)
        {
            THROW_EX(RuntimeError, "Failed to create new cluster.");
        }

        ClassAd cluster_ad;
        // Create a blank ad for job submission.
        ClassAd *tmpad = CreateJobAd(NULL, CONDOR_UNIVERSE_VANILLA, "/bin/echo");
        if (tmpad)
        {
            cluster_ad.CopyFrom(*tmpad);
            delete tmpad;
        }
        else
        {
            THROW_EX(RuntimeError, "Failed to create a new job ad.");
        }
        char path[4096];
        if (getcwd(path, 4095))
        {
            cluster_ad.InsertAttr(ATTR_JOB_IWD, path);
        }

        // Copy the attributes specified by the invoker.
        cluster_ad.Update(orig_cluster_ad);

        ShouldTransferFiles_t should = STF_IF_NEEDED;
        std::string should_str;
        if (cluster_ad.EvaluateAttrString(ATTR_SHOULD_TRANSFER_FILES, should_str))
        {
            if (should_str == "YES") {should = STF_YES;}
            else if (should_str == "NO") {should = STF_NO;}
        }

        ExprTree *old_reqs = cluster_ad.Lookup(ATTR_REQUIREMENTS);
        ExprTree *new_reqs = make_requirements(old_reqs, should).release();
        cluster_ad.Insert(ATTR_REQUIREMENTS, new_reqs);

        if (spool)
        {
            make_spool(cluster_ad);
        }

        // Set all the cluster attributes
        classad::ClassAdUnParser unparser;
        unparser.SetOldClassAd(true);

        for (classad::ClassAd::const_iterator it = cluster_ad.begin(); it != cluster_ad.end(); it++)
        {
            std::string rhs;
            unparser.Unparse(rhs, it->second);
                // Note I don't release the GIL here - as we are in NoAck mode, assume this is just
                // buffering up data into the socket.
            if (-1 == SetAttribute(cluster, -1, it->first.c_str(), rhs.c_str(), SetAttribute_NoAck))
            {
                THROW_EX(ValueError, it->first.c_str());
            }
        }

        orig_cluster_ad = cluster_ad;
        return cluster;
    }


    void submit_proc_internal(int cluster, const classad::ClassAd &orig_proc_ad, int count, bool spool, boost::python::object ad_results)
    {
        classad::ClassAd proc_ad;
        proc_ad.CopyFrom(orig_proc_ad);

        ExprTree *old_reqs = proc_ad.Lookup(ATTR_REQUIREMENTS);
        if (old_reqs)
        {   // Only update the requirements here; the cluster ad got reasonable ones inserted by default.
            ShouldTransferFiles_t should = STF_IF_NEEDED;
            std::string should_str;
            if (proc_ad.EvaluateAttrString(ATTR_SHOULD_TRANSFER_FILES, should_str))
            {
                if (should_str == "YES") {should = STF_YES;}
                else if (should_str == "NO") {should = STF_NO;}
            }

            ExprTree *new_reqs = make_requirements(old_reqs, should).release();
            proc_ad.Insert(ATTR_REQUIREMENTS, new_reqs);
        }

        if (spool)
        {
            make_spool(proc_ad);
        }

        bool keep_results = false;
        extract<list> try_ad_results(ad_results);
        if (try_ad_results.check())
        {
            keep_results = true;
        }

        for (int idx=0; idx<count; idx++)
        {
            int procid;
            {
            condor::ModuleLock ml;
            procid = NewProc(cluster);
            }
            if (procid < 0)
            {
                PyErr_SetString(PyExc_RuntimeError, "Failed to create new proc id.");
                throw_error_already_set();
            }
            proc_ad.InsertAttr(ATTR_CLUSTER_ID, cluster);
            proc_ad.InsertAttr(ATTR_PROC_ID, procid);

            classad::ClassAdUnParser unparser;
            unparser.SetOldClassAd( true );
            for (classad::ClassAd::const_iterator it = proc_ad.begin(); it != proc_ad.end(); it++)
            {
                std::string rhs;
                unparser.Unparse(rhs, it->second);
                    // Note I don't release the GIL here - as we are in NoAck mode, assume this is just
                    // buffering up data into the socket.
                if (-1 == SetAttribute(cluster, procid, it->first.c_str(), rhs.c_str(), SetAttribute_NoAck))
                {
                    PyErr_SetString(PyExc_ValueError, it->first.c_str());
                    throw_error_already_set();
                }
            }
            if (keep_results)
            {
                boost::shared_ptr<ClassAdWrapper> results_ad(new ClassAdWrapper());
                results_ad->CopyFromChain(proc_ad);
                ad_results.attr("append")(results_ad);
            }
        }
    }

    void spool(object jobs)
    {
        int len = py_len(jobs);
        std::vector<compat_classad::ClassAd*> job_array;
        std::vector<boost::shared_ptr<compat_classad::ClassAd> > job_tmp_array;
        job_array.reserve(len);
        job_tmp_array.reserve(len);
        for (int i=0; i<len; i++)
        {
            const ClassAdWrapper wrapper = extract<ClassAdWrapper>(jobs[i]);
            boost::shared_ptr<compat_classad::ClassAd> tmp_ad(new compat_classad::ClassAd());
            job_tmp_array.push_back(tmp_ad);
            tmp_ad->CopyFrom(wrapper);
            job_array.push_back(job_tmp_array[i].get());
        }
        CondorError errstack;
        bool result;
        DCSchedd schedd(m_addr.c_str());
        {
        condor::ModuleLock ml;
        result = schedd.spoolJobFiles( len,
                                       &job_array[0],
                                       &errstack );
        }
        if (!result)
        {
            PyErr_SetString(PyExc_RuntimeError, errstack.getFullText(true).c_str());
            throw_error_already_set();
        }
    }

    void retrieve(std::string jobs)
    {
        CondorError errstack;
        DCSchedd schedd(m_addr.c_str());
        bool result;
        {
        condor::ModuleLock ml;
        result = !schedd.receiveJobSandbox(jobs.c_str(), &errstack);
        }
        if (result)
        {
            THROW_EX(RuntimeError, errstack.getFullText(true).c_str());
        }
    }

    int refreshGSIProxy(int cluster, int proc, std::string proxy_filename, int lifetime=-1)
    {
        time_t now = time(NULL);
        time_t result_expiration;
        CondorError errstack;

        if (lifetime < 0)
        {
            lifetime = param_integer("DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME", 0);
        }

        DCSchedd schedd(m_addr.c_str());
        bool do_delegation = param_boolean("DELEGATE_JOB_GSI_CREDENTIALS", true);
        bool result;
        {
        condor::ModuleLock ml;
        result = do_delegation && !schedd.delegateGSIcredential(cluster, proc, proxy_filename.c_str(), lifetime ? now+lifetime : 0,
            &result_expiration, &errstack);
        }
        if (result)
        {
            THROW_EX(RuntimeError, errstack.getFullText(true).c_str());
        }
        else if (!do_delegation)
        {
            {
            condor::ModuleLock ml;
            result = !schedd.updateGSIcredential(cluster, proc, proxy_filename.c_str(), &errstack);
            }
            if (result)
            {
                THROW_EX(RuntimeError, errstack.getFullText(true).c_str());
            }
            // Note: x509_error_string() is not thread-safe; hence, we are not using the HTCondor-generated
            // error handling.
            int result = x509_proxy_seconds_until_expire(proxy_filename.c_str());
            if (result < 0) {THROW_EX(RuntimeError, "Unable to determine proxy expiration time");}
            return result;
        }
        return result_expiration - now;
    }


    // TODO: allow user to specify flags.
    void edit(object job_spec, std::string attr, object val)
    {
        std::vector<int> clusters;
        std::vector<int> procs;
        std::string constraint;
        bool use_ids = false;
        extract<std::string> constraint_extract(job_spec);
        if (constraint_extract.check())
        {
            constraint = constraint_extract();
        }
        else
        {
            int id_len = py_len(job_spec);
            clusters.reserve(id_len);
            procs.reserve(id_len);
            for (int i=0; i<id_len; i++)
            {
                object id_list = job_spec[i].attr("split")(".");
                if (py_len(id_list) != 2)
                {
                    PyErr_SetString(PyExc_ValueError, "Invalid ID");
                    throw_error_already_set();
                }
                clusters.push_back(extract<int>(long_(id_list[0])));
                procs.push_back(extract<int>(long_(id_list[1])));
            }
            use_ids = true;
        }

        std::string val_str;
        extract<ExprTreeHolder &> exprtree_extract(val);
        if (exprtree_extract.check())
        {
            classad::ClassAdUnParser unparser;
            unparser.Unparse(val_str, exprtree_extract().get());
        }
        else
        {
            val_str = extract<std::string>(val);
        }

        ConnectionSentry sentry(*this);

        bool result;
        if (use_ids)
        {
            for (unsigned idx=0; idx<clusters.size(); idx++)
            {
                {
                condor::ModuleLock ml;
                result = -1 == SetAttribute(clusters[idx], procs[idx], attr.c_str(), val_str.c_str());
                }
                if (result)
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to edit job");
                    throw_error_already_set();
                }
            }
        }
        else
        {
            {
            condor::ModuleLock ml;
            result = -1 == SetAttributeByConstraint(constraint.c_str(), attr.c_str(), val_str.c_str());
            }
            if (result)
            {
                PyErr_SetString(PyExc_RuntimeError, "Unable to edit jobs matching constraint");
                throw_error_already_set();
            }
        }
    }


    boost::shared_ptr<HistoryIterator> history(boost::python::object requirement, boost::python::list projection=boost::python::list(), int match=-1, boost::python::object since=boost::python::object())
    {
        std::string val_str;
        extract<ExprTreeHolder &> exprtree_extract(requirement);
        extract<std::string> string_extract(requirement);
        classad::ExprTree *expr = NULL;
        boost::shared_ptr<classad::ExprTree> expr_ref;
        if (string_extract.check())
        {
            classad::ClassAdParser parser;
            std::string val_str = string_extract();
            if (!parser.ParseExpression(val_str, expr))
            {
                THROW_EX(ValueError, "Unable to parse requirements expression");
            }
            expr_ref.reset(expr);
        }
        else if (exprtree_extract.check())
        {
            expr = exprtree_extract().get();
        }
        else
        {
            THROW_EX(ValueError, "Unable to parse requirements expression");
        }
        classad::ExprTree *expr_copy = expr->Copy();
        if (!expr_copy) THROW_EX(ValueError, "Unable to create copy of requirements expression");

	classad::ExprList *projList(new classad::ExprList());
	unsigned len_attrs = py_len(projection);
	for (unsigned idx = 0; idx < len_attrs; idx++)
	{
		classad::Value value; value.SetStringValue(boost::python::extract<std::string>(projection[idx]));
		classad::ExprTree *entry = classad::Literal::MakeLiteral(value);
		if (!entry) THROW_EX(ValueError, "Unable to create copy of list entry.")
		projList->push_back(entry);
	}

	// decode the since argument, this can either be an expression, or a string
	// containing either an expression, a cluster id or a full job id.
	classad::ExprTree *since_expr_copy = NULL;
	extract<ExprTreeHolder &> since_exprtree_extract(since);
	extract<std::string> since_string_extract(since);
	extract<int>  since_cluster_extract(since);
	if (since_cluster_extract.check()) {
		std::string expr_str;
		formatstr(expr_str, "ClusterId == %d", since_cluster_extract());
		classad::ClassAdParser parser;
		parser.ParseExpression(expr_str, since_expr_copy);
	} else if (since_string_extract.check()) {
		std::string since_str = since_string_extract();
		classad::ClassAdParser parser;
		if ( ! parser.ParseExpression(since_str, since_expr_copy)) {
			THROW_EX(ValueError, "Unable to parse since argument as an expression or as a job id.");
		} else {
			classad::Value val;
			if (ExprTreeIsLiteral(since_expr_copy, val) && val.IsNumber()) {
				delete since_expr_copy; since_expr_copy = NULL;
				// if the stop constraint is a numeric literal.
				// then there are a few special cases...
				// it might be a job id. or it might (someday) be a time value
				PROC_ID jid;
				const char * pend;
				if (StrIsProcId(since_str.c_str(), jid.cluster, jid.proc, &pend) && !*pend) {
					if (jid.proc >= 0) {
						formatstr(since_str, "ClusterId == %d && ProcId == %d", jid.cluster, jid.proc);
					} else {
						formatstr(since_str, "ClusterId == %d", jid.cluster);
					}
					parser.ParseExpression(since_str, since_expr_copy);
				}
			}
		}
	} else if (since_exprtree_extract.check()) {
		since_expr_copy = since_exprtree_extract().get()->Copy();
	} else if (since.ptr() != Py_None) {
		THROW_EX(ValueError, "invalid since argument");
	}


	classad::ClassAd ad;
	ad.Insert(ATTR_REQUIREMENTS, expr_copy);
	ad.InsertAttr(ATTR_NUM_MATCHES, match);
	if (since_expr_copy) { ad.Insert("Since", since_expr_copy); }

	classad::ExprTree *projTree = static_cast<classad::ExprTree*>(projList);
	ad.Insert(ATTR_PROJECTION, projTree);

	DCSchedd schedd(m_addr.c_str());
	Sock* sock;
        bool result;
        {
        condor::ModuleLock ml;
        result = !(sock = schedd.startCommand(QUERY_SCHEDD_HISTORY, Stream::reli_sock, 0));
        }
        if (result)
        {
            THROW_EX(RuntimeError, "Unable to connect to schedd");
        }
        boost::shared_ptr<Sock> sock_sentry(sock);

	if (!putClassAdAndEOM(*sock, ad)) THROW_EX(RuntimeError, "Unable to send request classad to schedd");

        boost::shared_ptr<HistoryIterator> iter(new HistoryIterator(sock_sentry));
        return iter;
    }


    boost::shared_ptr<ConnectionSentry> transaction(SetAttributeFlags_t flags=0, bool continue_txn=false)
    {
        boost::shared_ptr<ConnectionSentry> sentry_ptr(new ConnectionSentry(*this, true, flags, continue_txn));
        return sentry_ptr;
    }

    boost::shared_ptr<QueryIterator> xquery(boost::python::object requirement=boost::python::object(), boost::python::list projection=boost::python::list(), int limit=-1, CondorQ::QueryFetchOpts fetch_opts=CondorQ::fetch_Jobs, boost::python::object tag=boost::python::object())
    {
        std::string val_str;

        std::string tag_str = (tag == boost::python::object()) ? m_name : boost::python::extract<std::string>(tag);

        extract<ExprTreeHolder &> exprtree_extract(requirement);
        extract<std::string> string_extract(requirement);
        classad::ExprTree *expr = NULL;
        boost::shared_ptr<classad::ExprTree> expr_ref;
        if (requirement == boost::python::object())
        {
            classad::ClassAdParser parser;
            parser.ParseExpression("true", expr);
            expr_ref.reset(expr);
        }
        else if (string_extract.check())
        {
            classad::ClassAdParser parser;
            std::string val_str = string_extract();
            if (!parser.ParseExpression(val_str, expr))
            {
                THROW_EX(ValueError, "Unable to parse requirements expression");
            }
            expr_ref.reset(expr);
        }
        else if (exprtree_extract.check())
        {
            expr = exprtree_extract().get();
        }
        else
        {
            THROW_EX(ValueError, "Unable to parse requirements expression");
        }
        classad::ExprTree *expr_copy = expr ? expr->Copy() : NULL;
        if (!expr_copy) {THROW_EX(ValueError, "Unable to create copy of requirements expression");}

        classad::ExprList *projList(new classad::ExprList());
        unsigned len_attrs = py_len(projection);
        for (unsigned idx = 0; idx < len_attrs; idx++)
        {
                classad::Value value; value.SetStringValue(boost::python::extract<std::string>(projection[idx]));
                classad::ExprTree *entry = classad::Literal::MakeLiteral(value);
                if (!entry) {THROW_EX(ValueError, "Unable to create copy of list entry.")}
                projList->push_back(entry);
        }

        classad::ClassAd ad;
        ad.Insert(ATTR_REQUIREMENTS, expr_copy);
        ad.InsertAttr(ATTR_LIMIT_RESULTS, limit);
        if (fetch_opts)
        {
            ad.InsertAttr("QueryDefaultAutocluster", fetch_opts);
        }

        classad::ExprTree *projTree = static_cast<classad::ExprTree*>(projList);
        ad.Insert(ATTR_PROJECTION, projTree);

        DCSchedd schedd(m_addr.c_str());
        Sock* sock;
        bool result;
        {
        condor::ModuleLock ml;
        result = !(sock = schedd.startCommand(QUERY_JOB_ADS, Stream::reli_sock, 0));
        }
        if (result)
        {
                THROW_EX(RuntimeError, "Unable to connect to schedd");
        }
        boost::shared_ptr<Sock> sock_sentry(sock);

        if (!putClassAdAndEOM(*sock, ad)) THROW_EX(RuntimeError, "Unable to send request classad to schedd");

        boost::shared_ptr<QueryIterator> iter(new QueryIterator(sock_sentry, tag_str));
        return iter;
    }

private:

    ConnectionSentry* m_connection;

    std::string m_addr, m_name, m_version;

};

ConnectionSentry::ConnectionSentry(Schedd &schedd, bool transaction, SetAttributeFlags_t flags, bool continue_txn)
     : m_connected(false), m_transaction(false), m_cluster_id(0), m_flags(flags), m_schedd(schedd)
{
    if (schedd.m_connection)
    {
        if (transaction && !continue_txn) { THROW_EX(RuntimeError, "Transaction already in progress for schedd."); }
        return;
    }
    else
    {
        bool result;
        {
        condor::ModuleLock ml;
        result = ConnectQ(schedd.m_addr.c_str(), 0, false, NULL, NULL, schedd.m_version.c_str()) == 0;
        }
        if (result)
        {
            THROW_EX(RuntimeError, "Failed to connect to schedd.");
        }
    }
    schedd.m_connection = this;
    m_connected = true;
    if (transaction)
    {/*
        if (BeginTransaction())
        {
            THROW_EX(RuntimeError, "Failed to begin transaction.");
        }
    */}
    m_transaction = transaction;
}

int
ConnectionSentry::newCluster()
{
    condor::ModuleLock ml;
    m_cluster_id = NewCluster();
    return m_cluster_id;
}

void
ConnectionSentry::reschedule()
{
    m_schedd.reschedule();
}


std::string
ConnectionSentry::owner() const
{
    return m_schedd.owner();
}

std::string
ConnectionSentry::schedd_version()
{
	return m_schedd.m_version;
}


void
ConnectionSentry::abort()
{ 
    if (m_transaction)
    {
        m_transaction = false;
        bool result;
        {
        condor::ModuleLock ml;
        result = AbortTransaction();
        }
        if (result)
        {
            if (PyErr_Occurred()) {return;}
            THROW_EX(RuntimeError, "Failed to abort transaction.");
        }
        if (m_connected)
        {
            m_connected = false;
            m_schedd.m_connection = NULL;
            {
            condor::ModuleLock ml;
            DisconnectQ(NULL);
            }
        }
    }
    else if (m_schedd.m_connection && m_schedd.m_connection != this)
    {
        m_schedd.m_connection->abort();
    }
}


// Enter the context manager; as this is a dual-purpose object, this is a no-op
// We assume the transaction started with the constructor.
boost::shared_ptr<ConnectionSentry>
ConnectionSentry::enter(boost::shared_ptr<ConnectionSentry> obj)
{
    return obj;
}


bool
ConnectionSentry::exit(boost::shared_ptr<ConnectionSentry> mgr, boost::python::object obj1, boost::python::object /*obj2*/, boost::python::object /*obj3*/)
{
    if (obj1.ptr() == Py_None)
    {
        mgr->disconnect();
        return true;
    }
    mgr->abort();
    return false;
}


void
ConnectionSentry::disconnect()
{
    bool throw_commit_error = false;
    CondorError errstack;
    if (m_transaction)
    {
        m_transaction = false;
        {
        condor::ModuleLock ml;
        throw_commit_error = RemoteCommitTransaction(m_flags, &errstack);
        }
    }
    if (m_connected)
    {
        m_connected = false;
        m_schedd.m_connection = NULL;
        // WARNING: DisconnectQ returns a boolean; failure test is different from rest of qmgmt API.
        bool result;
        {
        condor::ModuleLock ml;
        result = !DisconnectQ(NULL, true, &errstack);
        }
        if (result)
        {
            if (PyErr_Occurred()) {return;}
            std::string errmsg = "Failed to commmit and disconnect from queue.";
            std::string esMsg = errstack.getFullText();
            if( ! esMsg.empty() ) { errmsg += " " + esMsg; }
            THROW_EX(RuntimeError, errmsg.c_str());
        }
    }
    if (throw_commit_error)
    {
        if (PyErr_Occurred()) {return;}
        std::string errmsg = "Failed to commit ongoing transaction.";
        std::string esMsg = errstack.getFullText();
        if( ! esMsg.empty() ) { errmsg += " " + esMsg; }
        THROW_EX(RuntimeError, errmsg.c_str());
    }
}


ConnectionSentry::~ConnectionSentry()
{
    if (PyErr_Occurred()) {abort();}
    disconnect();
}


struct Submit
{
public:
    Submit()
    {
        m_hash.init();
    }


    Submit(boost::python::dict input)
    {
        m_hash.init();
        update(input);
    }


    std::string
    expand(const std::string attr) const
    {
        char *val_char(const_cast<Submit*>(this)->m_hash.submit_param(attr.c_str()));
        std::string value(val_char);
        free(val_char);
        return value;
    }


    std::string
    getItem(const std::string attr) const
    {
        const char *val = const_cast<Submit*>(this)->m_hash.lookup(attr.c_str());
        if (val == NULL)
        {
            THROW_EX(KeyError, attr.c_str())
        }
        return std::string(val);
    }


    std::string
    get(const std::string attr, const std::string value) const
    {
        const char *val = const_cast<Submit*>(this)->m_hash.lookup(attr.c_str());
        if (val == NULL)
        {
            return value;
        }
        return std::string(val);
    }


    std::string
    setDefault(const std::string attr, const std::string default_value)
    {
        const char *val = m_hash.lookup(attr.c_str());
        if (val == NULL)
        {
            m_hash.set_submit_param(attr.c_str(), default_value.c_str());
            return default_value;
        }
        return std::string(val);
    }


    void
    setItem(const std::string attr, const std::string value)
    {
        m_hash.set_submit_param(attr.c_str(), value.c_str());
    }


    void
    deleteItem(const std::string attr)
    {
        const char *val = m_hash.lookup(attr.c_str());
        if (val == NULL) {THROW_EX(KeyError, attr.c_str());}
        m_hash.set_submit_param(attr.c_str(), NULL);
    }


    boost::python::list
    keys()
    {
        boost::python::list results;
        HASHITER it = hash_iter_begin(m_hash.macros(), HASHITER_NO_DEFAULTS);
        while (!hash_iter_done(it))
        {
            const char *name = hash_iter_key(it);
            try
            {
                results.append(name);
            }
            catch (boost::python::error_already_set)
            {
                hash_iter_delete(&it);
                throw;
            }
            hash_iter_next(it);
        }
        hash_iter_delete(&it);
        return results;
    }


    boost::python::list iter()
    {
        boost::python::object obj = keys().attr("__iter__")();
        return boost::python::list(obj);
    }


    size_t
    size()
    {
        HASHITER it = hash_iter_begin(m_hash.macros(), HASHITER_NO_DEFAULTS);
        size_t mylen = 0;
        while (!hash_iter_done(it))
        {
            mylen += 1;
            hash_iter_next(it);
        }
        hash_iter_delete(&it);
        return mylen;
    }


    boost::python::list
    items()
    {
        boost::python::list results;
        HASHITER it = hash_iter_begin(m_hash.macros(), HASHITER_NO_DEFAULTS);
        while (!hash_iter_done(it))
        {
            const char *name = hash_iter_key(it);
            const char *val = hash_iter_value(it);
            try
            {
                results.append(boost::python::make_tuple<std::string, std::string>(name, val));
            }
            catch (boost::python::error_already_set)
            {
                hash_iter_delete(&it);
                throw;
            }
            hash_iter_next(it);
        }
        hash_iter_delete(&it);
        return results;
    }


    boost::python::list
    values()
    {
        boost::python::list results;
        HASHITER it = hash_iter_begin(m_hash.macros(), HASHITER_NO_DEFAULTS);
        while (!hash_iter_done(it))
        {
            const char *val = hash_iter_value(it);
            try
            {
                results.append(val);
            }
            catch (boost::python::error_already_set)
            {
                hash_iter_delete(&it);
                throw;
            }
            hash_iter_next(it);
        }
        hash_iter_delete(&it);
        return results;
    }


    void
    update(boost::python::object source)
    {
        if (PyObject_HasAttrString(source.ptr(), "items"))
        {
            return this->update(source.attr("items")());
        }
        if (!PyObject_HasAttrString(source.ptr(), "__iter__")) THROW_EX(ValueError, "Must provide a dictionary-like object to update()");

        boost::python::object iter = source.attr("__iter__")();
        while (true) {
            PyObject *pyobj = PyIter_Next(iter.ptr());
            if (!pyobj) break;
            if (PyErr_Occurred()) {
                boost::python::throw_error_already_set();
            }

            boost::python::object obj = boost::python::object(boost::python::handle<>(pyobj));

            boost::python::tuple tup = boost::python::extract<boost::python::tuple>(obj);
            std::string attr = boost::python::extract<std::string>(tup[0]);
            std::string value = boost::python::extract<std::string>(tup[1]);
            m_hash.set_submit_param(attr.c_str(), value.c_str());
        }
    }


    std::string
    toString() const
    {
        std::stringstream ss;
        HASHITER it = hash_iter_begin(const_cast<Submit *>(this)->m_hash.macros(), HASHITER_NO_DEFAULTS);
        while (!hash_iter_done(it))
        {
            ss << hash_iter_key(it) << " = " << hash_iter_value(it) << "\n";
            hash_iter_next(it);
        }
        ss << "queue";
        hash_iter_delete(&it);
        return ss.str();
    }


    boost::python::object
    toRepr() const
    {
        boost::python::object obj(toString());
        return obj.attr("__repr__")();
    }


    int 
    queue(boost::shared_ptr<ConnectionSentry> txn, int count, boost::python::object ad_results)
    {
        if (!txn.get() || !txn->transaction())
        {
            THROW_EX(RuntimeError, "Job queue attempt without active transaction");
        }

        bool keep_results = false;
        boost::python::extract<boost::python::list> try_ad_results(ad_results);
        if (try_ad_results.check())
        {
            keep_results = true;
        }

		// Before calling init_base_ad(), we should invoke methods to tell
		// the submit code if we want file checks, and to tell the version of the
		// remote schedd.  If for some reason we do not have a the remote
		// schedd version, assume it is running the same version we are (this is
		// the same logic employed by condor_submit).

		m_hash.setDisableFileChecks( param_boolean_crufty("SUBMIT_SKIP_FILECHECKS", true) ? 1 : 0 );
		if ( txn->schedd_version().length() > 0 ) {
			m_hash.setScheddVersion(txn->schedd_version().c_str());
		} else {
			m_hash.setScheddVersion(CondorVersion());
		}

		bool factory_submit = false;
		long long max_materialize = INT_MAX, max_idle = INT_MAX;
		int cluster = txn->clusterId();

		// if this is not the first queue statement of this transaction, (or if we need to get a new cluster)
		// we have some once-per-cluster initialization to do.
		if ( ! m_hash.base_job_was_initialized() || cluster <= 0 || m_hash.getClusterId() != cluster) {

			if (m_hash.init_base_ad(time(NULL), txn->owner().c_str())) {
				process_submit_errstack(m_hash.error_stack());
				THROW_EX(RuntimeError, "Failed to create a cluster ad");
			}
			process_submit_errstack(m_hash.error_stack());

			// check to see if a factory submit was desired, and if the schedd supports it
			//
			if (m_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeLimit,ATTR_JOB_MATERIALIZE_LIMIT, max_materialize, true)) {
				factory_submit = true;
			} else if (m_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeMaxIdle, ATTR_JOB_MATERIALIZE_MAX_IDLE, max_idle, true)) {
				max_materialize = INT_MAX;
				factory_submit = true;
			}
			if (factory_submit) {
				// PRAGMA_REMIND("move this into the schedd object?")
				ClassAd capabilities;
				GetScheddCapabilites(0, capabilities);
				bool allows_late = false;
				if (capabilities.LookupBool("LateMaterialize", allows_late) && allows_late) {
					factory_submit = true;
				} else {
					factory_submit = false; // sorry, no can do.
				}
			}

			cluster = txn->newCluster();
			if (cluster < 0) {
				THROW_EX(RuntimeError, "Failed to create new cluster.");
			}

			if (factory_submit) {
				// turn the submit hash into a submit digest
				SubmitForeachArgs fea;
				std::string submit_digest;
				m_hash.make_digest(submit_digest, cluster, fea.vars, 0);

				// append the queue statement
				// PRAGMA_REMIND("fix this when python submit supports foreach, maybe make this common with condor_submit")
				submit_digest += "\n";
				submit_digest += "Queue ";
				if (fea.queue_num) { formatstr_cat(submit_digest, "%d ", count); }
				submit_digest += "\n";

				// materialize all of the jobs unless the user requests otherwise.
				// (the admin can also set a limit which is applied at the schedd)
				max_materialize = MIN(max_materialize, count);
				max_materialize = MAX(max_materialize, 1);

				// send the submit digest to the schedd. the schedd will parse the digest at this point
				// and return success or failure.
				if (SetJobFactory(cluster, (int)max_materialize, NULL, submit_digest.c_str()) < 0) {
					THROW_EX(RuntimeError, "Failed to send job factory for max_materilize.");
				}
			}
		}

		for (int idx=0; idx<count; idx++)
		{
			int procid = -99;
			if (factory_submit) {
				procid = 0;
			} else {
				condor::ModuleLock ml;
				procid = NewProc(cluster);
			}
			if (procid < 0) {
				THROW_EX(RuntimeError, "Failed to create new proc ID.");
			}

			JOB_ID_KEY jid(cluster, procid);
			ClassAd *proc_ad = m_hash.make_job_ad(jid, 0, idx, false, false, NULL, NULL);
			process_submit_errstack(m_hash.error_stack());
			if (!proc_ad) {
				THROW_EX(RuntimeError, "Failed to create new job ad");
			}

			// before sending procid 0, also send the cluster ad.
			int rval = 0;
			if ((procid == 0) || factory_submit) {
				classad::ClassAd * clusterad = proc_ad->GetChainedParentAd();
				if (clusterad) {
					rval = SendJobAttributes(JOB_ID_KEY(cluster, -1), *clusterad, SetAttribute_NoAck, m_hash.error_stack(), "Submit");
				}
			}
			// send the proc ad
			if ((rval >= 0) && ! factory_submit) {
				rval = SendJobAttributes(jid, *proc_ad, SetAttribute_NoAck, m_hash.error_stack(), "Submit");
			}
			process_submit_errstack(m_hash.error_stack());
			if (rval < 0) {
				THROW_EX(ValueError, "Failed to create send job attributes");
			}

			if (keep_results) {
				boost::shared_ptr<ClassAdWrapper> results_ad(new ClassAdWrapper());
				results_ad->CopyFromChain(*proc_ad);
				ad_results.attr("append")(results_ad);
			}

			// For factory submits, we don't actually loop over the queue number
			// and we want a new cluster (i.e. factory) for each queue statement.
			if (factory_submit) {
				// force re-initialization if this hash is used again.
				m_hash.reset();
				break;
			}
		}


        if (param_boolean("SUBMIT_SEND_RESCHEDULE",true))
        {
            txn->reschedule();
        }
        m_hash.warn_unused(stderr, "Submit object");
        process_submit_errstack(m_hash.error_stack());
        return cluster;
    }

private:
    SubmitHash m_hash;
};


BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(query_overloads, query, 0, 5);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(submit_overloads, submit, 1, 4);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(transaction_overloads, transaction, 0, 2);

void export_schedd()
{
    enum_<JobAction>("JobAction")
        .value("Hold", JA_HOLD_JOBS)
        .value("Release", JA_RELEASE_JOBS)
        .value("Remove", JA_REMOVE_JOBS)
        .value("RemoveX", JA_REMOVE_X_JOBS)
        .value("Vacate", JA_VACATE_JOBS)
        .value("VacateFast", JA_VACATE_FAST_JOBS)
        .value("Suspend", JA_SUSPEND_JOBS)
        .value("Continue", JA_CONTINUE_JOBS)
        ;

    enum_<SetAttributeFlags_t>("TransactionFlags")
        .value("None", 0)
        .value("NonDurable", NONDURABLE)
        .value("SetDirty", SETDIRTY)
        .value("ShouldLog", SHOULDLOG)
        ;

    enum_<CondorQ::QueryFetchOpts>("QueryOpts")
        .value("Default", CondorQ::fetch_Jobs)
        .value("AutoCluster", CondorQ::fetch_DefaultAutoCluster)
        .value("GroupBy", CondorQ::fetch_GroupBy)
        .value("DefaultMyJobsOnly", CondorQ::fetch_MyJobs)
        .value("SummaryOnly", CondorQ::fetch_SummaryOnly)
        ;

    enum_<BlockingMode>("BlockingMode")
        .value("Blocking", Blocking)
        .value("NonBlocking", NonBlocking)
        ;

    class_<ConnectionSentry>("Transaction", "An ongoing transaction in the HTCondor schedd", no_init)
        .def("__enter__", &ConnectionSentry::enter)
        .def("__exit__", &ConnectionSentry::exit)
        ;
    register_ptr_to_python< boost::shared_ptr<ConnectionSentry> >();

#if BOOST_VERSION >= 103400
    boost::python::docstring_options doc_options;
    doc_options.disable_cpp_signatures();
#endif
    class_<Schedd>("Schedd", "A client class for the HTCondor schedd")
        .def(init<const ClassAdWrapper &>(":param ad: An ad containing the location of the schedd"))
        .def("query", &Schedd::query, query_overloads("Query the HTCondor schedd for jobs.\n"
            ":param constraint: An optional constraint for filtering out jobs; defaults to 'true'\n"
            ":param attr_list: A list of attributes for the schedd to project along.  Defaults to having the schedd return all attributes.\n"
            ":param callback: A callback function to be invoked for each ad; the return value (if not None) is added to the list.\n"
            ":param limit: A limit on the number of matches to return.\n"
            ":param opts: Any one of the QueryOpts enum.\n"
            ":return: A list of matching jobs, containing the requested attributes.",
#if BOOST_VERSION < 103400
            (boost::python::arg("constraint")="true", boost::python::arg("attr_list")=boost::python::list(), boost::python::arg("callback")=boost::python::object(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs)
#else
            (boost::python::arg("self"), boost::python::arg("constraint")="true", boost::python::arg("attr_list")=boost::python::list(), boost::python::arg("callback")=boost::python::object(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs)
#endif
            ))
        .def("act", &Schedd::actOnJobs2)
        .def("act", &Schedd::actOnJobs, "Change status of job(s) in the schedd.\n"
            ":param action: Action to perform; must be from enum JobAction.\n"
            ":param job_spec: Job specification; can either be a list of job IDs or a string specifying a constraint to match jobs.\n"
            ":return: Number of jobs changed.")
        .def("submit", &Schedd::submit, submit_overloads("Submit one or more jobs to the HTCondor schedd.\n"
            ":param ad: ClassAd describing job cluster.\n"
            ":param count: Number of jobs to submit to cluster.\n"
            ":param spool: Set to true to spool files separately.\n"
            ":param ad_results: If set to a list, the resulting ClassAds will be added to the list post-submit.\n"
#if BOOST_VERSION < 103400
            ":return: Newly created cluster ID.", (boost::python::arg("ad"), boost::python::arg("count")=1, boost::python::arg("spool")=false, boost::python::arg("ad_results")=boost::python::list())))
#else
            ":return: Newly created cluster ID.", (boost::python::arg("self"), "ad", boost::python::arg("count")=1, boost::python::arg("spool")=false, boost::python::arg("ad_results")=boost::python::list())))
#endif
        .def("submitMany", &Schedd::submitMany, "Submit one or more jobs to the HTCondor schedd.\n"
             ":param cluster_ad: ClassAd describing the job cluster.  All jobs inherit from this base ad.\n"
             ":param proc_ads: A list of 2-tuples.  The tuples have the format (proc_ad, count).  This will result in 'count' jobs being submitted, inheriting from the proc_ad and cluster_ad.\n"
             ":param spool: Set to true to spool files separately.\n"
             ":param ad_results: A list object; the resulting job ads will be appended to this list.\n"
             ":return: Newly created cluster ID.", (
#if BOOST_VERSION >= 103400
             boost::python::arg("self"),
#endif
             boost::python::arg("cluster_ad"), boost::python::arg("proc_ads"), boost::python::arg("spool")=false, boost::python::arg("ad_results")=boost::python::list()))
        .def("spool", &Schedd::spool, "Spool a list of given ads to the remote HTCondor schedd.\n"
            ":param ads: A python list containing one or more ads to spool.\n")
        .def("transaction", &Schedd::transaction, transaction_overloads("Start a transaction with the schedd.\n"
            ":param flags: Transaction flags from the htcondor.TransactionFlags enum.\n"
            ":param continue_txn: Defaults to false; set to true to extend an ongoing transaction if present.  Otherwise, starting a new transaction while one is ongoing is an error.\n"
#if BOOST_VERSION < 103400
            ":return: Transaction context manager.\n", (boost::python::arg("flags")=0, boost::python::arg("continue_txn")=false))[boost::python::with_custodian_and_ward_postcall<1, 0>()])
#else
            ":return: Transaction context manager.\n", (boost::python::arg("self"), boost::python::arg("flags")=0, boost::python::arg("continue_txn")=false))[boost::python::with_custodian_and_ward_postcall<1, 0>()])
#endif
        .def("retrieve", &Schedd::retrieve, "Retrieve the output sandbox from one or more jobs.\n"
            ":param jobs: A expression string matching the list of job output sandboxes to retrieve.\n")
        .def("edit", &Schedd::edit, "Edit one or more jobs in the queue.\n"
            ":param job_spec: Either a list of jobs (CLUSTER.PROC) or a string containing a constraint to match jobs against.\n"
            ":param attr: Attribute name to edit.\n"
            ":param value: The new value of the job attribute; should be a string (which will be converted to a ClassAds expression) or a ClassAds expression.")
        .def("reschedule", &Schedd::reschedule, "Send reschedule command to the schedd.\n")
        .def("history", &Schedd::history, "Request records from schedd's history\n"
            ":param requirements: Either a ExprTree or a string that can be parsed as an expression; requirements all returned jobs should match.\n"
            ":param projection: The attributes to return; an empty list signifies all attributes.\n"
            ":param match: Number of matches to return.\n"
            ":param since: optional job id or expression that will signal the end of records to return; the job that matches this will not be returned.\n"
            ":return: An iterator for the matching job ads",
#if BOOST_VERSION >= 103400
             (boost::python::arg("self"),
#endif
             boost::python::arg("requirements"), boost::python::arg("projection"), boost::python::arg("match")=-1,
             boost::python::arg("since")=boost::python::object())
            )
        .def("refreshGSIProxy", &Schedd::refreshGSIProxy, "Refresh the GSI proxy for a given job\n"
            ":param cluster: Job cluster.\n"
            ":param proc: Job proc.\n"
            ":param filename: Filename of proxy to delegate or upload to job.\n"
            ":param lifetime: Desired lifetime (in seconds) of delegated proxy; 0 indicates to not shorten"
            " the proxy lifetime.  -1 indicates to use the value of parameter DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME."
            " NOTE: depending on the lifetime of the proxy in `filename`, the resulting lifetime may be shorter"
            " than the desired lifetime.\n"
            ":return: Lifetime of the resulting job proxy in seconds.")
        .def("xquery", &Schedd::xquery, "Query HTCondor schedd, returning an iterator.\n"
            ":param requirements: Either a ExprTree or a string that can be parsed as an expression; requirements all returned jobs should match.\n"
            ":param projection: The attributes to return; an empty list signifies all attributes.\n"
            ":param limit: A limit on the number of matches to return.\n"
            ":param opts: Any one of the QueryOpts enum.\n"
            ":param name: A name to identify the query (defaults to the schedd name).\n"
            ":return: An iterator for the matching job ads",
#if BOOST_VERSION < 103400
            (boost::python::arg("requirements") = "true", boost::python::arg("projection")=boost::python::list(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs, boost::python::arg("name")=boost::python::object())
#else
            (boost::python::arg("self"), boost::python::arg("requirements") = "true", boost::python::arg("projection")=boost::python::list(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs, boost::python::arg("name")=boost::python::object())
#endif
            )
        .def("negotiate", &Schedd::negotiate, boost::python::with_custodian_and_ward_postcall<1, 0>(),
            "Start negotiation session with a remote schedd.\n"
            ":param owner: The owner of the resource requests.\n"
            ":param ad: Additional information for the negotiation ad.\n"
            ":return: An iterator for the resource requests",
#if BOOST_VERSION < 103400
            (boost::python::arg("owner"), boost::python::arg("ad")=boost::python::object)
#else
            (boost::python::arg("self"), boost::python::arg("owner"), boost::python::arg("ad")=boost::python::dict())
#endif
            )
        ;

    class_<ScheddNegotiate>("ScheddNegotiate", no_init)
        .def("__iter__", &ScheddNegotiate::getRequests, "Get resource requests from schedd.", boost::python::with_custodian_and_ward_postcall<1, 0>())
        .def("sendClaim", &ScheddNegotiate::sendClaim, "Send a claim to the schedd.\n"
          ":param claim: A string containing the claim ID.\n"
          ":param offer: A ClassAd object containing a description of the resource claimed (the machine's ClassAd).\n"
          ":param request: A ClassAd object corresponding to the schedd resource request (optional).",
#if BOOST_VERSION < 103400
          (boost::python::arg("claim"), boost::python::arg("offer"), boost::python::arg("request")=boost::python::dict())
#else
          (boost::python::arg("self"), boost::python::arg("claim"), boost::python::arg("offer"), boost::python::arg("request")=boost::python::dict())
#endif
          )
        .def("__enter__", &ScheddNegotiate::enter)
        .def("__exit__", &ScheddNegotiate::exit)
        .def("disconnect", &ScheddNegotiate::disconnect, "Disconnect from negotiation session.");
        ;

    class_<Submit>("Submit")
        .def(init<boost::python::dict>())
        //.def_pickle(submit_pickle_suite())
        .def("expand", &Submit::expand, "Expand all macros for a given attribute")
        .def("queue", &Submit::queue, "Submit the current object to the remote queue\n"
             ":param txn: An active transaction object\n"
             ":return: Cluster ID of submitted job(s).  Throws a RuntimeError if the submission fails\n",
             (boost::python::arg("self"), boost::python::arg("txn"), boost::python::arg("count")=1, boost::python::arg("ad_results")=boost::python::object())
            )
        .def("__delitem__", &Submit::deleteItem)
        .def("__getitem__", &Submit::getItem)
        .def("__setitem__", &Submit::setItem)
        .def("__str__", &Submit::toString)
        .def("__repr__", &Submit::toRepr)
        .def("__iter__", &Submit::iter)
        .def("keys", &Submit::keys)
        .def("values", &Submit::values)
        .def("items", &Submit::items)
        .def("__len__", &Submit::size)
        .def("get", &Submit::get, "Retrieve a value from the submit description",
             (boost::python::arg("self"), boost::python::arg("default")=boost::python::object())
            )
        .def("setdefault", &Submit::setDefault, "Set a default value for a command")
        .def("update", &Submit::update, "Copy the contents of a given Submit object into the current object")
        ;

    class_<RequestIterator>("ResourceRequestIterator", no_init)
        .def(NEXT_FN, &RequestIterator::next, "Get next resource request.")
        ;

    class_<HistoryIterator>("HistoryIterator", no_init)
        .def(NEXT_FN, &HistoryIterator::next)
        .def("__iter__", &HistoryIterator::pass_through)
        ;

    class_<QueryIterator>("QueryIterator", no_init)
        .def(NEXT_FN, &QueryIterator::next, "Return the next ad from the query results.\n"
            ":param mode: One of the BlockingMode enum; Blocking or NonBlocking.\n"
            ":return: The next ad in the query results.  If NonBlocking mode is used, returns None if no ad is available..\n"
            "Throws a StopIterator exception if no results are available..\n",
#if BOOST_VERSION < 103400
            (boost::python::arg("mode") = Blocking)
#else
            (boost::python::arg("self"), boost::python::arg("mode") = Blocking)
#endif
            )
        .def("nextAdsNonBlocking", &QueryIterator::nextAds, "Return any available ads.\n"
            ":return: A list of ads.  If no ads are available, returns an empty list.\n"
            "Does not throw an exception if no ads are available or the iterator is finished.\n"
            )
        .def("tag", &QueryIterator::tag, "Return the query's tag.")
        .def("done", &QueryIterator::done, "Returns True if the iterator is finished; False otherwise.")
        .def("watch", &QueryIterator::watch, "Returns a file descriptor associated with this query.")
        .def("__iter__", &QueryIterator::pass_through)
        ;

    register_ptr_to_python< boost::shared_ptr<ScheddNegotiate> >();
    register_ptr_to_python< boost::shared_ptr<RequestIterator> >();
    register_ptr_to_python< boost::shared_ptr<HistoryIterator> >();
    register_ptr_to_python< boost::shared_ptr<QueryIterator> >();

}
