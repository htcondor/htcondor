DAGMan Completion
=================

.. sidebar:: Node Success/Failure

    **Table 2.1** Node **S**\ uccess or **F**\ ailure definition
    with :macro:`DAGMAN_ALWAYS_RUN_POST` = False (the default).

    +-----+-----------+-----------+-------+
    | PRE | JOB       | POST      | Node  |
    +=====+===========+===========+=======+
    | \-  | S         | \-        | **S** |
    +-----+-----------+-----------+-------+
    | \-  | F         | \-        | **F** |
    +-----+-----------+-----------+-------+
    | \-  | S         | S         | **S** |
    +-----+-----------+-----------+-------+
    | \-  | S         | F         | **F** |
    +-----+-----------+-----------+-------+
    | \-  | F         | S         | **S** |
    +-----+-----------+-----------+-------+
    | \-  | F         | F         | **F** |
    +-----+-----------+-----------+-------+
    | S   | S         | \-        | **S** |
    +-----+-----------+-----------+-------+
    | S   | F         | \-        | **F** |
    +-----+-----------+-----------+-------+
    | S   | S         | S         | **S** |
    +-----+-----------+-----------+-------+
    | S   | S         | F         | **F** |
    +-----+-----------+-----------+-------+
    | S   | F         | S         | **S** |
    +-----+-----------+-----------+-------+
    | S   | F         | F         | **F** |
    +-----+-----------+-----------+-------+
    | F   | not run   | \-        | **F** |
    +-----+-----------+-----------+-------+
    | F   | not run   | not run   | **F** |
    +-----+-----------+-----------+-------+

    **Table 2.2** Node **S**\ uccess or **F**\ ailure definition
    with :macro:`DAGMAN_ALWAYS_RUN_POST` = True

    +-----+-----------+--------+-------+
    | PRE | JOB       | POST   | Node  |
    +=====+===========+========+=======+
    | F   | not run   | \-     | **F** |
    +-----+-----------+--------+-------+
    | F   | not run   | S      | **S** |
    +-----+-----------+--------+-------+
    | F   | not run   | F      | **F** |
    +-----+-----------+--------+-------+

DAGMan exits the job queue when it has successfully completed, or when
it can no longer make forward progress. The latter case is considered
failure. Successful completion happens when every node in the DAG has
successfully completed. In the case of a DAGman failure, you can resubmit
the dag so that only the incomplete work is run. Alternatively, you can
re-run a DAG from pre-specified save points, and re-run previously completed nodes.

.. _DAG node success:

Node Success/Failure
--------------------

Progress towards completion of the DAG is based upon the success of the
nodes within the DAG. The success of a node is based upon the success of
the job(s), PRE script, and POST script. A job, PRE script, or POST
script with an exit value not equal to 0 is considered failed. **The
exit value of whatever component of the node was run last determines the
success or failure of the node.**

Table 2.1 lists the definition of node success and failure for all variations
of script and job success and failure, when :macro:`DAGMAN_ALWAYS_RUN_POST` is set
to ``False``. In this table, a dash (``-``) represents the case where a script
does not exist for the DAG, **S** represents success, and **F** represents
failure.

Table 2.2 lists the definition of node success and failure only for the cases
where the PRE script fails, when DAGMan is configured to always run POST scripts.

If Node jobs are multi-proc and one fails then the entire cluster is removed
and the node job is considered failed.

:index:`skipping node execution<single: DAGMan; Skipping node execution>`

.. _Node pre skip cmd:

PRE_SKIP
^^^^^^^^

The behavior of DAGMan with respect to node success or failure can be
changed with the addition of a *PRE_SKIP* command. A *PRE_SKIP* line
within the DAG input file uses the syntax:

.. code-block:: condor-dagman

    PRE_SKIP <NodeName | ALL_NODES> non-zero-exit-code

The PRE script of a node identified by *NodeName* that exits with the
value given by *non-zero-exit-code* skips the remainder of the node
entirely. Neither the job associated with the node nor the POST script
will be executed, and the node will be marked as successful.

:index:`retrying failed nodes<single: DAGMan; Retrying failed nodes>`

.. _Retry DAG Nodes:

Retrying Failed Nodes
^^^^^^^^^^^^^^^^^^^^^

DAGMan can retry any failed node in a DAG by specifying the node in the
DAG input file with the **RETRY** command. The syntax for retry is

.. sidebar:: Example Diamond DAG Using RETRY

    .. code-block:: condor-dagman

            # File name: diamond.dag

            JOB  A  A.condor
            JOB  B  B.condor
            JOB  C  C.condor
            JOB  D  D.condor
            PARENT A CHILD B C
            PARENT B C CHILD D
            RETRY  C 3

    If marked as failed, node C will retry execution until either
    success or the maximum number of retries (3) are attempted.

.. code-block:: condor-dagman

    RETRY <NodeName | ALL_NODES> NumberOfRetries [UNLESS-EXIT value]

where *NodeName* identifies the node. *NumberOfRetries* is an integer
number of times to retry the node after failure.

The implied number of retries for any node is 0, the same as not having a
retry line in the file. Retry causes the whole node to be rerun (i.e. PRE
Script, job, and POST Script).

Retry of a node may be short circuited using the optional keyword
*UNLESS-EXIT*, followed by an integer exit value. If the node exits with
the specified integer exit value, then no further processing will be
done on the node.

:index:`aborting a DAG<single: DAGMan; Aborting a DAG>`

.. _abort-dag-on:

Stopping the DAG on Node Failure
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The **ABORT-DAG-ON** command provides a way to abort the entire DAG if a
given node returns a specific exit code. The syntax for *ABORT-DAG-ON*
is

.. sidebar:: Example Diamond DAG Using ABORT-DAG-ON

    .. code-block:: condor-dagman

            # File name: diamond.dag

            JOB  A  A.condor
            JOB  B  B.condor
            JOB  C  C.condor
            JOB  D  D.condor
            PARENT A CHILD B C
            PARENT B C CHILD D
            RETRY  C 3
            ABORT-DAG-ON C 10 RETURN 1

    If node C exits with return value 10 then the DAG is aborted with
    an exit value of 1.

.. code-block:: condor-dagman

    ABORT-DAG-ON <NodeName | ALL_NODES> AbortExitValue [RETURN DAGReturnValue]

If the return value for the specified node matches *AbortExitValue*, the DAG
is immediately aborted. Meaning the DAG stops all currently running nodes,
cleans up, writes a rescue DAG, and exits with the optional specified return value.
If no DAG return value is specified then DAGMan exits with the node return
value that caused the abort.

A DAG return value other than 0, 1, or 2 will cause the :tool:`condor_dagman`
job to stay in the queue after it exits and get retried, unless the
:subcom:`on_exit_remove` expression in the ``*.condor.sub`` file is manually
modified.


The behavior differs based on the existence of PRE and/or POST scripts:

- If a PRE script returns the *AbortExitValue* value, the DAG is immediately aborted.
- If the HTCondor job within a node returns the *AbortExitValue* value, the DAG is
  aborted if the node has no POST script.
- If the POST script returns the *AbortExitValue* value, the DAG is aborted.

.. note::

    An abort overrides node retries. If a node returns the abort exit value,
    the DAG is aborted, even if the node has retry specified.

Resubmitting a Failed DAG
-------------------------

.. sidebar:: Check DAG Successful Exit

    To determine successful completion of a DAG that has left the
    queue, the final line in the ``*.dagman.out`` file should appear
    as similar to:

    .. code-block:: text

        (condor_DAGMAN) pid 445 EXITING WITH STATUS 0

DAGMan has two ways of restarting a failed DAG: Rescue and Recovery.
Rescue mode is most common for resubmitting a DAG manually while recovery
mode is most likely to occur automatically if a crash or something occurs.

If the DAG has failed, it can be be restarted such that work that needs
to be executed (including previously failed part) are ran. Resubmission should
be done via a Rescue DAG if the file exists, otherwise DAGMan will use
Recovery mode. To determine if Rescue mode is possible check the DAG
working directory for a Rescue DAG. A Rescue DAG is has a file name ending in
``.rescue<XXX>``, where ``<XXX>`` is replaced by a 3-digit number.

:index:`Rescue DAG<single: DAGMan; Rescue DAG>`

.. _Rescue DAG:

The Rescue DAG
^^^^^^^^^^^^^^

Any time a DAG exits unsuccessfully, DAGMan generates a Rescue DAG. The
Rescue DAG records the state of the DAG, with information such as which
nodes completed successfully, and the Rescue DAG will be used when the
DAG is again submitted. With the Rescue DAG, nodes that have already
successfully completed are not re-run. Nodes that are re-run will execute
every part of the node (PRE Script, job(s), and POST Script) even if
one part had previously completed successfully. There are a variety of
circumstances under which a Rescue DAG is generated:

.. sidebar:: Rescue DAG On Removal

    .. warning::

        On Windows no Rescue DAG is produced upon the removal of the DAGMan
        proper job, but re-submitting the original DAG will invoke recovery mode.

#. If a node in the DAG fails then DAGMan will continue executing until no more forward
   progress can be made. At this point, DAGMan produces the Rescue DAG and exits.
#. A Rescue DAG is produced when the :tool:`condor_dagman` job itself is removed via
   :tool:`condor_rm`. This only occurs on Unix platforms.
#. A Rescue DAG is produced when a node triggers an **ABORT-DAG-ON** with a non-zero
   value.

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

.. sidebar:: Example Rescue Diamond DAG

    If the ``diamond.dag`` was submitted and failed then the Rescue DAG
    ``diamond.dag.rescue001`` should be produced. Simply resubmit the
    DAG to re-run in rescue mode.

    .. code-block:: console

        $ condor_submit_dag diamond.dag
            //Failure occurs
        $ ls
            diamond.dag diamond.dag.rescue001 ...
        $ condor_submit_dag diamond.dag

    If the resubmitted DAG fails again then ``diamond.dag.rescue002``
    should be produced. This will then be used with the next resubmission.

The file name of the Rescue DAG is ``<DAG Input File>.rescue<XXX>``. Where ``<XXX>``
starts at ``001`` and increments with each new failure until the maximum value is hit.
The maximum value is defined by the configuration option :macro:`DAGMAN_MAX_RESCUE_NUM` .
If this limit is reached then the last Rescue DAG file is overwritten upon failure of
the DAG.

If multiple independent DAGs are submitted at one time via :tool:`condor_submit_dag`
then the Rescue DAG file will be named ``<Primary DAG>_multi.rescue<XXX>`` where
the primary DAG is the first DAG input file specified on the command line. This
multi-DAG rescue file will encompass all the nodes provided by the multiple
independent DAG files.

If a Rescue DAG exists when the original DAG is re-submitted, the Rescue
DAG with the largest magnitude value for ``<XXX>`` will be used, and its
usage is implied.

Using an Older Rescue DAG
'''''''''''''''''''''''''

If a DAG has failed multiple times and produced many Rescue DAG files, specific
Rescue DAGs can be specified to re-run the DAG from rather than the rescue with
the highest magnitude. This is achieved by using the *-DoRescueFrom* option for
:tool:`condor_submit_dag`.

.. code-block:: console

    $ condor_submit_dag -DoRescueFrom 2 diamond.dag

When an older rescue file is specified and the DAG fails, all existing rescue DAG
files of a higher magnitude will be renamed with the ``.old`` suffix. So,
``diamond.dag.rescue003`` will become ``diamond.dag.rescue003.old``.

Special Cases
'''''''''''''

#. If multiple DAG input files are provided on the :tool:`condor_submit_dag`
   command line, a single Rescue DAG encompassing all of the input DAG's is
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

DAG Recovery
^^^^^^^^^^^^

DAG recovery restores the state of a DAG upon resubmission by reading the
``*.nodes.log`` file that is used to enforce the dependencies of the DAG.
Once state is restore, DAGMan will continue the execution of the DAG.

Recovery is appropriate when no Rescue DAG has been created. The Rescue
DAG will fail to write if a crash occurs (Host machine, *condor_schedd*,
or :tool:`condor_dagman` job) or if the :tool:`condor_dagman` job is put
on hold.

Most of the time, when a not-completed DAG is re-submitted, it will
automatically be placed into recovery mode due to the existence and
contents of a lock file created as the DAG is first run. In recovery
mode, the ``*.nodes.log`` is used to identify nodes that have completed
and should not be re-submitted.

DAGMan can be told to work in recovery mode by including the
**-DoRecovery** option on the command line.

.. code-block:: console

    $ condor_submit_dag diamond.dag -DoRecovery

.. sidebar:: Example DAG Save Point Files

    Given the following DAG file, if ran from ``my_work`` directory
    then the following save files will be produced:

    .. code-block:: condor-dagman

        # File: savepointEx.dag
        JOB A node.sub
        JOB B node.sub
        JOB C node.sub
        JOB D node.sub

        PARENT A B C CHILD D

        #SAVE_POINT_FILE NodeName [Filename]
        SAVE_POINT_FILE A
        SAVE_POINT_FILE B Node-B_custom.save
        SAVE_POINT_FILE C ../example/Node-C_custom.save
        SAVE_POINT_FILE D ./Node-D_custom.save

    .. code-block:: text

        Directory Tree Visualized
        └─Home
            ├─example
            │   └─Node-C_custom.save
            └─my_work
                ├─savepointEx.dag
                ├─savepointEx.dag.condor.sub
                ├─savepointEx.dag.dagman.out
                ├─...
                ├─Node-D_custom.save
                └─save_files
                      ├─ A-savepointEx.dag.save
                      └─ Node-B_custom.save

:index:`DAG save point file<single: DAGMan; DAG save point file>`

.. _DAG Save Files:

DAG Save Point Files
--------------------

A successfully completed DAG can be re-run from a specific saved state if
the DAG originally run contained save point nodes. Save point nodes are
DAG nodes that have an associate **SAVE_POINT_FILE** command. The
**SAVE_POINT_FILE** syntax is as follows:

.. code-block:: condor-dagman

    SAVE_POINT_FILE NodeName [filename]

This file is written in the exact same format as the partial Rescue DAG
except all retries are reset. The save file is written as follows:

#. **When:**
    The save point file is written the first time a DAG node starts meaning
    it will not be written during a retry.

#. **Named:**
    If provided a filename then DAGMan will write the status to that provided
    file name otherwise the save file will be named ``[Node Name]-[DAG Input File].save``.
    Where the DAG input file is the DAG file that the save point was declared.

#. **Where:**
    If a path is provided in the save point filename then DAGMan will attempt to
    write to that location. If the path is relative then the file is written
    relative to the DAGs working directory. Otherwise, DAGMan will write
    the save file to a new directory call ``save_files`` which is created in
    the DAGs working directory.

.. note::

    The use of :tool:`condor_submit_dag`\s *-UseDagDir* option will effect
    where the ``save_files`` directory is created and where save files with
    relative paths are written since *-UseDagDir* changes alters the DAG
    working directory.

Once a DAG has ran and produced save point files, the DAG can be re-run from a
specific save point via the *-load_save* option for :tool:`condor_submit_dag`.
DAGMan will try attempt to read the save file from any path that is provided
otherwise DAGMan will assume the specified save file is located in the ``save_files``
directory. The paths for the specified save file is checked relative to the
DAGs working directory.

If a save file already exists at the time DAGMan goes to write it then DAGMan will
first rename the current file of the same name with the suffix ``.old``. This happens
whether the DAG is being re-run or if the same save filename is with multiple nodes
allowing for a progressing save file. For example, A progressing save point file can
be set up as the following:

.. code-block:: condor-dagman

    # File: progressSavefile.dag
    JOB A node.sub
    JOB B node.sub
    JOB C node.sub
    ...
    SAVE_POINT_FILE A dag-progress.save
    SAVE_POINT_FILE B dag-progress.save
    SAVE_POINT_FILE C dag-progress.save

.. mermaid::
    :align: center
    :caption: Progressing Save File DAG Actions

    flowchart LR
        subgraph A[Node A]
            w1[<font color="#08A04B">Write</font> dag-progress.save] --> r1((Run))
        end
        subgraph B[Node B]
            m1[<font color="blue">Rename</font> dag-progress.save<br>to dag-progress.save.old]
            w2[<font color="#08A04B">Write</font> dag-progress.save]
            m1 --> w2
            w2 --> r2((Run))
        end
        subgraph C[Node C]
            d[<font color="red">Remove</font> dag-progress.save.old]
            m2[<font color="blue">Rename</font> dag-progress.save<br>to dag-progress.save.old]
            w3[<font color="#08A04B">Write</font> dag-progress.save]
            d --> m2
            m2 --> w3
            w3 --> r3((Run))
        end
        A --> B
        B --> C
