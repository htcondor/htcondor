
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>

#include "condor_common.h"

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
#include "globus_utils.h"
#include "basename.h"

#include <classad/operators.h>

#include <boost/python.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/erase.hpp>

#include "old_boost.h"
#include "classad_wrapper.h"
#include "exprtree_wrapper.h"

using namespace boost::python;

#define DO_ACTION(action_name) \
    reason_str = extract<std::string>(reason); \
    if (use_ids) \
        result = schedd. action_name (&ids, reason_str.c_str(), NULL, AR_TOTALS); \
    else \
        result = schedd. action_name (constraint.c_str(), reason_str.c_str(), NULL, AR_TOTALS);

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
make_spool_remap(compat_classad::ClassAd& ad, const std::string &attr, const std::string &stream_attr, const std::string &working_name)
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
make_spool(compat_classad::ClassAd& ad)
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

std::auto_ptr<ExprTree>
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
    std::auto_ptr<ExprTree> result(reqs_copy);
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
        if (!getClassAd(m_sock.get(), *ad.get())) THROW_EX(RuntimeError, "Failed to recieve remote ad.");
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

struct QueryIterator
{
    QueryIterator(boost::shared_ptr<Sock> sock)
      : m_count(0), m_sock(sock)
    {}

    inline static boost::python::object pass_through(boost::python::object const& o) { return o; };

    boost::shared_ptr<ClassAdWrapper> next()
    {
        if (m_count < 0) THROW_EX(StopIteration, "All ads processed");

        boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
        if (!getClassAd(m_sock.get(), *ad.get())) THROW_EX(RuntimeError, "Failed to recieve remote ad.");
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
            //if (!ad->EvaluateAttrInt(ATTR_NUM_MATCHES, intVal) || (intVal != m_count)) THROW_EX(ValueError, "Incorrect number of ads returned");

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

struct query_process_helper
{
    object callable;
    list output_list;
};

void
query_process_callback(void * data, classad_shared_ptr<ClassAd> ad)
{
    if (PyErr_Occurred()) return;

    try
    {
        query_process_helper *helper = static_cast<query_process_helper *>(data);
        boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
        wrapper->CopyFrom(*ad.get());
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
}

struct Schedd {

    Schedd()
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
      : m_addr(), m_name("Unknown"), m_version("")
    {
        if (!ad.EvaluateAttrString(ATTR_SCHEDD_IP_ADDR, m_addr))
        {
            PyErr_SetString(PyExc_ValueError, "Schedd address not specified.");
            throw_error_already_set();
        }
        ad.EvaluateAttrString(ATTR_NAME, m_name);
        ad.EvaluateAttrString(ATTR_VERSION, m_version);
    }

    object query(const std::string &constraint="", list attrs=list(), object callback=object())
    {
        CondorQ q;

        if (constraint.size())
            q.addAND(constraint.c_str());

        StringList attrs_list(NULL, "\n");
        // Must keep strings alive; StringList does not create an internal copy.
        int len_attrs = py_len(attrs);
        std::vector<std::string> attrs_str; attrs_str.reserve(len_attrs);
        for (int i=0; i<len_attrs; i++)
        {
            std::string attrName = extract<std::string>(attrs[i]);
            attrs_str.push_back(attrName);
            attrs_list.append(attrs_str[i].c_str());
        }

        ClassAdList jobs;

        list retval;
        query_process_helper helper;
        helper.callable = callback;
        helper.output_list = retval;
        void *helper_ptr = static_cast<void *>(&helper);

        int fetchResult = q.fetchQueueFromHostAndProcess(m_addr.c_str(), attrs_list, query_process_callback, helper_ptr, true, NULL);

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
        default:
            PyErr_SetString(PyExc_IOError, "Failed to fetch ads from schedd.");
            throw_error_already_set();
            break;
        }

        return retval;
    }

    void reschedule()
    {
        DCSchedd schedd(m_addr.c_str());
        Stream::stream_type st = schedd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
        if (!schedd.sendCommand(RESCHEDULE, st, 0)) {
            dprintf(D_ALWAYS, "Can't send RESCHEDULE command to schedd.\n" );
        }
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
                result = schedd.holdJobs(&ids, reason_char, reason_code_char, NULL, AR_TOTALS);
            else
                result = schedd.holdJobs(constraint.c_str(), reason_char, reason_code_char, NULL, AR_TOTALS);
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
                result = schedd.vacateJobs(&ids, vacate_type, NULL, AR_TOTALS);
            else
                result = schedd.vacateJobs(constraint.c_str(), vacate_type, NULL, AR_TOTALS);
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
            PyErr_SetString(PyExc_RuntimeError, "Error when querying the schedd.");
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

    int submit(const ClassAdWrapper &wrapper, int count=1, bool spool=false, object ad_results=object())
    {
        ConnectionSentry sentry(*this); // Automatically connects / disconnects.

        int cluster = NewCluster();
        if (cluster < 0)
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create new cluster.");
            throw_error_already_set();
        }

        ClassAd ad;
        // Create a blank ad for job submission.
        ClassAd *tmpad = CreateJobAd(NULL, CONDOR_UNIVERSE_VANILLA, "/bin/echo");
        if (tmpad)
        {
            ad.CopyFrom(*tmpad);
            delete tmpad;
        }
        else
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create a new job ad.");
            throw_error_already_set();
        }
        char path[4096];
        if (getcwd(path, 4095))
        {
            ad.InsertAttr(ATTR_JOB_IWD, path);
        }

        // Copy the attributes specified by the invoker.
        ad.Update(wrapper);

        ShouldTransferFiles_t should = STF_IF_NEEDED;
        std::string should_str;
        if (ad.EvaluateAttrString(ATTR_SHOULD_TRANSFER_FILES, should_str))
        {
            if (should_str == "YES")
                should = STF_YES;
            else if (should_str == "NO")
                should = STF_NO;
        }

        ExprTree *old_reqs = ad.Lookup(ATTR_REQUIREMENTS);
        ExprTree *new_reqs = make_requirements(old_reqs, should).release();
        ad.Insert(ATTR_REQUIREMENTS, new_reqs);

        if (spool)
            make_spool(ad);

	bool keep_results = false;
        extract<list> try_ad_results(ad_results);
        if (try_ad_results.check())
            keep_results = true;

        for (int idx=0; idx<count; idx++)
        {
            int procid = NewProc (cluster);
            if (procid < 0)
            {
                PyErr_SetString(PyExc_RuntimeError, "Failed to create new proc id.");
                throw_error_already_set();
            }
            ad.InsertAttr(ATTR_CLUSTER_ID, cluster);
            ad.InsertAttr(ATTR_PROC_ID, procid);

            classad::ClassAdUnParser unparser;
            unparser.SetOldClassAd( true );
            for (classad::ClassAd::const_iterator it = ad.begin(); it != ad.end(); it++)
            {
                std::string rhs;
                unparser.Unparse(rhs, it->second);
                if (-1 == SetAttribute(cluster, procid, it->first.c_str(), rhs.c_str(), SetAttribute_NoAck))
                {
                    PyErr_SetString(PyExc_ValueError, it->first.c_str());
                    throw_error_already_set();
                }
            }
            if (keep_results)
            {
                boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
                wrapper->CopyFrom(ad);
                ad_results.attr("append")(wrapper);
            }
        }

        if (param_boolean("SUBMIT_SEND_RESCHEDULE",true))
        {
            reschedule();
        }
        return cluster;
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
            job_array[i] = job_tmp_array[i].get();
        }
        CondorError errstack;
        bool result;
        DCSchedd schedd(m_addr.c_str());
        result = schedd.spoolJobFiles( len,
                                       &job_array[0],
                                       &errstack );
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
        if (!schedd.receiveJobSandbox(jobs.c_str(), &errstack))
            THROW_EX(RuntimeError, errstack.getFullText(true).c_str());
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
        if (do_delegation && !schedd.delegateGSIcredential(cluster, proc, proxy_filename.c_str(), lifetime ? now+lifetime : 0,
                                                           &result_expiration, &errstack))
        {
            THROW_EX(RuntimeError, errstack.getFullText(true).c_str());
        }
        else if (!do_delegation)
        {
            if (!schedd.updateGSIcredential(cluster, proc, proxy_filename.c_str(), &errstack))
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

        if (use_ids)
        {
            for (unsigned idx=0; idx<clusters.size(); idx++)
            {
                if (-1 == SetAttribute(clusters[idx], procs[idx], attr.c_str(), val_str.c_str()))
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to edit job");
                    throw_error_already_set();
                }
            }
        }
        else
        {
            if (-1 == SetAttributeByConstraint(constraint.c_str(), attr.c_str(), val_str.c_str()))
            {
                PyErr_SetString(PyExc_RuntimeError, "Unable to edit jobs matching constraint");
                throw_error_already_set();
            }
        }
    }

    boost::shared_ptr<HistoryIterator> history(boost::python::object requirement, boost::python::list projection=boost::python::list(), int match=-1)
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

	classad::ClassAd ad;
	ad.Insert(ATTR_REQUIREMENTS, expr_copy);
	ad.InsertAttr(ATTR_NUM_MATCHES, match);

	classad::ExprTree *projTree = static_cast<classad::ExprTree*>(projList);
	ad.Insert(ATTR_PROJECTION, projTree);

	DCSchedd schedd(m_addr.c_str());
	Sock* sock;
	if (!(sock = schedd.startCommand(QUERY_SCHEDD_HISTORY, Stream::reli_sock, 0))) {
		THROW_EX(RuntimeError, "Unable to connect to schedd");
	}
	boost::shared_ptr<Sock> sock_sentry(sock);

	if (!putClassAd(sock, ad) || !sock->end_of_message()) THROW_EX(RuntimeError, "Unable to send request classad to schedd");

        boost::shared_ptr<HistoryIterator> iter(new HistoryIterator(sock_sentry));
        return iter;
    }

    boost::shared_ptr<QueryIterator> xquery(boost::python::object requirement=boost::python::object(), boost::python::list projection=boost::python::list(), int match=-1)
    {

        std::string val_str;

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

        classad::ClassAd ad;
        ad.Insert(ATTR_REQUIREMENTS, expr_copy);
        ad.InsertAttr(ATTR_NUM_MATCHES, match);

        classad::ExprTree *projTree = static_cast<classad::ExprTree*>(projList);
        ad.Insert(ATTR_PROJECTION, projTree);

        DCSchedd schedd(m_addr.c_str());
        Sock* sock;
        if (!(sock = schedd.startCommand(QUERY_JOB_ADS, Stream::reli_sock, 0))) {
                THROW_EX(RuntimeError, "Unable to connect to schedd");
        }
        boost::shared_ptr<Sock> sock_sentry(sock);

        if (!putClassAd(sock, ad) || !sock->end_of_message()) THROW_EX(RuntimeError, "Unable to send request classad to schedd");

        boost::shared_ptr<QueryIterator> iter(new QueryIterator(sock_sentry));
        return iter;
    }

private:
    struct ConnectionSentry
    {
    public:
        ConnectionSentry(Schedd &schedd) : m_connected(false)
        {
            if (ConnectQ(schedd.m_addr.c_str(), 0, false, NULL, NULL, schedd.m_version.c_str()) == 0)
            {
                PyErr_SetString(PyExc_RuntimeError, "Failed to connect to schedd.");
                throw_error_already_set();
            }
            m_connected = true;
        }

        void disconnect()
        {
            if (m_connected && !DisconnectQ(NULL))
            {
                m_connected = false;
                PyErr_SetString(PyExc_RuntimeError, "Failed to commmit and disconnect from queue.");
                throw_error_already_set();
            }
            m_connected = false;
        }

        ~ConnectionSentry()
        {
            disconnect();
        }
    private:
        bool m_connected;
    };

    std::string m_addr, m_name, m_version;
};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(query_overloads, query, 0, 3);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(xquery_overloads, xquery, 0, 3);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(submit_overloads, submit, 1, 4);

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

    class_<Schedd>("Schedd", "A client class for the HTCondor schedd")
        .def(init<const ClassAdWrapper &>(":param ad: An ad containing the location of the schedd"))
        .def("query", &Schedd::query, query_overloads("Query the HTCondor schedd for jobs.\n"
            ":param constraint: An optional constraint for filtering out jobs; defaults to 'true'\n"
            ":param attr_list: A list of attributes for the schedd to project along.  Defaults to having the schedd return all attributes.\n"
            ":param callback: A callback function to be invoked for each ad; the return value (if not None) is added to the list.\n"
            ":return: A list of matching jobs, containing the requested attributes."))
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
            ":return: Newly created cluster ID."))
        .def("spool", &Schedd::spool, "Spool a list of given ads to the remote HTCondor schedd.\n"
            ":param ads: A python list containing one or more ads to spool.\n")
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
            ":return: An iterator for the matching job ads")
        .def("xquery", &Schedd::xquery, xquery_overloads("Query HTCondor schedd, returning an iterator.\n"
            ":param requirements: Either a ExprTree or a string that can be parsed as an expression; requirements all returned jobs should match.\n"
            ":param projection: The attributes to return; an empty list signifies all attributes.\n"
            ":param match: Number of matches to return.\n"
            ":return: An iterator for the matching job ads"))
        .def("refreshGSIProxy", &Schedd::refreshGSIProxy, "Refresh the GSI proxy for a given job\n"
            ":param cluster: Job cluster.\n"
            ":param proc: Job proc.\n"
            ":param filename: Filename of proxy to delegate or upload to job.\n"
            ":param lifetime: Desired lifetime (in seconds) of delegated proxy; 0 indicates to not shorten"
            " the proxy lifetime.  -1 indicates to use the value of parameter DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME."
            " NOTE: depending on the lifetime of the proxy in `filename`, the resulting lifetime may be shorter"
            " than the desired lifetime.\n"
            ":return: Lifetime of the resulting job proxy in seconds.")
        ;

    class_<HistoryIterator>("HistoryIterator", no_init)
        .def("next", &HistoryIterator::next)
        .def("__iter__", &HistoryIterator::pass_through)
        ;

    class_<QueryIterator>("QueryIterator", no_init)
        .def("next", &QueryIterator::next)
        .def("__iter__", &QueryIterator::pass_through)
        ;

    register_ptr_to_python< boost::shared_ptr<HistoryIterator> >();
    register_ptr_to_python< boost::shared_ptr<QueryIterator> >();

}
