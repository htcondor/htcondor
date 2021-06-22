
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

static boost::python::list
toList(const boost::shared_ptr<classad::ClassAd> ad, const std::vector<std::string> &attrs)
{
    int idx = 1;
    bool hasattr = true;
    boost::python::list result;
    while (hasattr)
    {
        hasattr = false;
        boost::shared_ptr<ClassAdWrapper> nextAd(new ClassAdWrapper());
        for (std::vector<std::string>::const_iterator it = attrs.begin(); it != attrs.end(); it++)
        {
            std::stringstream attr; attr << *it << idx;
            classad::ExprTree *expr = NULL;
            if ((expr = ad->Lookup(attr.str())))
            {
                classad::ExprTree *copy = expr->Copy();
                if (!copy) { THROW_EX(HTCondorInternalError, "Unable to create copy of ClassAd expression"); }
                if (!nextAd->Insert(*it, copy)) { THROW_EX(HTCondorInternalError, "Unable to copy attribute into destination ClassAd"); }
                hasattr = true;
            }
        }
        if (hasattr)
        {
            result.append(nextAd);
        }
        idx++;
    }
    return result;
}


struct Negotiator {

    void use_local_negotiator()
    {
        Daemon neg( DT_NEGOTIATOR, 0, 0 );
        bool result;
        {
        condor::ModuleLock ml;
        result = neg.locate();
        }

        if (result)
        {
            if (neg.addr())
            {
                m_addr = neg.addr();
            }
            else
            {
                THROW_EX(HTCondorLocateError, "Unable to locate negotiator address.");
            }
            m_version = neg.version() ? neg.version() : "";
        }
        else
        {
            THROW_EX(HTCondorLocateError, "Unable to locate local daemon");
        }
    }

    Negotiator() {
        use_local_negotiator();
    }

    Negotiator(boost::python::object loc)
      : m_addr(), m_version("")
    {
		int rv = construct_for_location(loc, DT_NEGOTIATOR, m_addr, m_version);
		if (rv == 0) {
			use_local_negotiator();
		} else if (rv < 0) {
			if (rv == -2) { boost::python::throw_error_already_set(); }
			THROW_EX(HTCondorValueError, "Unknown type");
		}
	}

    ~Negotiator()
    {
    }

    boost::python::object location() const {
        return make_daemon_location(DT_NEGOTIATOR, m_addr, m_version);
    }

    void setPriority(const std::string &user, float prio)
    {
        if (prio < 0) THROW_EX(HTCondorValueError, "User priority must be non-negative");
        sendUserValue(SET_PRIORITY, user, prio);
    }

    void setFactor(const std::string &user, float factor)
    {
        if (factor<1) THROW_EX(HTCondorValueError, "Priority factors must be >= 1");
        sendUserValue(SET_PRIORITYFACTOR, user, factor);
    }

    void setUsage(const std::string &user, float usage)
    {
        if (usage < 0) THROW_EX(HTCondorValueError, "Usage must be non-negative.");
        sendUserValue(SET_ACCUMUSAGE, user, usage);
    }

    void setBeginUsage(const std::string &user, time_t time)
    {
        sendUserValue(SET_BEGINTIME, user, time);
    }

    void setLastUsage(const std::string &user, time_t time)
    {
        sendUserValue(SET_LASTTIME, user, time);
    }

	void setCeiling(const std::string &user, float ceiling) 
	{
        if (ceiling < -1) THROW_EX(HTCondorValueError, "Ceiling must be greater than -1.");
        sendUserValue(SET_CEILING, user, ceiling);
	}

    void resetUsage(const std::string &user)
    {
        sendUserCmd(RESET_USAGE, user);
    }

    void deleteUser(const std::string &user)
    {
        sendUserCmd(DELETE_USER, user);
    }

    void resetAllUsage()
    {
        Daemon negotiator(DT_NEGOTIATOR, m_addr.c_str());
        bool result;
        {
        condor::ModuleLock ml;
        result = negotiator.sendCommand(RESET_ALL_USAGE, Stream::reli_sock, 0);
        }
        if (!result)
        {
                THROW_EX(HTCondorIOError, "Failed to send RESET_ALL_USAGE command");
        }
    }

    boost::python::list
    getResourceUsage(const std::string &user)
    {
        checkUser(user);

        boost::shared_ptr<Sock> sock = getSocket(GET_RESLIST);
        if (!sock->put(user.c_str()) ||
            !sock->end_of_message())
        {
            sock->close();
            THROW_EX(HTCondorIOError, "Failed to send GET_RESLIST command to negotiator" );
        }
        sock->decode();
        boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
        bool result;
        {
        condor::ModuleLock ml;
        result = !getClassAdNoTypes(sock.get(), *ad.get()) || !sock->end_of_message();
        }
        if (result)
        {
            sock->close();
            THROW_EX(HTCondorIOError, "Failed to get classad from negotiator");
        }
        sock->close();

        std::vector<std::string> attrs;
        attrs.push_back("Name");
        attrs.push_back("StartTime");
        return toList(ad, attrs);
    }

    boost::python::list
    getPriorities(bool rollup=false)
    {
        boost::shared_ptr<Sock> sock = getSocket(rollup ? GET_PRIORITY_ROLLUP : GET_PRIORITY);

        sock->decode();
        boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
        bool result;
        {
        condor::ModuleLock ml;
        result = !getClassAdNoTypes(sock.get(), *ad.get()) || !sock->end_of_message();
        }
        if (result)
        {
            sock->close();
            THROW_EX(HTCondorIOError, "Failed to get classad from negotiator");
        }
        sock->close();

        std::vector<std::string> attrs;
        attrs.push_back("Name");
        attrs.push_back("Priority");
        attrs.push_back("ResourcesUsed");
        attrs.push_back("Requested");
        attrs.push_back("WeightedResourcesUsed");
        attrs.push_back("PriorityFactor");
        attrs.push_back("BeginUsageTime");
        attrs.push_back("LastUsageTime");
        attrs.push_back("WeightedAccumulatedUsage");
        attrs.push_back("AccountingGroup");
        attrs.push_back("IsAccountingGroup");
        attrs.push_back("AccumulatedUsage");
        return toList(ad, attrs);
    }

private:

    void checkUser(const std::string &user)
    {
        if( user.find('@') == std::string::npos )
        {
            THROW_EX(HTCondorValueError, "You must specify the full name of the submittor you wish (user@uid.domain)");
        }
    }

    boost::shared_ptr<Sock> getSocket(int cmd)
    {
        Daemon negotiator(DT_NEGOTIATOR, m_addr.c_str());
        Sock *raw_sock;
        {
        condor::ModuleLock ml;
        raw_sock = negotiator.startCommand(cmd, Stream::reli_sock, 0);
        }
        boost::shared_ptr<Sock> sock(raw_sock);
        if (!sock.get()) { THROW_EX(HTCondorIOError, "Unable to connect to the negotiator"); }
        return sock;
    }

    void sendUserValue(int cmd, const std::string &user, float val)
    {
        checkUser(user);
        boost::shared_ptr<Sock> sock = getSocket(cmd);

        bool retval;
        {
        condor::ModuleLock ml;
        retval = !sock->put(user.c_str()) || !sock->put(val) || !sock->end_of_message();
        }
        if (retval)
        {
            sock->close();
            THROW_EX(HTCondorIOError, "Failed to send command to negotiator\n" );
        }
        sock->close();
    }

    void sendUserValue(int cmd, const std::string &user, time_t val)
    {
        checkUser(user);
        boost::shared_ptr<Sock> sock = getSocket(cmd);

        bool retval;
        {
        condor::ModuleLock ml;
        retval = !sock->put(user.c_str()) || !sock->put(val) || !sock->end_of_message();
        }
        if (retval)
        {
            sock->close();
            THROW_EX(HTCondorIOError, "Failed to send command to negotiator\n" );
        }
        sock->close();
    }

    void sendUserCmd(int cmd, const std::string &user)
    {
        checkUser(user);
        boost::shared_ptr<Sock> sock = getSocket(cmd);

        bool retval;
        {
        condor::ModuleLock ml;
        retval = !sock->put(user.c_str()) || !sock->end_of_message();
        }
        if (retval)
        {
            sock->close();
            THROW_EX(HTCondorIOError, "Failed to send command to negotiator\n" );
        }
        sock->close();
    }


    std::string m_addr;
    std::string m_version;

};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(priority_overloads, getPriorities, 0, 1);

void export_negotiator()
{
    class_<Negotiator>("Negotiator",
            R"C0ND0R(
            This class provides a query interface to the *condor_negotiator*.
            It primarily allows one to query and set various parameters in the fair-share accounting.
            )C0ND0R",
        init<boost::python::object>(
            R"C0ND0R(
            :param location_ad: A ClassAd or DaemonLocation describing the *condor_negotiator*
                location and version.  If omitted, the default pool negotiator is assumed.
            :type location_ad: :class:`~classad.ClassAd` or :class:`DaemonLocation`
            )C0ND0R",
            boost::python::args("self", "ad")))
        .def(boost::python::init<>(boost::python::args("self")))
        .add_property("location", &Negotiator::location,
            R"C0ND0R(
            The negotiator to query or send commands to
            :rtype: location :class:`DaemonLocation`
            )C0ND0R")
        .def("setPriority", &Negotiator::setPriority,
            R"C0ND0R(
            Set the real priority of a specified user.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            :param float prio: The priority to be set for the user; must be greater-than 0.0.
            )C0ND0R",
            boost::python::args("self", "user", "prio"))
        .def("setFactor", &Negotiator::setFactor,
            R"C0ND0R(
            Set the priority factor of a specified user.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            :param float factor: The priority factor to be set for the user; must be greater-than or equal-to 1.0.
            )C0ND0R",
            boost::python::args("self", "user", "factor"))
        .def("setCeiling", &Negotiator::setCeiling,
            R"C0ND0R(
            Set the submitter ceiling of a specified user.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            :param float ceiling: The ceiling t be set for the submitter; must be greater-than or equal-to -1.0.
            )C0ND0R",
            boost::python::args("self", "user", "ceiling"))
        .def("setUsage", &Negotiator::setUsage,
            R"C0ND0R(
            Set the accumulated usage of a specified user.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            :param float usage: The usage, in hours, to be set for the user.
            )C0ND0R",
            boost::python::args("self", "user", "usage"))
        .def("setBeginUsage", &Negotiator::setBeginUsage,
            R"C0ND0R(
            Manually set the time that a user begins using the pool.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            :param int value: The Unix timestamp of initial usage.
            )C0ND0R",
            boost::python::args("self", "user", "value"))
        .def("setLastUsage", &Negotiator::setLastUsage,
            R"C0ND0R(
            Manually set the time that a user last used the pool.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            :param int value: The Unix timestamp of last usage.
            )C0ND0R",
            boost::python::args("self", "user", "value"))
        .def("resetUsage", &Negotiator::resetUsage,
            R"C0ND0R(
            Reset all usage accounting of the specified user.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            )C0ND0R",
            boost::python::args("self", "user"))
        .def("deleteUser", &Negotiator::deleteUser,
            R"C0ND0R(
            Delete all records of a user from the Negotiator's fair-share accounting.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            )C0ND0R",
            boost::python::args("self", "user"))
        .def("resetAllUsage", &Negotiator::resetAllUsage,
            R"C0ND0R(
            Reset all usage accounting.  All known user records in the negotiator are deleted.
            )C0ND0R",
            boost::python::args("self"))
        .def("getResourceUsage", &Negotiator::getResourceUsage,
            R"C0ND0R(
            Get the resources (slots) used by a specified user.

            :param str user: A fully-qualified user name (``USER@DOMAIN``).
            :return: List of ads describing the resources (slots) in use.
            :rtype: list[:class:`~classad.ClassAd`]
            )C0ND0R",
            boost::python::args("self", "user"))
        .def("getPriorities", &Negotiator::getPriorities, priority_overloads(
            R"C0ND0R(
            Retrieve the pool accounting information, one per entry.
            Returns a list of accounting ClassAds.

            :param bool rollup: Set to ``True`` if accounting information, as applied to
                hierarchical group quotas, should be summed for groups and subgroups.
            :return: A list of accounting ads, one per entity.
            :rtype: list[:class:`~classad.ClassAd`]
            )C0ND0R",
            boost::python::args("self", "rollup")))
        ;
}
