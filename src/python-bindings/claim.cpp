
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

#include "htcondor.h"

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
            THROW_EX(HTCondorValueError, "No contact string in ClassAd");
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

        ClassAd ad, reply;
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
        if (m_claim.empty()) {THROW_EX(HTCondorValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        ClassAd reply;
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
        if (m_claim.empty()) {THROW_EX(HTCondorValueError, "No claim set for object.");}

        ClassAd ad = boost::python::extract<ClassAdWrapper>(ad_obj)();
        if (ad.find(ATTR_JOB_KEYWORD) == ad.end())
        {
            ad.InsertAttr(ATTR_HAS_JOB_AD, true);
        }

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        ClassAd reply;
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
        if (m_claim.empty()) {THROW_EX(HTCondorValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        ClassAd reply;
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
        if (m_claim.empty()) {THROW_EX(HTCondorValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        ClassAd reply;
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
        if (m_claim.empty()) {THROW_EX(HTCondorValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        ClassAd reply;
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
        if (m_claim.empty()) {THROW_EX(HTCondorValueError, "No claim set for object.");}

        DCStartd startd(m_addr.c_str());
        startd.setClaimId(m_claim);
        ClassAd reply;
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
        if (m_claim.empty()) {THROW_EX(HTCondorValueError, "No claim set for object.");}

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
        ClassAd reply;
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
    boost::python::enum_<VacateType>("VacateTypes",
            R"C0ND0R(
            Vacate policies that can be sent to a *condor_startd*.

            The values of the enumeration are:

            .. attribute:: Fast
            .. attribute:: Graceful
            )C0ND0R")
        .value("Fast", VACATE_FAST)
        .value("Graceful", VACATE_GRACEFUL)
        ;

    boost::python::class_<Claim>("Claim",
            R"C0ND0R(
            The :class:`Claim` class provides access to HTCondor's Compute-on-Demand
            facilities.  The class represents a claim of a remote resource; it allows
            the user to manually activate a claim (start a job) or release the associated
            resources.

            The claim comes with a finite lifetime - the *lease*.  The lease may be
            extended for as long as the remote resource (the Startd) allows.

            To create a :class:`Claim` object for a given remote resource,
            you need to provide an ad which contains a description of the resource,
            as returned by :meth:`Collector.locate`.

            This only stores the remote resource's location; it is not
            contacted until :meth:`requestCOD` is invoked.
            )C0ND0R",
        boost::python::init<boost::python::object>(
            R"C0ND0R(
            :param ad: An ad describing the Claim (optionally) and a Startd location.
            :type ad: :class:`~classad.ClassAd`
            )C0ND0R",
            boost::python::args("self", "ad")))
        .def(boost::python::init<>(boost::python::args("self")))
        .def("requestCOD", &Claim::requestCOD,
            R"C0ND0R(
            Request a claim from the *condor_startd* represented by this object.

            On success, the :class:`Claim` object will represent a valid claim on the
            remote startd; other methods, such as :meth:`activate` should now function.

            :param str constraint:  ClassAd expression that pecifies which slot in
                the startd should be claimed.  Defaults to ``'true'``, which will
                result in the first slot becoming claimed.
            :param int lease_duration: Indicates how long the claim should be valid.
                Defaults to ``-1``, which indicates to lease the resource for as long as the Startd allows.
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("constraint")=boost::python::object(), boost::python::arg("lease_duration")=-1)
#else
            (boost::python::arg("self"), boost::python::arg("constraint")=boost::python::object(), boost::python::arg("lease_duration")=-1)
#endif
            )
        .def("release", &Claim::release,
            R"C0ND0R(
            Release the remote *condor_startd* from this claim; shut down any running job.

            :param vacate_type: The type of vacate to perform for the
              running job.
            :type vacate_type: :class:`VacateTypes`
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("vacate_type")=VACATE_GRACEFUL)
#else
            (boost::python::arg("self"), boost::python::arg("vacate_type")=VACATE_GRACEFUL)
#endif
            )
        .def("activate", &Claim::activate,
            R"C0ND0R(
            Activate a claim using a given job ad.

            :param ad: Description of the job to launch; this uses similar, *but not identical*
                attribute names as *condor_submit*.  See
                the HTCondor manual for a description of the job language.
            )C0ND0R",
            boost::python::args("self", "ad"))
        .def("suspend", &Claim::suspend,
            R"C0ND0R(
            Temporarily suspend the remote execution of the COD application.
            On Unix systems, this is done using ``SIGSTOP``.
            )C0ND0R",
            boost::python::args("self"))
        .def("renew", &Claim::renew,
            R"C0ND0R(
            Renew the lease on an existing claim.
            The renewal should last for the value of the ``lease_duration``.
            )C0ND0R",
            boost::python::args("self"))
        .def("resume", &Claim::resume,
            R"C0ND0R(
            Resume the temporarily suspended execution.
            On Unix systems, this is done using ``SIGCONT``.
            )C0ND0R",
            boost::python::args("self"))
        .def("deactivate", &Claim::deactivate,
            R"C0ND0R(
            Deactivate a claim; shuts down the currently running job,
            but holds onto the claim for future activation.

            :param vacate_type: The type of vacate to perform for the
              running job.
            :type vacate_type: :class:`VacateTypes`
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("vacate_type")=VACATE_GRACEFUL))
        .def("delegateGSIProxy", &Claim::delegateGSI,
            R"C0ND0R(
            Send an X509 proxy credential to an activated claim.

            :param str filename: Filename of the X509 proxy to send to the active claim.
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("filename")=boost::python::object()))
        .def("__repr__", &Claim::toString)
        .def("__str__", &Claim::toString)
        ;
}
