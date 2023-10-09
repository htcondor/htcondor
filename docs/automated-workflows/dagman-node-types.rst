Special Node Types
==================

While most DAGMan nodes are the standard JOB type that run a job and possibly
a PRE or POST script, special nodes can be specified in the DAG submit description
to help manage the DAG and its resources in various ways.

.. _final-node:

:index:`FINAL command<single: DAG input file; FINAL command>`
:index:`FINAL node<single: DAGMan; FINAL node>`

FINAL Node
----------

A FINAL node is a single and special node that is always run at the end
of the DAG, even if previous nodes in the DAG have failed. A FINAL node
can be used for tasks such as cleaning up intermediate files and
checking the output of previous nodes. The *FINAL* command in the DAG
input file specifies a node job to be run at the end of the DAG.

The syntax used for the *FINAL* command is

.. code-block:: condor-dagman

    FINAL JobName SubmitDescriptionFileName [DIR directory] [NOOP]

The FINAL node within the DAG is identified by *JobName*, and the
HTCondor job is described by the contents of the HTCondor submit
description file given by *SubmitDescriptionFileName*.

The keywords *DIR* and *NOOP* are as detailed in
:ref:`automated-workflows/dagman-introduction:Job` command documentation.
If both *DIR* and *NOOP* are used, they must appear in the order shown within
the syntax specification.

There may only be one FINAL node in a DAG. A parse error will be logged
by the *condor_dagman* job in the ``dagman.out`` file, if more than one
FINAL node is specified.

The FINAL node is virtually always run. It is run if the
*condor_dagman* job is removed with *condor_rm*. The only case in
which a FINAL node is not run is if the configuration variable
:macro:`DAGMAN_STARTUP_CYCLE_DETECT` is set to ``True``, and a
cycle is detected at start up time. If :macro:`DAGMAN_STARTUP_CYCLE_DETECT`
is set to ``False`` and a cycle is detected during the course of
the run, the FINAL node will be run.

The success or failure of the FINAL node determines the success or
failure of the entire DAG, overriding the status of all previous nodes.
This includes any status specified by any ABORT-DAG-ON specification
that has taken effect. If some nodes of a DAG fail, but the FINAL node
succeeds, the DAG will be considered successful. Therefore, it is
important to be careful about setting the exit status of the FINAL node.

The ``$DAG_STATUS`` and ``$FAILED_COUNT`` macros can be used both as PRE
and POST script arguments, and in node job submit description files. As
an example of this, here are the partial contents of the DAG input file,

.. code-block:: condor-dagman

        FINAL final_node final_node.sub
        SCRIPT PRE final_node final_pre.pl $DAG_STATUS $FAILED_COUNT

and here are the partial contents of the submit description file,
``final_node.sub``

.. code-block:: condor-submit

        arguments = "$(DAG_STATUS) $(FAILED_COUNT)"

If there is a FINAL node specified for a DAG, it will be run at the end
of the workflow. If this FINAL node must not do anything in certain
cases, use the ``$DAG_STATUS`` and ``$FAILED_COUNT`` macros to take
appropriate actions. Here is an example of that behavior. It uses a PRE
script that aborts if the DAG has been removed with *condor_rm*, which,
in turn, causes the FINAL node to be considered failed without actually
submitting the HTCondor job specified for the node. Partial contents of
the DAG input file:

.. code-block:: condor-dagman

    FINAL final_node final_node.sub
    SCRIPT PRE final_node final_pre.pl $DAG_STATUS

and partial contents of the Perl PRE script, ``final_pre.pl``:

.. code-block:: perl

    #!/usr/bin/env perl

    if ($ARGV[0] eq 4) {
        exit(1);
    }

There are restrictions on the use of a FINAL node. The DONE option is
not allowed for a FINAL node. And, a FINAL node may not be referenced in
any of the following specifications:

-  PARENT, CHILD
-  RETRY
-  ABORT-DAG-ON
-  PRIORITY
-  CATEGORY

As of HTCondor version 8.3.7, DAGMan allows at most two submit attempts
of a FINAL node, if the DAG has been removed from the queue with
*condor_rm*.

:index:`PROVISIONER command<single: DAG input file; PROVISIONER command>`
:index:`PROVISIONER node<single: DAGMan; PROVISIONER node>`

PROVISIONER Node
----------------

A PROVISIONER node is a single and special node that is always run at the
beginning of a DAG. It can be used to provision resources (ie. Amazon EC2
instances, in-memory database servers) that can then be used by the remainder
of the nodes in the workflow.

The syntax used for the *PROVISIONER* command is

.. code-block:: condor-dagman

    PROVISIONER JobName SubmitDescriptionFileName

When a PROVISIONER is defined in a DAG, it gets run at the beginning of the
DAG, and no other nodes are run until the PROVISIONER has advertised that it
is ready. It does this by setting the ``ProvisionerState`` attribute in its
job classad to the enumerated value ``ProvisionerState::PROVISIONING_COMPLETE``
(currently: 2). Once DAGMan sees that it is ready, it will start running
other nodes in the DAG as usual. At this point the PROVISIONER job continues
to run, typically sleeping and waiting while other nodes in the DAG use its
resources.

A PROVISIONER runs for a set amount of time defined in its job. It does not
get terminated automatically at the end of a DAG workflow. The expectation
is that it needs to explicitly deprovision any resources, such as expensive
cloud computing instances that should not be allowed to run indefinitely. 

:index:`SERVICE command<single: DAG input file; SERVICE command>`
:index:`SERVICE node<single: DAGMan; SERVICE node>`

SERVICE Node
------------

A **SERVICE** node is a special type of node that is always run at the
beginning of a DAG. These are typically used to run tasks that need to run
alongside a DAGMan workflow (ie. progress monitoring) without any direct
dependencies to the other nodes in the workflow.

The syntax used for the *SERVICE* command is

.. code-block:: condor-dagman

    SERVICE ServiceName SubmitDescriptionFileName

When a SERVICE is defined in a DAG, it gets started at the beginning of the
workflow. There is no guarantee that it will start running before any of the
other nodes, although running it directly from the access point using
``universe = local`` or ``universe = scheduler`` will almost always make this 
go first.

A SERVICE node runs on a **best-effort basis**. If this node fails to submit
correctly, this will not register as an error and the DAG workflow 
will continue normally.

If a DAGMan workflow finishes while there are SERVICE nodes still running,
it will shut these down and then exit the workflow successfully.

