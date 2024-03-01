Advance DAGMan Functionality
============================

.. _DAGMan VARS:

Custom Job Macros for Nodes
---------------------------

HTCondor has the ability for submit description files to include custom macros
``$(macroname)`` that can be set at submit time by passing ``key=value`` pairs
of information to :tool:`condor_submit`. DAGMan can be what told ``key=value``
pairs to pass at node job submit time allowing a single submit description to
easily be used for multiple nodes in a DAG with variance.

:index:`VARS command<single: DAG Commands; VARS command>`
:index:`VARS (macro for submit description file)<single: DAGMan; VARS (macro for submit description file)>`

Macro Variables for Nodes
^^^^^^^^^^^^^^^^^^^^^^^^^

.. sidebar:: Example Diamond DAG with VARS

    .. code-block:: condor-dagman

        # File name: diamond.dag
        JOB  A  A.submit
        JOB  B  B.submit
        JOB  C  C.submit
        JOB  D  D.submit
        VARS A state="Wisconsin"
        PARENT A CHILD B C
        PARENT B C CHILD D

    .. code-block:: condor-submit

        # file name: A.submit
        executable = A.exe
        log        = A.log
        arguments  = "$(state)"
        queue

    The above DAG file and ``A.submit`` description will result in the
    job evoking the following:

    .. code-block:: console

        $ A.exe Wisconsin

DAGMan can specify ``key=value`` pairs of information to be used within
a node job submit description as a referable macro via the **VARS**
command. This information is defined on a per-node basis using the
following syntax:

.. code-block:: condor-dagman

    VARS <JobName | ALL_NODES> [PREPEND | APPEND] macroname="string" [macroname2="string2" ... ]

A *macroname* may contain alphanumeric characters (a-z, A-Z, and 0-9)
and the underscore character. A restriction is that the *macroname*
itself cannot begin with the string ``queue``, in any combination of
upper or lower case letters.

Correct syntax requires that the *value* string must be enclosed in
double quotes. To use a double quote mark within a *string*, escape
the double quote mark with the backslash character (``\"``). To add
the backslash character itself, use two backslashes (``\\``).

Multiple ``key=value`` pairs can be specified in a single **VARS**
line with a space in between each pair. Multiple individual **VARS**
lines can also be used for the same node.

The use of **VARS** to provide information for submit description macros
is very useful to reduce the number of submit files needed when multiple
nodes have job submit descriptions with simple variance. The following
example shows this behavior for a DAG with jobs that only vary in filenames.

.. code-block:: condor-dagman

    # File: example.dag
    JOB A shared.sub
    JOB B shared.sub
    JOB C shared.sub

    VARS A filename="alpha"
    VARS B filename="beta"
    VARS C filename="charlie"

.. code-block:: condor-submit

    # Generic submit description: shared.sub
    executable   = progX
    output       = $(filename).out
    error        = $(filename).err
    log          = $(filename).log
    queue

For a DAG such as above, but with thousands of nodes, the ability to
write and maintain a single submit description file together with a
single DAG input file is worthwhile.

Prepend or Append Variables to Node
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. sidebar:: Example Conditional Submit Description

    .. code-block:: condor-submit

         # Submit Description: conditional.sub
         executable   = progX

         if defined var1
              # This will occur due to PREPEND
              Arguments = "$(var1) was prepended"
         else
              # This will occur due to APPEND
              Arguments = "No variables prepended"
         endif

         var2 = "C"

         output       = results-$(var2).out
         error        = error.txt
         log          = job.log
         queue

The **VARS** command can take either the optional *PREPEND* or *APPEND*
keyword to specify how the following variable information is passed to
a node at job submission time.

- *APPEND* will add the variable after the submit description is read.
  Resulting in the passed variable being added as a macro overwriting
  any already existing variable values.
- *PREPEND* will add the variable before the submit description file is read.
  This allows the variable to be used in submit description conditionals.

For example, a DAG such as the following in conjunction with the submit
description on the right will result in the jobs :ad-attr:`Arguments` to
be ``A was prepended`` and the output file being named ``results-B.out``.

.. code-block:: condor-dagman

     JOB A conditional.sub

     VARS A PREPEND var1="A"
     VARS A APPEND  var2="B"

If instead var1 used *APPEND* and var2 used *PREPEND* then :ad-attr:`Arguments`
will become ``No variables prepended`` and the output file will be named
``results-C.out``.

.. note::

    If neither *PREPEND* nor *APPEND* is used in the *VARS* line then the variable
    will either be prepended or appended based on the configuration variable
    :macro:`DAGMAN_DEFAULT_APPEND_VARS`.

Multiple macroname definitions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If a node has defined the same *macroname* multiple times in a DAG
then a warning will be written to the log and the last defined instance
will be used for the variables value. Given the following example,
``custom_macro`` will be set to ``bar`` and output the following
warning message.

.. code-block:: condor-dagman

    # File: example.dag
    JOB ONLY sample.sub
    VARS ONLY custom_macro="foo"
    VARS ONLY custom_macro="bar"

.. code-block:: text

    Warning: VAR custom_macro is already defined in job ONLY
    Discovered at file "example.dag", line 4

:index:`VARS (use of special characters)<single: DAGMan; VARS (use of special characters)>`

Variables for Job Arguments
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The value provided for a variable is capable of containing whitespace
such as spaces and tabs, single and double quotes, and backslashes. To
use these special characters in the :subcom:`arguments[and DAGMan VARS]`
line for :tool:`condor_submit` use the appropriate syntax and/or character
escaping mechanisms.

.. note::

    Regardless of chosen :subcom:`arguments` syntax, the variable value
    is surrounded in double quotes. Meaning proper double quote escaping
    must be provided to utilize double quotes in a node jobs :subcom:`arguments`.

.. sidebar:: DAG Passing VARS in Both Argument Syntaxes

    .. code-block:: condor-dagman

        # New Syntax
        VARS NodeA first="Alberto Contador"
        VARS NodeA second="\"\"Andy Schleck\"\""
        VARS NodeA third="Lance\\ Armstrong"
        VARS NodeA fourth="Vincenzo ''The Shark'' Nibali"
        VARS NodeA misc="!@#$%^&*()_-=+=[]{}?/"

        # Old Syntax
        VARS NodeB first="Lance_Armstrong"
        VARS NodeB second="\\\"Andreas_Kloden\\\""
        VARS NodeB third="Ivan_Basso"
        VARS NodeB fourth="Bernard_'The_Badger'_Hinault"
        VARS NodeB misc="!@#$%^&*()_-=+=[]{}?/"

        # New Syntax with single quote delimiting
        VARS NodeC args="'Nairo Quintana' 'Chris Froome'"

    .. note::

        The macro ``second`` for *NodeA* contains a tab

Single quotes can be used in three ways for :subcom:`arguments`:

-  in Old Syntax, within a macro's value specification
-  in New Syntax, within a macro's value specification
-  in New Syntax only, to delimit an argument containing white space
-  in New Syntax only, escape a single quote with another to pass
   a single quote as part of an argument. Example provided in NodeA's
   ``fourth`` macro (see right).

Provided the example DAG input file on the right, the following would
occur:

#. *NodeA* using the New Syntax:
    The following :subcom:`arguments` line would produce the subsequent
    values passed to NodeA's executable. The single quotes around each
    variable reference are only necessary if the variable value may
    contain spaces or tabs.

    .. code-block:: condor-submit

        arguments = "'$(first)' '$(second)' '$(third)' '($fourth)' '$(misc)'"

    .. code-block:: text

        Alberto Contador
        "Andy Schleck"
        Lance\ Armstrong
        Vincenzo 'The Shark' Nibali
        !@#$%^&*()_-=+=[]{}?/

#. *NodeB* using the Old Syntax:
    The following :subcom:`arguments` line would produce the subsequent
    values passed to NodeB's executable.

    .. code-block:: condor-submit

          arguments = $(first) $(second) $(third) $(fourth) $(misc)

    .. code-block:: text

        Lance_Armstrong
        "Andreas_Kloden"
        Ivan_Basso
        Bernard_'The_Badger'_Hinault
        !@#$%^&*()_-=+=[]{}?/

#. *NodeC* using the New Syntax for single quote delimiting:
    The following :subcom:`arguments` line would produce the subsequent
    values passed to NodeC's executable.

    .. code-block:: condor-submit

        arguments = "$(args)"

    .. code-block:: text

        Nairo Quintana
        Chris Froome

Referencing Macros Within a Definition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. sidebar:: Special DAGMan Macros

    DAGMan passes the following special macros at node job submission time:

    #. **JOB**: Represents the fully scoped node name to which this job belongs.
    #. **RETRY**: The current node retry value. Value is 0 the first time
       the node is run and increments for each subsequent execution.
    #. **DAG_STATUS**: The current status of the DAG as represented by
       :ad-attr:`DAG_Status`. Intended for the FINAL node.
    #. **FAILED_COUNT**: The current number of failed nodes in the DAG.
       Intended for the FINAL node.

The variables value can contain an HTCondor Job Submit Language (JSL)
macro expansion ``$(<macroname>)`` allowing for the DAGMan provided
macros to utilize other existing macros like the following:

.. code-block:: condor-dagman

    # File: example.dag
    JOB A sample.sub
    VARS A test_case="$(JOB)-$(ClusterId)"

.. code-block:: condor-submit

    # File: sample.sub
    executable = progX
    arguments  = $(args)
    output     = $(test_case).out
    error      = $(test_case).err
    log        = $(test_case).log

    queue

Given the example listed above, if the job :ad-attr:`ClusterId` is 42 then the
output file would be ``A-42.out``, the error file would be ``A-42.err``, and
the log file would be ``A-42.log``.

Using VARS to Define ClassAd Attributes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. sidebar:: Old Plus Syntax

    The VARS macro name can utilize the old ``+`` syntax to define
    a Classad attribute, but it is recommended to use the ``My.``
    syntax.

    .. code-block:: condor-dagman

        VARS NodeA +name="\"Cole\""

The *macroname* may also begin with a ``My.``, in which case it
names a ClassAd attribute. For example, the VARS specification

.. code-block:: condor-dagman

    VARS NodeA My.name="\"Greg\""

results in the the ``NodeA`` job ClassAd attribute

.. code-block:: condor-classad

    A = "Greg"

Special Node Types
------------------

While most DAGMan nodes are the standard JOB type that run a job and possibly
a PRE or POST script, special nodes can be specified in the DAG submit description
to help manage the DAG and its resources in various ways.

:index:`FINAL command<single: DAG Commands; FINAL command>`
:index:`FINAL node<single: DAGMan; FINAL node>`

.. _final-node:

FINAL Node
^^^^^^^^^^

.. sidebar:: Exception for Running FINAL Node

    The only case in which the FINAL node is not run is when a cycle is detected
    in the DAG at startup time. This detection is only run when
    :macro:`DAGMAN_STARTUP_CYCLE_DETECT[and the FINAL Node]` is ``True``.

The FINAL node is a single and special node that is always run at the end
of the DAG, even if previous nodes in the DAG have failed or the DAG is
removed via :tool:`condor_rm[and DAG Final Node]` (On Unix systems). The
FINAL node can be used for tasks such as cleaning up intermediate files
and checking the output of previous nodes. To declare a FINAL node simply
use the following syntax for the **FINAL** command:

.. code-block:: condor-dagman

    FINAL JobName SubmitDescription [DIR directory] [NOOP]

Like the **JOB** command the **FINAL** command produces a node with
name *JobName* and an associated job submit description. The *DIR*
and *NOOP* keywords work exactly like they do detailed in the
:ref:`DAGMan JOB` command.

.. warning::

    There can only be one FINAL node in a DAG. If multiple are defined then
    DAGMan will log a parse error and fail.

.. sidebar:: FINAL Nope Restrictions

    The FINAL node can not be referenced with the following DAG commands:

    - **PARENT/CHILD**
    - **RETRY**
    - **ABORT-DAG-ON**
    - **PRIORITY**
    - **CATEGORY**

The success or failure of the FINAL node determines the success or
failure of the entire DAG. This includes any status specified by any
ABORT-DAG-ON specification that has taken effect. If some nodes of
a DAG fail, but the FINAL node succeeds, the DAG will be considered
successful. Therefore, it is important to be careful about setting
the exit status of the FINAL node.

The FINAL node can utilize the special macros ``DAG_STATUS`` and/or
``FAILED_COUNT`` in the job submit description or the script (PRE/POST)
arguments to help determine the correct exit behavior of the FINAL
node, and subsequently the DAG as a whole.

If DAGMan is removed via :tool:`condor_rm` then DAGMan will allow two
submit attempts of the FINAL nodes job (On Unix only).

:index:`PROVISIONER command<single: DAG Commands; PROVISIONER command>`
:index:`PROVISIONER node<single: DAGMan; PROVISIONER node>`

.. _DAG Provisioner Node:

PROVISIONER Node
^^^^^^^^^^^^^^^^

The PROVISIONER node is a single and special node that is always run at the
beginning of a DAG. It can be used to provision resources (i.e. Amazon EC2
instances, in-memory database servers) that can then be used by the remainder
of the nodes in the workflow. The syntax used for the **PROVISIONER** command is

.. code-block:: condor-dagman

    PROVISIONER JobName SubmitDescription

When the PROVISIONER node is defined in a DAG, DAGMan will run the PROVISIONER
node before all other nodes and wait for the PROVISIONER job to state it is ready.
To achieve this, the PROVISIONER job must set it's job ClassAd attribute
:ad-attr:`ProvisionerState` to the enumerated value ``ProvisionerState::PROVISIONING_COMPLETE``
(currently: 2). Once notified, DAGMan will begin running the other nodes.

The PROVISIONER runs for a set amount of time defined in its job. It does not
get terminated automatically at the end of a DAG workflow. The expectation
is that it needs to explicitly de-provision any resources, such as expensive
cloud computing instances that should not be allowed to run indefinitely.

.. warning::

    Currently only one PROVISIONER node may exist for a DAG. If multiple are
    defined in a DAG then an error will be logged and the DAG will fail.

:index:`SERVICE command<single: DAG Commands; SERVICE command>`
:index:`SERVICE node<single: DAGMan; SERVICE node>`

SERVICE Node
^^^^^^^^^^^^

A **SERVICE** node is a special type of node that is always run at the
beginning of a DAG. These are typically used to run tasks that need to run
alongside a DAGMan workflow (i.e. progress monitoring) without any direct
dependencies to the other nodes in the workflow.

The syntax used for the **SERVICE** command is

.. code-block:: condor-dagman

    SERVICE ServiceName SubmitDescription

If a DAGMan workflow finishes while there are SERVICE nodes still running,
it will remove all running SERVICE nodes and exit.

While the SERVICE node is started before other nodes in the DAG, there is
no guarantee that it will start running before any of the other nodes.
However, running it directly on the access point by setting :subcom:`universe`
to ``Local`` will make it more likely to begin running prior to other nodes.

.. note::

    A SERVICE node runs on a **best-effort basis**. If this node fails to submit
    correctly, this will not register as an error and the DAG workflow will
    continue normally.

:index:`PRIORITY command<single: DAG Commands; PRIORITY command>`
:index:`node priorities<single: DAGMan; Node priorities>`

.. _DAG Node Priorities:

Node Priorities
---------------

.. sidebar:: Example Diamond DAG with Node Priority

    The following example Node C's priority of 1 will result
    in Node C being submitted and most likely running before
    Node B. If no priority was set then Node B would be run
    first due to it be defined earlier in the DAG input file.

    .. code-block:: condor-dagman

        # File name: diamond.dag

        JOB  A  A.condor
        JOB  B  B.condor
        JOB  C  C.condor
        JOB  D  D.condor
        PARENT A CHILD B C
        PARENT B C CHILD D
        RETRY  C 3
        PRIORITY C 1

DAGMan workflows can assign a priority to a node in its DAG. Doing so will
determine which nodes, who's PARENT dependencies have completed, will be
submitted. Just like the :ref:`jobprio` for a job in the queue, the priority
value is an integer (which can be negative). Where a larger numerical
priority is better. The default priority is 0. To assign a nodes priority
follow the syntax for the **PRIORITY** command as follows:

.. code-block:: condor-dagman

    PRIORITY <JobName | ALL_NODES> PriorityValue

Node priorities are most relevant when :ref:`DAGMan throttling` is being
utilized or if there are not enough resources in the pool to run all
recently submitted node jobs.

Properties of Setting Node Priority
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- If a node priority is set, then at submission time DAGMan will set
  the :ad-attr:`JobPrio` via :subcom:`priority`. This is passed before
  processing the submit description.
- When a Sub-DAG has an associated node PRIORITY, the Sub-DAG priority will
  affect all priorities for nodes in the Sub-DAG. See :ref:`DAG Effective node prio`.
- Splices cannot be assigned priorities, but individual nodes within a
  splice can.
- DAGs containing PRE scripts may not submit the nodes in exact priority
  order, even if doing so would satisfy the DAG dependencies.

.. note::

    When using an external submit file for a node (not inline or submit-description),
    any declared :subcom:`priority` take precedence over the DAGMan value passed at
    job submission time.

.. note::

    Node priorities do not override DAG PARENT/CHILD dependencies and
    are not guarantees of the relative order in which node jobs are run.

.. _DAG Effective node prio:

Effective node priorities
^^^^^^^^^^^^^^^^^^^^^^^^^

When a Sub-DAG has an associated node priority, all of the node priorities
within the Sub-DAG get modified to become the effective node priority. The
effective node priority is calculated by adding the Sub-DAGs priority to
each internal nodes priority. The default Sub-DAG priority is 0.

.. code-block:: condor-dagman

    # File: priorities.dag
    JOB A sample.sub
    SUBDAG EXTERNAL B lower.dag

    PRIORITY A 25
    PRIORITY B 100

.. code-block:: condor-dagman

    # File: lower.dag
    JOB lowA sample.sub
    JOB lowB sample.sub

    PRIORITY lowA 10
    PRIORITY lowB 50

Provided the DAGs described on the above, the effective node
priorities (not including the Sub-DAG node B) are as follows:

+--------+----------------+
|  Node  | Effective Prio |
+========+================+
|   A    |       25       |
+--------+----------------+
|  lowA  |      110       |
+--------+----------------+
|  lowB  |      150       |
+--------+----------------+

.. sidebar:: Adding Accounting Information at DAG Submit

    The :subcom:`accounting_group` and :subcom:`accounting_group_user` values can be
    specified using the **-append** flag to :tool:`condor_submit_dag`, for example:

    .. code-block:: console

        $ condor_submit_dag \
          -append accounting_group=group_physics \
          -append accounting_group_user=albert \
          relativity.dag

:index:`accounting groups<single: DAGMan; Accounting groups>`

DAGMan and Accounting Groups
----------------------------

:tool:`condor_dagman` will propagate it's :subcom:`accounting_group[and DAGMan]`
and :subcom:`accounting_group_user[and DAGMan]` values down to all nodes within
the DAG (including Sub-DAGs). Any explicitly set accounting group information
within DAGMan node job submit descriptions will take precedence over the propagated
accounting information. This allows for easy setting of accounting information
for all DAG nodes while giving a way for specific nodes to run with different
accounting information.

For more information about HTCondor's accounting behavior see :ref:`Group Accounting`
and/or :ref:`Hierarchical Group Quotas`.

:index:`ALL_NODES Keyword<single: DAG Commands; ALL_NODES Keyword>`

ALL_NODES Option
----------------

.. sidebar:: *ALL_NODES* Limitations

    Due to how DAGMan parses DAG files and sets up, the *ALL_NODES* keyword
    will not be applied to nodes across splices, Sub-DAGs, and multiple DAGs
    submitted in a single :tool:`condor_submit_dag` instance. Each separate
    DAG (via splice or Sub-DAG) can utilize *ALL_NODES* individually.

Certain DAG input file commands take the alternative case insensitive keyword
*ALL_NODES* in place of a specific node name. This allows for common node
property to be applied to all nodes (excluding service and the FINAL node).
The following commands can utilize *ALL_NODES*:

+------------------+------------------+------------------+
| **SCRIPT**       | **PRE_SKIP**     | **RETRY**        |
+------------------+------------------+------------------+
| **VARS**         | **PRIORITY**     |                  |
+------------------+------------------+------------------+
| **CATEGORY**     | **ABORT-DAG-ON** |                  |
+------------------+------------------+------------------+

When multiple commands set a DAG nodes property, the last one defined takes
precedent overriding other earlier definitions. For example:

.. sidebar:: Multi-Command Definition Node Info

    Final node properties for nodes defined in DAG described
    to the left.

    +--------+-------------+-----------+------------------+
    |  Node  |  # Retries  |  $(name)  |  PRE Script Exe  |
    +========+=============+===========+==================+
    |   A    |     10      |     A     |  ``my_script A`` |
    +--------+-------------+-----------+------------------+
    |   B    |      3      |   nodeB   |  ``my_script B`` |
    +--------+-------------+-----------+------------------+
    |   C    |      3      |     C     |  ``my_script C`` |
    +--------+-------------+-----------+------------------+

.. code-block:: condor-dagman

    # File: sample.dag
    JOB A node.sub
    JOB B node.sub
    JOB C node.sub

    SCRIPT PRE ALL_NODES my_script $JOB

    VARS A name="alphaNode"

    VARS ALL_NODES name="$(JOB)"

    # This overrides the above VARS command for node B.
    VARS B name="nodeB"

    RETRY all_nodes 3

    RETRY A 10

:index:`INCLUDE command<single: DAG Commands; INCLUDE command>`

INCLUDE
-------

.. sidebar:: Example DAG INCLUDE

    Provided the two following DAGs, DAGMan will produce a single
    DAGMan process containing nodes A,B, and C.

    .. code-block:: condor-dagman

        # File: foo.dag
        JOB A A.sub
        INCLUDE bar.dag

    .. code-block:: condor-dagman

        # File: bar.dag
        JOB B B.sub
        JOB C C.sub

The **INCLUDE** command allows the contents of one DAG file to be parsed
inline as if they were physically included in the referencing DAG file. The
syntax for *INCLUDE* is

.. code-block:: condor-dagman

    INCLUDE FileName

The INCLUDE command allows for easier DAG management and ability to easily
change the DAG without losing the older setup. For example, a DAG could
describe all the nodes to be executed in the workflow and include a file
the describes the PARENT/CHILD relationships. If multiple different DAG
structure files were created then by simply changing the INCLUDE line can
modify the entire DAG structure without manually changing each line in
between executions.

All INCLUDE files must contain proper DAG syntax, and INCLUDEs can nested
to any depth (be careful of creating a cycle).

.. warning::

    INCLUDE does not modify node names like splicing which will result in
    a parse error if the same node name is used more than once.

DAG Manager Job Specifications
------------------------------

While most DAG commands modify/describe the DAG workflow and its various pieces,
some commands modify the DAGMan proper job itself.

:index:`SET_JOB_ATTR command<single: DAG Commands; SET_JOB_ATTR command>`
:index:`Setting ClassAd Attributes in the DAGMan Job<single: DAGMan; Setting ClassAd Attributes in the DAGMan Job>`

Setting Job Ad Attributes
^^^^^^^^^^^^^^^^^^^^^^^^^

.. sidebar:: Example Setting DAGMan Proper Job Ad Attribute

    The following will set the attribute ``TestNumber`` to 17
    in the DAGMan proper job's ClassAd.

    .. code-block:: condor-dagman

        SET_JOB_ATTR TestNumber = 17

The **SET_JOB_ATTR** command sets an attribute/value pair to be set
in the DAGMan proper job's ClassAd. The syntax is:

.. code-block:: condor-dagman

    SET_JOB_ATTR AttributeName = AttributeValue

The *SET_JOB_ATTR* attribute is not propagated down to node jobs of
the DAG.

The provided value can contain spaces when contained in single or
double quotes. These quote marks will become part of the value.

If the same attribute is specified multiple times then the last-specified
value is utilized. An attribute set in the DAG file can be overridden
at submit time as follows:

.. code-block:: console

    $ condor_submit_dag -append 'My.<attribute> = <value>'

:index:`ENV command<single: DAG Commands; ENV command>`
:index:`Setting DAGMan job environment variables<single: DAGMan; Setting DAGMan job environment variables>`

Controlling the Job Environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The **ENV** command is used to specify environment variables to set
into the DAGMan jobs environment or get from the environment that
the DAGMan job was submitted from. It is important to know that the
environment variables in the DAG manager jobs environment effect
scripts and node jobs that rely environment variables since scripts
and node jobs are submitted from the DAGMan jobs environment. The
syntax is:

.. code-block:: condor-dagman

    ENV GET VAR-1 [VAR-2 ... ]
    #  or
    ENV SET Key=Value;Key=Value; ...

- **GET** Keyword:
    Takes a list of environment variable names to be added to the DAGMan jobs
    :subcom:`getenv` command in the ``*.condor.sub`` file.
- **SET** Keyword:
    Takes a semi-colon delimited list of **key=value** pairs of information to
    explicitly add to the DAGMan jobs :subcom:`environment` command in the
    ``*.condor.sub`` file.

    .. note::

        The added **key=value** pairs must follow the normal HTCondor job
        environment rules.

:index:`CONFIG command<single: DAG Commands; CONFIG command>`
:index:`configuration specific to a DAG<single: DAGMan; Configuration specific to a DAG>`

.. _Per DAG Config:

DAG Specific Configuration
--------------------------

.. sidebar:: Example Custom DAGMan Configuration

    .. code-block:: condor-dagman

        # File: sample.dag
        CONFIG dagman.conf

    .. code-block:: condor-config

        # File: dagman.conf
        DAGMAN_MAX_JOBS_IDLE = 10

DAGMan allows for all :ref:`DAGMan Configuration` to be applied on a per DAG
basis. To apply custom configuration for a DAGMan workflow simply create a
custom configuration file to provide the the **CONFIG** command.

Only one configuration file is permitted per DAGMan process. If multiple DAGs
are submitted at one time or a workflow is comprised of Splices then a fatal
error will occur upon detection of more than one configuration file. Sub-DAGs
run as their own DAGMan process allowing Sub-DAGs to have there own configuration
files.

Custom configuration values are applied for the entire DAG workflow. So, if
multiple DAGs are submitted at one time then all of the DAGs will use the custom
configuration even though some DAGs didn't specify a custom config file.

.. note::
    Only configuration options that apply specifically to DAGMan or to DaemonCore
    (like debug log levels) take effect when added to a custom DAG configuration file.

Given there are many layers of configuration processing, and some :tool:`condor_submit_dag`
options that have the same effect as a DAGMan configuration options, the values
DAGMan uses is dictated by the following ordered list where elements processed
later take precedence:

#. HTCondor system configuration as set up by the AP administrator(s).
#. Configuration options passed as special HTCondor environment variables
   ``_CONDOR_<config option>=Value``.
#. Custom configuration provided by the **CONFIG** command or
   :tool:`condor_submit_dag[custom DAG Configuration]`\ s **-config** option.
#. :tool:`condor_submit_dag` options that control the same behavior as a
   configuration option such as :macro:`DAGMAN_MAX_JOBS_SUBMITTED` and **-maxjobs**.
