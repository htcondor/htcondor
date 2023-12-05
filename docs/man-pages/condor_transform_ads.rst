*condor_transform_ads*
========================

Transform ClassAds according to specified rules, and output the
transformed ClassAds.
:index:`condor_transform_ads<single: condor_transform_ads; HTCondor commands>`\ :index:`condor_transform_ads command`

Synopsis
--------

**condor_transform_ads** [**-help [rules]** ]

**condor_transform_ads** [**-rules** *rules-file*]
[**-jobtransforms** *name-list*]
[**-jobroute** *route-name*]
[**-in[:<form>]  ** *infile*] [**-out[:<form>[,
nosort]]  ** *outfile*] [*<key>=<value>* ] [**-long** ] [**-json** ]
[**-xml** ] [**-verbose** ] [**-terse** ] [**-debug** ]
[**-unit-test** ] [**-testing** ] [**-convertoldroutes** ] [*infile1
...infileN* ]

Note that one or more transforms must be specified in the form of a rules
file or a ``JOB_TRANSFORM_`` or ``JOB_ROUTER_ROUTE_`` name and at least one input file must be
specified. Transforms will be applied in the order they are given on the command
line.  If a rules file has a TRANSFORM statement with arguments it must be the last
rules file.  If no output file is specified, output will be written to
``stdout``.

Description
-----------

*condor_transform_ads* reads ClassAds from a set of input files,
transforms them according to rules defined in a rules files or read from
configuration, and outputs the resulting transformed ClassAds.

See the :ref:`classads/transforms:Classad Transforms` section for a description of the transform language.

Options
-------

 **-help [rules | convert]**
    Display usage information and exit. **-help rules** displays
    information about the available transformation rules. **-help convert** displays
    information about the **-convertoldroutes** option.
 **-rules** *rules-file*
    Specifies the file containing definitions of the transformation
    rules, or configuration that declares a ``JOB_TRANSFORM_<name>`` or
    ``JOB_ROUTER_ROUTE_<name>`` variable for use in a subsequent ``-jobtransforms <name>``
    or ``-jobroute <name>`` argument.
 **-jobtransforms** *name-list*
    A comma-separated list of more transform names.  The transform rules will be read
    from a previous rules file or the configured ``JOB_TRANSFORM_<name>`` values
 **-jobroute** *name*
    A job route.  The transform rules will be read
    from a previous rules file or the configured ``JOB_ROUTER_ROUTE_<name>`` values
 **-in[:<form>]** *infile*
    Specifies an input file containing ClassAd(s) to be transformed.
    **<form>**, if specified, is one of:

    -  **long**: traditional long form (default)
    -  **xml**: XML form
    -  **json**: JSON ClassAd form
    -  **new**: "new" ClassAd form without newlines
    -  **auto**: guess format by reading the input

    | If ``-`` is specified for *infile*, input is read from ``stdin``.

 **-out[:<form>[, nosort]** *outfile*
    Specifies an output file to receive the transformed ClassAd(s).
    **<form>**, if specified, is one of:

    -  **long**: traditional long form (default)
    -  **xml**: XML form
    -  **json**: JSON ClassAd form
    -  **new**: "new" ClassAd form without newlines
    -  **auto**: use the same format as the first input

    | ClassAds are storted by attribute unless **nosort** is specified.

 [*<key>=<value>* ]
    Assign key/value pairs before rules file is parsed; can be used to
    pass arguments to rules. (More detail needed here.)
 **-long**
    Use long form for both input and output ClassAd(s). (This is the
    default.)
 **-json**
    Use JSON form for both input and output ClassAd(s).
 **-xml**
    Use XML form for both input and output ClassAd(s).
 **-verbose**
    Verbose mode, echo to stderr the transform names as they are applied
    and individual transform rules as they are executed.
 **-terse**
    Disable the **-verbose** option.
 **-debug**
    Causes debugging information to be sent to ``stderr``, based on the
    value of the configuration variable :macro:`TOOL_DEBUG`.

Exit Status
-----------

*condor_transform_ads* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Examples
--------

Here's a simple example that transforms the given input ClassAds
according to the given rules:

.. code-block:: text

      # File: my_input
      ResidentSetSize = 500
      DiskUsage = 2500000
      NumCkpts = 0
      TransferrErr = false
      Err = "/dev/null"

      # File: my_rules
      EVALSET MemoryUsage ( ResidentSetSize / 100 )
      EVALMACRO WantDisk = ( DiskUsage * 2 )
      SET RequestDisk ( $(WantDisk) / 1024 )
      RENAME NumCkpts NumCheckPoints
      DELETE /(.+)Err/

      # Command:
      condor_transform_ads -rules my_rules -in my_input

      # Output:
      DiskUsage = 2500000
      Err = "/dev/null"
      MemoryUsage = 5
      NumCheckPoints = 0
      RequestDisk = ( 5000000 / 1024 )
      ResidentSetSize = 500

