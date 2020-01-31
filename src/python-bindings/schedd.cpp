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
#include "../condor_utils/dagman_utils.h"

#include <classad/operators.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/version.hpp>
#include <boost/python/raw_function.hpp>

#include "old_boost.h"
#include "classad_wrapper.h"
#include "exprtree_wrapper.h"
#include "module_lock.h"
#include "query_iterator.h"
#include "submit_utils.h"
#include "condor_arglist.h"
#include "my_popen.h"

#include <algorithm>
#include <string>

using namespace boost::python;

#ifndef ATTR_REQUEST_GPUS
#  define ATTR_REQUEST_GPUS "RequestGPUs"
#endif

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
make_requirements(const ClassAd &jobAd, ExprTree *reqs, ShouldTransferFiles_t stf)
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

	if (jobAd.Lookup(ATTR_REQUEST_DISK)) {
		ADD_REQUIREMENT(REQUEST_DISK, "TARGET.Disk >= " ATTR_REQUEST_DISK);
	}

	if (jobAd.Lookup(ATTR_REQUEST_MEMORY)) {
		ADD_REQUIREMENT(REQUEST_MEMORY, "TARGET.Memory >= " ATTR_REQUEST_MEMORY);
	}

	if (jobAd.Lookup(ATTR_REQUEST_CPUS)) {
		ADD_REQUIREMENT(REQUEST_CPUS, "TARGET.Cpus >= " ATTR_REQUEST_CPUS);
	}

	if (jobAd.Lookup(ATTR_REQUEST_GPUS)) {
		ADD_REQUIREMENT(REQUEST_GPUS, "TARGET.Gpus >= " ATTR_REQUEST_GPUS);
	}

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

struct QueueItemsIterator {
	QueueItemsIterator() : num_rows(0) {}
	~QueueItemsIterator() { m_fea.clear(); }

	inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

	void init(SubmitHash & h, const char * qargs) {
		num_rows  = 0;
		m_fea.clear();
		if (qargs) {
			std::string errmsg;
			if (h.parse_q_args(qargs, m_fea, errmsg) != 0) { THROW_EX(RuntimeError, errmsg.c_str()); }
		}
	}

	bool needs_submit_lines() { return m_fea.items_filename == "<"; }

	boost::python::object next()
	{
		auto_free_ptr line(m_fea.items.pop());
		if ( ! line) { THROW_EX(StopIteration, "All items returned"); }

		if (m_fea.vars.number() > 1 || (m_fea.vars.number()==1 && (YourStringNoCase("Item") != m_fea.vars.first()))) {
			std::vector<const char*> splits;
			m_fea.split_item(line.ptr(), splits);

			boost::python::dict values;
			int ix = 0;
			for (const char * key = m_fea.vars.first(); key != NULL; key = m_fea.vars.next()) {
				values[boost::python::object(std::string(key))] = boost::python::object(std::string(splits[ix++]));
			}

			return boost::python::object(values);
		} else {
			return boost::python::object(std::string(line.ptr()));
		}
	}

	char * next_row()
	{
		char * row = m_fea.items.pop();
		if (row) { ++num_rows; }
		return row;
	}

	int row_count() { return num_rows; }

	int load_items(SubmitHash & h, MacroStreamMemoryFile &ms)
	{
		std::string errmsg;
		int rval = h.load_inline_q_foreach_items(ms, m_fea, errmsg);
		if (rval == 1) { // 1 means forech data is external
			rval = h.load_external_q_foreach_items(m_fea, false, errmsg);
		}
		if (rval < 0) { THROW_EX(RuntimeError, errmsg.c_str());}
		return 0;
	}

	friend struct SubmitStepFromQArgs;
private:
	int num_rows;
	SubmitForeachArgs m_fea;
};

struct SubmitResult {

	SubmitResult(JOB_ID_KEY id, int num_jobs, const classad::ClassAd * clusterAd)
		: m_id(id)
		, m_num(num_jobs)
	{
		if (clusterAd) m_ad.Update(*clusterAd);
	}

	int cluster() { return m_id.cluster; }
	int first_procid() { return m_id.proc; }
	int num_procs() { return m_num; }
	boost::shared_ptr<ClassAdWrapper> clusterad() {
		//PRAGMA_REMIND("does the ref counting work if our m_ad is actually a shared_ptr<ClassAdWrapper> ?")
		boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
		ad->Update(m_ad);
		return ad;
	}

	std::string toString() const
	{
		std::string str;
		formatstr(str, "Submitted %d jobs into cluster %d,%d :\n", m_num, m_id.cluster, m_id.proc);
		classad::References attrs;
		sGetAdAttrs(attrs, m_ad, false, NULL);
		sPrintAdAttrs(str, m_ad, attrs);
		return str;
	}

private:
	classad::ClassAd m_ad;
	JOB_ID_KEY m_id;
	int m_num;
};


// Helper class wrapping a python iterator that manages the iterator and the count/step
// and populates the resulting key/value pairs into into the given SubmitHash as 'live' values
// This class is not meant to be directly callable from Python, rather it provides common
// code used by the Submit class and by the SubmitJobsIterator class
struct SubmitStepFromPyIter {

	SubmitStepFromPyIter(SubmitHash & h, const JOB_ID_KEY & id, int num, boost::python::object from)
		: m_hash(h)
		, m_jidInit(id.cluster, id.proc)
		, m_items(NULL)
		, m_nextProcId(id.proc)
		, m_done(false)
	{
		if (num > 0) { m_fea.queue_num = num; }

		// get an interator for the foreach data. this grabs a refcount
		if (PyIter_Check(from.ptr())) {
			m_items = PyObject_GetIter(from.ptr());
		}
	}

	~SubmitStepFromPyIter() {
		// release the iterator refcount (if any)
		Py_XDECREF(m_items);

		// disconnnect the hashtable from our livevars pointers
		unset_live_vars();
	}

	bool done() { return m_done; }
	bool has_items() { return m_items; }
	int  step_size() { return m_fea.queue_num ? m_fea.queue_num : 1; }
	const char * errmsg() { if ( ! m_errmsg.empty()) { return m_errmsg.c_str(); } return NULL;}

	// returns < 0 on error
	// returns 0 if done iterating
	// returns 2 for first iteration
	// returns 1 for subsequent iterations
	int next(JOB_ID_KEY & jid, int & item_index, int & step)
	{
		if (m_done) return 0;

		int step_size = m_fea.queue_num ? m_fea.queue_num : 1;
		int iter_index = (m_nextProcId - m_jidInit.proc);

		jid.cluster = m_jidInit.cluster;
		jid.proc = m_nextProcId;
		item_index = iter_index / step_size;
		step = iter_index % step_size;

		if (0 == step) { // have we started a new row?
			if (m_items) {
				int rval = next_rowdata();
				if (rval <= 0) {
					// no more row data, we are done
					m_done = (rval == 0);
					return rval;
				}
				set_live_vars();
			} else {
				if (0 == iter_index) {
					// if no next row, then we are done iterating, unless it is the FIRST iteration
					// in which case we want to pretend there is a single empty item called "Item"
					m_hash.set_live_submit_variable("Item", "", true);
				} else {
					m_done = true;
					return 0;
				}
			}
		}

		++m_nextProcId;
		return (0 == iter_index) ? 2 : 1;
	}

	StringList & vars() { return m_fea.vars; }
	SubmitForeachArgs & fea() { return m_fea; }

	//
	void set_live_vars()
	{
		for (const char * key = m_fea.vars.first(); key != NULL; key = m_fea.vars.next()) {
			auto str = m_livevars.find(key);
			if (str != m_livevars.end()) {
				m_hash.set_live_submit_variable(key, str->second.c_str(), false);
			} else {
				m_hash.unset_live_submit_variable(key);
			}
		}
	}

	void unset_live_vars()
	{
		// set the pointers of the 'live' variables to the unset string (i.e. "")
		for (const char * key = m_fea.vars.first(); key != NULL; key = m_fea.vars.next()) {
			m_hash.unset_live_submit_variable(key);
		}
	}

	// load the livevars array from the next item of the iterator
	// returns < 0 on error, 0 if there is no next item, 1 on success.
	int next_rowdata()
	{
		PyObject *obj = PyIter_Next(m_items);
		if ( ! obj) {
			return 0;
		}

		bool no_vars_yet = m_fea.vars.number() == 0;

		// load the next row item
		if (PyDict_Check(obj)) {
			PyObject *k, *v;
			Py_ssize_t pos = 0;
			while (PyDict_Next(obj, &pos, &k, &v)) {
				std::string key = extract<std::string>(k);
				if (key[0] == '+') { key.replace(0, 1, "MY."); }
				m_livevars[key] = extract<std::string>(v);
				if (no_vars_yet) { m_fea.vars.append(key.c_str()); }
			}
		} else if (PyList_Check(obj)) {
			// use the key names that have been stored in m_fea.vars
			// and the items from the list, which must be strings...
			// if there are no keys, then make some up.
			Py_ssize_t num = PyList_Size(obj);
			if (no_vars_yet) {
				if (num > 10) { THROW_EX(ValueError, "Too many items in Queue itemdata element"); }
				// no vars have been specified, so make some up based on the number if items in the list
				std::string key("Item");
				for (Py_ssize_t ix = 0; ix < num; ++ix) {
					m_fea.vars.append(key.c_str());
					formatstr(key, "Item%d", (int)ix+1);
				}
			}
			const char * key = m_fea.vars.first();
			for (Py_ssize_t ix = 0; ix < num; ++ix) {
				PyObject * v = PyList_GetItem(obj, ix);
				m_livevars[key] = extract<std::string>(v);
				key = m_fea.vars.next();
				if ( ! key) break;
			}
		} else {
			// not a list or a dict, the item must be a string.
			extract<std::string> item_extract(obj);
			if ( ! item_extract.check()) {
				m_errmsg = "'from' data must be an iterator of strings or of dicts";
				return -1;
			}

			// if there are NO vars, then create a single Item var and store the whole string
			// if there are vars, then split the string in the same way that the QUEUE statement would
			if (no_vars_yet) {
				const char * key = "Item";
				m_fea.vars.append(key);
				m_livevars[key] = item_extract();
			} else {
				std::string str = item_extract();;
				auto_free_ptr data(strdup(str.c_str()));

				std::vector<const char*> splits;
				m_fea.split_item(data.ptr(), splits);
				int ix = 0;
				for (const char * key = m_fea.vars.first(); key != NULL; key = m_fea.vars.next()) {
					m_livevars[key] = splits[ix++];
				}
			}
		}

		Py_DECREF(obj);
		return 1;
	}

	// return all of the live value data as a single 'line' using the given item separator and line terminator
	int get_rowdata(std::string & line, const char * sep, const char * eol)
	{
		// so that the separator and line terminators can be \0, we make the size strlen()
		// unless the first character is \0, then the size is 1
		int cchSep = sep ? (sep[0] ? strlen(sep) : 1) : 0;
		int cchEol = eol ? (eol[0] ? strlen(eol) : 1) : 0;
		line.clear();
		for (const char * key = m_fea.vars.first(); key != NULL; key = m_fea.vars.next()) {
			if ( ! line.empty() && sep) line.append(sep, cchSep);
			auto str = m_livevars.find(key);
			if (str != m_livevars.end() && ! str->second.empty()) {
				line += str->second;
			}
		}
		if (eol && ! line.empty()) line.append(eol, cchEol);
		return (int)line.size();
	}

	// this is called repeatedly when we are sending rowdata to the schedd
	static int send_row(void* pv, std::string & rowdata) {
		SubmitStepFromPyIter *sii = (SubmitStepFromPyIter*)pv;

		rowdata.clear();
		if (sii->done())
			return 0;

		// Split and write into the string using US (0x1f) a field separator and LF as record terminator
		if ( ! sii->get_rowdata(rowdata, "\x1F", "\n"))
			return 0;

		int rval = sii->next_rowdata();
		if (rval < 0) { return rval; }
		if (rval == 0) { sii->m_done = true; } // so subsequent iterations will return 0
		return 1;
	}

protected:

	SubmitHash & m_hash;         // the (externally owned) submit hash we are updating as we iterate
	JOB_ID_KEY m_jidInit;
	PyObject * m_items;
	SubmitForeachArgs m_fea;
	NOCASE_STRING_MAP m_livevars; // holds live data for active vars
	int  m_nextProcId;
	bool m_done;
	std::string m_errmsg;
};


struct SubmitJobsIterator {

	SubmitJobsIterator(SubmitHash & h, bool procs, const JOB_ID_KEY & id, int num, boost::python::object from, time_t qdate, const std::string & owner)
		: m_sspi(m_hash, id, num, from)
		, m_ssqa(m_hash)
		, m_iter_qargs(false)
		, m_return_proc_ads(procs)
	{
			// copy the input submit hash into our new hash.
			m_hash.init();
			copy_hash(h);
			m_hash.setDisableFileChecks(true);
			m_hash.init_base_ad(qdate, owner.c_str());
	}

	SubmitJobsIterator(SubmitHash & h, bool procs, const JOB_ID_KEY & id, int num, const std::string & qargs, MacroStreamMemoryFile & ms_inline_items, time_t qdate, const std::string & owner)
		: m_sspi(m_hash, id, 0, boost::python::object())
		, m_ssqa(m_hash)
		, m_iter_qargs(true)
		, m_return_proc_ads(procs)
	{
		// copy the input submit hash into our new hash.
		m_hash.init();
		copy_hash(h);
		m_hash.setDisableFileChecks(true);
		m_hash.init_base_ad(qdate, owner.c_str());

		if (qargs.empty()) {
			m_ssqa.begin(id, num);
		} else {
			std::string errmsg;
			if (m_ssqa.begin(id, qargs.c_str()) != 0) { THROW_EX(RuntimeError, "Invalid queue arguments"); }
			else {
				size_t ix; int line;
				ms_inline_items.save_pos(ix, line);
				int rv = m_ssqa.load_items(ms_inline_items, false, errmsg);
				ms_inline_items.rewind_to(ix, line);
				if (rv != 0) { THROW_EX(RuntimeError, errmsg.c_str()); }
			}
		}
	}


	~SubmitJobsIterator() {}

	void copy_hash(SubmitHash & h)
	{
		//PRAGMA_REMIND("this will loose meta info, should have a MACRO_SET copy operation instead.")
		HASHITER it = hash_iter_begin(h.macros(), HASHITER_NO_DEFAULTS);
		for ( ; ! hash_iter_done(it); hash_iter_next(it)) {
			const char * key = hash_iter_key(it);
			const char * val = hash_iter_value(it);
			m_hash.set_submit_param(key, val);
		}
		hash_iter_delete(&it);

		const char * ver = h.getScheddVersion();
		if ( ! ver || ! ver[0]) ver = CondorVersion();

		m_hash.setScheddVersion(ver);
	}

	inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

	boost::shared_ptr<ClassAdWrapper> next()
	{
		JOB_ID_KEY jid;
		int item_index;
		int step;
		int rval;

		if (m_iter_qargs) {
			if (m_ssqa.done()) { THROW_EX(StopIteration, "All ads processed"); }
			rval = m_ssqa.next(jid, item_index, step);
		} else {
			if (m_sspi.done()) { THROW_EX(StopIteration, "All ads processed"); }
			rval = m_sspi.next(jid, item_index, step);
			if (rval < 0) { THROW_EX(RuntimeError, m_sspi.errmsg()); }
		}
		if (rval == 0)  { THROW_EX(StopIteration, "All ads processed"); }

		// on first iteration, if the initial proc id is not 0, then we need to make sure
		// to call make_job_ad with a proc id of 0 to force the cluster ad to be created.
		ClassAd * job = NULL;
		if (rval == 2 && jid.proc > 0) {
			JOB_ID_KEY cid(jid.cluster, 0);
			job = m_hash.make_job_ad(cid, item_index, step, false, false, NULL, NULL);
			if (job) {
				job = m_hash.make_job_ad(jid, item_index, step, false, false, NULL, NULL);
			}
		} else {
			job = m_hash.make_job_ad(jid, item_index, step, false, false, NULL, NULL);
		}

		if ( ! job) {
			THROW_EX(RuntimeError, "Failed to get next job");
		}

		boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
		if (m_return_proc_ads) {
			if (rval == 2) { // this is the cluster iteration
				ad->UpdateFromChain(*job);
			} else {
				// update just from the procad
				ad->Update(*job);
			}
		} else {
			ad->UpdateFromChain(*job);
		}
		return ad;
	}

	boost::shared_ptr<ClassAdWrapper> clusterad()
	{
		ClassAd* cad = m_hash.get_cluster_ad();
		if ( ! cad) {
			THROW_EX(RuntimeError, "No cluster ad");
		}
		boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
		ad->Update(*cad);
		return ad;
	}

private:
	SubmitHash m_hash;
	SubmitStepFromPyIter m_sspi;
	SubmitStepFromQArgs m_ssqa;
	bool m_iter_qargs;
	bool m_return_proc_ads;
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
    int  procId() const {return m_proc_id;}
    int  newCluster();
    int  newProc();
    void reschedule();
    std::string owner() const;
    std::string schedd_version();
    const ClassAd* capabilites();

    static boost::shared_ptr<ConnectionSentry> enter(boost::shared_ptr<ConnectionSentry> obj);
    static bool exit(boost::shared_ptr<ConnectionSentry> mgr, boost::python::object obj1, boost::python::object obj2, boost::python::object obj3);

private:
    bool m_connected;
    bool m_transaction;
    bool m_queried_capabilities;
    int  m_cluster_id; // non-zero when there is a submit transaction and newCluster has been called already
    int  m_proc_id; // the last value returned from newProc
    SetAttributeFlags_t m_flags;
    Schedd& m_schedd;
    ClassAd m_capabilities;	// populated via the GetScheddCapabilities
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

    compat_classad::CopyAttribute(ATTR_REMOTE_GROUP, offer_ad, ATTR_SUBMITTER_GROUP, request_ad);
    compat_classad::CopyAttribute(ATTR_REMOTE_NEGOTIATING_GROUP, offer_ad, ATTR_SUBMITTER_NEGOTIATING_GROUP, request_ad);
    compat_classad::CopyAttribute(ATTR_REMOTE_AUTOREGROUP, offer_ad, ATTR_SUBMITTER_AUTOREGROUP, request_ad);
    compat_classad::CopyAttribute(ATTR_RESOURCE_REQUEST_CLUSTER, offer_ad, ATTR_CLUSTER_ID, request_ad);
    compat_classad::CopyAttribute(ATTR_RESOURCE_REQUEST_PROC, offer_ad, ATTR_PROC_ID, request_ad);

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
    catch (boost::python::error_already_set &) {}
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
        catch (boost::python::error_already_set &)
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
    catch (boost::python::error_already_set &)
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
        ExprTree *new_reqs = make_requirements(cluster_ad, old_reqs, should).release();
        cluster_ad.Insert(ATTR_REQUIREMENTS, new_reqs);

        if (spool)
        {
            make_spool(cluster_ad);
        }

        // Set all the cluster attributes
        classad::ClassAdUnParser unparser;
        unparser.SetOldClassAd(true, true);
        std::string rhs, failed_attr;
        int setattr_result = 0;

        { // get module lock so we can call SetAttribute
            condor::ModuleLock ml;
            for (classad::ClassAd::const_iterator it = cluster_ad.begin(); it != cluster_ad.end(); it++) {
                rhs.clear();
                unparser.Unparse(rhs, it->second);
                setattr_result = SetAttribute(cluster, -1, it->first.c_str(), rhs.c_str(), SetAttribute_NoAck);
                if (-1 == setattr_result) {
                    failed_attr = it->first;
                    break;
                }
            }
        } // release module lock

        // report SetAttribute errors
        if (setattr_result == -1) {
            THROW_EX(ValueError, failed_attr.c_str());
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

            ExprTree *new_reqs = make_requirements(proc_ad, old_reqs, should).release();
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
            unparser.SetOldClassAd( true, true );
            int setattr_result = 0;
            std::string failed_attr;
            std::string rhs;

            { // take module lock (and release GIL)
                condor::ModuleLock ml;
                for (classad::ClassAd::const_iterator it = proc_ad.begin(); it != proc_ad.end(); it++) {
                    rhs.clear();
                    unparser.Unparse(rhs, it->second);
                    setattr_result = SetAttribute(cluster, procid, it->first.c_str(), rhs.c_str(), SetAttribute_NoAck);
                    if (setattr_result == -1) {
                        failed_attr = it->first;
                        break;
                    }
                }
            } // release module lock

            // report any errors of SetAttribute
            if (-1 == setattr_result) {
                PyErr_SetString(PyExc_ValueError, failed_attr.c_str());
                throw_error_already_set();
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
                result = -1 == SetAttribute(clusters[idx], procs[idx], attr.c_str(), val_str.c_str(), SetAttribute_NoAck);
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
            result = -1 == SetAttributeByConstraint(constraint.c_str(), attr.c_str(), val_str.c_str(), SetAttribute_NoAck);
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
     : m_connected(false), m_transaction(false), m_queried_capabilities(false), m_cluster_id(0), m_proc_id(-1), m_flags(flags), m_schedd(schedd)
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

const ClassAd* ConnectionSentry::capabilites()
{
	if ( ! m_queried_capabilities) {
		condor::ModuleLock ml;
		GetScheddCapabilites(0, m_capabilities);
		m_queried_capabilities = true;
	}
	if (m_queried_capabilities) {
		return &m_capabilities;
	}
	return NULL;
}

int
ConnectionSentry::newCluster()
{
    condor::ModuleLock ml;
    m_cluster_id = NewCluster();
    m_proc_id = -1; // we have not yet called newProc for this cluster
    return m_cluster_id;
}

int
ConnectionSentry::newProc()
{
    condor::ModuleLock ml;
    m_proc_id = NewProc(m_cluster_id);
    return m_proc_id;
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

void SetDagOptions(boost::python::dict opts, SubmitDagShallowOptions &shallow_opts, SubmitDagDeepOptions &deep_opts)
{
    // Start by setting some default options. These must be set for everything to work correctly.
    // Some of these might be changed later if different values are specified in opts.
    shallow_opts.strSubFile = shallow_opts.primaryDagFile + ".condor.sub";
    shallow_opts.strSchedLog = shallow_opts.primaryDagFile + ".nodes.log";
    shallow_opts.strLibOut = shallow_opts.primaryDagFile + ".lib.out";
    shallow_opts.strLibErr = shallow_opts.primaryDagFile + ".lib.err";
    deep_opts.doRescueFrom = 0;
    deep_opts.updateSubmit = false;

    // Iterate over the list of arguments passed in and set the appropriate
    // values in m_shallowOpts and m_deepOpts
    boost::python::object iter = opts.attr("__iter__")();
    while (true) {
        PyObject *pyobj = PyIter_Next(iter.ptr());
        if (!pyobj) break;
        if (PyErr_Occurred()) {
            boost::python::throw_error_already_set();
        }

        // Wrestle the key-value pair out of the dict object and save them
        // both as string objects. 
        // We can assume the key is a string type, but the the value can be 
        // a string or an int (or other?)
        std::string key, value;
        boost::python::object key_obj = boost::python::object(boost::python::handle<>(pyobj));
        key = boost::python::extract<std::string>(key_obj);
        boost::python::object value_obj = boost::python::extract<boost::python::object>(opts[key]);
        std::string value_type = boost::python::extract<std::string>(value_obj.attr("__class__").attr("__name__"));
        if(value_type == "str") {
            value = boost::python::extract<std::string>(opts[key]);
        }
        else if(value_type == "int") {
            int value_int = boost::python::extract<int>(opts[key]);
            value = std::to_string(value_int);
        }

        // Set shallowOpts or deepOpts variables as appropriate
        std::string key_lc = key;
        std::transform(key_lc.begin(), key_lc.end(), key_lc.begin(), ::tolower);
        if (key_lc == "maxidle") 
            shallow_opts.iMaxIdle = atoi(value.c_str());
        else if (key_lc == "maxjobs") 
            shallow_opts.iMaxJobs = atoi(value.c_str());
        else if (key_lc == "maxpre")
            shallow_opts.iMaxPre = atoi(value.c_str());
        else if (key_lc == "maxpost")
            shallow_opts.iMaxPre = atoi(value.c_str());
        else if (key_lc == "autorescue")
            deep_opts.autoRescue = atoi(value.c_str()) != 0;
        else if (key_lc == "dorescuefrom")
            deep_opts.doRescueFrom = atoi(value.c_str());
        else if (key_lc == "force")
            deep_opts.bForce = atoi(value.c_str()) == 1;
        else
            printf("WARNING: DAGMan option '%s' not recognized, skipping\n", key.c_str());
    }
}

struct Submit
{
	static MACRO_SOURCE EmptyMacroSrc;
public:
    Submit()
       : m_src_pystring(EmptyMacroSrc),
		 m_ms_inline("", 0, EmptyMacroSrc)
       , m_queue_may_append_to_cluster(false)
    {
        m_hash.init();
    }


    Submit(boost::python::dict input)
       : m_src_pystring(EmptyMacroSrc),
         m_ms_inline("", 0, EmptyMacroSrc)
       , m_queue_may_append_to_cluster(false)
    {
        m_hash.init();
        update(input);
    }

    static
    boost::python::object
    rawInit(boost::python::tuple args, boost::python::dict kwargs) {
        boost::python::object self = args[0];
        if (py_len(args) > 2) {
            THROW_EX(TypeError, "Keyword constructor cannot take more than one positional argument");
        } else if (py_len(args) == 1) {
            return self.attr("__init__")(kwargs);
        } else {
            // Can it be converted to a dictionary?  If so, we use that dictionary.
            // Otherwise, we convert it to a string.
            try {
                boost::python::dict input(args[1]);
                self.attr("__init__")(input);
                self.attr("update")(kwargs);
                return boost::python::object();
            } catch (boost::python::error_already_set &) {
                if (PyErr_ExceptionMatches(PyExc_ValueError)) {
                    PyErr_Clear();
                    boost::python::str input_str(args[1]);
                    self.attr("__init__")(input_str);
                    self.attr("update")(kwargs);
                    return boost::python::object();
                } else {
                    throw;
                }
            }
			// UNREACHABLE
            //return boost::python::object();
        }
    }


	Submit(const std::string lines)
       : m_src_pystring(EmptyMacroSrc) 
       , m_ms_inline("", 0, EmptyMacroSrc) 
       , m_queue_may_append_to_cluster(false)
	{
		m_hash.init();
		if ( ! lines.empty()) {
			m_hash.insert_source("<PythonString>", m_src_pystring);
			MacroStreamMemoryFile ms(lines.c_str(), lines.size(), m_src_pystring);

			std::string errmsg;
			char * qline = NULL;
			int rval = m_hash.parse_up_to_q_line(ms, errmsg, &qline);
			if (rval != 0) { THROW_EX(RuntimeError, errmsg.c_str()); }
			if (qline) {
				const char * qa = SubmitHash::is_queue_statement(qline);
				if (qa) {
					m_qargs = qa;
					// store the rest of the submit file raw. we can't parse it yet, but it might contain itemdata
					size_t cbremain;
					const char * remain = ms.remainder(cbremain);
					if (remain && cbremain) {
						m_remainder.assign(remain, cbremain);
						m_ms_inline.set(m_remainder.c_str(), cbremain, 0, m_src_pystring);
					}
				}
			}
		}
	}


    std::string
    expand(const std::string attr) const
    {
        char *val_char(const_cast<Submit*>(this)->m_hash.submit_param(plus_to_my(attr)));
        std::string value(val_char);
        free(val_char);
        return value;
    }


    std::string
    getItem(const std::string attr) const
    {
        const char *key = plus_to_my(attr);
        const char *val = const_cast<Submit*>(this)->m_hash.lookup(key);
        if (val == NULL)
        {
            THROW_EX(KeyError, key);
        }
        return std::string(val);
    }


    std::string
    get(const std::string attr, const std::string value) const
    {
        const char *val = const_cast<Submit*>(this)->m_hash.lookup(plus_to_my(attr));
        if (val == NULL)
        {
            return value;
        }
        return std::string(val);
    }


    std::string
    setDefault(const std::string attr, boost::python::object value_obj)
    {
        std::string default_value = convertToSubmitValue(value_obj);
        const char * key = plus_to_my(attr);
        const char *val = m_hash.lookup(key);
        if (val == NULL)
        {
            m_hash.set_submit_param(key, default_value.c_str());
            return default_value;
        }
        return std::string(val);
    }


    void
    setItem(const std::string attr, boost::python::object obj)
    {
        std::string value = convertToSubmitValue(obj);
        m_hash.set_submit_param(plus_to_my(attr), value.c_str());
    }


    void
    deleteItem(const std::string attr)
    {
        const char *key = plus_to_my(attr);
        const char *val = m_hash.lookup(key);
        if (val == NULL) {THROW_EX(KeyError, key);}
        m_hash.set_submit_param(key, NULL);
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
            catch (boost::python::error_already_set &)
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
            catch (boost::python::error_already_set &)
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
            catch (boost::python::error_already_set &)
            {
                hash_iter_delete(&it);
                throw;
            }
            hash_iter_next(it);
        }
        hash_iter_delete(&it);
        return results;
    }

    static boost::shared_ptr<Submit>
    from_dag(std::string dag_filename, boost::python::dict opts)
    {
        char* sub_data;
        DagmanUtils dagman_utils;
        FILE* sub_fp = NULL;
        size_t sub_size;
        std::string sub_args;
        std::string sub_filename = dag_filename + std::string(".condor.sub");
        StringList dag_file_attr_lines;
        SubmitDagDeepOptions deep_opts;
        SubmitDagShallowOptions shallow_opts;

        dagman_utils.usingPythonBindings = true;

        // Setting any submit options that may have been passed in (ie. max idle, max post)
        shallow_opts.dagFiles.insert(dag_filename.c_str());
        shallow_opts.primaryDagFile = dag_filename;
        SetDagOptions(opts, shallow_opts, deep_opts);

        // Make sure we can actually submit this DAG with the given options.
        // If we can't, throw an exception and exit.
        if ( !dagman_utils.ensureOutputFilesExist(deep_opts, shallow_opts) ) {
            THROW_EX(RuntimeError, "Unable to write condor_dagman output files");
        }

        // Write out the .condor.sub file we need to submit the DAG
        dagman_utils.setUpOptions(deep_opts, shallow_opts, dag_file_attr_lines);
        if ( !dagman_utils.writeSubmitFile(deep_opts, shallow_opts, dag_file_attr_lines) ) {
            THROW_EX(RuntimeError, "Unable to write condor_dagman submit file");
        }

        // Now write the submit file and open it
        sub_fp = safe_fopen_wrapper_follow(sub_filename.c_str(), "r");
        if(sub_fp == NULL) {
            printf("ERROR: Could not read generated DAG submit file %s\n", sub_filename.c_str());
            return NULL;
        }

        // Determine size of the file and store its contents in a buffer
        fseek(sub_fp, 0, SEEK_END);
        sub_size = ftell(sub_fp);
        sub_data = new char[sub_size];
        rewind(sub_fp);
        if(fread(sub_data, sizeof(char), sub_size, sub_fp) != sub_size) {
            printf("ERROR: DAG submit file %s returned wrong size\n", sub_filename.c_str());
        }
        fclose(sub_fp);

        sub_args = sub_data;
        delete[] sub_data;

        // Create a Submit object with contents of the .condor.sub file
        boost::shared_ptr<Submit> sub(new Submit(sub_args));
        return sub;
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

            boost::python::object value(tup[0]);
            std::string value_str = convertToSubmitValue(tup[1]);

            m_hash.set_submit_param(plus_to_my(attr), value_str.c_str());
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
        if ( ! m_qargs.empty()) {
            ss << "queue " << m_qargs;
        }
        hash_iter_delete(&it);
        return ss.str();
    }


    boost::python::object
    toRepr() const
    {
        boost::python::object obj(toString());
        return obj.attr("__repr__")();
    }

	// helper function for determining if this is a factory submit for a job submit
	bool is_factory(long long & max_materialize, boost::shared_ptr<ConnectionSentry> txn)
	{
		bool factory_submit = false;
		long long max_idle = INT_MAX;

		// check to see if a factory submit was desired, and if the schedd supports it
		//
		if (m_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeLimit,ATTR_JOB_MATERIALIZE_LIMIT, max_materialize, true)) {
			factory_submit = true;
		} else if (m_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeMaxIdle, ATTR_JOB_MATERIALIZE_MAX_IDLE, max_idle, true) ||
			m_hash.submit_param_long_exists(SUBMIT_KEY_JobMaterializeMaxIdleAlt, ATTR_JOB_MATERIALIZE_MAX_IDLE, max_idle, true)) {
			max_materialize = INT_MAX;
			factory_submit = true;
		}
		if (factory_submit) {
			// PRAGMA_REMIND("move this into the schedd object?")
			const ClassAd *capabilities = txn->capabilites();
			bool allows_late = false;
			if (capabilities && capabilities->LookupBool("LateMaterialize", allows_late) && allows_late) {
				int late_ver = 0;
				// we require materialize version 2 or later (digest is pruned, requirments generated from job ad)
				if (capabilities->LookupInteger("LateMaterializeVersion", late_ver) && late_ver >= 2) {
					factory_submit = true;
				} else {
					factory_submit = false;
				}
			} else {
				factory_submit = false; // sorry, no can do.
			}
		}

		return factory_submit;
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
		long long max_materialize = INT_MAX;
		int cluster = txn->clusterId();
		const int first_procid = 0;
		JOB_ID_KEY jid;
		int step=0, item_index=0, rval=0;
		int num_jobs = 0;

		SubmitStepFromQArgs ssi(m_hash);

		// if this is the first queue statement of this transaction, (or if we need to get a new cluster)
		// we have some once-per-cluster initialization to do.
		if ( ! m_hash.base_job_was_initialized() || cluster <= 0 || m_hash.getClusterId() != cluster || ! m_queue_may_append_to_cluster) {

			if (m_hash.init_base_ad(time(NULL), txn->owner().c_str())) {
				process_submit_errstack(m_hash.error_stack());
				THROW_EX(RuntimeError, "Failed to create a cluster ad");
			}
			process_submit_errstack(m_hash.error_stack());

			factory_submit = is_factory(max_materialize, txn);

			cluster = txn->newCluster();
			if (cluster < 0) {
				THROW_EX(RuntimeError, "Failed to create new cluster.");
			}

			// begin the iterator for QUEUE foreach data. we only allow multiple queue statements
			// if there is NOT any foreach data.  so we will only get here when
			if (m_qargs.empty()) {
				ssi.begin(JOB_ID_KEY(cluster, first_procid), count);
			} else {
				if (ssi.begin(JOB_ID_KEY(cluster, first_procid), m_qargs.c_str()) != 0) { THROW_EX(RuntimeError, "Invalid QUEUE statement"); }
				else {
					std::string errmsg;
					size_t ix; int line;
					m_ms_inline.save_pos(ix, line);
					int rv = ssi.load_items(m_ms_inline, false, errmsg);
					m_ms_inline.rewind_to(ix, line);
					if (rv != 0) { THROW_EX(RuntimeError, errmsg.c_str()); }
				}
			}
			if (count != 0 && count != ssi.step_size()) { THROW_EX(RuntimeError, "count argument supplied to queue method conflicts with count in submit QUEUE statement"); }
			count = ssi.step_size();

			if (factory_submit) {
				char path[4096];
				if (getcwd(path, 4095)) { m_hash.set_submit_param("FACTORY.Iwd", path); }
			}

			// if there were no queue arguments supplied, then we allow multiple calls to this queue() method
			// to append jobs to a single cluster.
			if (m_qargs.empty() && ! factory_submit) {
				m_queue_may_append_to_cluster = true;
			}

		} else {
			// pick up where we left off
			int last_proc_id = txn->procId();
			ssi.begin(JOB_ID_KEY(cluster, last_proc_id+1), count);
		}

#if 1

		if (factory_submit) {

			// force re-initialization (and a new cluster) if this hash is used again.
			m_queue_may_append_to_cluster = false;

			// load the first item, this also sets jid, item_index, and step
			rval = ssi.next(jid, item_index, step);

			// turn the submit hash into a submit digest
			std::string submit_digest;
			m_hash.make_digest(submit_digest, cluster, ssi.vars(), 0);

			// make and send the cluster ad
			ClassAd *proc_ad = m_hash.make_job_ad(jid, item_index, step, false, false, NULL, NULL);
			process_submit_errstack(m_hash.error_stack());
			if ( ! proc_ad) {
				THROW_EX(RuntimeError, "Failed to create new job ad");
			}
			classad::ClassAd * clusterad = proc_ad->GetChainedParentAd();
			if (clusterad) {
				rval = SendJobAttributes(JOB_ID_KEY(cluster, -1), *clusterad, SetAttribute_NoAck, m_hash.error_stack(), "Submit");
				process_submit_errstack(m_hash.error_stack());
			} else {
				rval = -1;
			}
			if (rval < 0) { THROW_EX(ValueError, "Failed to create send job attributes"); }

			submit_digest += "\n";
			submit_digest += "Queue ";
			if (count) { formatstr_cat(submit_digest, "%d ", count); }

			// send over the itemdata
			int row_count = 1;
			if (ssi.has_items()) {
				MyString items_filename;
				if (SendMaterializeData(cluster, 0, ssi.send_row, &ssi, items_filename, &row_count) < 0 || row_count <= 0) {
					THROW_EX(ValueError, "Failed to to send materialize itemdata");
				}

				// PRAGMA_REMIND("fix this when python submit supports foreach, maybe make this common with condor_submit")
				auto_free_ptr submit_vars(ssi.vars().print_to_delimed_string(","));
				if (submit_vars) { submit_digest += submit_vars.ptr(); submit_digest += " "; }

				//char slice_str[16*3+1];
				//if (ssi.m_fea.slice.to_string(slice_str, COUNTOF(slice_str))) { submit_digest += slice_str; submit_digest += " "; }
				if ( ! items_filename.empty()) { submit_digest += "from "; submit_digest += items_filename.c_str(); }
			}
			submit_digest += "\n";

			num_jobs = row_count * count;

			// materialize all of the jobs unless the user requests otherwise.
			// (the admin can also set a limit which is applied at the schedd)
			max_materialize = MIN(max_materialize, num_jobs);
			max_materialize = MAX(max_materialize, 1);

			// send the submit digest to the schedd. the schedd will parse the digest at this point
			// and return success or failure.
			if (SetJobFactory(cluster, (int)max_materialize, NULL, submit_digest.c_str()) < 0) {
				THROW_EX(RuntimeError, "Failed to send job factory for max_materilize.");
			}

		} else {
			// loop through the itemdata, sending jobs for each item
			//
			while ((rval = ssi.next(jid, item_index, step)) > 0) {

				int procid = txn->newProc();
				if (procid < 0) { THROW_EX(RuntimeError, "Failed to create new proc ID."); }
				if (procid != jid.proc) { THROW_EX(RuntimeError, "Internal error: newProc does not match iterator procid"); }

				ClassAd *proc_ad = m_hash.make_job_ad(jid, item_index, step, false, false, NULL, NULL);
				process_submit_errstack(m_hash.error_stack());
				if ( ! proc_ad) {
					THROW_EX(RuntimeError, "Failed to create new job ad");
				}

				// on first iteration, send the cluster ad (if any) before the first proc ad
				if (rval == 2) {
					classad::ClassAd * clusterad = proc_ad->GetChainedParentAd();
					if (clusterad) {
						condor::ModuleLock ml;
						rval = SendJobAttributes(JOB_ID_KEY(cluster, -1), *clusterad, SetAttribute_NoAck, m_hash.error_stack(), "Submit");
					}
				}
				// send the proc ad unless there was a failure.
				if (rval >= 0) {
					condor::ModuleLock ml;
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

				++num_jobs;
			}
		}
#else
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
#endif

        if (param_boolean("SUBMIT_SEND_RESCHEDULE",true))
        {
            txn->reschedule();
        }
        m_hash.warn_unused(stderr, "Submit object");
        process_submit_errstack(m_hash.error_stack());
        return cluster;
    }

	boost::shared_ptr<SubmitResult>
	queue_from_iter(boost::shared_ptr<ConnectionSentry> txn, int count, boost::python::object from)
	{
		if (!txn.get() || !txn->transaction()) { THROW_EX(RuntimeError, "Job queue attempt without active transaction"); }

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
		long long max_materialize = INT_MAX;
		const int first_proc_id = 0; // someday maybe this will be non-zero?
		int num_jobs = 0;

		if (m_hash.init_base_ad(time(NULL), txn->owner().c_str())) {
			process_submit_errstack(m_hash.error_stack());
			THROW_EX(RuntimeError, "Failed to create a cluster ad");
		}
		process_submit_errstack(m_hash.error_stack());

		factory_submit = is_factory(max_materialize, txn);

		int cluster = txn->newCluster();
		if (cluster < 0) {
			THROW_EX(RuntimeError, "Failed to create new cluster.");
		}
		// this will be a single use cluster,
		m_queue_may_append_to_cluster = false;

		if (factory_submit) {
			char path[4096];
			if (getcwd(path, 4095)) { m_hash.set_submit_param("FACTORY.Iwd", path); }
		}

		JOB_ID_KEY jid;
		int step=0, item_index=0, rval;
		SubmitStepFromPyIter ssi(m_hash, JOB_ID_KEY(cluster, first_proc_id), count, from);

		if (factory_submit) {

			// get the first rowdata, we need that to build the submit digest, etc
			rval = ssi.next(jid, item_index, step);
			if (rval < 0) { THROW_EX(RuntimeError, ssi.errmsg()); }

			// turn the submit hash into a submit digest
			std::string submit_digest;
			m_hash.make_digest(submit_digest, cluster, ssi.vars(), 0);

			// make a proc0 ad (because that's the same as a cluster ad)
			ClassAd *proc_ad = m_hash.make_job_ad(JOB_ID_KEY(cluster, 0), 0, 0, false, false, NULL, NULL);
			process_submit_errstack(m_hash.error_stack());
			if ( ! proc_ad) {
				THROW_EX(RuntimeError, "Failed to create new job ad");
			}

			// send the cluster ad
			classad::ClassAd * clusterad = proc_ad->GetChainedParentAd();
			if (clusterad) {
				rval = SendJobAttributes(JOB_ID_KEY(cluster, -1), *clusterad, SetAttribute_NoAck, m_hash.error_stack(), "Submit");
				process_submit_errstack(m_hash.error_stack());
				if (rval < 0) {
					THROW_EX(ValueError, "Failed to create send job attributes");
				}
			}

			// send the itemdata (if any)
			int row_count = 1;
			if (ssi.has_items()) {
				if (SendMaterializeData(cluster, 0, ssi.send_row, &ssi, ssi.fea().items_filename, &row_count) < 0 || row_count <= 0) {
					THROW_EX(ValueError, "Failed to to send materialize itemdata");
				}
				num_jobs = row_count * ssi.step_size();
			}

			// append the queue statement
			submit_digest += "\n";
			submit_digest += "Queue ";
			if (count) { formatstr_cat(submit_digest, "%d ", count); }
			auto_free_ptr submit_vars(ssi.vars().print_to_delimed_string(","));
			if (submit_vars.ptr()) { submit_digest += submit_vars.ptr(); submit_digest += " "; }
			//char slice_str[16*3+1];
			//if (ssi.m_fea.slice.to_string(slice_str, COUNTOF(slice_str))) { submit_digest += slice_str; submit_digest += " "; }
			if ( ! ssi.fea().items_filename.empty()) { submit_digest += "from "; submit_digest += ssi.fea().items_filename.c_str(); }
			submit_digest += "\n";

			// materialize all of the jobs unless the user requests otherwise.
			// (the admin can also set a limit which is applied at the schedd)
			max_materialize = MIN(max_materialize, num_jobs);
			max_materialize = MAX(max_materialize, 1);

			// send the submit digest to the schedd. the schedd will parse the digest at this point
			// and return success or failure.
			if (SetJobFactory(cluster, (int)max_materialize, NULL, submit_digest.c_str()) < 0) {
				THROW_EX(RuntimeError, "Failed to send job factory for max_materilize.");
			}


		} else {

			while ((rval = ssi.next(jid, item_index, step)) > 0) {

				int procid = txn->newProc();
				if (procid < 0) { THROW_EX(RuntimeError, "Failed to create new proc ID."); }
				if (procid != jid.proc) { THROW_EX(RuntimeError, "Internal error: newProc does not match iterator procid"); }

				ClassAd *proc_ad = m_hash.make_job_ad(jid, item_index, step, false, false, NULL, NULL);
				process_submit_errstack(m_hash.error_stack());
				if ( ! proc_ad) { THROW_EX(RuntimeError, "Failed to create new job ad"); }

				// on first iteration, send the cluster ad (if any) before the first proc ad
				if (rval == 2) {
					classad::ClassAd * clusterad = proc_ad->GetChainedParentAd();
					if (clusterad) {
						rval = SendJobAttributes(JOB_ID_KEY(cluster, -1), *clusterad, SetAttribute_NoAck, m_hash.error_stack(), "Submit");
					}
				}
				// send the proc ad unless there was a failure.
				if (rval >= 0) {
					rval = SendJobAttributes(jid, *proc_ad, SetAttribute_NoAck, m_hash.error_stack(), "Submit");
				}
				process_submit_errstack(m_hash.error_stack());
				if (rval < 0) {
					THROW_EX(ValueError, "Failed to create send job attributes");
				}

				++num_jobs;
			}

		}

		if (rval < 0) { THROW_EX(RuntimeError, ssi.errmsg()); }

		if (param_boolean("SUBMIT_SEND_RESCHEDULE",true)) {
			txn->reschedule();
		}
		m_hash.warn_unused(stderr, "Submit object");
		process_submit_errstack(m_hash.error_stack());

		boost::shared_ptr<SubmitResult> result(new SubmitResult(JOB_ID_KEY(cluster,0), num_jobs, m_hash.get_cluster_ad()));

		return result;
	}

// (boost::python::arg("self"),
	//boost::python::arg("count")=1,
	//boost::python::arg("itemdata")=boost::python::object(),
	//boost::python::arg("clusterid")=1,
	//boost::python::arg("procid")=0,
	//boost::python::arg("qdate")=0
	//boost::python::arg("owner")=std::string(),
	boost::shared_ptr<SubmitJobsIterator>
	iterjobs(int count, boost::python::object from, int clusterid, int procid, time_t qdate, const std::string owner)
	{
		if (clusterid < 0 || procid < 0) { THROW_EX(RuntimeError, "Job id out of range"); }

		if ( ! clusterid) clusterid = 1;
		if ( ! qdate) qdate = time(NULL);

		std::string p0wner;
		if ( ! owner.empty()) {
			// PRAGMA_REMIND("replace this with a proper username validation function?")
			if (std::string::npos != owner.find_first_of(" \t\n\r")) { THROW_EX(ValueError, "Invalid characters in Owner"); }
			p0wner = owner;
		} else {
			auto_free_ptr user(my_username());
			if (user) {
				p0wner = user.ptr();
			} else {
				p0wner = "unknown";
			}
		}

		SubmitJobsIterator *sji;
		if (PyIter_Check(from.ptr())) {
			sji = new SubmitJobsIterator(m_hash, false, JOB_ID_KEY(clusterid, procid), count, from, qdate, p0wner);
		} else {
			sji = new SubmitJobsIterator(m_hash, false, JOB_ID_KEY(clusterid, procid), count, m_qargs, m_ms_inline, qdate, p0wner);
		}
		boost::shared_ptr<SubmitJobsIterator> iter(sji);
		return iter;
	}

	boost::shared_ptr<SubmitJobsIterator>
	iterprocs(int count, boost::python::object from, int clusterid, int procid, time_t qdate, const std::string owner)
	{
		if (clusterid < 0 || procid < 0) { THROW_EX(RuntimeError, "Job id out of range"); }
		if ( ! clusterid) clusterid = 1;
		if ( ! qdate) qdate = time(NULL);

		std::string p0wner;
		if ( ! owner.empty()) {
			// PRAGMA_REMIND("replace this with a proper username validation function?")
			if (std::string::npos != owner.find_first_of(" \t\n\r")) { THROW_EX(ValueError, "Invalid characters in Owner"); }
			p0wner = owner;
		} else {
			auto_free_ptr user(my_username());
			if (user) {
				p0wner = user.ptr();
			} else {
				p0wner = "unknown";
			}
		}

		SubmitJobsIterator * sji;
		if (PyIter_Check(from.ptr())) {
			sji = new SubmitJobsIterator(m_hash, true, JOB_ID_KEY(clusterid, procid), count, from, qdate, p0wner);
		} else {
			sji = new SubmitJobsIterator(m_hash, true, JOB_ID_KEY(clusterid, procid), count, m_qargs, m_ms_inline, qdate, p0wner);
		}
		boost::shared_ptr<SubmitJobsIterator> iter(sji);
		return iter;
	}

	boost::shared_ptr<QueueItemsIterator>
	iterqitems(const std::string qline)
	{
		const char * pqargs = "";
		bool use_remainder = true;

		if ( ! qline.empty()) {
			// incase they sent a string that starts with the word "queue", skip over that now.
			pqargs = SubmitHash::is_queue_statement(qline.c_str());
			if ( ! pqargs) pqargs = qline.c_str();
			use_remainder = false;
		} else if ( ! m_qargs.empty()) {
			pqargs = m_qargs.c_str();
			use_remainder = true;
		}

		QueueItemsIterator * qit = new QueueItemsIterator();
		if (qit) {
			qit->init(m_hash, pqargs);
			if (qit->needs_submit_lines() && ! use_remainder) {
				THROW_EX(RuntimeError, "inline items not available");
			}

			size_t ix; int line;
			m_ms_inline.save_pos(ix, line);
			qit->load_items(m_hash, m_ms_inline);
			m_ms_inline.rewind_to(ix, line);
		}

		boost::shared_ptr<QueueItemsIterator> iter(qit);
		return iter;
	}

	std::string
	getQArgs() const
	{
		if (m_qargs.empty()) { return std::string(); }
		return std::string(m_qargs);
	}

	// set queue arguments from input, stripping of the leading word 'queue' if needed.
	void
	setQArgs(const std::string qline)
	{
		if (qline.empty()) { m_qargs.clear(); m_ms_inline.reset(); m_remainder.clear(); }

		if (qline.find_first_of("\n") != std::string::npos) {
			THROW_EX(ValueError, "QArgs cannot contain a newline character");
		}

		const char * qargs = SubmitHash::is_queue_statement(qline.c_str());
		if (qargs) { // if we skipped over a "queue" keyword, then by definition the args were changed
			m_qargs = qargs;
			m_ms_inline.reset();
			m_remainder.clear();
		} else if (qline != m_qargs) { // if the args changed, clear the submit remainder
			m_qargs = qline;
			m_ms_inline.reset();
			m_remainder.clear();
		}
	}

private:

    std::string
    convertToSubmitValue(boost::python::object value) {
        boost::python::extract<std::string> extract_str(value);
        std::string attr;
        if (extract_str.check()) {
            attr = extract_str();
        } else {
            boost::python::extract<ExprTreeHolder*> extract_expr(value);
            if (extract_expr.check()) {
                ExprTreeHolder *holder = extract_expr();
                attr = holder->toString();
            } else {
                boost::python::extract<ClassAdWrapper*> extract_classad(value);
                if (extract_classad.check()) {
                    auto wrapper = extract_classad();
                    attr = wrapper->toRepr();
                } else {
                    boost::python::str value_str(value);
                    attr = boost::python::extract<std::string>(value_str);
                }
            }
        }
        return attr;
    }

	const char * plus_to_my(const std::string & attr) const {
		if ( ! attr.empty() && attr[0] == '+') {
			m_attr_fixup_buf.reserve(attr.size() + 2);
			m_attr_fixup_buf = "MY";
			m_attr_fixup_buf += attr;
			m_attr_fixup_buf[2] = '.';
			return m_attr_fixup_buf.c_str();
		}
		return attr.c_str();
	}

    SubmitHash m_hash;
    std::string m_qargs;
    std::string m_remainder; // holds remainder of input after queue statement.
    mutable std::string m_attr_fixup_buf;
    MACRO_SOURCE m_src_pystring; // needed for MacroStreamMemoryFile to point to
    MacroStreamMemoryFile m_ms_inline; // extra lines after queue statement, used if we are doing inline foreach data
    bool m_queue_may_append_to_cluster; // when true, the queue() method can add jobs to the existing cluster
};

// shared source for all instances of MacroStreamMemoryFile that have an empty stream
MACRO_SOURCE Submit::EmptyMacroSrc = { false, false, 3, -2, -1, -2 };

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(query_overloads, query, 0, 5);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(submit_overloads, submit, 1, 4);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(transaction_overloads, transaction, 0, 2);

void export_schedd()
{
    enum_<JobAction>("JobAction",
            R"C0ND0R(
            An enumeration describing the actions that may be performed on a job in queue.

            The values of the enumeration are:

            .. attribute:: Hold

                Put a job on hold, vacating a running job if necessary.  A job will stay in the hold state
                until explicitly acted upon by the admin or owner.

            .. attribute:: Release

                Release a job from the hold state, returning it to ``Idle``.

            .. attribute:: Suspend

                Suspend the processes of a running job (on Unix platforms, this triggers a ``SIGSTOP``).
                The job's processes stay in memory but no longer get scheduled on the CPU.

            .. attribute:: Continue

                Continue a suspended jobs (on Unix, ``SIGCONT``).
                The processes in a previously suspended job will be scheduled to get CPU time again.

            .. attribute:: Remove

                Remove a job from the Schedd's queue, cleaning it up first on the remote host (if running).
                This requires the remote host to acknowledge it has successfully vacated the job, meaning ``Remove`` may not be instantaneous.

            .. attribute:: RemoveX

                Immediately remove a job from the schedd queue, even if it means the job is left running on the remote resource.

            .. attribute:: Vacate

                Cause a running job to be killed on the remote resource and return to idle state.
                With ``Vacate``, jobs may be given significant time to cleanly shut down.

            .. attribute:: VacateFast

                Vacate a running job as quickly as possible, without providing time for the job to cleanly terminate.
            )C0ND0R")
        .value("Hold", JA_HOLD_JOBS)
        .value("Release", JA_RELEASE_JOBS)
        .value("Remove", JA_REMOVE_JOBS)
        .value("RemoveX", JA_REMOVE_X_JOBS)
        .value("Vacate", JA_VACATE_JOBS)
        .value("VacateFast", JA_VACATE_FAST_JOBS)
        .value("Suspend", JA_SUSPEND_JOBS)
        .value("Continue", JA_CONTINUE_JOBS)
        ;

    enum_<SetAttributeFlags_t>("TransactionFlags",
            R"C0ND0R(
            Enumerated flags affecting the characteristics of a transaction.

            The values of the enumeration are:

            .. attribute:: NonDurable

                Non-durable transactions are changes that may be lost when the ``condor_schedd``
                crashes.  ``NonDurable`` is used for performance, as it eliminates extra ``fsync()`` calls.

            .. attribute:: SetDirty

                This marks the changed ClassAds as dirty, causing an update notification to be sent
                to the ``condor_shadow`` and the ``condor_gridmanager``, if they are managing the job.

            .. attribute:: ShouldLog

                Causes any changes to the job queue to be logged in the relevant job event log.
            )C0ND0R")
        .value("None", 0)
        .value("NonDurable", NONDURABLE)
        .value("SetDirty", SETDIRTY)
        .value("ShouldLog", SHOULDLOG)
        ;

    enum_<CondorQ::QueryFetchOpts>("QueryOpts",
            R"C0ND0R(
            Enumerated flags sent to the ``condor_schedd`` during a query to alter its behavior.

            The values of the enumeration are:

            .. attribute:: Default

                Queries should use all default behaviors.

            .. attribute:: AutoCluster

                Instead of returning job ads, return an ad per auto-cluster.

            .. attribute:: GroupBy

            .. attribute:: DefaultMyJobsOnly

            .. attribute:: SummaryOnly

            .. attribute:: IncludeClusterAd
            )C0ND0R")
        .value("Default", CondorQ::fetch_Jobs)
        .value("AutoCluster", CondorQ::fetch_DefaultAutoCluster)
        .value("GroupBy", CondorQ::fetch_GroupBy)
        .value("DefaultMyJobsOnly", CondorQ::fetch_MyJobs)
        .value("SummaryOnly", CondorQ::fetch_SummaryOnly)
        .value("IncludeClusterAd", CondorQ::fetch_IncludeClusterAd)
        ;

    enum_<BlockingMode>("BlockingMode",
            R"C0ND0R(
            An enumeration that controls the behavior of query iterators once they are out of data.

            The values of the enumeration are:

            .. attribute:: Blocking

                Sets the iterator to block until more data is available.

            .. attribute:: NonBlocking

                Sets the iterator to return immediately if additional data is not available.
            )C0ND0R")
        .value("Blocking", Blocking)
        .value("NonBlocking", NonBlocking)
        ;

    class_<ConnectionSentry>("Transaction", "An ongoing transaction in the HTCondor schedd", no_init)
        .def("__enter__", &ConnectionSentry::enter)
        .def("__exit__", &ConnectionSentry::exit)
        ;
    register_ptr_to_python< boost::shared_ptr<ConnectionSentry> >();

#if BOOST_VERSION >= 103400
//    boost::python::docstring_options doc_options;
//    doc_options.disable_cpp_signatures();
#endif
    class_<Schedd>("Schedd",
            R"C0ND0R(
            Client object for a ``condor_schedd``.
            )C0ND0R",
        init<const ClassAdWrapper &>(
            boost::python::args("self", "location_ad"),
            R"C0ND0R(
            :param location_ad: An Ad describing the location of the remote ``condor_schedd``
                daemon, as returned by the :meth:`Collector.locate` method. If the parameter is omitted,
                the local ``condor_schedd`` daemon is used.
            :type location_ad: :class:`~classad.ClassAd`
            )C0ND0R"))
        .def(boost::python::init<>(boost::python::args("self")))
        .def("query", &Schedd::query, query_overloads(
            R"C0ND0R(
            Query the ``condor_schedd`` daemon for jobs.

            .. warning:: This returns a *list* of :class:`~classad.ClassAd` objects, meaning all results must
                be buffered in memory.  This may be memory-intensive for large responses; we strongly recommend
                to utilize the :meth:`xquery`

            :param constraint: Query constraint; only jobs matching this constraint will be returned; defaults to ``'true'``.
            :type constraint: str or :class:`~classad.ExprTree`
            :param attr_list: Attributes for the ``condor_schedd`` daemon to project along.
                At least the attributes in this list will be returned.
                The default behavior is to return all attributes.
            :type attr_list: list[str]
            :param callback: A callable object; if provided, it will be invoked for each ClassAd.
                The return value (if not ``None``) will be added to the returned list instead of the ad.
            :param int limit: The maximum number of ads to return; the default (``-1``) is to return all ads.
            :param opts: Additional flags for the query; these may affect the behavior of the ``condor_schedd``.
            :type opts: :class:`QueryOpts`.
            :return: ClassAds representing the matching jobs.
            :rtype: list[:class:`~classad.ClassAd`]
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("constraint")="true", boost::python::arg("attr_list")=boost::python::list(), boost::python::arg("callback")=boost::python::object(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs)
#else
            (boost::python::arg("self"), boost::python::arg("constraint")="true", boost::python::arg("attr_list")=boost::python::list(), boost::python::arg("callback")=boost::python::object(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs)
#endif
            ))
        .def("act", &Schedd::actOnJobs,
            R"C0ND0R(
            Change status of job(s) in the ``condor_schedd`` daemon. The return value is a ClassAd object
            describing the number of jobs changed.

            This will throw an exception if no jobs are matched by the constraint.

            :param action: The action to perform; must be of the enum JobAction.
            :type action: :class:`JobAction`
            :param job_spec: The job specification. It can either be a list of job IDs or a string specifying a constraint.
                Only jobs matching this description will be acted upon.
            :type job_spec: list[str] or str
            :param reason: The reason for the action.
                If omitted, the reason will be "Python-initiated action".
            :type reason: str
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("action"), boost::python::arg("job_spec"), boost::python::arg("reason")=boost::python::object()))
        .def("act", &Schedd::actOnJobs2)
        .def("submit", &Schedd::submit, submit_overloads(
            R"C0ND0R(
            Submit one or more jobs to the ``condor_schedd`` daemon.

            This method requires the invoker to provide a ClassAd for the new job cluster;
            such a ClassAd contains attributes with different names than the commands in a
            submit description file. As an example, the stdout file is referred to as ``output``
            in the submit description file, but ``Out`` in the ClassAd.

            .. hint:: To generate an example ClassAd, take a sample submit description
                file and invoke::

                    condor_submit -dump <filename> [cmdfile]

                Then, load the resulting contents of ``<filename>`` into Python.

            :param ad: The ClassAd describing the job cluster.
            :type ad: :class:`~classad.ClassAd`
            :param int count: The number of jobs to submit to the job cluster. Defaults to ``1``.
            :param bool spool: If ``True``, the clinent inserts the necessary attributes
                into the job for it to have the input files spooled to a remote
                ``condor_schedd`` daemon. This parameter is necessary for jobs submitted
                to a remote ``condor_schedd`` that use HTCondor file transfer.
            :param ad_results: If set to a list, the list object will contain the job ads
                resulting from the job submission.
                These are needed for interacting with the job spool after submission.
            :type ad_results: list[:class:`~classad.ClassAd`]
            :return: The newly created cluster ID.
            :rtype: int
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("ad"), boost::python::arg("count")=1, boost::python::arg("spool")=false, boost::python::arg("ad_results")=boost::python::object())))
#else
            (boost::python::arg("self"), "ad", boost::python::arg("count")=1, boost::python::arg("spool")=false, boost::python::arg("ad_results")=boost::python::object())))
#endif
        .def("submitMany", &Schedd::submitMany,
            R"C0ND0R(
            Submit multiple jobs to the ``condor_schedd`` daemon, possibly including
            several distinct processes.

            :param cluster_ad: The base ad for the new job cluster; this is the same format
                as in the :meth:`submit` method.
            :type cluster_ad: :class:`~classad.ClassAd`
            :param list proc_ads: A list of 2-tuples; each tuple has the format of ``(proc_ad, count)``.
                For each list entry, this will result in count jobs being submitted inheriting from
                both ``cluster_ad`` and ``proc_ad``.
            :param bool spool: If ``True``, the clinent inserts the necessary attributes
                into the job for it to have the input files spooled to a remote
                ``condor_schedd`` daemon. This parameter is necessary for jobs submitted
                to a remote ``condor_schedd`` that use HTCondor file transfer.
            :param ad_results: If set to a list, the list object will contain the job ads
                resulting from the job submission.
                These are needed for interacting with the job spool after submission.
            :type ad_results: list[:class:`~classad.ClassAd`]
            :return: The newly created cluster ID.
            :rtype: int
            )C0ND0R", (
#if BOOST_VERSION >= 103400
             boost::python::arg("self"),
#endif
             boost::python::arg("cluster_ad"), boost::python::arg("proc_ads"), boost::python::arg("spool")=false, boost::python::arg("ad_results")=boost::python::object()))
        .def("spool", &Schedd::spool,
            R"C0ND0R(
            Spools the files specified in a list of job ClassAds
            to the ``condor_schedd``.

            :param ad_list: A list of job descriptions; typically, this is the list
                filled by the ``ad_results`` argument of the :meth:`submit` method call.
            :type ad_list: list[:class:`~classad.ClassAds`]
            :raises RuntimeError: if there are any errors.
            )C0ND0R",
            boost::python::args("self", "ad_list"))
        .def("transaction", &Schedd::transaction, transaction_overloads(
            R"C0ND0R(
            Start a transaction with the ``condor_schedd``.

            Starting a new transaction while one is ongoing is an error unless the ``continue_txn``
            flag is set.

            :param flags: Flags controlling the behavior of the transaction, defaulting to 0.
            :type flags: :class:`TransactionFlags`
            :param bool continue_txn: Set to ``True`` if you would like this transaction to extend any
                pre-existing transaction; defaults to ``False``.  If this is not set, starting a transaction
                inside a pre-existing transaction will cause an exception to be thrown.
            :return: A transaction context manager object.
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("flags")=0, boost::python::arg("continue_txn")=false))[boost::python::with_custodian_and_ward_postcall<1, 0>()])
#else
            (boost::python::arg("self"), boost::python::arg("flags")=0, boost::python::arg("continue_txn")=false))[boost::python::with_custodian_and_ward_postcall<1, 0>()])
#endif
        .def("retrieve", &Schedd::retrieve,
            R"C0ND0R(
            Retrieve the output sandbox from one or more jobs.

            :param job_spec: An expression matching the list of job output sandboxes to retrieve.
            :type job_spec: list[:class:`~classad.ClassAd`]
            )C0ND0R")
        .def("edit", &Schedd::edit,
            R"C0ND0R(
            Edit one or more jobs in the queue.

            This will throw an exception if no jobs are matched by the ``job_spec`` constraint.

            :param job_spec: The job specification. It can either be a list of job IDs or a string specifying a constraint.
                Only jobs matching this description will be acted upon.
            :type job_spec: list[str] or str
            :param str attr: The name of the attribute to edit.
            :param value: The new value of the attribute.  It should be a string, which will
                be converted to a ClassAd expression, or an ExprTree object.  Be mindful of quoting
                issues; to set the value to the string ``foo``, one would set the value to ``''foo''``
            :type value: str or :class:`~classad.ExprTree`
            )C0ND0R",
            boost::python::args("self", "job_spec", "attr", "value"))
        .def("reschedule", &Schedd::reschedule,
            R"C0ND0R(
            Send reschedule command to the schedd.
            )C0ND0R",
            boost::python::args("self"))
        .def("history", &Schedd::history,
            R"C0ND0R(
            Fetch history records from the ``condor_schedd`` daemon.

            :param requirements: Query constraint; only jobs matching this constraint will be returned;
                defaults to ``'true'``.
            :type constraint: str or :class:`class.ExprTree`
            :param projection: Attributes that are to be included for each returned job.
                The empty list causes all attributes to be included.
            :type projection: list[str]
            :param int match: An limit on the number of jobs to include; the default (``-1``)
                indicates to return all matching jobs.
            :return: All matching ads in the Schedd history, with attributes according to the
                ``projection`` keyword.
            :rtype: :class:`HistoryIterator`
            )C0ND0R",
#if BOOST_VERSION >= 103400
             (boost::python::arg("self"),
#endif
             boost::python::arg("requirements"), boost::python::arg("projection"), boost::python::arg("match")=-1,
             boost::python::arg("since")=boost::python::object())
            )
        .def("refreshGSIProxy", &Schedd::refreshGSIProxy,
            R"C0ND0R(
            Refresh the GSI proxy of a job; the job's proxy will be replaced the contents
            of the provided ``proxy_filename``.

            .. note:: Depending on the lifetime of the proxy in ``proxy_filename``, the resulting lifetime
                may be shorter than the desired lifetime.

            :param int cluster: Cluster ID of the job to alter.
            :param int proc: Process ID of the job to alter.
            :param str proxy_filename: The name of the file containing the new proxy for the job.
            :param int lifetime: Indicates the desired lifetime (in seconds) of the delegated proxy.
                A value of ``0`` specifies to not shorten the proxy lifetime.
                A value of ``-1`` specifies to use the value of configuration variable
                ``DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME``.
            )C0ND0R",
            boost::python::args("self", "cluster", "proc", "proxy_filename", "lifetime"))
        .def("xquery", &Schedd::xquery,
            R"C0ND0R(
            Query the ``condor_schedd`` daemon for jobs.

            As opposed to :meth:`query`, this returns an *iterator*, meaning only one ad is buffered in memory at a time.

            :param requirements: provides a constraint for filtering out jobs. It defaults to ``'true'``.
            :type requirements: str or :class:`~classad.ExprTree`
            :param projection: The attributes to return; an empty list (the default) signifies all attributes.
            :type projection: list[str]
            :param int limit: A limit on the number of matches to return.  The default (``-1``) indicates all
                matching jobs should be returned.
            :param opts: Additional flags for the query, from :class:`QueryOpts`.
            :type opts: :class:`QueryOpts`
            :param str name: A tag name for the returned query iterator. This string will always be
                returned from the :meth:`QueryIterator.tag` method of the returned iterator.
                The default value is the ``condor_schedd``'s name. This tag is useful to identify
                different queries when using the :func:`poll` function.
            :return: An iterator for the matching job ads
            :rtype: :class:`~htcondor.QueryIterator`
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("requirements") = "true", boost::python::arg("projection")=boost::python::list(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs, boost::python::arg("name")=boost::python::object())
#else
            (boost::python::arg("self"), boost::python::arg("requirements") = "true", boost::python::arg("projection")=boost::python::list(), boost::python::arg("limit")=-1, boost::python::arg("opts")=CondorQ::fetch_Jobs, boost::python::arg("name")=boost::python::object())
#endif
            )
        .def("negotiate", &Schedd::negotiate, boost::python::with_custodian_and_ward_postcall<1, 0>(),
            R"C0ND0R(
            Begin a negotiation cycle with the remote schedd for a given user.

            .. note:: The returned :class:`ScheddNegotiate` additionally serves as a context manager,
                automatically destroying the negotiation session when the context is left.

            :param str accounting_name: Determines which user the client will start negotiating with.
            :return: An iterator which yields resource request ClassAds from the ``condor_schedd``.
                Each resource request represents a set of jobs that are next in queue for the schedd
                for this user.
            :rtype: :class:`ScheddNegotiate`
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("owner"), boost::python::arg("ad")=boost::python::object)
#else
            (boost::python::arg("self"), boost::python::arg("owner"), boost::python::arg("ad")=boost::python::dict())
#endif
            )
        ;

    class_<ScheddNegotiate>("ScheddNegotiate",
            R"C0ND0R(
            The :class:`ScheddNegotiate` class represents an ongoing negotiation session
            with a schedd.  It is a context manager, returned by the :meth:`~htcondor.Schedd.negotiate`
            method.
            )C0ND0R",
            no_init)
        .def("__iter__", &ScheddNegotiate::getRequests, "Get resource requests from schedd.", boost::python::with_custodian_and_ward_postcall<1, 0>())
        .def("sendClaim", &ScheddNegotiate::sendClaim,
            R"C0ND0R(
                Send a claim to the schedd; if possible, the schedd will activate this and run
                one or more jobs.

                :param str claim: The claim ID, typically from the ``Capability`` attribute in the
                    corresponding Startd's private ad.
                :param offer: A description of the resource claimed (typically, the machine's ClassAd).
                :type offer: :class:`~classad.ClassAd`
                :param request: The resource request this claim is responding to; if not provided
                    (default), the Schedd will decide which job receives this resource.
                :type request: :class:`~classad.ClassAd`
            )C0ND0R",
#if BOOST_VERSION < 103400
          (boost::python::arg("claim"), boost::python::arg("offer"), boost::python::arg("request")=boost::python::dict())
#else
          (boost::python::arg("self"), boost::python::arg("claim"), boost::python::arg("offer"), boost::python::arg("request")=boost::python::dict())
#endif
          )
        .def("__enter__", &ScheddNegotiate::enter)
        .def("__exit__", &ScheddNegotiate::exit)
        .def("disconnect", &ScheddNegotiate::disconnect,
            R"C0ND0R(
            Disconnect from this negotiation session.  This can also be achieved by exiting
            the context.
            )C0ND0R",
            boost::python::args("self"))
        ;

    class_<Submit>("Submit",
            R"C0ND0R(
            An object representing a job submit description.  It uses the same submit
            language as ``condor_submit``.

            The submit description contains ``key = value`` pairs and implements the python
            dictionary protocol, including the ``get``, ``setdefault``, ``update``, ``keys``,
            ``items``, and ``values`` methods.
            )C0ND0R", boost::python::no_init)
        .def("__init__", boost::python::raw_function(&Submit::rawInit, 1),
            R"C0ND0R(
            Construct the Submit object from a number of ``key = value`` keyword arguments.
            )C0ND0R")
        .def(init<boost::python::dict>(
            R"C0ND0R(
            :param input: ``key = value`` pairs as a dictionary or string for initializing the submit description,
                or a string containing the text of a submit file.
                If a string is used, the text should consist of valid *condor_submit*
                statments optionally followed by a a single *condor_submit* ``QUEUE``
                statement. The arguments to the QUEUE statement will be stored
                in the ``QArgs`` member of this class and used when the :method:`Submit.queue()`
                method is called.
                If omitted, the submit object is initially empty.
            :type input: dict or str
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("input")=boost::python::object())))
        .def(init<std::string>())
        //.def_pickle(submit_pickle_suite())
        .def("expand", &Submit::expand,
            R"C0ND0R(
            Expand all macros for the given attribute.

            :param str attr: The name of the relevant attribute.
            :return: The value of the given attribute; all macros are expanded.
            :rtype: str
            )C0ND0R",
            boost::python::args("self", "attr"))
        .def("queue", &Submit::queue,
            R"C0ND0R(
            Submit the current object to a remote queue.

            :param txn: An active transaction object (see :meth:`Schedd.transaction`).
            :type txn: :class:`Transaction`
            :param int count: The number of jobs to create (defaults to ``0``).
                If not specified, or a value of ``0`` is given the ``QArgs`` member
                of this class is used to determine the number of procs to submit.
                If no ``QArgs`` were specified, one job is submitted.
            :param ad_results: A list to receive the ClassAd resulting from this submit.
                As with :meth:`Schedd.submit`, this is often used to later spool the input
                files.
            :return: The ClusterID of the submitted job(s).
            :rtype: int
            :raises RuntimeError: if the submission fails.
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("txn"), boost::python::arg("count")=0, boost::python::arg("ad_results")=boost::python::object())
            )
        .def("queue_with_itemdata", &Submit::queue_from_iter,
            R"C0ND0R(
            Submit the current object to a remote queue.

            :param txn: An active transaction object (see :meth:`Schedd.transaction`).
            :type txn: :class:`Transaction`
            :param int count: A queue count for each item from the iterator, defaults to 1.
            :param from: an iterator of strings or dictionaries containing the itemdata
                for each job as in ``queue in`` or ``queue from``.
            :return: a :class:`SubmitResult`, containing the cluster ID, cluster ClassAd and
                range of Job ids Cluster ID of the submitted job(s).
            :rtype: :class:`SubmitResult`
            :raises RuntimeError: if the submission fails.
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("txn"), boost::python::arg("count")=1, boost::python::arg("itemdata")=boost::python::object())
            )
        .def("jobs", &Submit::iterjobs,
            R"C0ND0R(
            Turn the current object into a sequence of simulated job ClassAds

            :param int count: the queue count for each item in the from list, defaults to 1
            :param from: a iterator of strings or dictionaries containing the itemdata for each job e.g. 'queue in' or 'queue from'
            :param int clusterid: the value to use for ClusterId when making job ads, defaults to 1
            :param int procid: the initial value for ProcId when making job ads, defaults to 0
            :param str qdate: a UNIX timestamp value for the QDATE attribute of the jobs, 0 means use the current time.
            :param str owner: a string value for the Owner attribute of the job
            :return: An iterator for the resulting job ads.

            :raises RuntimeError: if valid job ads cannot be made
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("count")=0, boost::python::arg("itemdata")=boost::python::object(), boost::python::arg("clusterid")=1, boost::python::arg("procid")=0, boost::python::arg("qdate")=0, boost::python::arg("owner")=std::string())
            )
        .def("procs", &Submit::iterprocs,
            R"C0ND0R(
            Turn the current object into a sequence of simulated job proc ClassAds.
            The first ClassAd will be the cluster ad plus a ProcId attribute

            :param int count: the queue count for each item in the from list, defaults to 1
            :param from: a iterator of strings or dictionaries containing the foreach data e.g. 'queue in' or 'queue from'
            :param int clusterid: the value to use for ClusterId when making job ads, defaults to 1
            :param int procid: the initial value for ProcId when making job ads, defaults to 0
            :param str qdate: a UNIX timestamp value for the QDATE attribute of the jobs, 0 means use the current time.
            :param str owner: a string value for the Owner attribute of the job
            :return: An iterator for the resulting job ads.

            :raises RuntimeError: if valid job ads cannot be made
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("count")=0, boost::python::arg("itemdata")=boost::python::object(), boost::python::arg("clusterid")=1, boost::python::arg("procid")=0, boost::python::arg("qdate")=0, boost::python::arg("owner")=std::string())
            )
        .def("itemdata", &Submit::iterqitems,
            R"C0ND0R(
            Create an iterator over itemdata derived from a queue statement.

            For example ``itemdata("matching *.dat")`` would return an iterator
            of filenames that end in ``.dat`` from the current directory.
            This is the same iterator used by ``condor_submit`` when processing
            ``QUEUE`` statements.

            :param str queue: a submit queue statement, or the arguments to a submit queue statement.
            :return: An iterator for the resulting items
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("qargs")=std::string())
            )
        .def("getQArgs", &Submit::getQArgs,
            R"C0ND0R(
            Returns arguments specified in the ``QUEUE`` statement passed to the contructor.
            These are the arguments that will be used by the :meth:`Submit.queue`
            and :meth:`Submit.queue_with_itemdata` methods if not overridden by arguments to those methods.
            )C0ND0R",
            boost::python::args("self"))
        .def("setQArgs", &Submit::setQArgs,
            R"C0ND0R(
            Sets the arguments to be used by
            subsequent calls to the :meth:`Submit.queue`
            and :meth:`Submit.queue_with_itemdata` methods
            if not overridden by arguments to those methods.

            :param str args: The arguments to pass to the ``QUEUE`` statement.
            )C0ND0R",
            boost::python::args("self", "args"))
        .def("__delitem__", &Submit::deleteItem)
        .def("__getitem__", &Submit::getItem)
        .def("__setitem__", &Submit::setItem)
        .def("__str__", &Submit::toString)
        .def("__repr__", &Submit::toRepr)
        .def("__iter__", &Submit::iter)
        .def("keys", &Submit::keys,
            R"C0ND0R(
            As :meth:`dict.keys`.
            )C0ND0R")
        .def("values", &Submit::values,
            R"C0ND0R(
            As :meth:`dict.values`.
            )C0ND0R")
        .def("items", &Submit::items,
            R"C0ND0R(
            As :meth:`dict.items`.
            )C0ND0R")
        .def("__len__", &Submit::size)
        .def("get", &Submit::get,
            R"C0ND0R(
            As :meth:`dict.get`.
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("default")=boost::python::object())
            )
        .def("setdefault", &Submit::setDefault,
            R"C0ND0R(
            As :meth:`dict.setdefault`.
            )C0ND0R")
        .def("update", &Submit::update,
            R"C0ND0R(
            As :meth:`dict.update`.
            )C0ND0R")
        .def("from_dag", &Submit::from_dag,
            R"C0ND0R(
            Constructs a new :class:`Submit` that could be used to submit the
            DAG described by the file at ``dag_filename``.

            This static method essentially does the first half of the work
            that ``condor_submit_dag`` does: it produces the submit description
            for the DAGMan job that will execute the DAG. However, in addition
            to writing this submit description to disk, it also produces a
            :class:`Submit` object with the same information that can be
            submitted via the normal bindings submit machinery.

            :param str filename: The path to the DAG description file.
            :param dict options: Additional arguments to ``condor_submit_dag``,
                such as ``maxidle`` or ``maxpost``, as key-value pairs, like
                ``{'maxidle': 10}``.
            :return: :class:`Submit` description for a ``.dag`` file
            )C0ND0R",
            (boost::python::arg("filename"), boost::python::arg("options")=boost::python::dict())
            )
        .staticmethod("from_dag")
        ;
    register_ptr_to_python< boost::shared_ptr<Submit> >();

    class_<SubmitResult>("SubmitResult", no_init)
        .def("__str__", &SubmitResult::toString)
        .def("cluster", &SubmitResult::cluster,
            R"C0ND0R(
            :return: the ClusterID of the submitted jobs.
            :rtype: int
            )C0ND0R",
            boost::python::args("self"))
        .def("clusterad", &SubmitResult::clusterad,
            R"C0ND0R(
            :return: the cluster Ad of the submitted jobs.
            :rtype: :class:`classad.ClassAd`
            )C0ND0R",
            boost::python::args("self"))
        .def("first_proc", &SubmitResult::first_procid,
            R"C0ND0R(
            :return: the first ProcID of the submitted jobs.
            :rtype: int
            )C0ND0R",
            boost::python::args("self"))
        .def("num_procs", &SubmitResult::num_procs,
            R"C0ND0R(
            :return: the number of submitted jobs.
            :rtype: int
            )C0ND0R",
            boost::python::args("self"))
        ;
    register_ptr_to_python< boost::shared_ptr<SubmitResult>>();

    class_<SubmitJobsIterator>("SubmitJobsIterator", no_init)
        .def(NEXT_FN, &SubmitJobsIterator::next, "return the next ad from Submit.jobs.")
        .def("__iter__", &SubmitJobsIterator::pass_through)
        .def("clusterad", &SubmitJobsIterator::clusterad,
            R"C0ND0R(
            Return the Cluster ad that proc ads should be chained to.
            )C0ND0R")
        ;

    class_<QueueItemsIterator>("QueueItemsIterator",
            R"C0ND0R(
            An iterator over itemdata produced by :meth:`Submit.itemdata`.
            )C0ND0R",
            no_init)
        .def(NEXT_FN, &QueueItemsIterator::next, "return the next item from a submit queue statement.")
        .def("__iter__", &QueueItemsIterator::pass_through)
        ;

    class_<RequestIterator>("ResourceRequestIterator", no_init)
        .def(NEXT_FN, &RequestIterator::next, "Get next resource request.")
        ;

    class_<HistoryIterator>("HistoryIterator",
        R"C0ND0R(
        An iterator over ads in the history produced by :meth:`Schedd.history`.
        )C0ND0R",
     no_init)
        .def(NEXT_FN, &HistoryIterator::next)
        .def("__iter__", &HistoryIterator::pass_through)
        ;

    class_<QueryIterator>("QueryIterator",
            R"C0ND0R(
            An iterator class for managing results of the :meth:`Schedd.query` and
            :meth:`Schedd.xquery` methods.
            )C0ND0R",
            no_init)
        .def(NEXT_FN, &QueryIterator::next,
            R"C0ND0R(
            :param mode: The blocking mode for this call to :meth:`next`; defaults
                to :attr:`~BlockingMode.Blocking`.
            :type mode: :class:`BlockingMode`
            :return: the next available job ad.
            :rtype: :class:`~classad.ClassAd`
            :raises StopIteration: when no additional ads are available.
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("mode") = Blocking)
#else
            (boost::python::arg("self"), boost::python::arg("mode") = Blocking)
#endif
            )
        .def("nextAdsNonBlocking", &QueryIterator::nextAds,
            R"C0ND0R(
            Retrieve as many ads are available to the iterator object.

            If no ads are available, returns an empty list.  Does not throw
            an exception if no ads are available or the iterator is finished.

            :return: Zero-or-more job ads.
            :rtype: list[:class:`~classad.ClassAd`]
            )C0ND0R",
            boost::python::args("self"))
        .def("tag", &QueryIterator::tag,
            R"C0ND0R(
            Retrieve the tag associated with this iterator; when using the :func:`poll` method,
            this is useful to distinguish multiple iterators.

            :return: The query's tag.
            )C0ND0R",
            boost::python::args("self"))
        .def("done", &QueryIterator::done,
            R"C0ND0R(
            :return: ``True`` if the iterator is finished; ``False`` otherwise.
            :rtype: bool
            )C0ND0R",
            boost::python::args("self"))
        .def("watch", &QueryIterator::watch,
            R"C0ND0R(
            Returns an ``inotify``-based file descriptor; if this descriptor is given
            to a ``select()`` instance, ``select`` will indicate this file descriptor is ready
            to read whenever there are more jobs ready on the iterator.

            If ``inotify`` is not available on this platform, this will return ``-1``.

            :return: A file descriptor associated with this query.
            :rtype: int
            )C0ND0R",
            boost::python::args("self"))
        .def("__iter__", &QueryIterator::pass_through)
        ;

    register_ptr_to_python< boost::shared_ptr<ScheddNegotiate> >();
    register_ptr_to_python< boost::shared_ptr<RequestIterator> >();
    register_ptr_to_python< boost::shared_ptr<HistoryIterator> >();
    register_ptr_to_python< boost::shared_ptr<QueryIterator> >();
    register_ptr_to_python< boost::shared_ptr<QueueItemsIterator> >();
    register_ptr_to_python< boost::shared_ptr<SubmitJobsIterator> >();

}
