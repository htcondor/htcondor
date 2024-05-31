
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
#include "htcondor.h"
#include "daemon_location.h"

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
        THROW_EX(HTCondorInternalError, "Failed to allocate a new ClassAds expression.");
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
    case DT_GRIDMANAGER:
        ad_type = GRID_AD;
        break;
    default:
        THROW_EX(HTCondorEnumError, "Unknown daemon type.");
    }
    return ad_type;
}

struct Collector {

    Collector(boost::python::object pool = boost::python::object())
      : m_collectors(NULL), m_default(false)
    {
        std::string addr, version;
        int rv = construct_for_location(pool, DT_COLLECTOR, addr, version);
        if (rv == -2) { boost::python::throw_error_already_set(); } // pool was an invalid classad or DaemonLocation
        else if (rv == -1) { PyErr_Clear(); } // pool was not a classad or DaemonLocation, just fall thorough

        if (rv == 0) // pool is None
        {
            m_collectors = CollectorList::create();
            m_default = true;
        }
        else if (rv == 1)
        {
             // pool is actually a location ad or DaemonLocation, so addr was set
             m_collectors = CollectorList::create(addr.c_str());
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
            PyErr_Clear();
            std::vector<std::string> collector_list;
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
                    collector_list.emplace_back(pool_str);
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
            std::string pool_str = join(collector_list,",");
            m_collectors = CollectorList::create(pool_str.c_str());
        }
        if (!m_collectors)
        {
            // All of the above code paths set m_collectors.
            THROW_EX(HTCondorInternalError, "No collector specified");
        }
    }

    ~Collector()
    {
        if (m_collectors) delete m_collectors;
    }

    boost::python::object location() const {
		// PRAGMA_REMIND("TODO: handle the case of a non-default collector list")
        return boost::python::object();
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
        boost::python::list attrlist;
        attrlist.append("MyAddress");
        attrlist.append("AddressV1");
        attrlist.append(ATTR_CONDOR_VERSION);
        attrlist.append("CondorPlatform");
        attrlist.append("Name");
        attrlist.append("Machine");
        return query(ad_type, boost::python::object(""), attrlist, "");
    }

    object locate(daemon_t d_type, const std::string &name="")
    {
        if (!name.size()) {return locateLocal(d_type);}
        std::string constraint = "stricmp(" ATTR_NAME ", " + quote_classads_string(name) + ") == 0";
        boost::python::list attrlist;
        attrlist.append("MyAddress");
        attrlist.append("AddressV1");
        attrlist.append(ATTR_CONDOR_VERSION);
        attrlist.append("CondorPlatform");
        attrlist.append("Name");
        attrlist.append("Machine");
        object result = query_internal(convert_to_ad_type(d_type), boost::python::object(constraint), attrlist, "", name);
        if (py_len(result) >= 1) {
            return result[0];
        }
        THROW_EX(HTCondorLocateError, "Unable to find daemon.");
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
            THROW_EX(HTCondorLocateError, "Unable to find daemon.");
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
                    THROW_EX(HTCondorInternalError, "Unable to locate daemon address.");
                }
                std::string name = my_daemon.name() ? my_daemon.name() : "Unknown";
                if (!wrapper->InsertAttr(ATTR_NAME, name))
                {
                    THROW_EX(HTCondorInternalError, "Unable to insert daemon name.");
                }
                std::string hostname = my_daemon.fullHostname() ? my_daemon.fullHostname() : "Unknown";
                if (!wrapper->InsertAttr(ATTR_MACHINE, hostname))
                {
                    THROW_EX(HTCondorInternalError, "Unable to insert daemon hostname.");
                }
                std::string version = my_daemon.version() ? my_daemon.version() : "";
                if (!wrapper->InsertAttr(ATTR_VERSION, version))
                {
                    THROW_EX(HTCondorInternalError, "Unable to insert daemon version.");
                }
                const char * my_type = AdTypeToString(convert_to_ad_type(d_type));
                if (!my_type)
                {
                    THROW_EX(HTCondorEnumError, "Unable to determined daemon type.");
                }
                std::string my_type_str = my_type;
                if (!wrapper->InsertAttr(ATTR_MY_TYPE, my_type_str))
                {
                    THROW_EX(HTCondorInternalError, "Unable to insert daemon type.");
                }
                std::string cversion = CondorVersion(); std::string platform = CondorPlatform();
                if (!wrapper->InsertAttr(ATTR_VERSION, cversion) || !wrapper->InsertAttr(ATTR_PLATFORM, platform))
                {
                    THROW_EX(HTCondorInternalError, "Unable to insert HTCondor version.");
                }
            }
        }
        else
        {
            THROW_EX(HTCondorLocateError, "Unable to locate local daemon");
        }
        return boost::python::object(wrapper);
    }


    // TODO: this has crappy error handling when there are multiple collectors.
    void advertise(list ads, const std::string &command_str="UPDATE_AD_GENERIC", bool use_tcp=true)
    {
        std::unique_ptr<Sock> sock;

        int command = getCollectorCommandNum(command_str.c_str());
        if (command == -1)
        {
            THROW_EX(HTCondorEnumError, ("Invalid command " + command_str).c_str());
        }

        if (command == UPDATE_STARTD_AD_WITH_ACK)
        {
            THROW_EX(NotImplementedError, "Startd-with-ack protocol is not implemented at this time.");
        }

        int list_len = py_len(ads);
        if (!list_len)
            return;

        ClassAd ad;
        for (auto& collector : m_collectors->getList())
        {
            if(!collector->locate()) {
                THROW_EX(HTCondorLocateError, "Unable to locate collector.");
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
                    THROW_EX(HTCondorIOError, "Failed to advertise to collector");
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
        if ( ! convert_python_to_constraint(constraint_obj, constraint, true, NULL)) {
            THROW_EX(HTCondorValueError, "Invalid constraint.");
        }

        CondorQuery query(ad_type);
        if (constraint.length())
        {
            query.addANDConstraint(constraint.c_str());
        }
        if (statistics.size())
        {
            query.addExtraAttributeString("STATISTICS_TO_PUBLISH", statistics);
        }
        if (locationName.size())
        {
            std::string result = quote_classads_string(locationName);
            query.addExtraAttribute(ATTR_LOCATION_QUERY, result.c_str());
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

        // We think that these can't happen.
        // case Q_INVALID_CATEGORY:
        // case Q_MEMORY_ERROR:
        // case Q_PARSE_ERROR:

        case Q_COMMUNICATION_ERROR:
            THROW_EX(HTCondorIOError, "Failed communication with collector.");
        case Q_INVALID_QUERY:
            THROW_EX(HTCondorIOError, "Invalid query.");
        case Q_NO_COLLECTOR_HOST:
            THROW_EX(HTCondorLocateError, "Unable to determine collector host.");

        default:
            THROW_EX(HTCondorInternalError, "Unknown error from collector query.");
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
    class_<Collector>("Collector",
            R"C0ND0R(
            Client object for a remote *condor_collector*.
            The :class:`Collector` can be used to:

            * Locate a daemon.
            * Query the *condor_collector* for one or more specific ClassAds.
            * Advertise a new ad to the *condor_collector*.
            )C0ND0R",
        init<boost::python::object>(
            boost::python::args("self", "pool"),
            R"C0ND0R(
            :param pool: A ``host:port`` pair specified for the remote collector
              (or a list of pairs for HA setups). If omitted, the value of
              configuration parameter ``COLLECTOR_HOST`` is used.
            :type pool: str or list[str]
            )C0ND0R"))
        .def(init<>(boost::python::args("self")))
        .add_property("location", &Collector::location,
            R"C0ND0R(
            The collector to query.  None indicates use the default collector list specified in :macro:`COLLECTOR_HOST`
            :rtype: None or :class:`~htcondor.DaemonLocation`
            )C0ND0R") // PRAGMA_REMIND("TODO: remove this or handle a list of collectors")
        .def("query", &Collector::query, query_overloads(
            R"C0ND0R(
            Query the contents of a condor_collector daemon. Returns a list of ClassAds that match the constraint parameter.

            :param ad_type: The type of ClassAd to return. If not specified, the type will be ANY_AD.
            :type ad_type: :class:`AdTypes`
            :param constraint: A constraint for the collector query; only ads matching this constraint are returned.
                If not specified, all matching ads of the given type are returned.
            :type constraint: str or :class:`~classad.ExprTree`
            :param projection: A list of attributes to use for the projection.  Only these attributes, plus a few server-managed,
                are returned in each :class:`~classad.ClassAd`.
            :type projection: list[str]
            :param list[str] statistics: Statistics attributes to include, if they exist for the specified daemon.
            :return: A list of matching ads.
            :rtype: list[:class:`~classad.ClassAd`]
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("ad_type")=ANY_AD, boost::python::arg("constraint")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#else
            (boost::python::arg("self"), boost::python::arg("ad_type")=ANY_AD, boost::python::arg("constraint")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#endif
             ))
        .def("directQuery", &Collector::directquery, directquery_overloads(
            R"C0ND0R(
            Query the specified daemon directly for a ClassAd, instead of using the ClassAd from the *condor_collector* daemon.
            Requires the client library to first locate the daemon in the collector, then querying the remote daemon.

            :param daemon_type: Specifies the type of the remote daemon to query.
            :type daemon_type: :class:`DaemonTypes`
            :param str name: Specifies the daemon's name. If not specified, the local daemon is used.
            :param projection: is a list of attributes requested, to obtain only a subset of the attributes from the daemon's :class:`~classad.ClassAd`.
            :type projection: list[str]
            :param statistics: Statistics attributes to include, if they exist for the specified daemon.
            :type statistics: str
            :return: The ad of the specified daemon.
            :rtype: :class:`~classad.ClassAd`
            )C0ND0R",
#if BOOST_VERSION < 103400
            (boost::python::arg("daemon_type"), boost::python::arg("name")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#else
            (boost::python::arg("self"), boost::python::arg("daemon_type"), boost::python::arg("name")="", boost::python::arg("projection")=boost::python::list(), boost::python::arg("statistics")="")
#endif
            ))
        .def("locate", &Collector::locate, locate_overloads(
            (boost::python::arg("self"), boost::python::arg("daemon_type"), boost::python::arg("name")),
            R"C0ND0R(
            Query the *condor_collector* for a particular daemon.

            :param daemon_type: The type of daemon to locate.
            :type daemon_type: :class:`DaemonTypes`
            :param str name: The name of daemon to locate. If not specified, it searches for the local daemon.
            :return: a minimal ClassAd of the requested daemon, sufficient only to contact the daemon;
                typically, this limits to the ``MyAddress`` attribute.
            :rtype: :class:`~classad.ClassAd`
            )C0ND0R"))
        .def("locateAll", &Collector::locateAll,
            R"C0ND0R(
            Query the condor_collector daemon for all ClassAds of a particular type. Returns a list of matching ClassAds.

            :param daemon_type: The type of daemon to locate.
            :type daemon_type: :class:`DaemonTypes`
            :return: Matching ClassAds
            :rtype: list[:class:`~classad.ClassAd`]
            )C0ND0R",
            (boost::python::arg("self"), boost::python::arg("daemon_type")))
        .def("advertise", &Collector::advertise, advertise_overloads(
            (boost::python::arg("self"), boost::python::arg("ad_list"), boost::python::arg("command")="UPDATE_AD_GENERIC", boost::python::arg("use_tcp")=true),
            R"C0ND0R(
            Advertise a list of ClassAds into the condor_collector.

            :param ad_list: :class:`~classad.ClassAds` to advertise.
            :type ad_list: list[:class:`~classad.ClassAds`]
            :param str command: An advertise command for the remote *condor_collector*.
                It defaults to ``UPDATE_AD_GENERIC``.
                Other commands, such as ``UPDATE_STARTD_AD``, may require different authorization levels with the remote daemon.
            :param bool use_tcp: When set to ``True``, updates are sent via TCP.  Defaults to ``True``.
            )C0ND0R"))
        ;
}
