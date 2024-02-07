Resubmitting a Failed DAG
=========================

DAGMan has two ways of restarting a failed DAG: Rescue and Recovery.
Rescue mode is most common for resubmitting a DAG manually while recovery
mode is most likely to occur automatically if a crash or something occurs.

If a DAG has left the queue and the ``.dagman.out`` file doesn't end
with a successful exit line similar to

.. code-block:: text

    (condor_DAGMAN) pid 445 EXITING WITH STATUS 0

Then the DAG has failed and needs to be restarted. Resubmission should
be done via a Rescue DAG if the file exists, otherwise DAGMan will use
Recover mode. To determine if Rescue mode is possible check the DAG
working directory for a Rescue DAG. A Rescue DAG is has a file name ending in
``.rescue<XXX>``, where ``<XXX>`` is replaced by a 3-digit number.

:index:`Rescue DAG<single: DAGMan; Rescue DAG>`

The Rescue DAG
--------------

Any time a DAG exits unsuccessfully, DAGMan generates a Rescue DAG. The
Rescue DAG records the state of the DAG, with information such as which
nodes completed successfully, and the Rescue DAG will be used when the
DAG is again submitted. With the Rescue DAG, nodes that have already
successfully completed are not re-run.

There are a variety of circumstances under which a Rescue DAG is
generated. If a node in the DAG fails, the DAG does not exit
immediately; the remainder of the DAG is continued until no more forward
progress can be made based on the DAG's dependencies. At this point,
DAGMan produces the Rescue DAG and exits. A Rescue DAG is produced on
Unix platforms if the :tool:`condor_dagman` job itself is removed with
:tool:`condor_rm`. On Windows, a Rescue DAG is not generated in this
situation, but re-submitting the original DAG will invoke a lower-level
recovery functionality, and it will produce similar behavior to using a
Rescue DAG. A Rescue DAG is produced when a node sets and triggers an
*ABORT-DAG-ON* event with a non-zero return value. A zero return value
constitutes successful DAG completion, and therefore a Rescue DAG is not
generated.

The granularity defining success or failure in the Rescue DAG is the
node. For a node that fails, all parts of the node will be re-run, even
if some parts were successful the first time. For example, if a node's
PRE script succeeds, but then the node's HTCondor job cluster fails, the
entire node, including the PRE script, will be re-run. A job cluster may
result in the submission of multiple HTCondor jobs. If one of the jobs
within the cluster fails, the node fails. Therefore, the Rescue DAG will
re-run the entire node, implying the submission of the entire cluster of
jobs, not just the one(s) that failed.

If the Rescue DAG file is generated before all retries of a node are
completed, then the Rescue DAG file will also contain *RETRY* entries.
The number of retries will be set to the appropriate remaining number of
retries. The configuration variable :macro:`DAGMAN_RESET_RETRIES_UPON_RESCUE`
controls whether or not node retries are reset in a Rescue DAG.

Statistics about the failed DAG execution are presented as comments at
the beginning of the Rescue DAG input file.

By default, if a Rescue DAG exists, it will be used when the DAG is
submitted specifying the original DAG input file. If more than one
Rescue DAG exists, the newest one will be used. By using the Rescue DAG,
DAGMan will avoid re-running nodes that completed successfully in the
previous run.

.. note::

    Passing the **-force** option to :tool:`condor_submit_dag` or
    :tool:`condor_dagman` will cause DAGMman to not use any existing
    Rescue DAG's. This means that previously-completed node jobs will
    be re-run.

Rescue DAG Naming
'''''''''''''''''

The file name of the Rescue DAG is obtained by appending the string
``.rescue<XXX>`` to the original DAG input file name. Values for ``<XXX>`` start
at ``001`` and continue to ``002``, ``003``, and beyond. The configuration variable
:macro:`DAGMAN_MAX_RESCUE_NUM` sets a maximum value for ``<XXX>``. If you hit the
:macro:`DAGMAN_MAX_RESCUE_NUM` limit, the last Rescue DAG file is overwritten
if the DAG fails again.

If a Rescue DAG exists when the original DAG is re-submitted, the Rescue
DAG with the largest magnitude value for <XXX> will be used, and its
usage is implied.

**Example**

Here is an example showing file naming and DAG submission for the case
of a failed DAG. The initial DAG is submitted with

.. code-block:: console

    $ condor_submit_dag diamond.dag

A failure of this DAG results in the Rescue DAG named
``diamond.dag.rescue001``. The DAG is resubmitted using the same command:

.. code-block:: console

    $ condor_submit_dag diamond.dag

This resubmission of the DAG uses the Rescue DAG file
``diamond.dag.rescue001``, because it exists. Failure of this Rescue DAG
results in another Rescue DAG called ``diamond.dag.rescue002``. If the DAG is
again submitted, using the same command as with the first two
submissions, but not repeated here, then this third submission uses the
Rescue DAG file ``diamond.dag.rescue002``, because it exists, and because the
value 002 is larger in magnitude than 001.

Using an Older Rescue DAG
'''''''''''''''''''''''''

To explicitly specify a particular Rescue DAG, use the optional
command-line argument *-dorescuefrom* with :tool:`condor_submit_dag`.
For example, assume that ``diamond.dag`` has failed 4 times, resulting in
the following Rescue DAGs ``diamond.dag.rescue001``, ``diamond.dag.rescue002``,
``diamond.dag.rescue003``, and ``diamond.dag.rescue004``. A decision is made to
re-run using ``diamond.dag.rescue002``.

.. code-block:: console

    $ condor_submit_dag -dorescuefrom 2 diamond.dag

The specified rescue DAG ``diamond.dag.rescue002`` is used in conjunction
with ``diamond.dag`` to restart the DAG. Any rescue DAG files with a larger
magnitude than the specified rescue number will be renamed with the
``.old`` suffix. Meaning ``diamond.dag.rescue003`` becomes ``diamond.dag.rescue.003.old``
and ``diamond.dag.rescue004`` becomes ``diamond.dag.rescue.004.old``.

Special Cases
'''''''''''''

#. If multiple DAG input files are provided on the :tool:`condor_submit_dag`
   command line, a single Rescue DAG encompasing all of the input DAG's is
   generated. The primary DAG (first DAG specified in the command line) will
   be used as the base of the Rescue DAG name.
#. A DAG file that contains DAG splices also only produces a single Rescue DAG
   file since the spliced DAG nodes are inherited by the top-level DAG.
#. A DAG that contains sub-DAG's will produce one Rescue DAG file per sub-DAG
   since each sub-DAG is it's own job running in the queue along with the
   top-level DAG. The Rescue DAG files will be created relative to the specified
   DAG input files.

Partial versus Full Rescue DAGs
'''''''''''''''''''''''''''''''

By default the Rescue DAG file is written as a partial DAG file that is
not intended to be used directly as a DAG input file. This partial file
only contains information about completed nodes and remaining retries for
non-completed nodes. Partial Rescue DAG files are parsed in combination of
the original DAG input file that contains the actual DAG structure. This
allows updates to the original DAG files structure to take effect when ran
in rescues mode.

.. note::

    If a partial Rescue DAG contains a *DONE* specification for a node that
    is removed from the original DAG input file will produce and error
    unless :macro:`DAGMAN_USE_STRICT` is set to zero in which case a warning
    will be produced. Commenting out the *DONE* line in the Rescue DAG file
    will avoid an error or warning.

If the default of writing a partial Rescue DAG is turned off by setting
:macro:`DAGMAN_WRITE_PARTIAL_RESCUE` to ``False``, then DAGMan will produce
a full Rescue DAG that contains the majority DAG information (i.e. DAG structure,
state, Scripts, VARS, etc.). In contrary to the partial Rescue DAG that is
parsed in combination with the original DAG input file, a full Rescue DAG is
to be submitted via the :tool:`condor_submit_dag` command line as the DAG
input. For example:

.. code-block:: console

    $ condor_submit_dag diamond.dag.rescue002

Attempting to re-submit the original DAG file, if the Rescue DAG file is
a complete DAG, will result in a parse failure.

.. warning::

    The full Rescue DAG functionality is deprecated and slated to be removed
    during the lifetime of the HTCondor V24 feature series.

Rescue for Parse Failure
''''''''''''''''''''''''

.. sidebar:: Example Parse Failure Rescue DAG

    .. code-block:: console

        $ condor_submit_dag -DumpRescue diamond.dag

    The following example would produce the file ``diamond.dag.parse_failed``
    if the ``diamond.dag`` failed to parse.

    .. note::

        The parse failure Rescue DAG cannot be used when resubmitting
        a failed DAG.

When using the **-DumpRescue** flag for :tool:`condor_submit_dag` or
:tool:`condor_dagman`, DAGMan will produce a special Rescue DAG file
if a the parsing of DAG input files fail. This special Rescue DAG file
will contain whatever DAGMan has successfully parsed up to the point of
failure. This may be helpful for debugging parse errors with complex DAG's.
Especially DAG's using splices.

To distinguish between a usable Rescue DAG file and a parse failure DAG file,
the parse failure Rescue DAG file has a different naming scheme. In which
the file is named ``<dag file>.parse_failed``. Further more, the parse failure
rescue DAG contains the **REJECT** command which prevents the parse failure
Rescue DAG from being executed by DAGMan. This is because the special Rescue
DAG is written in the full format regardless of :macro:`DAGMAN_WRITE_PARTIAL_RESCUE`.
Due to the nature of the full Recuse file being syntactically correct DAG
file, it will be perceived as a successfully executed workflow despite
being an incomplete DAG.

:index:`DAG recovery<single: DAGMan; DAG recovery>`
:index:`Difference between Rescue DAG and DAG recovery<single: DAGMan; Difference between Rescue DAG and DAG recovery>`

DAG Recovery
------------

DAG recovery restores the state of a DAG upon resubmission by reading the
``.nodes.log`` file that is used to enforce the dependencies of the DAG.
Once state is restore, DAGMan will continue the execution of the DAG.

Recovery is appropriate when no Rescue DAG has been created. The Rescue
DAG will fail to write if a crash occurs (Host machine, *condor_schedd*,
or :tool:`condor_dagman` job) or if the :tool:`condor_dagman` job is put
on hold.

Most of the time, when a not-completed DAG is re-submitted, it will
automatically be placed into recovery mode due to the existence and
contents of a lock file created as the DAG is first run. In recovery
mode, the ``.nodes.log`` is used to identify nodes that have completed
and should not be re-submitted.

DAGMan can be told to work in recovery mode by including the
**-DoRecovery** option on the command line.

.. code-block:: console

    $ condor_submit_dag diamond.dag -DoRecovery

