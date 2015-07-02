
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

#include "enum_utils.h"
#include "condor_attributes.h"
#include "dc_startd.h"
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

        if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, m_addr) && !ad.EvaluateAttrString(ATTR_STARTD_IP_ADDR, m_addr))
        {
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
            if (!parser.ParseExpression(constraint_str, expr_tmp)) {THROW_EX(ValueError, "Failed to parse request requirements expression");}
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
        if (!rval) {THROW_EX(RuntimeError, "Failed to request claim from startd.");}

        if (!reply.EvaluateAttrString(ATTR_CLAIM_ID, m_claim)) {THROW_EX(RuntimeError, "Startd did not return a ClaimId.");}
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
        if (!rval) {THROW_EX(RuntimeError, "Startd failed to release claim.");}
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
    
}

