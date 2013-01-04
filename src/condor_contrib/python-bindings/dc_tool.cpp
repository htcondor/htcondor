
#include "condor_common.h"

#include <boost/python.hpp>

#include "daemon.h"
#include "daemon_types.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "compat_classad.h"

#include "classad_wrapper.h"

using namespace boost::python;

enum DaemonCommands {
  DDAEMONS_OFF = DAEMONS_OFF,
  DDAEMONS_OFF_FAST = DAEMONS_OFF_FAST,
  DDAEMONS_OFF_PEACEFUL = DAEMONS_OFF_PEACEFUL,
  DDAEMON_OFF = DAEMON_OFF,
  DDAEMON_OFF_FAST = DAEMON_OFF_FAST,
  DDAEMON_OFF_PEACEFUL = DAEMON_OFF_PEACEFUL,
  DDC_OFF_FAST = DC_OFF_FAST,
  DDC_OFF_PEACEFUL = DC_OFF_PEACEFUL,
  DDC_OFF_GRACEFUL = DC_OFF_GRACEFUL,
  DDC_SET_PEACEFUL_SHUTDOWN = DC_SET_PEACEFUL_SHUTDOWN,
  DDC_RECONFIG_FULL = DC_RECONFIG_FULL,
  DRESTART = RESTART,
  DRESTART_PEACEFUL = RESTART_PEACEFUL
};

void send_command(const ClassAdWrapper & ad, DaemonCommands dc, const std::string &target="")
{
    std::string addr;
    if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, addr))
    {
        PyErr_SetString(PyExc_ValueError, "Address not available in location ClassAd.");
        throw_error_already_set();
    }
    std::string ad_type_str;
    if (!ad.EvaluateAttrString(ATTR_MY_TYPE, ad_type_str))
    {
        PyErr_SetString(PyExc_ValueError, "Daemon type not available in location ClassAd.");
        throw_error_already_set();
    }
    int ad_type = AdTypeFromString(ad_type_str.c_str());
    if (ad_type == NO_AD)
    {
        printf("ad type %s.\n", ad_type_str.c_str());
        PyErr_SetString(PyExc_ValueError, "Unknown ad type.");
        throw_error_already_set();
    }
    daemon_t d_type;
    switch (ad_type) {
    case MASTER_AD: d_type = DT_MASTER; break;
    case STARTD_AD: d_type = DT_STARTD; break;
    case SCHEDD_AD: d_type = DT_SCHEDD; break;
    case NEGOTIATOR_AD: d_type = DT_NEGOTIATOR; break;
    case COLLECTOR_AD: d_type = DT_COLLECTOR; break;
    default:
        d_type = DT_NONE;
        PyErr_SetString(PyExc_ValueError, "Unknown daemon type.");
        throw_error_already_set();
    }

    ClassAd ad_copy; ad_copy.CopyFrom(ad);
    Daemon d(&ad_copy, d_type, NULL);
    if (!d.locate())
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to locate daemon.");
        throw_error_already_set();
    }
    ReliSock sock;
    if (!sock.connect(d.addr()))
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to connect to the remote daemon");
        throw_error_already_set();
    }
    if (!d.startCommand(dc, &sock, 0, NULL))
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to start command.");
        throw_error_already_set();
    }
    if (target.size())
    {
        std::vector<unsigned char> target_cstr; target_cstr.reserve(target.size()+1);
        memcpy(&target_cstr[0], target.c_str(), target.size()+1);
        if (!sock.code(&target_cstr[0]))
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to send target.");
            throw_error_already_set();
        }
        if (!sock.end_of_message())
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to send end-of-message.");
            throw_error_already_set();
        }
    }
    sock.close();
}

BOOST_PYTHON_FUNCTION_OVERLOADS(send_command_overloads, send_command, 2, 3);

void
export_dc_tool()
{
    enum_<DaemonCommands>("DaemonCommands")
        .value("DaemonsOff", DDAEMONS_OFF)
        .value("DaemonsOffFast", DDAEMONS_OFF_FAST)
        .value("DaemonsOffPeaceful", DDAEMONS_OFF_PEACEFUL)
        .value("DaemonOff", DDAEMON_OFF)
        .value("DaemonOffFast", DDAEMON_OFF_FAST)
        .value("DaemonOffPeaceful", DDAEMON_OFF_PEACEFUL)
        .value("OffGraceful", DDC_OFF_GRACEFUL)
        .value("OffPeaceful", DDC_OFF_PEACEFUL)
        .value("OffFast", DDC_OFF_FAST)
        .value("SetPeacefulShutdown", DDC_SET_PEACEFUL_SHUTDOWN)
        .value("Reconfig", DDC_RECONFIG_FULL)
        .value("Restart", DRESTART)
        .value("RestartPeacful", DRESTART_PEACEFUL)
        ;

    def("send_command", send_command, send_command_overloads("Send a command to a HTCondor daemon specified by a location ClassAd\n"
        ":param ad: An ad specifying the location of the daemon; typically, found by using Collector.locate(...).\n"
        ":param dc: A command type; must be a member of the enum DaemonCommands.\n"
        ":param target: Some commands require additional arguments; for example, sending DaemonOff to a master requires one to specify which subsystem to turn off."
        "  If this parameter is given, the daemon is sent an additional argument."))
        ;
}
