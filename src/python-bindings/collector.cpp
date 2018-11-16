
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"

#include "condor_adtypes.h"
#include "dc_collector.h"
#include "condor_version.h"

#include <memory>

#include "old_boost.h"
#include "classad_wrapper.h"

#include "module_lock.h"

using namespace boost::python;


// It's not clear how bulletproof various condor internals are against improperly quoted input.
// ClassAds is clumsy at this, but this function will properly quote a string.
//
static std::string
quote_classads_string(const std::string &input)
{
    classad::Value val; val.SetStringValue(input);
    classad_shared_ptr<classad::ExprTree> expr(classad::Literal::MakeLiteral(val));
    if (!expr.get())
    {
        THROW_EX(MemoryError, "Failed to allocate a new ClassAds expression.");
    }
    classad::ClassAdUnParser sink;
    std::string result;
    sink.Unparse(result, expr.get());
    return result;
}


AdTypes convert_to_ad_type(daemon_t d_type)
{
    AdTypes ad_type = NO_AD;
    switch (d_type)
    {
    case DT_MASTER:
        ad_type = MASTER_AD;
        break;
    case DT_STARTD:
        ad_type = STARTD_AD;
        break;
    case DT_SCHEDD:
        ad_type = SCHEDD_AD;
        break;
    case DT_NEGOTIATOR:
        ad_type = NEGOTIATOR_AD;
        break;
    case DT_COLLECTOR:
        ad_type = COLLECTOR_AD;
        break;
    case DT_GENERIC:
        ad_type = GENERIC_AD;
		break;
    case DT_HAD:
        ad_type = HAD_AD;
		break;
    case DT_CREDD:
        ad_type = CREDD_AD;
        break;
    default:
        PyErr_SetString(PyExc_ValueError, "Unknown daemon type.");
        throw_error_already_set();
    }
    return ad_type;
}

struct Collector {

    Collector(boost::python::object pool = boost::python::object())
      : m_collectors(NULL), m_default(false)
    {
        if (pool.ptr() == Py_None)
        {
            m_collectors = CollectorList::create();
            m_default = true;
        }
        else if (PyBytes_Check(pool.ptr()) || PyUnicode_Check(pool.ptr()))
        {
            std::string pool_str = boost::python::extract<std::string>(pool);
            if (pool_str.size())
            {
                m_collectors = CollectorList::create(pool_str.c_str());
            }
            else
            {
                m_collectors = CollectorList::create();
                m_default = true;
            }
        }
        else
        {
            StringList collector_list;
            boost::python::object my_iter = pool.attr("__iter__")();
            if (!PyIter_Check(my_iter.ptr())) {
                PyErr_Format(PyExc_TypeError,
                             "__iter__ returned non-iterator "
                             "of type '%.100s'",
                             my_iter.ptr()->ob_type->tp_name);
                boost::python::throw_error_already_set();
            }
            while (true)
            {
                try
                {
                    boost::python::object next_obj = my_iter.attr(NEXT_FN)();
                    std::string pool_str = boost::python::extract<std::string>(next_obj);
                    collector_list.append(pool_str.c_str());
                }
                catch (const boost::python::error_already_set&)
                {
                    if (PyErr_ExceptionMatches(PyExc_StopIteration))
                    {
                        PyErr_Clear();
                        break;
                    }
                    else
                    {
                        boost::python::throw_error_already_set();
                    }
                }
            }
            char * pool_str = collector_list.print_to_string();
            m_collectors = CollectorList::create(pool_str);
            free(pool_str);
        }
        if (!m_collectors)
        {
            THROW_EX(ValueError, "No collector specified");
        }
    }

    ~Collector()
    {
        if (m_collectors) delete m_collectors;
    }

    boost::python::object directquery(daemon_t d_type, const std::string &name="", boost::python::list attrs=boost::python::list(), const std::string &statistics="")
    {
        boost::python::object daemon_ad = locate(d_type, name);
        Collector daemon(daemon_ad[ATTR_MY_ADDRESS]);
        return daemon.query(convert_to_ad_type(d_type), boost::python::object(""), attrs, statistics)[0];
    }


    boost::python::object query(AdTypes ad_type=ANY_AD, boost::python::object constraint_obj=boost::python::object(""), boost::python::list attrs=boost::python::list(), const std::string &statistics="")
    {
        return query_internal(ad_type, constraint_obj, attrs, statistics, "");
    }


    object locateAll(daemon_t d_type)
    {
        AdTypes ad_type = convert_to_ad_type(d_type);
        return query(ad_type, boost::python::object(""), list(), "");
    }

    object locate(daemon_t d_type, const std::string &name="")
    {
        if (!name.size()) {return locateLocal(d_type);}
        std::string constraint = "stricmp(" ATTR_NAME ", " + quote_classads_string(name) + ") == 0";
        boost::python::list attrlist;
        attrlist.append("MyAddress");
        attrlist.append("AddressV1");
        attrlist.append("CondorVersion");
        attrlist.append("CondorPlatform");
        attrlist.append("Name");
        attrlist.append("Machine");
        object result = query_internal(convert_to_ad_type(d_type), boost::python::object(constraint), attrlist, "", name);
        if (py_len(result) >= 1) {
            return result[0];
        }
        PyErr_SetString(PyExc_ValueError, "Unable to find daemon.");
        throw_error_already_set();
        return object();
    }

    object locateLocal(daemon_t d_type)
    {
        if (!m_default)
        {
            std::string constraint = "true";
            object result = query(convert_to_ad_type(d_type), boost::python::object(constraint), list(), "");
            if (py_len(result) >= 1) {
                return result[0];
            }
            PyErr_SetString(PyExc_ValueError, "Unable to find daemon.");
            throw_error_already_set();
        }

        Daemon my_daemon( d_type, 0, 0 );

        boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
        if (my_daemon.locate())
        {
			/***  Note: calls to Daemon::locate() cannot invoke daemonAd() anymore.
             *** classad::ClassAd *daemonAd;
             *** if ((daemonAd = my_daemon.daemonAd()))
             *** {
             ***   wrapper->CopyFrom(*daemonAd);
             *** }
             *** else
			 ***/
            {
                std::string addr = my_daemon.addr();
                if (!my_daemon.addr() || !wrapper->InsertAttr(ATTR_MY_ADDRESS, addr))
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to locate daemon address.");
                    throw_error_already_set();
                }
                std::string name = my_daemon.name() ? my_daemon.name() : "Unknown";
                if (!wrapper->InsertAttr(ATTR_NAME, name))
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to insert daemon name.");
                    throw_error_already_set();
                }
                std::string hostname = my_daemon.fullHostname() ? my_daemon.fullHostname() : "Unknown";
                if (!wrapper->InsertAttr(ATTR_MACHINE, hostname))
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to insert daemon hostname.");
                    throw_error_already_set();
                }
                std::string version = my_daemon.version() ? my_daemon.version() : "";
                if (!wrapper->InsertAttr(ATTR_VERSION, version))
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to insert daemon version.");
                    throw_error_already_set();
                }
                const char * my_type = AdTypeToString(convert_to_ad_type(d_type));
                if (!my_type)
                {
                    PyErr_SetString(PyExc_ValueError, "Unable to determined daemon type.");
                    throw_error_already_set();
                }
                std::string my_type_str = my_type;
                if (!wrapper->InsertAttr(ATTR_MY_TYPE, my_type_str))
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to insert daemon type.");
                    throw_error_already_set();
                }
                std::string cversion = CondorVersion(); std::string platform = CondorPlatform();
                if (!wrapper->InsertAttr(ATTR_VERSION, cversion) || !wrapper->InsertAttr(ATTR_PLATFORM, platform))
                {
                    PyErr_SetString(PyExc_RuntimeError, "Unable to insert HTCondor version.");
                    throw_error_already_set();
                }
            }
        }
        else
        {
            PyErr_SetString(PyExc_RuntimeError, "Unable to locate local daemon");
            boost::python::throw_error_already_set();
        }
        return boost::python::object(wrapper);
    }


    // TODO: this has crappy error handling when there are multiple collectors.
    void advertise(list ads, const std::string &command_str="UPDATE_AD_GENERIC", bool use_tcp=false)
    {
        m_collectors->rewind();
        Daemon *collector;
        std::unique_ptr<Sock> sock;

        int command = getCollectorCommandNum(command_str.c_str());
        if (command == -1)
        {
            PyErr_SetString(PyExc_ValueError, ("Invalid command " + command_str).c_str());
            throw_error_already_set();
        }

        if (command == UPDATE_STARTD_AD_WITH_ACK)
        {
            PyErr_SetString(PyExc_NotImplementedError, "Startd-with-ack protocol is not implemented at this time.");
        }

        int list_len = py_len(ads);
        if (!list_len)
            return;

        compat_classad::ClassAd ad;
        while (m_collectors->next(collector))
        {
            if(!collector->locate()) {
                PyErr_SetString(PyExc_ValueError, "Unable to locate collector.");
                throw_error_already_set();
            }
            int list_len = py_len(ads);
            sock.reset();
            for (int i=0; i<list_len; i++)
            {
                const ClassAdWrapper wrapper = extract<const ClassAdWrapper>(ads[i]);
                ad.CopyFrom(wrapper);
                int result = 0;
                {
                condor::ModuleLock ml;
                if (use_tcp)
                {
                    if (!sock.get())
                        sock.reset(collector->startCommand(command,Stream::reli_sock,20));
                    else
                    {
                        sock->encode();
                        sock->put(command);
                    }
                }
                else
                {
                    sock.reset(collector->startCommand(command,Stream::safe_sock,20));
                }
                if (sock.get()) {
                    result += putClassAd(sock.get(), ad);
                    result += sock->end_of_message();
                }
                }
                if (result != 2) {
                    PyErr_SetString(PyExc_ValueError, "Failed to advertise to collector");
                    throw_error_already_set();
                }
            }
            sock->encode();
            sock->put(DC_NOP);
            sock->end_of_message();
        }
    }

private:

    object query_internal(AdTypes ad_type, boost::python::object constraint_obj, boost::python::list attrs, const std::string &statistics, std::string locationName)
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


        CondorQuery query(ad_type);
        if (constraint.length())
        {
            query.addANDConstraint(constraint.c_str());
        }
        if (statistics.size())
        {
            std::string result = quote_classads_string(statistics);
            result = "STATISTICS_TO_PUBLISH = " + result;
            query.addExtraAttribute(result.c_str());
        }
        if (locationName.size())
        {
            std::string result = quote_classads_string(locationName);
            result = "LocationQuery = " + result;
            query.addExtraAttribute(result.c_str());
        }

        int len_attrs = py_len(attrs);
        if (len_attrs)
        {
            std::vector<std::string> attrs_str;
            attrs_str.reserve(len_attrs);
            for (int i=0; i<len_attrs; i++)
            {
                std::string str = extract<std::string>(attrs[i]);
                attrs_str.push_back(str);
            }
            query.setDesiredAttrs(attrs_str);
        }

        ClassAdList adList;
        QueryResult result;
        {
        condor::ModuleLock ml;
        result = m_collectors->query(query, adList, NULL);
        }

        switch (result)
        {
        case Q_OK:
            break;
        case Q_INVALID_CATEGORY:
            PyErr_SetString(PyExc_RuntimeError, "Category not supported by query type.");
            boost::python::throw_error_already_set();
        case Q_MEMORY_ERROR:
            PyErr_SetString(PyExc_MemoryError, "Memory allocation error.");
            boost::python::throw_error_already_set();
        case Q_PARSE_ERROR:
            PyErr_SetString(PyExc_SyntaxError, "Query constraints could not be parsed.");
            boost::python::throw_error_already_set();
        case Q_COMMUNICATION_ERROR:
            PyErr_SetString(PyExc_IOError, "Failed communication with collector.");
            boost::python::throw_error_already_set();
        case Q_INVALID_QUERY:
            PyErr_SetString(PyExc_RuntimeError, "Invalid query.");
            boost::python::throw_error_already_set();
        case Q_NO_COLLECTOR_HOST:
            PyErr_SetString(PyExc_RuntimeError, "Unable to determine collector host.");
            boost::python::throw_error_already_set();
        default:
            PyErr_SetString(PyExc_RuntimeError, "Unknown error from collector query.");
            boost::python::throw_error_already_set();
        }

        list retval;
        ClassAd * ad;
        adList.Open();
        while ((ad = adList.Next()))
        {
            boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
            wrapper->CopyFrom(*ad);
            retval.append(wrapper);
        }
        return retval;
    }


    CollectorList *m_collectors;
    bool m_default;

};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(advertise_overloads, advertise, 1, 3);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(query_overloads, query, 0, 4);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(directquery_overloads, directquery, 1, 4);
BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(locate_overloads, locate, 1, 2);


void export_collector()
{
    class_<Collector>("Collector", "Client-side operations for the HTCondor collector")
        .def(init<boost::python::object>(":param pool: Name of collector to query; if not specified, uses the local one."))
        .def("query", &Collector::query, query_overloads(
            "Query the contents of a collector.\n"
            ":param ad_type: Type of ad to return from the AdTypes enum; if not specified, uses ANY_AD.\n"
            ":param constraint: A constraint for the ad query; defaults to true.\n"
            ":param projection: A list of attributes; if specified, the returned ads will be "
            "projected along these attributes.\n"
            ":param statistics: A list of additional statistics to return, if the remote daemon has any.\n"
            ":return: A list of ads in the collector matching the constraint.",
#if BOOST_VERSION < 103400
            (boost::python::arg("ad_type")=ANY_AD, boost::python::arg("constraint")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#else
            (boost::python::arg("self"), boost::python::arg("ad_type")=ANY_AD, boost::python::arg("constraint")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#endif
             ))
        .def("directQuery", &Collector::directquery, directquery_overloads(
            "Query a given daemon directly instead of returning a collector ad.\n"
            "This may return more up-to-date or detailed information than what is in the collector.\n"
            ":param daemon_type: Type of daemon; must be from the DaemonTypes enum.\n"
            ":param name: Name of daemon to locate.  If not specified, it searches for the local daemon.\n"
            ":param projection: A list of attributes; if specified, the returned ads will be "
            "projected along these attributes.\n"
            ":param statistics: A list of additional statistics to return, if the remote daemon has any.\n"
            ":return: The ad of the matching daemon, from the daemon itself.\n",
#if BOOST_VERSION < 103400
            (boost::python::arg("daemon_type"), boost::python::arg("name")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#else
            (boost::python::arg("self"), boost::python::arg("daemon_type"), boost::python::arg("name")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#endif
            ))
        .def("locate", &Collector::locate, locate_overloads(
            "Query the collector for a particular daemon.\n"
            ":param daemon_type: Type of daemon; must be from the DaemonTypes enum.\n"
            ":param name: Name of daemon to locate.  If not specified, it searches for the local daemon.\n"
            ":return: The ad of the corresponding daemon."))
        .def("locateAll", &Collector::locateAll,
            "Query the collector for all ads of a particular type.\n"
            ":param daemon_type: Type of daemon; must be from the DaemonTypes enum.\n"
            ":return: A list of matching ads.")
        .def("advertise", &Collector::advertise, advertise_overloads(
            "Advertise a list of ClassAds into the collector.\n"
            ":param ad_list: A list of ClassAds.\n"
            ":param command: A command for the collector; defaults to UPDATE_AD_GENERIC;"
            " other commands, such as UPDATE_STARTD_AD, may require reduced authorization levels.\n"
            ":param use_tcp: When set to true, updates are sent via TCP."))
        ;
}
