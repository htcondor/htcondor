Introduction to Configuration
=============================

:index:`configuration-intro<single: configuration-intro; HTCondor>`
:index:`configuration: introduction`

This section of the manual contains general information about HTCondor
configuration, relating to all parts of the HTCondor system. If you're
setting up an HTCondor pool, you should read this section before you
read the other configuration-related sections:

-  The :ref:`admin-manual/introduction-to-configuration:configuration templates` section contains
   information about configuration templates, which are now the
   preferred way to set many configuration macros.
-  The :doc:`/admin-manual/configuration-macros` section contains
   information about the hundreds of individual configuration macros. In
   general, it is best to try to achieve your desired configuration
   using configuration templates before resorting to setting individual
   configuration macros, but it is sometimes necessary to set individual
   configuration macros.
-  The settings that control the policy under which HTCondor will start,
   suspend, resume, vacate or kill jobs are described in
   the :doc:`/admin-manual/ep-policy-configuration` section on Policy
   Configuration for the *condor_startd*.

HTCondor Configuration Files
----------------------------

The HTCondor configuration files are used to customize how HTCondor
operates at a given site. The basic configuration as shipped with
HTCondor can be used as a starting point, but most likely you will want
to modify that configuration to some extent.

Each HTCondor program will, as part of its initialization process,
configure itself by calling a library routine which parses the various
configuration files that might be used, including pool-wide,
platform-specific, and machine-specific configuration files. Environment
variables may also contribute to the configuration.

The result of configuration is a list of key/value pairs. Each key is a
configuration variable name, and each value is a string literal that may
utilize macro substitution (as defined below). Some configuration
variables are evaluated by HTCondor as ClassAd expressions; some are
not. Consult the documentation for each specific case. Unless otherwise
noted, configuration values that are expected to be numeric or boolean
constants can be any valid ClassAd expression of operators on constants.
Example:

.. code-block:: condor-config

    MINUTE          = 60
    HOUR            = (60 * $(MINUTE))
    SHUTDOWN_GRACEFUL_TIMEOUT = ($(HOUR)*24)

.. _ordered_evaluation_to_set_the_configuration:

Ordered Evaluation to Set the Configuration
-------------------------------------------

:index:`evaluation order<single: evaluation order; configuration file>`

Multiple files, as well as a program's environment variables, determine
the configuration. The order in which attributes are defined is
important, as later definitions override earlier definitions. The order
in which the (multiple) configuration files are parsed is designed to
ensure the security of the system. Attributes which must be set a
specific way must appear in the last file to be parsed. This prevents
both the naive and the malicious HTCondor user from subverting the
system through its configuration. The order in which items are parsed
is:

#. a single initial configuration file, which has historically been
   known as the global configuration file (see below);
#. other configuration files that are referenced and parsed due to
   specification within the single initial configuration file (these
   files have historically been known as local configuration files);
#. if HTCondor daemons are not running as root on Unix platforms, the
   file ``$(HOME)/.condor/user_config`` if it exists, or the file
   defined by configuration variable :macro:`USER_CONFIG_FILE`;

   if HTCondor daemons are not running as Local System on Windows
   platforms, the file %USERPROFILE\\.condor\\user_config if it exists,
   or the file defined by configuration variable :macro:`USER_CONFIG_FILE`;

#. specific environment variables whose names are prefixed with
   ``_CONDOR_`` (note that these environment variables directly define
   macro name/value pairs, not the names of configuration files).

Some HTCondor tools utilize environment variables to set their
configuration; these tools search for specifically-named environment
variables. The variable names are prefixed by the string ``_CONDOR_`` or
``_condor_``. The tools strip off the prefix, and utilize what remains
as configuration. As the use of environment variables is the last within
the ordered evaluation, the environment variable definition is used. The
security of the system is not compromised, as only specific variables
are considered for definition in this manner, not any environment
variables with the ``_CONDOR_`` prefix.

The location of the single initial configuration file differs on Windows
from Unix platforms. For Unix platforms, the location of the single
initial configuration file starts at the top of the following list. The
first file that exists is used, and then remaining possible file
locations from this list become irrelevant.

#. the file specified by the ``CONDOR_CONFIG`` environment variable. If
   there is a problem reading that file, HTCondor will print an error
   message and exit right away.
#. ``/etc/condor/condor_config``
#. ``/usr/local/etc/condor_config``
#. ``~condor/condor_config``

For Windows platforms, the location of the single initial configuration
file is determined by the contents of the environment variable
``CONDOR_CONFIG``. If this environment variable is not defined, then the
location is the registry value of
``HKEY_LOCAL_MACHINE/Software/Condor/CONDOR_CONFIG``.

The single, initial configuration file may contain the specification of
one or more other configuration files, referred to here as local
configuration files. Since more than one file may contain a definition
of the same variable, and since the last definition of a variable sets
the value, the parse order of these local configuration files is fully
specified here. In order:

#. The value of configuration variable
   :macro:`LOCAL_CONFIG_DIR` lists one or more directories which
   contain configuration files. The list is parsed from left to right.
   The leftmost (first) in the list is parsed first. Within each
   directory, a lexicographical ordering by file name determines the
   ordering of file consideration.
#. The value of configuration variable
   :macro:`LOCAL_CONFIG_FILE` lists one or more configuration
   files. These listed files are parsed from left to right. The leftmost
   (first) in the list is parsed first.
#. If one of these steps changes the value (right hand side) of
   :macro:`LOCAL_CONFIG_DIR`, then :macro:`LOCAL_CONFIG_DIR` is processed for a
   second time, using the changed list of directories.

The parsing and use of configuration files may be bypassed by setting
environment variable ``CONDOR_CONFIG`` with the string ``ONLY_ENV``.
With this setting, there is no attempt to locate or read configuration
files. This may be useful for testing where the environment contains all
needed information.

Configuration File Macros
-------------------------

:index:`in configuration file<single: in configuration file; macro>`
:index:`macro definitions<single: macro definitions; configuration file>`

Macro definitions are of the form:

.. code-block:: text

    <macro_name> = <macro_definition>

The macro name given on the left hand side of the definition is a case
insensitive identifier. There may be white space between the macro name,
the equals sign (=), and the macro definition. The macro definition is a
string literal that may utilize macro substitution.

Macro invocations are of the form:

.. code-block:: text

    $(macro_name[:<default if macro_name not defined>])

The colon and default are optional in a macro invocation. Macro
definitions may contain references to other macros, even ones that are
not yet defined, as long as they are eventually defined in the
configuration files. All macro expansion is done after all configuration
files have been parsed, with the exception of macros that reference
themselves.

.. code-block:: condor-config

    A = xxx
    C = $(A)

is a legal set of macro definitions, and the resulting value of ``C`` is
``xxx``. Note that ``C`` is actually bound to ``$(A)``, not its value.

As a further example,

.. code-block:: condor-config

    A = xxx
    C = $(A)
    A = yyy

is also a legal set of macro definitions, and the resulting value of
``C`` is ``yyy``.

A macro may be incrementally defined by invoking itself in its
definition. For example,

.. code-block:: condor-config

    A = xxx
    B = $(A)
    A = $(A)yyy
    A = $(A)zzz

is a legal set of macro definitions, and the resulting value of ``A`` is
``xxxyyyzzz``. Note that invocations of a macro in its own definition
are immediately expanded. ``$(A)`` is immediately expanded in line 3 of
the example. If it were not, then the definition would be impossible to
evaluate.

Recursively defined macros such as

.. code-block:: condor-config

    A = $(B)
    B = $(A)

are not allowed. They create definitions that HTCondor refuses to parse.

A macro invocation where the macro name is not defined results in a
substitution of the empty string. Consider the example

.. code-block:: condor-config

    MAX_ALLOC_CPUS = $(NUMCPUS)-1

If ``NUMCPUS`` is not defined, then this macro substitution becomes

.. code-block:: condor-config

    MAX_ALLOC_CPUS = -1

The default value may help to avoid this situation. The default value
may be a literal

.. code-block:: condor-config

    MAX_ALLOC_CPUS = $(NUMCPUS:4)-1

such that if ``NUMCPUS`` is not defined, the result of macro
substitution becomes

.. code-block:: condor-config

    MAX_ALLOC_CPUS = 4-1

The default may be another macro invocation:

.. code-block:: condor-config

    MAX_ALLOC_CPUS = $(NUMCPUS:$(DETECTED_CPUS_LIMIT))-1

These default specifications are restricted such that a macro invocation
with a default can not be nested inside of another default. An
alternative way of stating this restriction is that there can only be
one colon character per line. The effect of nested defaults can be
achieved by placing the macro definitions on separate lines of the
configuration.

All entries in a configuration file must have an operator, which will be
an equals sign (=). Identifiers are alphanumerics combined with the
underscore character, optionally with a subsystem name and a period as a
prefix. As a special case, a line without an operator that begins with a
left square bracket will be ignored. The following two-line example
treats the first line as a comment, and correctly handles the second
line.

.. code-block:: text

    [HTCondor Settings]
    my_classad = [ foo=bar ]

To simplify pool administration, any configuration variable name may be
prefixed by a subsystem (see the ``$(SUBSYSTEM)`` macro in
:ref:`admin-manual/introduction-to-configuration:pre-defined macros` for the
list of subsystems) and the period (.) character. For configuration variables
defined this way, the value is applied to the specific subsystem. For example,
the ports that HTCondor may use can be restricted to a range using the
:macro:`HIGHPORT` and :macro:`LOWPORT` configuration variables.

.. code-block:: condor-config

    MASTER.LOWPORT   = 20000
    MASTER.HIGHPORT  = 20100

Note that all configuration variables may utilize this syntax, but
nonsense configuration variables may result. For example, it makes no
sense to define

.. code-block:: condor-config

    NEGOTIATOR.MASTER_UPDATE_INTERVAL = 60

since the *condor_negotiator* daemon does not use the
:macro:`MASTER_UPDATE_INTERVAL` variable.

It makes little sense to do so, but HTCondor will configure correctly
with a definition such as

.. code-block:: condor-config

    MASTER.MASTER_UPDATE_INTERVAL = 60

The :tool:`condor_master` uses this configuration variable, and the prefix of
``MASTER.`` causes this configuration to be specific to the
:tool:`condor_master` daemon.

As of HTCondor version 8.1.1, evaluation works in the expected manner
when combining the definition of a macro with use of a prefix that gives
the subsystem name and a period. Consider the example

.. code-block:: condor-config

    FILESPEC = A
    MASTER.FILESPEC = B

combined with a later definition that incorporates ``FILESPEC`` in a
macro:

.. code-block:: condor-config

    USEFILE = mydir/$(FILESPEC)

When the :tool:`condor_master` evaluates variable ``USEFILE``, it evaluates
to ``mydir/B``. Previous to HTCondor version 8.1.1, it evaluated to
``mydir/A``. When any other subsystem evaluates variable ``USEFILE``, it
evaluates to ``mydir/A``.

This syntax has been further expanded to allow for the specification of
a local name on the command line using the command line option

.. code-block:: text

    -local-name <local-name>

This allows multiple instances of a daemon to be run by the same
:tool:`condor_master` daemon, each instance with its own local configuration
variable.

The ordering used to look up a variable, called <parameter name>:

#. <subsystem name>.<local name>.<parameter name>
#. <local name>.<parameter name>
#. <subsystem name>.<parameter name>
#. <parameter name>

If this local name is not specified on the command line, numbers 1 and 2
are skipped. As soon as the first match is found, the search is
completed, and the corresponding value is used.

This example configures a :tool:`condor_master` to run 2 *condor_schedd*
daemons. The :tool:`condor_master` daemon needs the configuration:

.. code-block:: condor-config

    XYZZY           = $(SCHEDD)
    XYZZY_ARGS      = -local-name xyzzy
    DAEMON_LIST     = $(DAEMON_LIST) XYZZY
    DC_DAEMON_LIST  = + XYZZY
    XYZZY_LOG       = $(LOG)/SchedLog.xyzzy

Using this example configuration, the :tool:`condor_master` starts up a
second *condor_schedd* daemon, where this second *condor_schedd*
daemon is passed **-local-name** *xyzzy* on the command line.

Continuing the example, configure the *condor_schedd* daemon named
``xyzzy``. This *condor_schedd* daemon will share all configuration
variable definitions with the other *condor_schedd* daemon, except for
those specified separately.

.. code-block:: condor-config

    SCHEDD.XYZZY.SCHEDD_NAME = XYZZY
    SCHEDD.XYZZY.SCHEDD_LOG  = $(XYZZY_LOG)
    SCHEDD.XYZZY.SPOOL       = $(SPOOL).XYZZY

Note that the example :macro:`SCHEDD_NAME` and :macro:`SPOOL` are specific to the
*condor_schedd* daemon, as opposed to a different daemon such as the
*condor_startd*. Other HTCondor daemons using this feature will have
different requirements for which parameters need to be specified
individually. This example works for the *condor_schedd*, and more
local configuration can, and likely would be specified.

Also note that each daemon's log file must be specified individually,
and in two places: one specification is for use by the :tool:`condor_master`,
and the other is for use by the daemon itself. In the example, the
``XYZZY`` *condor_schedd* configuration variable
``SCHEDD.XYZZY.SCHEDD_LOG`` definition references the :tool:`condor_master`
daemon's ``XYZZY_LOG``.

Comments and Line Continuations
-------------------------------

An HTCondor configuration file may contain comments and line
continuations. A comment is any line beginning with a pound character
(#). A continuation is any entry that continues across multiples lines.
Line continuation is accomplished by placing the backslash character (\\)
at the end of any line to be continued onto another. Valid examples of
line continuation are

.. code-block:: condor-config

    START = (KeyboardIdle > 15 * $(MINUTE)) && \
    ((LoadAvg - CondorLoadAvg) <= 0.3)

and

.. code-block:: condor-config

    ADMIN_MACHINES = condor.cs.wisc.edu, raven.cs.wisc.edu, \
    stork.cs.wisc.edu, ostrich.cs.wisc.edu, \
    bigbird.cs.wisc.edu
    ALLOW_ADMINISTRATOR = $(ADMIN_MACHINES)

Where a line continuation character directly precedes a comment, the
entire comment line is ignored, and the following line is used in the
continuation. Line continuation characters within comments are ignored.

Both this example

.. code-block:: condor-config

    A = $(B) \
    # $(C)
    $(D)

and this example

.. code-block:: condor-config

    A = $(B) \
    # $(C) \
    $(D)

result in the same value for A:

.. code-block:: condor-config

    A = $(B) $(D)

Multi-Line Values
-----------------

As of version 8.5.6, the value for a macro can comprise multiple lines
of text. The syntax for this is as follows:

.. code-block:: text

    <macro_name> @=<tag>
    <macro_definition lines>
    @<tag>

For example:

.. code-block:: condor-config

    # modify routed job attributes:
    # remove it if it goes on hold or stays idle for over 6 hours
    JOB_ROUTER_DEFAULTS @=jrd
      [
        requirements = target.WantJobRouter is true;
        MaxIdleJobs = 10;
        MaxJobs = 200;

        set_PeriodicRemove = JobStatus == 5 || (JobStatus == 1 && (time() - QDate) > 3600*6);
        delete_WantJobRouter = true;
        set_requirements = true;
      ]
      @jrd

Note that in this example, the square brackets are part of the
JOB_ROUTER_DEFAULTS value.

Executing a Program to Produce Configuration Macros
---------------------------------------------------

Instead of reading from a file, HTCondor can run a program to obtain
configuration macros. The vertical bar character (``|``) as the last
character defining a file name provides the syntax necessary to tell
HTCondor to run a program. This syntax may only be used in the
definition of the ``CONDOR_CONFIG`` environment variable, or the
:macro:`LOCAL_CONFIG_FILE` configuration variable.

The command line for the program is formed by the characters preceding
the vertical bar character. The standard output of the program is parsed
as a configuration file would be.

An example:

.. code-block:: condor-config

    LOCAL_CONFIG_FILE = /bin/make_the_config|

Program */bin/make_the_config* is executed, and its output is the set
of configuration macros.

Note that either a program is executed to generate the configuration
macros or the configuration is read from one or more files. The syntax
uses space characters to separate command line elements, if an executed
program produces the configuration macros. Space characters would
otherwise separate the list of files. This syntax does not permit
distinguishing one from the other, so only one may be specified.

(Note that the ``include command`` :index:`include command`
syntax (see below) is now the preferred way to execute a program to
generate configuration macros.)

Including Configuration from Elsewhere
--------------------------------------

:index:`INCLUDE syntax<single: INCLUDE syntax; configuration>`
:index:`INCLUDE configuration syntax`

Externally defined configuration can be incorporated using the following
syntax:

.. code-block:: condor-config

      include [ifexist] : <file>
      include : <cmdline>|
      include [ifexist] command [into <cache-file>] : <cmdline>

(Note that the ``ifexist`` and ``into`` options were added in version 8.5.7.
Also note that the command option must be specified in order to use the
``into`` option - just using the bar after <cmdline> will not work.)

In the file form of the ``include`` command, the <file> specification
must describe a single file, the contents of which will be parsed and
incorporated into the configuration. Unless the ``ifexist`` option is
specified, the non-existence of the file is a fatal error.

In the command line form of the ``include`` command (specified with
either the command option or by appending a bar (``|``) character after the
<cmdline> specification), the <cmdline> specification must describe a
command line (program and arguments); the command line will be executed,
and the output will be parsed and incorporated into the configuration.

If the ``into`` option is not used, the command line will be executed every
time the configuration file is referenced. This may well be undesirable,
and can be avoided by using the ``into`` option. The ``into`` keyword must be
followed by the full pathname of a file into which to write the output
of the command line. If that file exists, it will be read and the
command line will not be executed. If that file does not exist, the
output of the command line will be written into it and then the cache
file will be read and incorporated into the configuration. If the
command line produces no output, a zero length file will be created. If
the command line returns a non-zero exit code, configuration will abort
and the cache file will not be created unless the ``ifexist`` keyword is
also specified.

The ``include`` key word is case insensitive. There are no requirements
for white space characters surrounding the colon character.

Consider the example

.. code-block:: condor-config

      FILE = config.$(FULL_HOSTNAME)
      include : $(LOCAL_DIR)/$(FILE)

Values are acquired for configuration variables ``FILE``, and
:macro:`LOCAL_DIR` by immediate evaluation, causing variable
``FULL_HOSTNAME`` to also be immediately evaluated. The resulting value
forms a full path and file name. This file is read and parsed. The
resulting configuration is incorporated into the current configuration.
This resulting configuration may contain further nested ``include``
specifications, which are also parsed, evaluated, and incorporated.
Levels of nested ``include`` are limited, such that infinite nesting
is discovered and thwarted, while still permitting nesting.

Consider the further example

.. code-block:: condor-config

      SCRIPT_FILE = script.$(IP_ADDRESS)
      include : $(RELEASE_DIR)/$(SCRIPT_FILE) |

In this example, the bar character at the end of the line causes a
script to be invoked, and the output of the script is incorporated into
the current configuration. The same immediate parsing and evaluation
occurs in this case as when a file's contents are included.

For pools that are transitioning to using this new syntax in
configuration, while still having some tools and daemons with HTCondor
versions earlier than 8.1.6, special syntax in the configuration will
cause those daemons to fail upon startup, rather than continuing, but
incorrectly parsing the new syntax. Newer daemons will ignore the extra
syntax. Placing the ``@`` character before the ``include`` key word causes
the older daemons to fail when they attempt to parse this syntax.

Here is the same example, but with the syntax that causes older daemons
to fail when reading it.

.. code-block:: condor-config

      FILE = config.$(FULL_HOSTNAME)
      @include : $(LOCAL_DIR)/$(FILE)

A daemon older than version 8.1.6 will fail to start. Running an older
:tool:`condor_config_val` identifies the ``@include`` line as being bad. A
daemon of HTCondor version 8.1.6 or more recent sees:

.. code-block:: condor-config

      FILE = config.$(FULL_HOSTNAME)
      include : $(LOCAL_DIR)/$(FILE)

and starts up successfully.

Here is an example using the new ``ifexist`` and ``into`` options:

.. code-block:: condor-config

      # stuff.pl writes "STUFF=1" to stdout
      include ifexist command into $(LOCAL_DIR)/stuff.config : perl $(LOCAL_DIR)/stuff.pl

Reporting Errors and Warnings
-----------------------------

:index:`Error and warning syntax<single: Error and warning syntax; configuration>`
:index:`Error and warning configuration syntax`

As of version 8.5.7, warning and error messages can be included in
HTCondor configuration files.

The syntax for warning and error messages is as follows:

.. code-block:: condor-config

      warning : <warning message>
      error : <error message>

The warning and error messages will be printed when the configuration
file is used (when almost any HTCondor command is run, for example).
Error messages (unlike warnings) will prevent the successful use of the
configuration file. This will, for example, prevent a daemon from
starting, and prevent :tool:`condor_config_val` from returning a value.

Here's an example of using an error message in a configuration file
(combined with some of the new include features documented above):

.. code-block:: condor-config

    # stuff.pl writes "STUFF=1" to stdout
    include command into $(LOCAL_DIR)/stuff.config : perl $(LOCAL_DIR)/stuff.pl
    if ! defined stuff
      error : stuff is needed!
    endif

Conditionals in Configuration
-----------------------------

:index:`IF/ELSE syntax<single: IF/ELSE syntax; configuration>`
:index:`IF/ELSE configuration syntax`

Conditional if/else semantics are available in a limited form. The
syntax:

.. code-block:: text

      if <simple condition>
         <statement>
         . . .
         <statement>
      else
         <statement>
         . . .
         <statement>
      endif

An else key word and statements are not required, such that simple if
semantics are implemented. The <simple condition> does not permit
compound conditions. It optionally contains the exclamation point
character (!) to represent the not operation, followed by

-  the defined keyword followed by the name of a variable. If the
   variable is defined, the statement(s) are incorporated into the
   expanded input. If the variable is not defined, the statement(s) are
   not incorporated into the expanded input. As an example,

   .. code-block:: condor-config

         if defined MY_UNDEFINED_VARIABLE
            X = 12
         else
            X = -1
         endif

   results in ``X = -1``, when ``MY_UNDEFINED_VARIABLE`` is not yet
   defined.

-  the version keyword, representing the version number of of the daemon
   or tool currently reading this conditional. This keyword is followed
   by an HTCondor version number. That version number can be of the form
   x.y.z or x.y. The version of the daemon or tool is compared to the
   specified version number. The comparison operators are

   -  == for equality. Current version 8.2.3 is equal to 8.2.
   -  >= to see if the current version number is greater than or equal
      to. Current version 8.2.3 is greater than 8.2.2, and current
      version 8.2.3 is greater than or equal to 8.2.
   -  <= to see if the current version number is less than or equal to.
      Current version 8.2.0 is less than 8.2.2, and current version
      8.2.3 is less than or equal to 8.2.

   As an example,

   .. code-block:: condor-config

         if version >= 8.1.6
            DO_X = True
         else
            DO_Y = True
         endif

   results in defining ``DO_X`` as ``True`` if the current version of
   the daemon or tool reading this if statement is 8.1.6 or a more
   recent version.

-  True or yes or the value 1. The statement(s) are incorporated.
-  False or no or the value 0 The statement(s) are not incorporated.
-  $(<variable>) may be used where the immediately evaluated value is a
   simple boolean value. A value that evaluates to the empty string is
   considered False, otherwise a value that does not evaluate to a
   simple boolean value is a syntax error.

The syntax

.. code-block:: text

      if <simple condition>
         <statement>
         . . .
         <statement>
      elif <simple condition>
         <statement>
         . . .
         <statement>
      endif

is the same as syntax

.. code-block:: text

      if <simple condition>
         <statement>
         . . .
         <statement>
      else
         if <simple condition>
            <statement>
            . . .
            <statement>
         endif
      endif

Function Macros in Configuration
--------------------------------

:index:`function macros<single: function macros; configuration>`

A set of predefined functions increase flexibility. Both submit
description files and configuration files are read using the same
parser, so these functions may be used in both submit description files
and configuration files.

Case is significant in the function's name, so use the same letter case
as given in these definitions.

``$CHOICE(index, listname)`` or ``$CHOICE(index, item1, item2, ...)``
    An item within the list is returned. The list is represented by a
    parameter name, or the list items are the parameters. The ``index``
    parameter determines which item. The first item in the list is at
    index 0. If the index is out of bounds for the list contents, an
    error occurs.

``$ENV(environment-variable-name[:default-value])``
    Evaluates to the value of environment variable
    ``environment-variable-name``. If there is no environment variable
    with that name, Evaluates to UNDEFINED unless the optional
    :default-value is used; in which case it evaluates to default-value.
    For example,

    .. code-block:: condor-config

        A = $ENV(HOME)

    binds ``A`` to the value of the ``HOME`` environment variable.

``$F[fpduwnxbqa](filename)``
    One or more of the lower case letters may be combined to form the
    function name and thus, its functionality. Each letter operates on
    the ``filename`` in its own way.

    -  ``f`` convert relative path to full path by prefixing the current
       working directory to it. This option works only in
       :tool:`condor_submit` files.
 
    -  ``p`` refers to the entire directory portion of ``filename``,
       with a trailing slash or backslash character. Whether a slash or
       backslash is used depends on the platform of the machine. The
       slash will be recognized on Linux platforms; either a slash or
       backslash will be recognized on Windows platforms, and the parser
       will use the same character specified.

    -  ``d`` refers to the last portion of the directory within the
       path, if specified. It will have a trailing slash or backslash,
       as appropriate to the platform of the machine. The slash will be
       recognized on Linux platforms; either a slash or backslash will
       be recognized on Windows platforms, and the parser will use the
       same character specified unless u or w is used. if b is used the
       trailing slash or backslash will be omitted.

    -  ``u`` convert path separators to Unix style slash characters

    -  ``w`` convert path separators to Windows style backslash
       characters

    -  ``n`` refers to the file name at the end of any path, but without
       any file name extension. As an example, the return value from
       ``$Fn(/tmp/simulate.exe)`` will be ``simulate`` (without the
       ``.exe`` extension).

    -  ``x`` refers to a file name extension, with the associated period
       (``.``). As an example, the return value from
       ``$Fx(/tmp/simulate.exe)`` will be ``.exe``.

    -  ``b`` when combined with the d option, causes the trailing slash
       or backslash to be omitted. When combined with the x option,
       causes the leading period (``.``) to be omitted.

    -  ``q`` causes the return value to be enclosed within quotes.
       Double quote marks are used unless a is also specified.

    -  ``a`` When combined with the q option, causes the return value to
       be enclosed within single quotes.

``$DIRNAME(filename)`` is the same as ``$Fp(filename)``

``$BASENAME(filename)`` is the same as ``$Fnx(filename)``

``$INT(item-to-convert)`` or ``$INT(item-to-convert, format-specifier)``
    Expands, evaluates, and returns a string version of
    ``item-to-convert``. The ``format-specifier`` has the same syntax as
    a C language or Perl format specifier. If no ``format-specifier`` is
    specified, "%d" is used as the format specifier. The format
    is everything after the comma, including spaces.  It can include other text.

    .. code-block:: condor-config

        X = 2
        Y = 6
        XYArea = $(X) * $(Y)

    -  ``$INT(XYArea)`` is ``12``
    -  ``$INT(XYArea,%04d)`` is ``0012``
    -  ``$INT(XYArea,Area=%d)`` is ``Area=12``


``$RANDOM_CHOICE(choice1, choice2, choice3, ...)``
    :index:`RANDOM_CHOICE<pair RANDOM_CHOICE; config macros>` A random choice
    of one of the parameters in the list of parameters is made. For
    example, if one of the integers 0-8 (inclusive) should be randomly
    chosen:

    .. code-block:: console

        $RANDOM_CHOICE(0,1,2,3,4,5,6,7,8)

``$RANDOM_INTEGER(min, max [, step])``
    :index:`in configuration<single: in configuration; $RANDOM_INTEGER()>` A random integer
    within the range min and max, inclusive, is selected. The optional
    step parameter controls the stride within the range, and it defaults
    to the value 1. For example, to randomly chose an even integer in
    the range 0-8 (inclusive):

    .. code-block:: console

        $RANDOM_INTEGER(0, 8, 2)

``$REAL(item-to-convert)`` or ``$REAL(item-to-convert, format-specifier)``
    Expands, evaluates, and returns a string version of
    ``item-to-convert`` for a floating point type. The
    ``format-specifier`` is a C language or Perl format specifier. If no
    ``format-specifier`` is specified, "%16G" is used as a format
    specifier.

``$SUBSTR(name, start-index)`` or ``$SUBSTR(name, start-index, length)``
    Expands name and returns a substring of it. The first character of
    the string is at index 0. The first character of the substring is at
    index start-index. If the optional length is not specified, then the
    substring includes characters up to the end of the string. A
    negative value of start-index works back from the end of the string.
    A negative value of length eliminates use of characters from the end
    of the string. Here are some examples that all assume

    .. code-block:: condor-config

        Name = abcdef

    -  ``$SUBSTR(Name, 2)`` is ``cdef``.
    -  ``$SUBSTR(Name, 0, -2)`` is ``abcd``.
    -  ``$SUBSTR(Name, 1, 3)`` is ``bcd``.
    -  ``$SUBSTR(Name, -1)`` is ``f``.
    -  ``$SUBSTR(Name, 4, -3)`` is the empty string, as there are no
       characters in the substring for this request.

``$STRING(item-to-convert)`` or ``$STRING(item-to-convert, format-specifier)``
    Expands, evaluates, and returns a string version of
    ``item-to-convert`` for a string type. The
    ``format-specifier`` is a C language or Perl format specifier. If no
    ``format-specifier`` is specified, "%s" is used as a format specifier.  The format
    is everything after the comma, including spaces.  It can include other text
    besides %s.

    .. code-block:: condor-config

        FULL_HOSTNAME = host.DOMAIN
        LCFullHostname = toLower("$(FULL_HOSTNAME)")

    -  ``$STRING(LCFullHostname)`` is ``host.domain``
    -  ``$STRING(LCFullHostname,Name: %s)`` is ``Name: host.domain``


``$EVAL(item-to-convert)``
    Expands, evaluates, and returns an classad unparsed version of
    ``item-to-convert`` for any classad type, the resulting value is
    formatted using the equivalent of the "%v" format specifier - If it
    is a string it is printed without quotes, otherwise it is unparsed
    as a classad value.  Due to the way the parser works, you must use
    a variable to hold the expression to be evaluated if the expression
    has a close brace ')' character.

    .. code-block:: condor-config

        slist = "a,B,c"
        lcslist = tolower($(slist))
        list = split($(slist))
        clist = size($(list)) * 10
        semilist = join(";",split($(lcslist)))

    -  ``$EVAL(slist)`` is ``a,B,c``
    -  ``$EVAL(lcslist)`` is ``a,b,c``
    -  ``$EVAL(list)`` is ``{"a", "B", "c"}``
    -  ``$EVAL(clist)`` is ``30``
    -  ``$EVAL(semilist)`` is ``a;b;c``


Environment references are not currently used in standard HTCondor
configurations. However, they can sometimes be useful in custom
configurations.

Macros That Will Require a Restart When Changed
-----------------------------------------------

:index:`configuration change requiring a restart of HTCondor`

The HTCondor daemons will generally not undo any work they have already done when the configuration changes
so any change that would require undoing of work will require a restart before it takes effect.  There a very
few exceptions to this rule.  The :tool:`condor_master` will pick up changes to :macro:`DAEMON_LIST` on a reconfig.
Although it may take hours for a *condor_startd* to drain and exit when it is removed from the daemon list.

Examples of changes requiring a restart would any change to how HTCondor uses the network. A configuration change 
to :macro:`NETWORK_INTERFACE`, :macro:`NETWORK_HOSTNAME`, :macro:`ENABLE_IPV4` and :macro:`ENABLE_IPV6` require a restart. A change in the
way daemons locate each other, such as :macro:`PROCD_ADDRESS`, :macro:`BIND_ALL_INTERFACES`, :macro:`USE_SHARED_PORT` or :macro:`SHARED_PORT_PORT`
require a restart of the :tool:`condor_master` and all of the daemons under it.

The *condor_startd* requires a restart to make any change to the slot resource configuration, This would include :macro:`MEMORY`,
:macro:`NUM_CPUS` and :macro:`NUM_SLOTS_TYPE_<N>`.  It would also include resource detection like GPUs and Docker support.
A general rule of thumb is that changes to the *condor_startd* require a restart, but there are a few exceptions.
:macro:`STARTD_ATTRS` as well as :macro:`START`, :macro:`PREEMPT`, and other policy expressions take effect on reconfig.

For more information about specific configuration variables and whether a restart is required, refer to the documentation
of the individual variables.


Pre-Defined Macros
------------------

:index:`pre-defined macros<single: pre-defined macros; configuration>`
:index:`pre-defined macros<single: pre-defined macros; configuration file>`

HTCondor provides pre-defined macros that help configure HTCondor.
Pre-defined macros are listed as ``$(macro_name)``.

This first set are entries whose values are determined at run time and
cannot be overwritten. These are inserted automatically by the library
routine which parses the configuration files. This implies that a change
to the underlying value of any of these variables will require a full
restart of HTCondor in order to use the changed value.

``$(FULL_HOSTNAME)`` :index:`FULL_HOSTNAME`
    The fully qualified host name of the local machine, which is host
    name plus domain name.

``$(HOSTNAME)`` :index:`HOSTNAME`
    The host name of the local machine, without a domain name.

``$(IP_ADDRESS)`` :index:`IP_ADDRESS`
    The ASCII string version of the local machine's "most public" IP
    address. This address may be IPv4 or IPv6, but the macro will always
    be set.

    HTCondor selects the "most public" address heuristically. Your
    configuration should not depend on HTCondor picking any particular
    IP address for this macro; this macro's value may not even be one of
    the IP addresses HTCondor is configured to advertise.

``$(IPV4_ADDRESS)`` :index:`IPV4_ADDRESS`
    The ASCII string version of the local machine's "most public" IPv4
    address; unset if the local machine has no IPv4 address.

    See ``IP_ADDRESS`` about "most public".

``$(IPV6_ADDRESS)`` :index:`IPV6_ADDRESS`
    The ASCII string version of the local machine's "most public" IPv6
    address; unset if the local machine has no IPv6 address.

    See ``IP_ADDRESS`` about "most public".

``$(IP_ADDRESS_IS_V6)`` :index:`IP_ADDRESS_IS_V6`
    A boolean which is true if and only if ``IP_ADDRESS``
    :index:`IP_ADDRESS` is an IPv6 address. Useful for conditional
    configuration.

``$(TILDE)`` :index:`TILDE`
    The full path to the home directory of the Unix user condor, if such
    a user exists on the local machine.

``$(SUBSYSTEM)`` :index:`SUBSYSTEM` :index:`subsystem names<single: subsystem names; configuration file>`
    The subsystem name of the daemon or tool that is evaluating the
    macro. This is a unique string which identifies a given daemon
    within the HTCondor system. The possible subsystem names are:
    :index:`subsystem names`
    :index:`subsystem names<single: subsystem names; macro>`

    .. include:: subsystems.rst

``$(DETECTED_CPUS)`` :index:`DETECTED_CPUS`
    The integer number of hyper-threaded CPUs, as given by
    ``$(DETECTED_CORES)``, when :macro:`COUNT_HYPERTHREAD_CPUS` is ``True``.
    The integer number of physical (non hyper-threaded) CPUs, as given
    by ``$(DETECTED_PHYSICAL_CPUS)``, when :macro:`COUNT_HYPERTHREAD_CPUS`
    :index:`COUNT_HYPERTHREAD_CPUS` is ``False``. 

``$(DETECTED_PHYSICAL_CPUS)`` :index:`DETECTED_PHYSICAL_CPUS`
    The integer number of physical (non hyper-threaded) CPUs. This will
    be equal the number of unique CPU IDs.

``$(DETECTED_CPUS_LIMIT)`` :index:`DETECTED_CPUS_LIMIT`
    An integer value which is set to the minimum of ``$(DETECTED_CPUS)`` 
    and values from the environment variables ``OMP_THREAD_LIMIT`` and
    ``SLURM_CPUS_ON_NODE``.  It intended for use as the value of
    :macro:`NUM_CPUS` to insure that the number of CPUS that a *condor_startd* will
    provision does not exceed the limits indicated by the environment.
    Defaults to ``$(DETECTED_CPUS)`` when there is no environment variable that sets a lower value.

This second set of macros are entries whose default values are
determined automatically at run time but which can be overwritten.
:index:`macros<single: macros; configuration file>`

``$(ARCH)`` :index:`ARCH`
    Defines the string used to identify the architecture of the local
    machine to HTCondor. The *condor_startd* will advertise itself with
    this attribute so that users can submit binaries compiled for a
    given platform and force them to run on the correct machines.
    :tool:`condor_submit` will append a requirement to the job ClassAd that
    it must run on the same :ad-attr:`Arch` and :ad-attr:`OpSys` of the machine where
    it was submitted, unless the user specifies :ad-attr:`Arch` and/or
    :ad-attr:`OpSys` explicitly in their submit file. See the :tool:`condor_submit`
    manual page (doc:`/man-pages/condor_submit`) for details.

``$(OPSYS)`` :index:`OPSYS`
    Defines the string used to identify the operating system of the
    local machine to HTCondor. If it is not defined in the configuration
    file, HTCondor will automatically insert the operating system of
    this machine as determined by *uname*.

``$(OPSYS_VER)`` :index:`OPSYS_VER`
    Defines the integer used to identify the operating system version
    number.

``$(OPSYS_AND_VER)`` :index:`OPSYS_AND_VER`
    Defines the string used prior to HTCondor version 7.7.2 as
    ``$(OPSYS)``.

``$(UNAME_ARCH)`` :index:`UNAME_ARCH`
    The architecture as reported by *uname* (2)'s ``machine`` field.
    Always the same as :ad-attr:`Arch` on Windows.

``$(UNAME_OPSYS)`` :index:`UNAME_OPSYS`
    The operating system as reported by *uname* (2)'s ``sysname``
    field. Always the same as :ad-attr:`OpSys` on Windows.

``$(DETECTED_MEMORY)`` :index:`DETECTED_MEMORY`
    The amount of detected physical memory (RAM) in MiB.

``$(DETECTED_CORES)`` :index:`DETECTED_CORES`
    The number of CPU cores that the operating system schedules. On
    machines that support hyper-threading, this will be the number of
    hyper-threads.

``$(PID)`` :index:`PID`
    The process ID for the daemon or tool.

``$(PPID)`` :index:`PPID`
    The process ID of the parent process for the daemon or tool.

``$(USERNAME)`` :index:`USERNAME`
    The user name of the UID of the daemon or tool. For daemons started
    as root, but running under another UID (typically the user condor),
    this will be the other UID.

``$(FILESYSTEM_DOMAIN)`` :index:`FILESYSTEM_DOMAIN`
    Defaults to the fully qualified host name of the machine it is
    evaluated on. See the :doc:`/admin-manual/configuration-macros` section, Shared File
    System Configuration File Entries for the full description of its
    use and under what conditions it could be desirable to change it.

``$(UID_DOMAIN)`` :index:`UID_DOMAIN`
    Defaults to the fully qualified host name of the machine it is
    evaluated on. See the :doc:`/admin-manual/configuration-macros` section for the full
    description of this configuration variable.

``$(CONFIG_ROOT)`` :index:`CONFIG_ROOT`
   Set to the directory where the the main config file will be read prior to reading any 
   config files. The value will usually be ``/etc/condor`` for an RPM install,
   ``C:\Condor`` for a Windows MSI install and the directory part of the ``CONDOR_CONFIG`` environment
   variable for a tarball install. This variable will not be set when ``CONDOR_CONFIG`` is
   set to ``ONLY_ENV`` so that no configuration files are read.

Since ``$(ARCH)`` and ``$(OPSYS)`` will automatically be set to the
correct values, we recommend that you do not overwrite them.

Configuration Templates
-----------------------

:index:`configuration templates<single: configuration templates; HTCondor>`
:index:`configuration: templates`

Achieving certain behaviors in an HTCondor pool often requires setting
the values of a number of configuration macros in concert with each
other. We have added configuration templates as a way to do this more
easily, at a higher level, without having to explicitly set each
individual configuration macro.

Configuration templates are pre-defined; users cannot define their own
templates.

Note that the value of an individual configuration macro that is set by
a configuration template can be overridden by setting that configuration
macro later in the configuration.

Detailed information about configuration templates (such as the macros
they set) can be obtained using the :tool:`condor_config_val` ``use`` option
(see the :doc:`/man-pages/condor_config_val` manual page). (This
document does not contain such information because the
:tool:`condor_config_val` command is a better way to obtain it.)

Configuration Templates: Using Predefined Sets of Configuration
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

:index:`USE syntax<single: USE syntax; configuration>`
:index:`USE configuration syntax`

Predefined sets of configuration can be identified and incorporated into
the configuration using the syntax

.. code-block:: text

      use <category name> : <template name>

The ``use`` key word is case insensitive. There are no requirements for
white space characters surrounding the colon character. More than one
``<template name>`` identifier may be placed within a single ``use``
line. Separate the names by a space character. There is no mechanism by
which the administrator may define their own custom ``<category name>``
or ``<template name>``.

Each predefined ``<category name>`` has a fixed, case insensitive name
for the sets of configuration that are predefined. Placement of a
``use`` line in the configuration brings in the predefined configuration
it identifies.

Some of the configuration templates take arguments (as described below).

Available Configuration Templates
'''''''''''''''''''''''''''''''''

There are four ``<category name>`` values. Within a category, a
predefined, case insensitive name identifies the set of configuration it
incorporates.

:config-template:`ROLE` category
    Describes configuration for the various roles that a machine might
    play within an HTCondor pool. The configuration will identify which
    daemons are running on a machine.

    -  :config-template:`Personal<ROLE>`

       Settings needed for when a single machine is the entire pool.

    -  :config-template:`Submit<ROLE>`

       Settings needed to allow this machine to submit jobs to the pool.
       May be combined with ``Execute`` and ``CentralManager`` roles.

    -  :config-template:`Execute<ROLE>`

       Settings needed to allow this machine to execute jobs. May be
       combined with ``Submit`` and ``CentralManager`` roles.

    -  :config-template:`CentralManager<ROLE>`

       Settings needed to allow this machine to act as the central
       manager for the pool. May be combined with ``Submit`` and
       ``Execute`` roles.

:config-template:`FEATURE` category
    Describes configuration for implemented features.

    -  :config-template:`Remote_Runtime_Config<FEATURE>`

       Enables the use of :tool:`condor_config_val` **-rset** to the machine
       with this configuration. Note that there are security
       implications for use of this configuration, as it potentially
       permits the arbitrary modification of configuration. Variable
       :macro:`SETTABLE_ATTRS_CONFIG` must also be defined.

    -  :config-template:`Remote_Config<FEATURE>`

       Enables the use of :tool:`condor_config_val` **-set** to the machine
       with this configuration. Note that there are security
       implications for use of this configuration, as it potentially
       permits the arbitrary modification of configuration. Variable
       :macro:`SETTABLE_ATTRS_CONFIG` must also be defined.

    -  :config-template:`GPUs([discovery_args])<FEATURE>`

       Sets configuration based on detection with the
       :tool:`condor_gpu_discovery` tool, and defines a custom resource
       using the name ``GPUs``. Supports both OpenCL and CUDA, if
       detected. Automatically includes the ``GPUsMonitor`` feature.
       Optional discovery_args are passed to :tool:`condor_gpu_discovery`
       Includes :macro:`GPU_DISCOVERY_EXTRA` when calling
       :tool:`condor_gpu_discovery`, even if *discovery_args* are defined.

    -  :config-template:`GPUsMonitor<FEATURE>`

       Also adds configuration to report the usage of NVidia GPUs.

    -  :config-template:`Monitor( resource_name, mode, period, executable, metric[, metric]+ )<FEATURE>`

       Configures a custom machine resource monitor with the given name,
       mode, period, executable, and metrics. See
       :ref:`admin-manual/ep-policy-configuration:Startd Cron` for the definitions of
       these terms.

    -  :config-template:`PartitionableSlot( slot_type_num [, allocation] )<FEATURE>`

       Sets up a partitionable slot of the specified slot type number
       and allocation (defaults for slot_type_num and allocation are 1
       and 100% respectively). See the 
       :ref:`admin-manual/ep-policy-configuration:*condor_startd* policy
       configuration` for information on partitionable slot policies.

    -  :config-template:`StaticSlots( slot_type_num [, num_slots, [, allocation] ] )<FEATURE>`

       Sets up a number of static slots of the specified slot type number
       (defaults for slot_type_num and num_slots are 1 and ``$(NUM_CPUS)`` respectively).
       The number of slots will be equal to ``num_slots``. If no value is provided for the allocation,
       the default is to divide 100% of the machine resources evenly across the slots.

    -  :config-template:`AssignAccountingGroup( map_filename [, check_request] )<FEATURE>`
       Sets up a
       *condor_schedd* job transform that assigns an accounting group
       to each job as it is submitted. The accounting group is determined by
       mapping the Owner attribute of the job using the given map file, which
       should specify the allowed accounting groups each Owner is permitted to use.
       If the submitted job has an accounting group, that is treated as a requested
       accounting group and validated against the map.  If the optional
       ``check_request`` argument is true or not present submission will
       fail if the requested accounting group is present and not valid.  If the argument
       is false, the requested accounting group will be ignored if it is not valid.

    -  :config-template:`ScheddUserMapFile( map_name, map_filename )<FEATURE>`
       Defines a
       *condor_schedd* usermap named map_name using the given map
       file.

    -  :config-template:`SetJobAttrFromUserMap( dst_attr, src_attr, map_name [, map_filename] )<FEATURE>`
       Sets up a *condor_schedd* job transform that sets the dst_attr
       attribute of each job as it is submitted. The value of dst_attr
       is determined by mapping the src_attr of the job using the
       usermap named map_name. If the optional map_filename argument
       is specified, then this metaknob also defines a *condor_schedd*
       usermap named map_Name using the given map file.

    -  :config-template:`StartdCronOneShot( job_name, exe [, hook_args] )<FEATURE>`

       Create a one-shot *condor_startd* job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`StartdCronPeriodic( job_name, period, exe [, hook_args] )<FEATURE>`

       Create a periodic-shot *condor_startd* job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`StartdCronContinuous( job_name, exe [, hook_args] )<FEATURE>`

       Create a (nearly) continuous *condor_startd* job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`ScheddCronOneShot( job_name, exe [, hook_args] )<FEATURE>`

       Create a one-shot *condor_schedd* job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`ScheddCronPeriodic( job_name, period, exe [, hook_args] )<FEATURE>`

       Create a periodic-shot *condor_schedd* job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`ScheddCronContinuous( job_name, exe [, hook_args] )<FEATURE>`

       Create a (nearly) continuous *condor_schedd* job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`OneShotCronHook( STARTD_CRON | SCHEDD_CRON, job_name, hook_exe [,hook_args] )<FEATURE>`

       Create a one-shot job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`PeriodicCronHook( STARTD_CRON | SCHEDD_CRON , job_name, period, hook_exe [,hook_args] )<FEATURE>`

       Create a periodic job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`ContinuousCronHook( STARTD_CRON | SCHEDD_CRON , job_name, hook_exe [,hook_args] )<FEATURE>`

       Create a (nearly) continuous job hook.
       (See :ref:`admin-manual/ep-policy-configuration:Startd Cron` for more information
       about job hooks.)

    -  :config-template:`OAuth<FEATURE>`

       Sets configuration that enables the *condor_credd* and *condor_credmon_oauth* daemons,
       which allow for the automatic renewal of user-supplied OAuth2 credentials.
       See section :ref:`enabling_oauth_credentials` for more information.

    -  :config-template:`Adstash<FEATURE>`

       Sets configuration that enables :tool:`condor_adstash` to run as a daemon.
       :tool:`condor_adstash` polls job history ClassAds and pushes them to an
       Elasticsearch index, see section
       :ref:`admin-manual/cm-configuration:Elasticsearch` for more information.

    -  :config-template:`UWCS_Desktop_Policy_Values<FEATURE>`

       Configuration values used in the ``UWCS_DESKTOP`` policy. (Note
       that these values were previously in the parameter table;
       configuration that uses these values will have to use the
       ``UWCS_Desktop_Policy_Values`` template. For example,
       ``POLICY : UWCS_Desktop`` uses the
       ``FEATURE : UWCS_Desktop_Policy_Values`` template.)

.. _CommonCloudAttributesConfiguration:

    - :config-template:`CommonCloudAttributesAWS<FEATURE>`
    - :config-template:`CommonCloudAttributesGoogle<FEATURE>`

       Sets configuration that will put some common cloud-related attributes
       in the slot ads.  Use the version which specifies the cloud you're
       using.  See :ref:`CommonCloudAttributes` for details.

    - :config-template:`JobsHaveInstanceIDs<FEATURE>`

       Sets configuration that will cause job ads to track the instance IDs
       of slots that they ran on (if available).

    - :config-template:`HPC_ANNEX<FEATURE>`

       Set configuration that enables the use of the ``annex`` noun
       in the :doc:`../man-pages/htcondor` command.

    - :config-template:`DefaultCheckpointDestination<FEATURE>`

       Implements the :macro:`EXTENDED_SUBMIT_COMMANDS` ``checkpoint_storage``,
       which allows the administrator to define a default
       :subcom:`checkpoint_destination`, and the user to explicitly request
       either that default (with the literal ``default``),
       storage in :macro:`SPOOL` (with the literal ``spool``; this is also
       the default if :subcom:`checkpoint_destination` is unset),
       or their own specific checkpoint destination.

       The administrator must either set the configuration macro
       ``DEFAULT_CHECKPOINT_DESTINATION_PREFIX`` to a URL prefix, to which
       ``/`` and :ad-attr:`Owner` will be concatenated; or set the configuration
       macro ``DEFAULT_CHECKPOINT_DESTINATION`` to the whole URL.  As an
       example, the default ``DEFAULT_CHECKPOINT_DESTINATION`` is
       ``"$(DEFAULT_CHECKPOINT_DESTINATION_PREFIX)/$(MY.Owner)"``.

:config-template:`POLICY` category
    Describes configuration for the circumstances under which machines
    choose to run jobs.

    -  :config-template:`Always_Run_Jobs<POLICY>`

       Always start jobs and run them to completion, without
       consideration of *condor_negotiator* generated preemption or
       suspension. This is the default policy, and it is intended to be
       used with dedicated resources. If this policy is used together
       with the ``Limit_Job_Runtimes`` policy, order the specification
       by placing this ``Always_Run_Jobs`` policy first.

.. _OnlyRegisteredCheckpointDestinations:

    -  :config-template:`OnlyRegisteredCheckpointDestinations<POLICY>`

       Jobs which specify a checkpoint destination must specify a checkpoint
       destination that the AP knows how to clean up (that has a matching
       entry in :macro:`CHECKPOINT_DESTINATION_MAPFILE`).

    -  :config-template:`UWCS_Desktop<POLICY>`

       This was the default policy before HTCondor version 8.1.6. It is
       intended to be used with desktop machines not exclusively running
       HTCondor jobs. It injects ``UWCS`` into the name of some
       configuration variables.

    -  :config-template:`Desktop<POLICY>`

       An updated and re-implementation of the ``UWCS_Desktop`` policy,
       but without the ``UWCS`` naming of some configuration variables.

    -  :config-template:`Limit_Job_Runtimes( limit_in_seconds )<POLICY>`

       Limits running jobs to a maximum of the specified time using
       preemption. (The default limit is 24 hours.) This policy does not
       work while the machine is draining; use the following policy
       instead.

       If this policy is used together with the ``Always_Run_Jobs``
       policy, order the specification by placing this
       ``Limit_Job_Runtimes`` policy second.

    -  :config-template:`Preempt_if_Runtime_Exceeds( limit_in_seconds )<POLICY>`

       Limits running jobs to a maximum of the specified time using
       preemption. (The default limit is 24 hours).

    -  :config-template:`Hold_if_Runtime_Exceeds( limit_in_seconds )<POLICY>`

       Limits running jobs to a maximum of the specified time by placing
       them on hold immediately (ignoring any job retirement time). (The
       default limit is 24 hours).

    -  :config-template:`Preempt_If_Cpus_Exceeded<POLICY>`

       If the startd observes the number of CPU cores used by the job
       exceed the number of cores in the slot by more than 0.8 on
       average over the past minute, preempt the job immediately
       ignoring any job retirement time.

    -  :config-template:`Hold_If_Cpus_Exceeded<POLICY>`

       If the startd observes the number of CPU cores used by the job
       exceed the number of cores in the slot by more than 0.8 on
       average over the past minute, immediately place the job on hold
       ignoring any job retirement time. The job will go on hold with a
       reasonable hold reason in job attribute :ad-attr:`HoldReason` and a
       value of 101 in job attribute :ad-attr:`HoldReasonCode`. The hold reason
       and code can be customized by specifying
       ``HOLD_REASON_CPU_EXCEEDED`` and ``HOLD_SUBCODE_CPU_EXCEEDED``
       respectively.

    -  :config-template:`Preempt_If_Disk_Exceeded<POLICY>`

       If the startd observes the amount of disk space used by the job
       exceed the disk in the slot, preempt the job immediately
       ignoring any job retirement time.

    -  :config-template:`Hold_If_Disk_Exceeded<POLICY>`

       If the startd observes the amount of disk space used by the job
       exceed the disk in the slot, immediately place the job on hold
       ignoring any job retirement time. The job will go on hold with a
       reasonable hold reason in job attribute :ad-attr:`HoldReason` and a
       value of 104 in job attribute :ad-attr:`HoldReasonCode`. The hold reason
       and code can be customized by specifying
       ``HOLD_REASON_DISK_EXCEEDED`` and ``HOLD_SUBCODE_DISK_EXCEEDED``
       respectively.

    -  :config-template:`Preempt_If_Memory_Exceeded<POLICY>`

       If the startd observes the memory usage of the job exceed the
       memory provisioned in the slot, preempt the job immediately
       ignoring any job retirement time.

    -  :config-template:`Hold_If_Memory_Exceeded<POLICY>`

       If the startd observes the memory usage of the job exceed the
       memory provisioned in the slot, immediately place the job on hold
       ignoring any job retirement time. The job will go on hold with a
       reasonable hold reason in job attribute :ad-attr:`HoldReason` and a
       value of 102 in job attribute :ad-attr:`HoldReasonCode`. The hold reason
       and code can be customized by specifying
       ``HOLD_REASON_MEMORY_EXCEEDED`` and
       ``HOLD_SUBCODE_MEMORY_EXCEEDED`` respectively.

    -  :config-template:`Preempt_If( policy_variable )<POLICY>`

       Preempt jobs according to the specified policy.
       ``policy_variable`` must be the name of a configuration macro
       containing an expression that evaluates to ``True`` if the job
       should be preempted.

       See an example here:
       :ref:`admin-manual/introduction-to-configuration:configuration template examples`.

    -  :config-template:`Want_Hold_If( policy_variable, subcode, reason_text )<POLICY>`

       Add the given policy to the :macro:`WANT_HOLD` expression; if the
       :macro:`WANT_HOLD` expression is defined, ``policy_variable`` is
       prepended to the existing expression; otherwise :macro:`WANT_HOLD` is
       simply set to the value of the policy_variable macro.

       See an example here:
       :ref:`admin-manual/introduction-to-configuration:configuration template examples`.

    -  :config-template:`Startd_Publish_CpusUsage<POLICY>`

       Publish the number of CPU cores being used by the job into the
       slot ad as attribute :ad-attr:`CpusUsage`. This value will be the
       average number of cores used by the job over the past minute,
       sampling every 5 seconds.

:config-template:`SECURITY` category
    Describes configuration for an implemented security model.

    -  :config-template:`Host_Based<SECURITY>`

       The default security model (based on IPs and DNS names). Do not
       combine with ``User_Based`` security.

    -  :config-template:`User_Based<SECURITY>`

       Grants permissions to an administrator and uses
       ``With_Authentication``. Do not combine with ``Host_Based``
       security.

    -  :config-template:`With_Authentication<SECURITY>`

       Requires both authentication and integrity checks.

    -  :config-template:`Strong<SECURITY>`

       Requires authentication, encryption, and integrity checks.

Configuration Template Transition Syntax
''''''''''''''''''''''''''''''''''''''''

For pools that are transitioning to using this new syntax in
configuration, while still having some tools and daemons with HTCondor
versions earlier than 8.1.6, special syntax in the configuration will
cause those daemons to fail upon start up, rather than use the new, but
misinterpreted, syntax. Newer daemons will ignore the extra syntax.
Placing the @ character before the ``use`` key word causes the older
daemons to fail when they attempt to parse this syntax.

As an example, consider the *condor_startd* as it starts up. A
*condor_startd* previous to HTCondor version 8.1.6 fails to start when
it sees:

.. code-block:: condor-config

    @use feature : GPUs

Running an older :tool:`condor_config_val` also identifies the ``@use`` line
as being bad. A *condor_startd* of HTCondor version 8.1.6 or more
recent sees

.. code-block:: condor-config

    use feature : GPUs

Configuration Template Examples
'''''''''''''''''''''''''''''''

-  Preempt a job if its memory usage exceeds the requested memory:

   .. code-block:: condor-config

        MEMORY_EXCEEDED = (isDefined(MemoryUsage) && MemoryUsage > RequestMemory)
        use POLICY : PREEMPT_IF(MEMORY_EXCEEDED) 

-  Put a job on hold if its memory usage exceeds the requested memory:

   .. code-block:: condor-config

        MEMORY_EXCEEDED = (isDefined(MemoryUsage) && MemoryUsage > RequestMemory)
        use POLICY : WANT_HOLD_IF(MEMORY_EXCEEDED, 102, memory usage exceeded request_memory) 

-  Update dynamic GPU information every 15 minutes:

   .. code-block:: condor-config

        use FEATURE : StartdCronPeriodic(DYNGPU, 15*60, $(LOCAL_DIR)\dynamic_gpu_info.pl, $(LIBEXEC)\condor_gpu_discovery -dynamic)

   where ``dynamic_gpu_info.pl`` is a simple perl script that strips off
   the DetectedGPUs line from :tool:`condor_gpu_discovery`:

   .. code-block:: perl

        #!/usr/bin/env perl
        my @attrs = `@ARGV`; 
        for (@attrs) { 
            next if ($_ =~ /^Detected/i); 
            print $_; 
        } 

Configuring HTCondor for Multiple Platforms
-------------------------------------------

A single, initial configuration file may be used for all platforms in an
HTCondor pool, with platform-specific settings placed in separate files.  This
greatly simplifies administration of a heterogeneous pool by allowing
specification of platform-independent, global settings in one place, instead of
separately for each platform. This is made possible by treating the
:macro:`LOCAL_CONFIG_FILE` configuration variable as a list of files, instead
of a single file. Of course, this only helps when using a shared file system
for the machines in the pool, so that multiple machines can actually share a
single set of configuration files.

With multiple platforms, put all platform-independent settings (the vast
majority) into the single initial configuration file, which will be
shared by all platforms. Then, set the :macro:`LOCAL_CONFIG_FILE`
configuration variable from that global configuration file to specify
both a platform-specific configuration file and optionally, a local,
machine-specific configuration file.

The name of platform-specific configuration files may be specified by
using ``$(ARCH)`` and ``$(OPSYS)``, as defined automatically by
HTCondor. For example, for 32-bit Intel Windows 7 machines and 64-bit
Intel Linux machines, the files ought to be named:

.. code-block:: console

      $ condor_config.INTEL.WINDOWS
      condor_config.X86_64.LINUX

Then, assuming these files are in the directory defined by the ``ETC``
configuration variable, and machine-specific configuration files are in
the same directory, named by each machine's host name,
:macro:`LOCAL_CONFIG_FILE` becomes:

.. code-block:: condor-config

    LOCAL_CONFIG_FILE = $(ETC)/condor_config.$(ARCH).$(OPSYS), \
                        $(ETC)/$(HOSTNAME).local

Platform-Specific Configuration File Settings
'''''''''''''''''''''''''''''''''''''''''''''

The configuration variables that are truly platform-specific are:

:macro:`RELEASE_DIR`
    Full path to the installed HTCondor binaries. While the
    configuration files may be shared among different platforms, the
    binaries certainly cannot. Therefore, maintain separate release
    directories for each platform in the pool.

:macro:`MAIL`
    The full path to the mail program.

:macro:`CONSOLE_DEVICES`
    Which devices in ``/dev`` should be treated as console devices.

:macro:`DAEMON_LIST`
    Which daemons the :tool:`condor_master` should start up. The reason this
    setting is platform-specific is to distinguish the *condor_kbdd*.
    It is needed on many Linux and Windows machines, and it is not
    needed on other platforms.

Reasonable defaults for all of these configuration variables will be
found in the default configuration files inside a given platform's
binary distribution (except the :macro:`RELEASE_DIR`, since the location of
the HTCondor binaries and libraries is installation specific). With
multiple platforms, use one of the ``condor_config`` files from either
running :tool:`condor_configure` or from the
``$(RELEASE_DIR)``/etc/examples/condor_config.generic file, take these
settings out, save them into a platform-specific file, and install the
resulting platform-independent file as the global configuration file.
Then, find the same settings from the configuration files for any other
platforms to be set up, and put them in their own platform-specific
files. Finally, set the :macro:`LOCAL_CONFIG_FILE` configuration variable to
point to the appropriate platform-specific file, as described above.

Not even all of these configuration variables are necessarily going to
be different. For example, if an installed mail program understands the
**-s** option in ``/usr/local/bin/mail`` on all platforms, the :macro:`MAIL`
macro may be set to that in the global configuration file, and not
define it anywhere else. For a pool with only Linux or Windows machines,
the :macro:`DAEMON_LIST` will be the same for each, so there is no reason not
to put that in the global configuration file.

Other Uses for Platform-Specific Configuration Files
''''''''''''''''''''''''''''''''''''''''''''''''''''

An installation may want other configuration variables to be platform-specific.
Perhaps a different policy is desired for one of the platforms.  Perhaps
different people should get the e-mail about problems with the different
platforms. There is nothing hard-coded about any of this. What is shared and
what should not shared is entirely configurable.

Since the :macro:`LOCAL_CONFIG_FILE` macro
can be an arbitrary list of files, an installation can even break up the
global, platform-independent settings into separate files. In fact, the
global configuration file might only contain a definition for
:macro:`LOCAL_CONFIG_FILE`, and all other configuration variables would be
placed in separate files.

Different people may be given different permissions to change different
HTCondor settings. For example, if a user is to be able to change
certain settings, but nothing else, those settings may be placed in a
file which was early in the :macro:`LOCAL_CONFIG_FILE` list, to give that
user write permission on that file. Then, include all the other files
after that one. In this way, if the user was attempting to change
settings that the user should not be permitted to change, the settings
would be overridden.

This mechanism is quite flexible and powerful. For very specific
configuration needs, they can probably be met by using file permissions,
the :macro:`LOCAL_CONFIG_FILE` configuration variable, and imagination.

