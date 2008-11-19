The following services are provided in condor_c++_util/cplus_lib.a and
condor_c++_util/liburl.a.

selector.h NON_POSIX.select.C

    Defines an object called Selector. This object monitors a set of
    user specified I/O file descriptors for a user specified amount of
    time. It returns control when any of the file descriptors are ready
    for read, write, or have an exceptional condition pending, or when the
    specified amount of time has elapsed.

NON_POSIX.set_prog_env.C

    Tell the operating system that we want to operate in POSIX
    conforming mode.

afs.C
afs.h

    Defines AFS_info object. It can be used to get information about
    things like the AFS cell of the current workstation, or the AFS cell,
    volume, etc. of given files.

alarm.C
alarm.h

    Defines Alarm object. It can be used to set an alarm timer,
    suspend the timer, resume timing, or cancel the alarm.

carmi_context.C
carmi_context.h

    Defines object CARMI_Context which basically is the same machine
    context used elsewhere in condor with a C++ wrapper and some carmi
    specific information.

cat_url.c

    Same as unix command "cat" except that instead of taking a file
    path name as it's argument, "cat_url" take a URL. This command uses
    functions from library lib_url.a.

condor_event.C
condor_event.h
user_log.C
read_user_log.h
write_user_log.h

    Defines objects that represent condor events like job submitted,
    job executed, etc.. These events are imported from or exported to user
    log files.

condor_query.C
condor_query.h

    Defines CondorQuery object. This is the API for querying condor
    collector about differrent kinds of classified ads in the pool.

disk.C
condor_disk.h
condor_updown.h
up_down.C

    Defines UpDown object which is used to calculate and maintain user
    priorities based on updown algorithm, and to read the priorities from
    or write them to disk.

directory.C
directory.h

    Defines Directory object. It is used to scan a directory and
    return the name of each file or directory it contains.

environ.C
environ.h

    Defines Environ object which keeps record of environment variable
    values.

event_handler.C
event_handler.h

    Defines EventHandler object which allows user to install and
    display event handlers.

file_lock.C

    Defines FileLock object which is used to lock a file for read or
    write.

filter.C
filter.h

    Defines object Filter. Several of the condor programs accept
    command line arguments which determine a subset of jobs in the queue
    which are of interest. This object is intended to be used as a filter
    when constructing process lists from the job queue.

generic_query.C
generic_query.h

    Defined the generic query object. Different queries like condor_q
    or condor_status have similar query interface which can all be derived
    from this generic query object.

list.h

    Defines a list template class.

memory_file.h
memory_file.C

    A simple implementation of read/write/seek on a memory object.
    This is handy for verifying the (much) more complex implementation
    of read/write/seek in Condor.

my_hostname.C

    Find out the name of host the process is running on.

name_tab.C
name_tab.h

    Implements a table of names and values. One instance of usage is
    to make a table of all the signal names and values.

proc_obj.C
proc_obj.h
proc_obj_tmpl.h

    C++ wrappers around proc structures.

simplelist.h

    Has the same interface as the list class found in list.h except
    that this is for small persistent information such as integers,
    floats, etc.

stat_func.h

    Calculate min, max, ave of an array of objects.

state_machine_driver.C
state_machine_driver.h

    A state machine object.

string_list.C
string_list.h

    Implements StringList object which stores and searches arrays of
    strings.

string_set.C
string_set.h

    Implements a set of strings and returns the cardinality of the
    set.

subproc.C
subproc.h

    Create a subprocess and either send data to its stdin or read data
    from its stdout via a pipe.

sysparam_aix.C
sysparam_hpux.C
sysparam_irix.C
sysparam_linux.C
sysparam_main.C
sysparam_osf1.C
sysparam_solaris.C
sysparam_sun.C
sysparam_ultrix.C

    Objects to query system parameters on different platforms.

url_condor.C
url_condor.h
cbstp_url.C
cfilter_url.C
file_url.C
filter_url.C
ftp_url.C
http_url.C
include_urls.C
mailto_url.C

    liburl.a source. This library provides means to open a uniform
    resource locator that represents a regular unix file, a filter, a
    mail program, an ftp site, etc..
