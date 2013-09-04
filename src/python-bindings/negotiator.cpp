
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>

#include "condor_adtypes.h"
#include "dc_collector.h"
#include "condor_version.h"

#include <memory>
#include <boost/python.hpp>

#include "old_boost.h"
#include "classad_wrapper.h"

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
                if (!copy) THROW_EX(RuntimeError, "Unable to create copy of ClassAd expression");
                if (!nextAd->Insert(*it, copy)) THROW_EX(RuntimeError, "Unable to copy attribute into destination ClassAd");
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

    Negotiator()
    {
        Daemon neg( DT_NEGOTIATOR, 0, 0 );

        if (neg.locate())
        {
            if (neg.addr())
            {
                m_addr = neg.addr();
            }
            else
            {
                THROW_EX(RuntimeError, "Unable to locate schedd address.");
            }
            m_name = neg.name() ? neg.name() : "Unknown";
            m_version = neg.version() ? neg.version() : "";
        }
        else
        {
            THROW_EX(RuntimeError, "Unable to locate local daemon");
        }
    }

    Negotiator(const ClassAdWrapper &ad)
      : m_addr(), m_name("Unknown"), m_version("")
    {
        if (!ad.EvaluateAttrString(ATTR_NEGOTIATOR_IP_ADDR, m_addr))
        {
            THROW_EX(ValueError, "Schedd address not specified.");
        }
        ad.EvaluateAttrString(ATTR_NAME, m_name);
        ad.EvaluateAttrString(ATTR_VERSION, m_version);
    }

    ~Negotiator()
    {
    }

    void setPriority(const std::string &user, float prio)
    {
        if (prio < 0) THROW_EX(ValueError, "User priority must be non-negative");
        sendUserValue(SET_PRIORITY, user, prio);
    }

    void setFactor(const std::string &user, float factor)
    {
        if (factor<1) THROW_EX(ValueError, "Priority factors must be >= 1");
        sendUserValue(SET_PRIORITYFACTOR, user, factor);
    }

    void setUsage(const std::string &user, float usage)
    {
        if (usage < 0) THROW_EX(ValueError, "Usage must be non-negative.");
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
        if (!negotiator.sendCommand(RESET_ALL_USAGE, Stream::reli_sock, 0) )
        {
                THROW_EX(RuntimeError, "Failed to send RESET_ALL_USAGE command");
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
            THROW_EX(RuntimeError, "Failed to send GET_RESLIST command to negotiator" );
        }
        sock->decode();
        boost::shared_ptr<ClassAdWrapper> ad(new ClassAdWrapper());
        if (!getClassAdNoTypes(sock.get(), *ad.get()) ||
            !sock->end_of_message())
        {
            sock->close();
            THROW_EX(RuntimeError, "Failed to get classad from negotiator");
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
        if (!getClassAdNoTypes(sock.get(), *ad.get()) ||
            !sock->end_of_message())
        {
            sock->close();
            THROW_EX(RuntimeError, "Failed to get classad from negotiator");
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
            THROW_EX(ValueError, "You must specify the full name of the submittor you wish (user@uid.domain)");
        }
    }

    boost::shared_ptr<Sock> getSocket(int cmd)
    {
        Daemon negotiator(DT_NEGOTIATOR, m_addr.c_str());
        boost::shared_ptr<Sock> sock(negotiator.startCommand(cmd, Stream::reli_sock, 0));
        if (!sock.get()) THROW_EX(RuntimeError, "Unable to connect to the negotiator");
        return sock;
    }

    void sendUserValue(int cmd, const std::string &user, float val)
    {
        checkUser(user);
        boost::shared_ptr<Sock> sock = getSocket(cmd);

        if (    !sock->put(user.c_str()) ||
                !sock->put(val) ||
                !sock->end_of_message()) {
            sock->close();
            THROW_EX(RuntimeError, "Failed to send command to negotiator\n" );
        }
        sock->close();
    }

    void sendUserValue(int cmd, const std::string &user, time_t val)
    {
        checkUser(user);
        boost::shared_ptr<Sock> sock = getSocket(cmd);

        if (    !sock->put(user.c_str()) ||
                !sock->put(val) ||
                !sock->end_of_message()) {
            sock->close();
            THROW_EX(RuntimeError, "Failed to send command to negotiator\n" );
        }
        sock->close();
    }

    void sendUserCmd(int cmd, const std::string &user)
    {
        checkUser(user);
        boost::shared_ptr<Sock> sock = getSocket(cmd);

        if (    !sock->put(user.c_str()) ||
                !sock->end_of_message()) {
            sock->close();
            THROW_EX(RuntimeError, "Failed to send command to negotiator\n" );
        }
        sock->close();
    }


    std::string m_addr;
    std::string m_name;
    std::string m_version;

};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(priority_overloads, getPriorities, 0, 1);

void export_negotiator()
{
    class_<Negotiator>("Negotiator", "Client-side operations for the HTCondor negotiator")
        .def(init<const ClassAdWrapper &>(":param ad: An ad containing the location of the negotiator; if not specified, uses the default pool"))
        .def("setPriority", &Negotiator::setPriority, "Set the fairshare of a user\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.\n"
            ":param value: New fairshare priority.")
        .def("setFactor", &Negotiator::setFactor, "Set the usage factor of a user\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.\n"
            ":param value: New fairshare priority.")
        .def("setUsage", &Negotiator::setUsage, "Set the usage of a user\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.\n"
            ":param value: New fairshare priority.")
        .def("setBeginUsage", &Negotiator::setBeginUsage, "Set the first time a user began using the pool\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.\n"
            ":param value: New fairshare priority.")
        .def("setLastUsage", &Negotiator::setLastUsage, "Set the last time the user began using the pool\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.\n"
            ":param value: New fairshare priority.")
        .def("resetUsage", &Negotiator::resetUsage, "Reset the usage of user\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.")
        .def("deleteUser", &Negotiator::deleteUser, "Delete a user from the accounting\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.")
        .def("resetAllUsage", &Negotiator::resetAllUsage, "Reset all usage accounting")
        .def("getResourceUsage", &Negotiator::getResourceUsage, "Get the resource usage for a given user\n"
            ":param user: A fully-qualified (USER@DOMAIN) username.\n"
            ":return: A list of resource ClassAds.")
        .def("getPriorities", &Negotiator::getPriorities, priority_overloads("Retrieve the pool accounting information"
            ":return: A list of accounting ClassAds."))
        ;
}
