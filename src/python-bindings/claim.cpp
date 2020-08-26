
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"

# if defined(__APPLE__)
# undef HAVE_SSIZE_T
# include <pyport.h>
# endif

#include "condor_common.h"

#include <boost/python/overloads.hpp>

#include "enum_utils.h"
#include "condor_attributes.h"
#include "dc_startd.h"
#include "globus_utils.h"
#include "classad/source.h"

#include "old_boost.h"
#include "module_lock.h"
#include "classad_wrapper.h"

/*
 * Object representing a claim on a remote startd.
 *
 * Purpose is twofold: one, to represent 'full' claims as found in the
 * collector.  Two, to provide a python binding for the COD API.
 */
struct Claim
{


    Claim() {}


    Claim(boost::python::object ad_obj)
    {
        ClassAdWrapper ad = boost::python::extract<ClassAdWrapper>(ad_obj);

        // TODO: Handle child d-slot claims, claim lists, etc
        // Note this may leave 'ad' blank.
        if (!ad.EvaluateAttrString(ATTR_CLAIM_ID, m_claim))
        {
            ad.EvaluateAttrString(ATTR_CAPABILITY, m_claim);
        }

        if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, m_addr))
        {
			// To quote the Python docs: "If you have an argument whose value
			// must be in a particular range or must satisfy other conditions,
			// PyExc_ValueError is appropriate."
            THROW_EX(ValueError, "No contact string in ClassAd");
        }
    }


    void
    requestCOD(boost::python::object constraint_obj, int lease_duration)
    {
        classad_shared_ptr<classad::ExprTree> constraint;
        boost::python::extract<std::string> constraint_extract(constraint_obj);
        if (constraint_obj.ptr() == Py_None) {}
        else if (constraint_extract.check())
        {
            classad::ClassAdParser parser;
            std::string constraint_str = constraint_extract();
            classad::ExprTree *expr_tmp = NULL;
            if (!parser.ParseExpression(constraint_str, expr_tmp)) {
            	THROW_EX(ClassAdParseError, "Failed to parse request requirements expression");
            }
            constraint.reset(expr_tmp);
        }
        else
        {
            constraint.reset(convert_python_to_exprtree(constraint_obj));
        }

        compat_classad::ClassAd ad, reply;
        if (constraint.get())
        {
            classad::ExprTree *expr_tmp = constraint->Copy();
            ad.Insert(ATTR_REQUIREMENTS, expr_tmp);
        }
        ad.InsertAttr(ATTR_JOB_LEASE_DURATION, lease_duration);
        bool rval;
        DCStartd startd(m_addr.c_str());
        {
            condor::ModuleLock ml;
            rval = startd.requestClaim(CLAIM_COD, &ad, &reply, 20);
        }
        if (!rval) {
        	THROW_EX(HTCondorIOError, "Failed to request claim from startd.");
        }

        if (!reply.EvaluateAttrString(ATTR_CLAIM_ID, m_claim)) {
        	THROW_EX(HTCondorIOError, "Startd did not return a ClaimId.");
        }
    }


    void
    release(VacateType vacate_type)
    {
        if (m_claim.empty()) {THROW_EX(ValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        compat_classad::ClassAd reply;
        bool rval;
        {
            condor::ModuleLock ml;
            rval = startd.releaseClaim(vacate_type, &reply, 20);
        }
        if (!rval) {
        	THROW_EX(HTCondorIOError, "Startd failed to release claim.");
        }

        m_claim = "";
    }


    void
    activate(boost::python::object ad_obj)
    {
        if (m_claim.empty()) {THROW_EX(ValueError, "No claim set for object.");}

        compat_classad::ClassAd ad = boost::python::extract<ClassAdWrapper>(ad_obj)();
        if (ad.find(ATTR_JOB_KEYWORD) == ad.end())
        {
            ad.InsertAttr(ATTR_HAS_JOB_AD, true);
        }

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        compat_classad::ClassAd reply;
        int irval;
        {
            condor::ModuleLock ml;
            irval = startd.activateClaim(&ad, &reply, 20);
        }
        if (irval != OK) {
        	THROW_EX(HTCondorIOError, "Startd failed to activate claim.");
        }
    }


    void
    deactivate(VacateType vacate_type)
    {
        if (m_claim.empty()) {THROW_EX(ValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        compat_classad::ClassAd reply;
        bool rval;
        {
            condor::ModuleLock ml;
            rval = startd.deactivateClaim(vacate_type, &reply, 20);
        }
        if (!rval) {
        	THROW_EX(HTCondorIOError, "Startd failed to deactivate claim.");
        }
    }


    void
    suspend()
    {
        if (m_claim.empty()) {THROW_EX(ValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        compat_classad::ClassAd reply;
        bool rval;
        {
            condor::ModuleLock ml;
            rval = startd.suspendClaim(&reply, 20);
        }
        if (!rval) {
        	THROW_EX(HTCondorIOError, "Startd failed to suspend claim.");
        }
    }


    void
    renew()
    {
        if (m_claim.empty()) {THROW_EX(ValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        compat_classad::ClassAd reply;
        bool rval;
        {
            condor::ModuleLock ml;
            rval = startd.renewLeaseForClaim(&reply, 20);
        }
        if (!rval) {
        	THROW_EX(HTCondorIOError, "Startd failed to renew claim.");
        }
    }


    void
    resume()
    {
        if (m_claim.empty()) {THROW_EX(ValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        compat_classad::ClassAd reply;
        bool rval;
        {
            condor::ModuleLock ml;
            rval = startd.resumeClaim(&reply, 20);
        }
        if (!rval) {
        	THROW_EX(HTCondorIOError, "Startd failed to resume claim.");
        }
    }


    void
    delegateGSI(boost::python::object fname)
    {
        if (m_claim.empty()) {THROW_EX(ValueError, "No claim set for object.");}

        std::string proxy_file;
        if (fname.ptr() == Py_None)
        {
            proxy_file = get_x509_proxy_filename();
        }
        else
        {
            proxy_file = boost::python::extract<std::string>(fname);
        }

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        compat_classad::ClassAd reply;
        int irval;
        {
            condor::ModuleLock ml;
            irval = startd.delegateX509Proxy(proxy_file.c_str(), 0, NULL);
        }
        if (irval != OK) {
        	THROW_EX(HTCondorIOError, "Startd failed to delegate GSI proxy.");
        }
    }


    std::string
    toString()
    {
        return m_claim.empty() ? ("Unclaimed startd at " + m_addr) : ("Claim " + m_claim);
    }


private:

    std::string m_claim;
    std::string m_addr;
};

void
export_claim()
{
    boost::python::enum_<VacateType>("VacateTypes")
        .value("Fast", VACATE_FAST)
        .value("Graceful", VACATE_GRACEFUL)
        ;

#if BOOST_VERSION >= 103400
    boost::python::docstring_options doc_options;
    doc_options.disable_cpp_signatures();
#endif

    boost::python::class_<Claim>("Claim", "A client class for Claims in HTCondor")
        .def(boost::python::init<>())
        .def(boost::python::init<boost::python::object>(":param ad: An ad describing the Claim (optionally) and a Startd location."))
        .def("requestCOD", &Claim::requestCOD, "Request a COD claim from the remote Startd."
            ":param constraint: A constraint on the remote slot to use (defaults to 'true')"
            ":param lease_duration: Time, in seconds, of the claim's lease.  Defaults to -1",
#if BOOST_VERSION < 103400
            (boost::python::arg("constraint")=boost::python::object(), boost::python::arg("lease_duration")=-1)
#else
            (boost::python::arg("self"), boost::python::arg("constraint")=boost::python::object(), boost::python::arg("lease_duration")=-1)
#endif
            )
        .def("release", &Claim::release, "Release startd from the claim.",
            ":param vacate_type: Type of vacate to perform (Fast or Graceful); must be from VacateTypes enum.",
#if BOOST_VERSION < 103400
            (boost::python::arg("vacate_type")=VACATE_GRACEFUL)
#else
            (boost::python::arg("self"), boost::python::arg("vacate_type")=VACATE_GRACEFUL)
#endif
            )
        .def("activate", &Claim::activate, "Activate an existing claim.\n"
            ":param ad: Ad describing job to activate.")
        .def("suspend", &Claim::suspend, "Suspend an activated claim.")
        .def("renew", &Claim::renew, "Renew the lease on an existing claim.")
        .def("resume", &Claim::resume, "Resume a suspended claim.")
        .def("deactivate", &Claim::deactivate, "Deactivate a claim.")
        .def("delegateGSIProxy", &Claim::delegateGSI, "Delegate a GSI proxy to the claim.\n"
            ":param filename: Filename of GSI proxy; defaults to the Globus proxy detection logic",
            (boost::python::arg("filename")=boost::python::object()))
        .def("__repr__", &Claim::toString)
        .def("__str__", &Claim::toString)
        ;
}
