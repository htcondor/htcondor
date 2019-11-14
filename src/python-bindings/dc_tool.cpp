
// Note - python_bindings_common.h must be included before condor_common to avoid
// re-definition warnings.
#include "python_bindings_common.h"
#include "condor_common.h"

#include "daemon.h"
#include "daemon_types.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "compat_classad.h"
#include "condor_config.h"
#include "subsystem_info.h"

#include "classad_wrapper.h"
#include "old_boost.h"
#include "module_lock.h"

using namespace boost::python;

enum DaemonCommands
{
  DDAEMONS_ON = DAEMONS_ON,
  DDAEMONS_OFF = DAEMONS_OFF,
  DDAEMONS_OFF_FAST = DAEMONS_OFF_FAST,
  DDAEMONS_OFF_PEACEFUL = DAEMONS_OFF_PEACEFUL,
  DDAEMON_ON = DAEMON_ON,
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


void send_alive(boost::python::object ad_obj=boost::python::object(), boost::python::object pid_obj=boost::python::object(), boost::python::object timeout_obj=boost::python::object())
{
    std::string addr;
    if (ad_obj.ptr() == Py_None)
    {
        char *inherit_var = getenv("CONDOR_INHERIT");
        if (!inherit_var) {THROW_EX(RuntimeError, "No location specified and $CONDOR_INHERIT not in Unix environment.");}
        std::string inherit(inherit_var);
        boost::python::object inherit_obj(inherit);
        boost::python::object inherit_split = inherit_obj.attr("split")();
        if (py_len(inherit_split) < 2) {THROW_EX(RuntimeError, "$CONDOR_INHERIT Unix environment variable malformed.");}
        addr = boost::python::extract<std::string>(inherit_split[1]);
    }
    else
    {
        const ClassAdWrapper ad = boost::python::extract<ClassAdWrapper>(ad_obj);
        if (!ad.EvaluateAttrString(ATTR_MY_ADDRESS, addr))
        {
            THROW_EX(ValueError, "Address not available in location ClassAd.");
        }
    }
    int pid = getpid();
    if (pid_obj.ptr() != Py_None)
    {
        pid = boost::python::extract<int>(pid_obj);
    }
    int timeout;
    if (timeout_obj.ptr() == Py_None)
    {
        timeout = param_integer("NOT_RESPONDING_TIMEOUT");
    }
    else
    {
        timeout = boost::python::extract<int>(timeout_obj);
    }
    if (timeout < 1) {timeout = 1;}

    classy_counted_ptr<Daemon> daemon = new Daemon(DT_ANY, addr.c_str());
    classy_counted_ptr<ChildAliveMsg> msg = new ChildAliveMsg(pid, timeout, 0, 0, true);

    {
        condor::ModuleLock ml;
        daemon->sendBlockingMsg(msg.get());
    }
        if (msg->deliveryStatus() != DCMsg::DELIVERY_SUCCEEDED)
        {
            THROW_EX(RuntimeError, "Failed to deliver keepalive message.");
        }
}


void
enable_debug()
{
    dprintf_set_tool_debug(get_mySubSystem()->getName(), 0);
}


void
enable_log()
{
    dprintf_config(get_mySubSystem()->getName());
}


void
set_subsystem(std::string subsystem, SubsystemType type=SUBSYSTEM_TYPE_AUTO)
{
    set_mySubSystem(subsystem.c_str(), type);
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
        .value("DaemonsOn", DDAEMONS_ON)
        .value("DaemonsOff", DDAEMONS_OFF)
        .value("DaemonsOffFast", DDAEMONS_OFF_FAST)
        .value("DaemonsOffPeaceful", DDAEMONS_OFF_PEACEFUL)
        .value("DaemonOn", DDAEMON_ON)
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

    enum_<SubsystemType>("SubsystemType")
        .value("Master", SUBSYSTEM_TYPE_MASTER)
        .value("Collector", SUBSYSTEM_TYPE_COLLECTOR)
        .value("Negotiator", SUBSYSTEM_TYPE_NEGOTIATOR)
        .value("Schedd", SUBSYSTEM_TYPE_SCHEDD)
        .value("Shadow", SUBSYSTEM_TYPE_SHADOW)
        .value("Startd", SUBSYSTEM_TYPE_STARTD)
        .value("Starter", SUBSYSTEM_TYPE_STARTER)
        .value("GAHP", SUBSYSTEM_TYPE_GAHP)
        .value("Dagman", SUBSYSTEM_TYPE_DAGMAN)
        .value("SharedPort", SUBSYSTEM_TYPE_SHARED_PORT)
        .value("Daemon", SUBSYSTEM_TYPE_DAEMON)
        .value("Tool", SUBSYSTEM_TYPE_TOOL)
        .value("Submit", SUBSYSTEM_TYPE_SUBMIT)
        .value("Job", SUBSYSTEM_TYPE_JOB)
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

    def("send_alive", send_alive, "Send a keepalive to a HTCondor daemon\n"
        ":param ad: An ad specifying the location of the daemon; typically, found by using Collector.locate(...).\n"
        ":param pid: A process identifier for the keepalive.  Defaults to None, which indicates to utilize the value of os.getpid().\n"
        ":param timeout: The number of seconds this keepalive is valid.  After that time, if the condor_master has not\nreceived a new .keepalive for this process, it will be terminated.  Defaults is controlled by the parameter NOT_RESPONDING_TIMEOUT.\n",
        (boost::python::arg("ad") = boost::python::object(), boost::python::arg("pid")=boost::python::object(), boost::python::arg("timeout")=boost::python::object())
       )
       ;

    def("set_subsystem", set_subsystem, "Set the subsystem name for configuration.\n"
        ":param name: The used for the config subsystem.\n"
        ":param type: The daemon type for configuration.  Defaults to Auto, which indicates to determine the type from the parameter name.\n",
        (boost::python::arg("subsystem"), boost::python::arg("type")=SUBSYSTEM_TYPE_AUTO))
        ;

    def("enable_debug", enable_debug, "Turn on debug logging output from HTCondor.  Logs to stderr.");
    def("enable_log", enable_log, "Turn on logging output from HTCondor.  Logs to the file specified by the parameter TOOL_LOG.");

    def("log", dprintf_wrapper, "Log a message to the HTCondor logging subsystem.\n"
        ":param level: Log category and formatting indicator; use the LogLevel enum for a list of these (may be OR'd together).\n"
        ":param msg: String message to log.\n")
        ;

    if ( ! has_mySubSystem()) { set_mySubSystem("TOOL", SUBSYSTEM_TYPE_TOOL); }
    dprintf_pause_buffering();
}
