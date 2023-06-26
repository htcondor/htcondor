Machine-Readable Event History
==============================

:index:`JOBSTATE_LOG command<single: DAG input file; JOBSTATE_LOG command>`
:index:`jobstate.log file<single: DAGMan; jobstate.log file>`
:index:`machine-readable event history<single: DAGMan; Machine-readable event history>`

DAGMan can produce a machine-readable history of events. The
``jobstate.log`` file is designed for use by the Pegasus Workflow
Management System, which operates as a layer on top of DAGMan. Pegasus
uses the ``jobstate.log`` file to monitor the state of a workflow. The
``jobstate.log`` file can used by any automated tool for the monitoring
of workflows.

DAGMan produces this file when the command *JOBSTATE_LOG* is in the DAG
input file. The syntax for *JOBSTATE_LOG* is

**JOBSTATE_LOG** *JobstateLogFileName*

No more than one ``jobstate.log`` file can be created by a single
instance of *condor_dagman*. If more than one ``jobstate.log`` file is
specified, the first file name specified will take effect, and a warning
will be printed in the ``dagman.out`` file when subsequent
*JOBSTATE_LOG* specifications are parsed. Multiple specifications may
exist in the same DAG file, within splices, or within multiple,
independent DAGs run with a single *condor_dagman* instance.

The ``jobstate.log`` file can be considered a filtered version of the
``dagman.out`` file, in a machine-readable format. It contains the
actual node job events that from *condor_dagman*, plus some additional
meta-events.

The ``jobstate.log`` file is different from the node status file, in
that the ``jobstate.log`` file is appended to, rather than being
overwritten as the DAG runs. Therefore, it contains a history of the
DAG, rather than a snapshot of the current state of the DAG.

There are 5 line types in the ``jobstate.log`` file. Each line begins
with a Unix timestamp in the form of seconds since the Epoch. Fields
within each line are separated by a single space character.

*   **DAGMan start**: This line identifies the *condor_dagman* job. 
    The formatting of the line is

    .. code-block:: text

        timestamp INTERNAL \*** DAGMAN_STARTED dagmanCondorID \***

    The *dagmanCondorID* field is the *condor_dagman* job's
    ``ClusterId`` attribute, a period, and the ``ProcId`` attribute.

*   **DAGMan exit**: This line identifies the completion of the *condor_dagman*
    job. The formatting of the line is

    .. code-block:: text

        timestamp INTERNAL \*** DAGMAN_FINISHED exitCode \***

    The *exitCode* field is value the *condor_dagman* job returns upon
    exit.

*   **Recovery started**:  If the *condor_dagman* job goes into recovery mode,
    this meta-event is printed. During recovery mode, events will only be 
    printed in the file if they were not already printed before recovery mode
    started. The formatting of the line is

    .. code-block:: text

        timestamp INTERNAL \*** RECOVERY_STARTED \***

*   **Recovery finished or Recovery failure**: At the end of recovery mode, 
    either a RECOVERY_FINISHED or RECOVERY_FAILURE meta-event will be printed,
    as appropriate. The formatting of the line is

    .. code-block:: text

        timestamp INTERNAL \*** RECOVERY_FINISHED \***

    or

    .. code-block:: text

        timestamp INTERNAL \*** RECOVERY_FAILURE \***

*   **Normal**:  This line is used for all other event and meta-event types.
    The formatting of the line is

    .. code-block:: text

        timestamp JobName eventName condorID jobTag - sequenceNumber

    The *JobName* is the name given to the node job as defined in the
    DAG input file with the command *JOB*. It identifies the node within
    the DAG.

    The *eventName* is one of the many defined event or meta-events
    given in the lists below.

    The *condorID* field is the job's ``ClusterId`` attribute, a period,
    and the ``ProcId`` attribute. There is no *condorID* assigned yet
    for some meta-events, such as PRE_SCRIPT_STARTED. For these, the
    dash character ('-') is printed.

    The *jobTag* field is defined for the Pegasus workflow manager. Its
    usage is generalized to be useful to other workflow managers.
    Pegasus-managed jobs add a line of the following form to their
    HTCondor submit description file:

    .. code-block:: condor-submit

        +pegasus_site = "local"

    This defines the string ``local`` as the *jobTag* field.

    Generalized usage adds a set of 2 commands to the HTCondor submit
    description file to define a string as the *jobTag* field:

    .. code-block:: condor-submit

        +job_tag_name = "+job_tag_value"
        +job_tag_value = "viz"

    This defines the string ``viz`` as the *jobTag* field. Without any
    of these added lines within the HTCondor submit description file,
    the dash character ('-') is printed for the *jobTag* field.

    The *sequenceNumber* is a monotonically-increasing number that
    starts at one. It is associated with each attempt at running a node.
    If a node is retried, it gets a new sequence number; a submit
    failure does not result in a new sequence number. When a Rescue DAG
    is run, the sequence numbers pick up from where they left off within
    the previous attempt at running the DAG. Note that this only applies
    if the Rescue DAG is run automatically or with the *-dorescuefrom*
    command-line option.

Here is an example of a very simple Pegasus ``jobstate.log`` file,
assuming the example *jobTag* field of ``local``:

.. code-block:: text

    1292620511 INTERNAL *** DAGMAN_STARTED 4972.0 ***
    1292620523 NodeA PRE_SCRIPT_STARTED - local - 1
    1292620523 NodeA PRE_SCRIPT_SUCCESS - local - 1
    1292620525 NodeA SUBMIT 4973.0 local - 1
    1292620525 NodeA EXECUTE 4973.0 local - 1
    1292620526 NodeA JOB_TERMINATED 4973.0 local - 1
    1292620526 NodeA JOB_SUCCESS 0 local - 1
    1292620526 NodeA POST_SCRIPT_STARTED 4973.0 local - 1
    1292620531 NodeA POST_SCRIPT_TERMINATED 4973.0 local - 1
    1292620531 NodeA POST_SCRIPT_SUCCESS 4973.0 local - 1
    1292620535 INTERNAL *** DAGMAN_FINISHED 0 ***
