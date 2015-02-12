
// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>
# if defined(__APPLE__)
# undef HAVE_SSIZE_T
# include <pyport.h>
# endif

#include "condor_common.h"

#include <boost/python.hpp>

#include "daemon.h"
#include "daemon_types.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "compat_classad.h"

#include "classad_wrapper.h"
#include "module_lock.h"

using namespace boost::python;

enum DaemonCommands
{
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
  DDC_SET_FORCE_SHUTDOWN = DC_SET_FORCE_SHUTDOWN,
  DDC_OFF_FORCE = DC_OFF_FORCE,
  DDC_RECONFIG_FULL = DC_RECONFIG_FULL,
  DRESTART = RESTART,
  DRESTART_PEACEFUL = RESTART_PEACEFUL
};

enum LogLevel
{
  DALWAYS = D_ALWAYS,
  DERROR = D_ERROR,
  DSTATUS = D_STATUS,
  DJOB = D_JOB,
  DMACHINE = D_MACHINE,
  DCONFIG = D_CONFIG,
  DPROTOCOL = D_PROTOCOL,
  DPRIV = D_PRIV,
  DDAEMONCORE = D_DAEMONCORE,
  DSECURITY = D_SECURITY,
  DNETWORK = D_NETWORK,
  DHOSTNAME = D_HOSTNAME,
  DAUDIT = D_AUDIT,
  DTERSE = D_TERSE,
  DVERBOSE = D_VERBOSE,
  DFULLDEBUG = D_FULLDEBUG,
  DBACKTRACE = D_BACKTRACE,
  DIDENT = D_IDENT,
  DSUBSECOND = D_SUB_SECOND,
  DTIMESTAMP = D_TIMESTAMP,
  DPID = D_PID,
  DNOHEADER = D_NOHEADER
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
    bool result;
    {
    condor::ModuleLock ml;
    result = !d.locate();
    }
    if (result)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to locate daemon.");
        throw_error_already_set();
    }
    ReliSock sock;
    {
    condor::ModuleLock ml;
    result = !sock.connect(d.addr());
    }
    if (result)
    {
        PyErr_SetString(PyExc_RuntimeError, "Unable to connect to the remote daemon");
        throw_error_already_set();
    }
    {
    condor::ModuleLock ml;
    result = !d.startCommand(dc, &sock, 0, NULL);
    }
    if (result)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to start command.");
        throw_error_already_set();
    }
    if (target.size())
    {
        std::string target_to_send = target;
        if (!sock.code(target_to_send))
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

void
enable_debug()
{
    dprintf_set_tool_debug("TOOL", 0);
}

void
enable_log()
{
    dprintf_config("TOOL");
}

static void
dprintf_wrapper2(int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _condor_dprintf_va(level, static_cast<DPF_IDENT>(0), fmt, args);
    va_end(args);
}

void
dprintf_wrapper(int level, std::string msg)
{
    dprintf_wrapper2(level, "%s\n", msg.c_str());
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
        .value("OffForce", DDC_OFF_FORCE)
        .value("SetPeacefulShutdown", DDC_SET_PEACEFUL_SHUTDOWN)
        .value("SetForceShutdown", DDC_SET_FORCE_SHUTDOWN)
        .value("Reconfig", DDC_RECONFIG_FULL)
        .value("Restart", DRESTART)
        .value("RestartPeacful", DRESTART_PEACEFUL)
        ;

    enum_<LogLevel>("LogLevel")
        .value("Always", DALWAYS)
        .value("Error", DERROR)
        .value("Status", DSTATUS)
        .value("Job", DJOB)
        .value("Machine", DMACHINE)
        .value("Config", DCONFIG)
        .value("Protocol", DPROTOCOL)
        .value("Priv", DPRIV)
        .value("DaemonCore", DDAEMONCORE)
        .value("Security", DSECURITY)
        .value("Network", DNETWORK)
        .value("Hostname", DHOSTNAME)
        .value("Audit", DAUDIT)
        .value("Terse", DTERSE)
        .value("Verbose", DVERBOSE)
        .value("FullDebug", DFULLDEBUG)
        .value("SubSecond", DSUBSECOND)
        .value("Timestamp", DTIMESTAMP)
        .value("PID", DPID)
        .value("NoHeader", DNOHEADER)
        ;

    def("send_command", send_command, send_command_overloads("Send a command to a HTCondor daemon specified by a location ClassAd\n"
        ":param ad: An ad specifying the location of the daemon; typically, found by using Collector.locate(...).\n"
        ":param dc: A command type; must be a member of the enum DaemonCommands.\n"
        ":param target: Some commands require additional arguments; for example, sending DaemonOff to a master requires one to specify which subsystem to turn off."
        "  If this parameter is given, the daemon is sent an additional argument."))
        ;

    def("enable_debug", enable_debug, "Turn on debug logging output from HTCondor.  Logs to stderr.");
    def("enable_log", enable_log, "Turn on logging output from HTCondor.  Logs to the file specified by the parameter TOOL_LOG.");

    def("log", dprintf_wrapper, "Log a message to the HTCondor logging subsystem.\n"
        ":param level: Log category and formatting indicator; use the LogLevel enum for a list of these (may be OR'd together).\n"
        ":param msg: String message to log.\n")
        ;
}
