Resubmitting a Failed DAG
=========================

When debugging a DAG in which something has gone wrong, a first
determination is whether a resubmission will use a Rescue DAG or benefit
from recovery. The existence of a Rescue DAG means that recovery would
be inappropriate. A Rescue DAG is has a file name ending in
``.rescue<XXX>``, where ``<XXX>`` is replaced by a 3-digit number.

Determine if a DAG ever completed (independent of whether it was
successful or not) by looking at the last lines of the ``.dagman.out``
file. If there is a line similar to

.. code-block:: text

    (condor_DAGMAN) pid 445 EXITING WITH STATUS 0

then the DAG completed. This line explains that the *condor_dagman* job
finished normally. If there is no line similar to this at the end of the
``.dagman.out`` file, and output from *condor_q* shows that the
*condor_dagman* job for the DAG being debugged is not in the queue,
then recovery is indicated.

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
Unix platforms if the *condor_dagman* job itself is removed with
*condor_rm*. On Windows, a Rescue DAG is not generated in this
situation, but re-submitting the original DAG will invoke a lower-level
recovery functionality, and it will produce similar behavior to using a
Rescue DAG. A Rescue DAG is produced when a node sets and triggers an
*ABORT-DAG-ON* event with a non-zero return value. A zero return value
constitutes successful DAG completion, and therefore a Rescue DAG is not
generated.

By default, if a Rescue DAG exists, it will be used when the DAG is
submitted specifying the original DAG input file. If more than one
Rescue DAG exists, the newest one will be used. By using the Rescue DAG,
DAGMan will avoid re-running nodes that completed successfully in the
previous run. **Note that passing the -force option to condor_submit_dag
or condor_dagman will cause condor_dagman to not use any existing rescue
DAG. This means that previously-completed node jobs will be re-run.**

The granularity defining success or failure in the Rescue DAG is the
node. For a node that fails, all parts of the node will be re-run, even
if some parts were successful the first time. For example, if a node's
PRE script succeeds, but then the node's HTCondor job cluster fails, the
entire node, including the PRE script, will be re-run. A job cluster may
result in the submission of multiple HTCondor jobs. If one of the jobs
within the cluster fails, the node fails. Therefore, the Rescue DAG will
re-run the entire node, implying the submission of the entire cluster of
jobs, not just the one(s) that failed.

Statistics about the failed DAG execution are presented as comments at
the beginning of the Rescue DAG input file.

Rescue DAG Naming
'''''''''''''''''

The file name of the Rescue DAG is obtained by appending the string
.rescue<XXX> to the original DAG input file name. Values for <XXX> start
at 001 and continue to 002, 003, and beyond. The configuration variable
:macro:`DAGMAN_MAX_RESCUE_NUM` sets a maximum value for <XXX>. If you hit the
:macro:`DAGMAN_MAX_RESCUE_NUM` limit, the last Rescue DAG file is overwritten
if the DAG fails again.

If a Rescue DAG exists when the original DAG is re-submitted, the Rescue
DAG with the largest magnitude value for <XXX> will be used, and its
usage is implied.

**Example**

Here is an example showing file naming and DAG submission for the case
of a failed DAG. The initial DAG is submitted with

.. code-block:: console

    $ condor_submit_dag my.dag

A failure of this DAG results in the Rescue DAG named
``my.dag.rescue001``. The DAG is resubmitted using the same command:

.. code-block:: console

    $ condor_submit_dag my.dag

This resubmission of the DAG uses the Rescue DAG file
``my.dag.rescue001``, because it exists. Failure of this Rescue DAG
results in another Rescue DAG called ``my.dag.rescue002``. If the DAG is
again submitted, using the same command as with the first two
submissions, but not repeated here, then this third submission uses the
Rescue DAG file ``my.dag.rescue002``, because it exists, and because the
value 002 is larger in magnitude than 001.

Using an Older Rescue DAG
'''''''''''''''''''''''''

To explicitly specify a particular Rescue DAG, use the optional
command-line argument *-dorescuefrom* with *condor_submit_dag*. Note
that this will have the side effect of renaming existing Rescue DAG
files with larger magnitude values of <XXX>. Each renamed file has its
existing name appended with the string ``.old``. For example, assume
that ``my.dag`` has failed 4 times, resulting in the Rescue DAGs named
``my.dag.rescue001``, ``my.dag.rescue002``, ``my.dag.rescue003``, and
``my.dag.rescue004``. A decision is made to re-run using
``my.dag.rescue002``. The submit command is

.. code-block:: console

    $ condor_submit_dag -dorescuefrom 2 my.dag

The DAG specified by the DAG input file ``my.dag.rescue002`` is
submitted. The existing Rescue DAG ``my.dag.rescue003`` is renamed
to be ``my.dag.rescue003.old``, while the existing Rescue DAG
``my.dag.rescue004`` is renamed to be ``my.dag.rescue004.old``.

Special Cases
'''''''''''''

Note that if multiple DAG input files are specified on the
*condor_submit_dag* command line, a single Rescue DAG encompassing all
of the input DAGs is generated. A DAG file containing splices also
produces a single Rescue DAG file. On the other hand, a DAG containing
sub-DAGs will produce a separate Rescue DAG for each sub-DAG that is
queued (and for the top-level DAG).

If the Rescue DAG file is generated before all retries of a node are
completed, then the Rescue DAG file will also contain *RETRY* entries.
The number of retries will be set to the appropriate remaining number of
retries. The configuration variable :macro:`DAGMAN_RESET_RETRIES_UPON_RESCUE`
controls whether or not node retries are reset in a Rescue DAG.

Partial versus Full Rescue DAGs
'''''''''''''''''''''''''''''''

As of HTCondor version 7.7.2, the Rescue DAG file is a partial DAG file,
not a complete DAG input file as in the past.

A partial Rescue DAG file contains only information about which nodes
are done and the number of retries remaining for nodes with retries. It
does not contain information such as the actual DAG structure and the
specification of the submit description file for each node job. Partial
Rescue DAGs are automatically parsed in combination with the original
DAG input file, which contains information about the DAG structure. This
updated implementation means that a change in the original DAG input
file, such as specifying a different submit description file for a node
job, will take effect when running the partial Rescue DAG. In other
words, you can fix mistakes in the original DAG file while still gaining
the benefit of using the Rescue DAG.

To use a partial Rescue DAG, you must re-run *condor_submit_dag* on
the original DAG file, not the Rescue DAG file.

Note that the existence of a DONE specification in a partial Rescue DAG
for a node that no longer exists in the original DAG input file is a
warning, as opposed to an error, unless the :macro:`DAGMAN_USE_STRICT`
configuration variable is set to a value of 1 or higher
(which is now the default). Comment out the line with *DONE* in the
partial Rescue DAG file to avoid a warning or error.

The previous (prior to version 7.7.2) behavior of producing full DAG
input file as the Rescue DAG is obtained by setting the configuration
variable :macro:`DAGMAN_WRITE_PARTIAL_RESCUE` to ``False``. **Note that
the option to generate full Rescue DAGs is likely to disappear some
time during the 8.3 series.**

To run a full Rescue DAG, either one left over from an older version of
DAGMan, or one produced by setting :macro:`DAGMAN_WRITE_PARTIAL_RESCUE`
to ``False``, directly specify the full Rescue DAG file on the command
line instead of the original DAG file. For example:

.. code-block:: console

    $ condor_submit_dag my.dag.rescue002

Attempting to re-submit the original DAG file, if the Rescue DAG file is
a complete DAG, will result in a parse failure.

Rescue for Parse Failure
''''''''''''''''''''''''

Starting in HTCondor version 7.5.5, passing the **-DumpRescue** option
to either *condor_dagman* or *condor_submit_dag* causes
*condor_dagman* to output a Rescue DAG file, even if the parsing of a
DAG input file fails. In this parse failure case, *condor_dagman*
produces a specially named Rescue DAG containing whatever it had
successfully parsed up until the point of the parse error. This Rescue
DAG may be useful in debugging parse errors in complex DAGs, especially
ones using splices. This incomplete Rescue DAG is not meant to be used
when resubmitting a failed DAG. Note that this incomplete Rescue DAG
generated by the **-DumpRescue** option is a full DAG input file, as
produced by versions of HTCondor prior to HTCondor version 7.7.2. It is
not a partial Rescue DAG file, regardless of the value of the
configuration variable :macro:`DAGMAN_WRITE_PARTIAL_RESCUE`.

To avoid confusion between this incomplete Rescue DAG generated in the
case of a parse failure and a usable Rescue DAG, a different name is
given to the incomplete Rescue DAG. The name appends the string
``.parse_failed`` to the original DAG input file name. Therefore, if the
submission of a DAG with

.. code-block:: console

    $ condor_submit_dag my.dag

has a parse failure, the resulting incomplete Rescue DAG will be named
``my.dag.parse_failed``.

To further prevent one of these incomplete Rescue DAG files from being
used, a line within the file contains the single command *REJECT*. This
causes *condor_dagman* to reject the DAG, if used as a DAG input file.
This is done because the incomplete Rescue DAG may be a syntactically
correct DAG input file. It will be incomplete relative to the original
DAG, such that if the incomplete Rescue DAG could be run, it could
erroneously be perceived as having successfully executed the desired
workflow, when, in fact, it did not.

:index:`DAG recovery<single: DAGMan; DAG recovery>`
:index:`Difference between Rescue DAG and DAG recovery<single: DAGMan; Difference between Rescue DAG and DAG recovery>`

DAG Recovery
------------

DAG recovery restores the state of a DAG upon resubmission. Recovery is
accomplished by reading the ``.nodes.log`` file that is used to enforce
the dependencies of the DAG. The DAG can then continue towards
completion.

Recovery is different than a Rescue DAG. Recovery is appropriate when no
Rescue DAG has been created. There will be no Rescue DAG if the machine
running the *condor_dagman* job crashes, or if the *condor_schedd*
daemon crashes, or if the *condor_dagman* job crashes, or if the
*condor_dagman* job is placed on hold.

Much of the time, when a not-completed DAG is re-submitted, it will
automatically be placed into recovery mode due to the existence and
contents of a lock file created as the DAG is first run. In recovery
mode, the ``.nodes.log`` is used to identify nodes that have completed
and should not be re-submitted.

DAGMan can be told to work in recovery mode by including the
**-DoRecovery** option on the command line, as in the example

.. code-block:: console

    $ condor_submit_dag diamond.dag -DoRecovery

where ``diamond.dag`` is the name of the DAG input file.
