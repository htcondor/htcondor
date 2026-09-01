Description
-----------

The snaked is a daemon core daemon that calls Python functions specified by
its configuration in order to handle wire commands, update the collector,
and (if configured) periodically do other work.

The goal of this daemon to twofold: to allow the credmons to become "real"
daemon core daemons with a minimum of recoding in C++; and to allow for
simpler testing at or of the CEDAR protocol level (for instance, via the
implementation of mocks with certain specific failure modes).

Design
------

The snaked spins up a single Python interpreter using the Python C API.  This
interpreter is used for all calls into Python, but each individual command
being handled (or equivalent) gets its own set of locals.

There are three kinds of calls into Python.

**Command handlers** must be generators.  They may (and probably should be)
Python coroutines, in the sense of using the return value of their `yield`
expressions.  A command handler is called with the single argument of
the command int it is expected to handle.  When a command handler yields,
it _must_ yield a three-tuple whose first and last elements are also tuples
(or `None`), and whose middle elements is an integer (or `None`).  The first
tuple is the _reply_, and must consist of ints, strings, bytes,
`classad2.ClassAd`s, or `end_of_message`; the first tuple's elements are
converted into their CEDAR representations and sent back down the wire.
(The `end_of_message` local is inserted into the function by the snaked.)
The second tuple is the _timeout_: how to long to wait (or `None`, if not
at all) for the (first byte of the) `payload` to arrive.  The third tuple
is the `payload`, and is specified in the same way as the `reply`; the
yield will return a tuple identical in type but not in value, or `None`
if the time out passed.

The **daemon update handler** is always named `updateDaemonAd` and called
with a single argument, the `classad2.ClassAd` of the daemon ad as generated
by daemon core.  The function must return a `classad2.ClassAd`, which will
be passed along to the collector.  Attributes specified in the returned ad
will be overwritten with attributes from the daemon ad before transmission.

A **polling handler**, for lack of a better name, is used for daemons which
have work to do outside of its command handlers.  There may be more than
one, although because of implementation limitations, each must be associated
with its own signal; because of daemon core limitations, you may presently
only register `SIGUSR1` and `SIGUSR2`.  Polling handlers must be generators,
and may (and probably should be) coroutines in the Python sense.  Each polling
handler is called once at just after daemon start-up with no arguments.  A
polling handler must yield the duration for which it ought to sleep.  The
signal it was configured with may interrupt the sleep early, and the return
value of the yield will indicated if this occurred.  If the polling handler
exits, so will the daemon.

Code Layout
----------

The snaked proper, coded in `snaked.cpp`, is responsible for reading the
configuration, registering the corresponding commands (and the necessary
timers) with daemon core, and -- by far the most complicated -- handling
the reconfiguration of the daemon, which throws away the current Python
interpreter and restarts it from scratch.  This allows the Python code to
be updated without restarting the daemon, which can be very useful in
some testing situations (because you don't have to reacquire the daemon's
address).  However, the implementation is complicated by the signal perhaps
occuring while one or more coroutines and their corresponding pointers into
the Python interpreter's state are outstanding.

All direct interaction with the Python C API is handled in `snake.cpp`.  The
snake object is not presently coded as a C++ singleton, but since it controls
the lifetime of the (single possibly) Python interpreter, it really should be.

Config Magic
------------

The snake daemon requires the `-genus` command-line flag in order to
determine which name it uses for its configuration, which subsystem
type it sets, and which `MyType` it reports to the collector.

The `CREDMON_OAUTH` daemon is therefore configured like this:

    # Param table defaults.
    SNAKED                      = $(SBIN)/condor_snaked
    DC_DAEMON_LIST              = +SNAKED

    # Param table defaults.
    CREDMON_OAUTH               = $(SNAKED)
    CREDMON_OAUTH_ARGS          = -genus CREDMON_OAUTH
    CREDMON_OAUTH_LOG           = $(LOG)/CredmonOauthLog

    # Simplifies testing.
    CREDMON_OAUTH_ADDRESS_FILE  = $(LOG)/.credmon_ouauth_address

    # Param table defaults.
    CREDMON_OAUTH_SNAKE_PATH    = $(LIB)/python3:$(ETC)/snake.d/CREDMON_OAUTH
    CREDMON_OAUTH_SNAKE_POLLING = pollingBodyGenerator SIGUSR1

    DAEMON_LIST                 = $(DAEMON_LIST) $(CREDMON_OAUTH)

The `SNAKE_PATH` sets the `PYTHONPATH` for that snaked's interpreter.  The
SNAKE_PATH must include a `snake` module, in which the snaked will look for
for all functions, including `updateDaemonAd()`, which is not optional.  You
may perform one-time (that is, per-reconfig) initialization in the module's
`__init__.py`, but work that takes any appreciable amount of time should
instead be done in a polling handler.

Use `SNAKE_POLLING` to set the polling handler function's name and the
name of the signal used to rouse it from its sleep.

You specify command handlers using a table:

    # One space, Vasili, and one space only.
    CREDMON_OAUTH_SNAKE_COMMAND_TABLE @=end
    QUERY_STARTD_ADS READ handleCommand
    QUERY_SCHEDD_ADS READ handleScheddCommand
    @end

which specifies, one per line, the command int, the permissions level,
and the name of the Python function to call.  (This particular example
was developed to test the command handler functionality and doesn't
presently do anything useful in the OAuth daemon -- although it certainly
could.)
