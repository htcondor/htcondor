      

*condor_ping*
==============

Attempt a security negotiation to determine if it succeeds
:index:`condor_ping<single: condor_ping; HTCondor commands>`\ :index:`condor_ping command`

Synopsis
--------

**condor_ping** [**-help | -version** ]

**condor_ping** [**-debug** ] [**-address** *<a.b.c.d:port>*]
[**-pool** *host name*] [**-name** *daemon name*]
[**-type** *subsystem*] [**-config** *filename*] [**-quiet |
-table | -verbose | -long | -format | -af** ] *token* [*token [...]* ]

Description
-----------

*condor_ping* attempts a security negotiation to discover whether the
configuration is set such that the negotiation succeeds. The target of
the negotiation is defined by one or a combination of the **address**,
**pool**, **name**, or **type** options. If no target is specified, the
default target is the *condor_schedd* daemon on the local machine.

One or more *token* s may be listed, thereby specifying one or more
authorization level to impersonate in security negotiation. A token is
the value ``ALL``, an authorization level, a command name, or the
integer value of a command. The many command names and their associated
integer values will more likely be used by experts, and they are defined
in the file ``condor_includes/condor_commands.h``.

An authorization level may be one of the following strings. If ``ALL``
is listed, then negotiation is attempted for each of these possible
authorization levels.
Note that OWNER is no longer used in HTCondor, but is kept here for use
when talking to older daemons (prior to 9.9.0).

 READ
 WRITE
 ADMINISTRATOR
 SOAP
 CONFIG
 OWNER
 DAEMON
 NEGOTIATOR
 ADVERTISE_MASTER
 ADVERTISE_STARTD
 ADVERTISE_SCHEDD
 CLIENT

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Print extra debugging information as the command executes.
 **-config** *filename*
    Attempt the negotiation based on the contents of the configuration
    file contents in file *filename*.
 **-address** *<a.b.c.d:port>*
    Target the given IP address with the negotiation attempt.
 **-pool** *hostname*
    Target the given *host* with the negotiation attempt. May be
    combined with specifications defined by **name** and **type**
    options.
 **-name** *daemonname*
    Target the daemon given by *daemonname* with the negotiation
    attempt.
 **-type** *subsystem*
    Target the daemon identified by *subsystem*, one of the values of
    the predefined ``$(SUBSYSTEM)`` macro.
 **-quiet**
    Set exit status only; no output displayed.
 **-table**
    Output is displayed with one result per line, in a table format.
 **-verbose**
    Display all available output.
 **-long**
    Display result classads.
 **-format** *fmt attr*
    (Custom option) Display attribute or expression *attr* in format
    *fmt*. To display the attribute or expression the format must
    contain a single ``printf(3)``-style conversion specifier.
    Attributes must be from the resource ClassAd. Expressions are
    ClassAd expressions and may refer to attributes in the resource
    ClassAd. If the attribute is not present in a given ClassAd and
    cannot be parsed as an expression, then the format option will be
    silently skipped. %r prints the unevaluated, or raw values. The
    conversion specifier must match the type of the attribute or
    expression. %s is suitable for strings such as ``Name``, %d for
    integers such as ``LastHeardFrom``, and %f for floating point
    numbers such as :ad-attr:`LoadAvg`. %v identifies the type of the
    attribute, and then prints the value in an appropriate format. %V
    identifies the type of the attribute, and then prints the value in
    an appropriate format as it would appear in the **-long** format. As
    an example, strings used with %V will have quote marks. An incorrect
    format will result in undefined behavior. Do not use more than one
    conversion specifier in a given format. More than one conversion
    specifier will result in undefined behavior. To output multiple
    attributes repeat the **-format** option once for each desired
    attribute. Like ``printf(3)``-style formats, one may include other
    text that will be reproduced directly. A format without any
    conversion specifiers may be specified, but an attribute is still
    required. Include a backslash followed by an 'n' to specify a line
    break.
 **-autoformat[:lhVr,tng]** *attr1 [attr2 ...]* or **-af[:lhVr,tng]** *attr1 [attr2 ...]*
    (Output option) Display attribute(s) or expression(s) formatted in a
    default way according to attribute types. This option takes an
    arbitrary number of attribute names as arguments, and prints out
    their values, with a space between each value and a newline
    character after the last value. It is like the **-format** option
    without format strings. This output option does not work in
    conjunction with the **-run** option.

    It is assumed that no attribute names begin with a dash character,
    so that the next word that begins with dash is the start of the next
    option. The **autoformat** option may be followed by a colon
    character and formatting qualifiers to deviate the output formatting
    from the default:

    **l** label each field,

    **h** print column headings before the first line of output,

    **V** use %V rather than %v for formatting (string values are
    quoted),

    **r** print "raw", or unevaluated values,

    **,** add a comma character after each field,

    **t** add a tab character before each field instead of the default
    space character,

    **n** add a newline character after each field,

    **g** add a newline character between ClassAds, and suppress spaces
    before each field.

    Use **-af:h** to get tabular values with headings.

    Use **-af:lrng** to get -long equivalent format.

    The newline and comma characters may not be used together. The
    **l** and **h** characters may not be used together.

Examples
--------

The example Unix command

.. code-block:: console

    $ condor_ping  -address "<127.0.0.1:9618>" -table READ WRITE DAEMON

places double quote marks around the sinful string to prevent the less
than and the greater than characters from causing redirect of input and
output. The given IP address is targeted with 3 attempts to negotiate:
one at the ``READ`` authorization level, one at the ``WRITE``
authorization level, and one at the ``DAEMON`` authorization level.

Exit Status
-----------

*condor_ping* will exit with the status value of the negotiation it
attempted, where 0 (zero) indicates success, and 1 (one) indicates
failure. If multiple security negotiations were attempted, the exit
status will be the logical OR of all values.

