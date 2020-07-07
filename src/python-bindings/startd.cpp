
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
struct Startd
{


    Startd() {}


    Startd(boost::python::object ad_obj)
    {
        ClassAdWrapper ad = boost::python::extract<ClassAdWrapper>(ad_obj);
        if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, m_addr))
        {
            THROW_EX(ValueError, "No contact string in ClassAd");
        }
    }

    std::string
    drain_jobs(int how_fast=DRAIN_GRACEFUL, bool resume_on_completion=false, boost::python::object check_obj=boost::python::object(""), boost::python::object start_obj = boost::python::object() )
    {
		std::string check_expr;
		if (check_obj.ptr() != Py_None) {
			boost::python::extract<std::string> expr_extract(check_obj);
			if (expr_extract.check())
			{
				check_expr = expr_extract();
			} else
			{
				classad::ClassAdUnParser printer;
				classad_shared_ptr<classad::ExprTree> expr(convert_python_to_exprtree(check_obj));
				printer.Unparse(check_expr, expr.get());
			}
		}
		const char * check_expr_ptr = nullptr;
		if ( ! check_expr.empty()) { check_expr_ptr = check_expr.c_str(); }

		std::string start_expr;
		boost::python::extract<std::string> start_extract( start_obj );
		if( start_extract.check() ) {
			start_expr = start_extract();
		} else {
			classad::ClassAdUnParser printer;
			classad_shared_ptr<classad::ExprTree> expr(convert_python_to_exprtree(start_obj));
			printer.Unparse( start_expr, expr.get());
		}

        std::string request_id;

        DCStartd startd(m_addr.c_str());
        bool rval = startd.drainJobs(how_fast, resume_on_completion, check_expr_ptr, start_expr.c_str(), request_id);
        if (!rval) {THROW_EX(RuntimeError, "Startd failed to begin draining jobs.");}
        return request_id;
    }

    void
    cancel_drain_jobs(boost::python::object rid=boost::python::object())
    {
        const char * request_id = NULL;
        std::string request_id_str;
        if (rid.ptr() == Py_None)
        {
            request_id = NULL;
        }
        else
        {
            request_id_str = boost::python::extract<std::string>(rid);
            request_id = request_id_str.c_str();
        }

        DCStartd startd(m_addr.c_str());
        bool rval = startd.cancelDrainJobs( request_id );
        if (!rval) {THROW_EX(RuntimeError, "Startd failed to cancel draining jobs.");}
    }

private:

    std::string m_addr;
};

// declare a formal enum so that python doesn't think all ints are DrainTypes (sigh)
enum DrainTypes {
	DrainFast=DRAIN_FAST,
	DrainGraceful=DRAIN_GRACEFUL,
	DrainQuick=DRAIN_QUICK,
};

void
export_startd()
{
    boost::python::enum_<DrainTypes>("DrainTypes")
        .value("Fast", (DrainTypes)DRAIN_FAST)
        .value("Graceful", (DrainTypes)DRAIN_GRACEFUL)
        .value("Quick", (DrainTypes)DRAIN_QUICK)
        ;

#if BOOST_VERSION >= 103400
    boost::python::docstring_options doc_options;
    doc_options.disable_cpp_signatures();
    #define SELFARG boost::python::arg("self"),
#else
    #define SELFARG
#endif
    
    boost::python::class_<Startd>("Startd", "A client class for controlling Startds in HTCondor")
        .def(boost::python::init<>())
        .def(boost::python::init<boost::python::object>(":param ad: An ad describing the Claim (optionally) and a Startd location."))

        .def("drainJobs", &Startd::drain_jobs, "Drain jobs from a startd.",
            ":param drain_type: Type of drain to perform (Fast, Graceful or Quick); must be from DrainTypes enum."
            ":param resume_on_completion: If true, startd will exit the draining state when draining completes.\n"
            ":param constraint: An optional check expression that must be true for all slots for draining to begin; defaults to 'true'\n"
            ":param start_expr: An optional expression that will be used as the START expression while draining; defaults to FALSE.\n"
            ":return: a drain request id that can be used to cancel draining.",
               (SELFARG
                boost::python::arg("drain_type")=DRAIN_GRACEFUL,
                boost::python::arg("resume_on_completion")=false,
                boost::python::arg("constraint")="true",
                boost::python::arg("start_expr")="false"
               )
            )

        .def("cancelDrainJobs", &Startd::cancel_drain_jobs, "Cancel draining jobs from a startd.",
            ":param request_id: optional, if specified cancels only the drain command with the given request_id.",
               (SELFARG
                boost::python::arg("request_id")=""
               )
           )
        ;
}

