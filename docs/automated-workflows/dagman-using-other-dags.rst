Composing workflows from multiple DAG files
===========================================

:index:`Composing workflows<single: DAG input file; Composing workflows>`
:index:`Composing workflows<single: DAGMan; Composing workflows>`

The organization and dependencies of the jobs within a DAG are the keys
to its utility. Some workflows are naturally constructed hierarchically,
such that a node within a DAG is also a DAG (instead of a "simple"
HTCondor job). HTCondor DAGMan handles this situation easily, and allows
DAGs to be nested to any depth.

There are two ways that DAGs can be nested within other DAGs:
  #. Sub-DAGs
  #. Splices.

With Sub-DAGs, each DAG has its own *condor_dagman* job, which then
becomes a node job within the higher-level DAG. With splices, on the
other hand, the nodes of the spliced DAG are directly incorporated into
the higher-level DAG. Therefore, splices do not result in additional
*condor_dagman* instances.

A weakness in scalability exists when submitting external Sub-DAGs,
because each executing independent DAG requires its own instance of
*condor_dagman* to be running. The outer DAG has an instance of
*condor_dagman*, and each named SUBDAG has an instance of
*condor_dagman* while it is in the HTCondor queue. The scaling issue
presents itself when a workflow contains hundreds or thousands of
Sub-DAGs that are queued at the same time. (In this case, the resources
(especially memory) consumed by the multiple *condor_dagman* instances
can be a problem.) Further, there may be many Rescue DAGs created if a
problem occurs. (Note that the scaling issue depends only on how many
Sub-DAGs are queued at any given time, not the total number of Sub-DAGs
in a given workflow; division of a large workflow into sequential
Sub-DAGs can actually enhance scalability.) To alleviate these concerns,
the DAGMan language introduces the concept of graph splicing.

Because splices are simpler in some ways than sub-DAGs, they are
generally preferred unless certain features are needed that are only
available with Sub-DAGs. This document:
`https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=SubDagsVsSplices <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=SubDagsVsSplices>`_
explains the pros and cons of splices and external sub-DAGs, and should
help users decide which alternative is better for their application.

Note that Sub-DAGs and splices can be combined in a single workflow, and
can be nested to any depth (but be sure to avoid recursion, which will
cause problems!).

.. _subdag-external:

:index:`SUBDAG command<single: DAG input file; SUBDAG command;>`
:index:`DAGs within DAGs<single: DAGMan; DAGs within DAGs>`

A DAG Within a DAG Is a SUBDAG
------------------------------

As stated above, the SUBDAG EXTERNAL command causes the specified DAG
file to be run by a separate instance of *condor_dagman*, with the
*condor_dagman* job becoming a node job within the higher-level DAG.

The syntax for the SUBDAG command is

.. code-block:: condor-dagman

    SUBDAG EXTERNAL JobName DagFileName [DIR directory] [NOOP] [DONE]

The optional specifications of **DIR**, **NOOP**, and **DONE**, if used,
must appear in this order within the entry. **NOOP** and **DONE** for
**SUBDAG** nodes have the same effect that they do for **JOB** nodes.

A **SUBDAG** node is essentially the same as any other node, except that
the DAG input file for the inner DAG is specified, instead of the
HTCondor submit file. The keyword **EXTERNAL** means that the SUBDAG is
run within its own instance of *condor_dagman*.

Since more than one DAG is being discussed, here is terminology
introduced to clarify which DAG is which. Reuse the example
diamond-shaped DAG as given in previous examples. Assume that node
B of this diamond-shaped DAG will itself be a DAG. The DAG of
node B is called a SUBDAG, inner DAG, or lower-level DAG. The
diamond-shaped DAG is called the outer or top-level DAG.

Work on the inner DAG first. Here is a very simple linear DAG input file
used as an example of the inner DAG.

.. code-block:: condor-dagman

    # File name: inner.dag

    JOB  X  X.sub
    JOB  Y  Y.sub
    JOB  Z  Z.sub
    PARENT X CHILD Y
    PARENT Y CHILD Z

The HTCondor submit description file, used by *condor_dagman*,
corresponding to ``inner.dag`` will be named ``inner.dag.condor.sub``.
The DAGMan submit description file is always named
``<DAG file name>.condor.sub``. Each DAG or SUBDAG results in the
submission of *condor_dagman* as an HTCondor job, and
*condor_submit_dag* creates this submit description file.

The preferred specification of the DAG input file for the outer DAG is

.. code-block:: condor-dagman

    # File name: diamond.dag

    JOB  A  A.submit
    SUBDAG EXTERNAL  B  inner.dag
    JOB  C  C.submit
    JOB  D  D.submit
    PARENT A CHILD B C
    PARENT B C CHILD D

Within the outer DAG's input file, the **SUBDAG** command specifies a
special case of a **JOB** node, where the job is itself a DAG.

One of the benefits of using the SUBDAG feature is that portions of the
overall workflow can be constructed and modified during the execution of
the DAG (a SUBDAG file doesn't have to exist until just before it is
submitted). A drawback can be that each SUBDAG causes its own distinct
job submission of *condor_dagman*, leading to a larger number of jobs,
together with their potential need of carefully constructed policy
configuration to throttle node submission or execution (because each
SUBDAG has its own throttles).

Here are details that affect SUBDAGs:

-  Nested DAG Submit Description File Generation

   There are three ways to generate the ``<DAG file name>.condor.sub``
   file of a SUBDAG:

   -  **Lazily** (the default in HTCondor version 7.5.2 and later
      versions)
   -  **Eagerly** (the default in HTCondor versions 7.4.1 through 7.5.1)
   -  **Manually** (the only way prior to version HTCondor version
      7.4.1)

   When the ``<DAG file name>.condor.sub`` file is generated **lazily**,
   this file is generated immediately before the SUBDAG job is
   submitted. Generation is accomplished by running

   .. code-block:: console

        $ condor_submit_dag -no_submit

   on the DAG input file specified in the **SUBDAG** entry. This is the
   default behavior. There are advantages to this lazy mode of submit
   description file creation for the SUBDAG:

   -  The DAG input file for a SUBDAG does not have to exist until the
      SUBDAG is ready to run, so this file can be dynamically created by
      earlier parts of the outer DAG or by the PRE script of the node
      containing the SUBDAG.
   -  It is now possible to have SUBDAGs within splices. That is not
      possible with eager submit description file creation, because
      *condor_submit_dag* does not understand splices.

   The main disadvantage of lazy submit file generation is that a syntax
   error in the DAG input file of a SUBDAG will not be discovered until
   the outer DAG tries to run the inner DAG.

   When ``<DAG file name>.condor.sub`` files are generated **eagerly**,
   *condor_submit_dag* runs itself recursively (with the *-no_submit*
   option) on each SUBDAG, so all of the ``<DAG file name>.condor.sub``
   files are generated before the top-level DAG is actually submitted.
   To generate the ``<DAG filename>.condor.sub`` files eagerly,
   pass the *-do_recurse* flag to *condor_submit_dag*; also set the
   :macro:`DAGMAN_GENERATE_SUBDAG_SUBMITS` configuration variable to
   ``False``, so that *condor_dagman* does not re-run
   *condor_submit_dag* at run time thereby regenerating the submit
   description files.

   To generate the ``.condor.sub`` files **manually**, run

   .. code-block:: console

       $ condor_submit_dag -no_submit

   on each lower-level DAG file, before running *condor_submit_dag* on
   the top-level DAG file; also set the :macro:`DAGMAN_GENERATE_SUBDAG_SUBMITS`
   configuration variable to ``False``, so that *condor_dagman* does not
   re-run *condor_submit_dag* at run time. The main reason for generating
   the ``<DAG file name>.condor.sub`` files manually is to set options for
   the lower-level DAG that one would not otherwise be able to set An
   example of this is the *-insert_sub_file* option. For instance,
   using the given example do the following to manually generate
   HTCondor submit description files:

   .. code-block:: console

         $ condor_submit_dag -no_submit -insert_sub_file fragment.sub inner.dag
         $ condor_submit_dag diamond.dag

   Note that most *condor_submit_dag* command-line flags have
   corresponding configuration variables, so we encourage the use of
   per-DAG configuration files, especially in the case of nested DAGs.
   This is the easiest way to set different options for different DAGs
   in an overall workflow.

   It is possible to combine more than one method of generating the
   ``<DAG file name>.condor.sub`` files. For example, one might pass the
   *-do_recurse* flag to *condor_submit_dag*, but leave the
   ``DAGMAN_GENERATE_SUBDAG_SUBMITS`` configuration variable set to the
   default of ``True``. Doing this would provide the benefit of an
   immediate error message at submit time, if there is a syntax error in
   one of the inner DAG input files, but the lower-level
   ``<DAG file name>.condor.sub`` files would still be regenerated
   before each nested DAG is submitted.

   The values of the following command-line flags are passed from the
   top-level *condor_submit_dag* instance to any lower-level
   *condor_submit_dag* instances. This occurs whether the lower-level
   submit description files are generated lazily or eagerly:

   -  **-verbose**
   -  **-force**
   -  **-notification**
   -  **-allowlogerror**
   -  **-dagman**
   -  **-usedagdir**
   -  **-outfile_dir**
   -  **-oldrescue**
   -  **-autorescue**
   -  **-dorescuefrom**
   -  **-allowversionmismatch**
   -  **-no_recurse/do_recurse**
   -  **-update_submit**
   -  **-import_env**
   -  **-include_env**
   -  **-insert_env**
   -  **-suppress_notification**
   -  **-priority**
   -  **-dont_use_default_node_log**

   The values of the following command-line flags are preserved in any
   already-existing lower-level DAG submit description files:

   -  **-maxjobs**
   -  **-maxidle**
   -  **-maxpre**
   -  **-maxpost**
   -  **-debug**

   Other command-line arguments are set to their defaults in any
   lower-level invocations of *condor_submit_dag*.

   The **-force** option will cause existing DAG submit description
   files to be overwritten without preserving any existing values.

-  Submission of the outer DAG

   The outer DAG is submitted as before, with the command

   .. code-block:: console

        $ condor_submit_dag diamond.dag

-  Interaction with Rescue DAGs

   The use of new-style Rescue DAGs is now the default. With new-style
   rescue DAGs, the appropriate rescue DAG(s) will be run automatically
   if there is a failure somewhere in the workflow. For example (given
   the DAGs in the example at the beginning of the SUBDAG section), if
   one of the nodes in ``inner.dag`` fails, this will produce a Rescue
   DAG for ``inner.dag`` (named ``inner.dag.rescue.001``). Then, since
   ``inner.dag`` failed, node B of ``diamond.dag`` will fail, producing
   a Rescue DAG for ``diamond.dag`` (named ``diamond.dag.rescue.001``,
   etc.). If the command

   .. code-block:: console

        $ condor_submit_dag diamond.dag

   is re-run, the most recent outer Rescue DAG will be run, and this
   will re-run the inner DAG, which will in turn run the most recent
   inner Rescue DAG.

-  File Paths

   Remember that, unless the DIR keyword is used in the outer DAG, the
   inner DAG utilizes the current working directory when the outer DAG
   is submitted. Therefore, all paths utilized by the inner DAG file
   must be specified accordingly.

:index:`SPLICE command<single: DAG input file; SPLICE command>`
:index:`splicing DAGs<single: DAGMan; Splicing DAGs>`

DAG Splicing
------------

As stated above, the SPLICE command causes the nodes of the spliced DAG
to be directly incorporated into the higher-level DAG (the DAG
containing the SPLICE command).

The syntax for the *SPLICE* command is

.. code-block:: condor-dagman

    SPLICE SpliceName DagFileName [DIR directory]

A splice is a named instance of a subgraph which is specified in a
separate DAG file. The splice is treated as an entity for dependency
specification in the including DAG. (Conceptually, a splice is treated
as a node within the DAG containing the SPLICE command, although there
are some limitations, which are discussed below. This means, for
example, that splices can have parents and children.) A splice can also
be incorporated into an including DAG without any dependencies; it is
then considered a disjoint DAG within the including DAG.

The same DAG file can be reused as differently named splices, each one
incorporating a copy of the dependency graph (and nodes therein) into
the including DAG.

The nodes within a splice are scoped according to a hierarchy of names
associated with the splices, as the splices are parsed from the top
level DAG file. The scoping character to describe the inclusion
hierarchy of nodes into the top level dag is '+'. (In other words, if a
splice named "SpliceX" contains a node named "NodeY", the full node name
once the DAGs are parsed is "SpliceX+NodeY". This character is chosen
due to a restriction in the allowable characters which may be in a file
name across the variety of platforms that HTCondor supports. In any DAG
input file, all splices must have unique names, but the same splice name
may be reused in different DAG input files.

HTCondor does not detect nor support splices that form a cycle within
the DAG. A DAGMan job that causes a cyclic inclusion of splices will
eventually exhaust available memory and crash.

The *SPLICE* command in a DAG input file creates a named instance of a
DAG as specified in another file as an entity which may have *PARENT*
and *CHILD* dependencies associated with other splice names or node
names in the including DAG file.

The following series of examples illustrate potential uses of splicing.
To simplify the examples, presume that each and every job uses the same,
simple HTCondor submit description file:

.. code-block:: condor-submit

      # BEGIN SUBMIT FILE simple-job.sub
      executable   = /bin/echo
      arguments    = OK
      universe     = vanilla
      output       = $(jobname).out
      error        = $(jobname).err
      log          = submit.log
      notification = NEVER

      request_cpus   = 1
      request_memory = 1024M
      request_disk   = 10240K

      queue
      # END SUBMIT FILE simple-job.sub

Simple SPLICE Example
'''''''''''''''''''''

This first simple example splices a diamond-shaped DAG in between the
two nodes of a top level DAG. Here is the DAG input file for the
diamond-shaped DAG:

.. code-block:: condor-dagman

      # BEGIN DAG FILE diamond.dag
      JOB A simple-job.sub
      VARS A jobname="$(JOB)"

      JOB B simple-job.sub
      VARS B jobname="$(JOB)"

      JOB C simple-job.sub
      VARS C jobname="$(JOB)"

      JOB D simple-job.sub
      VARS D jobname="$(JOB)"

      PARENT A CHILD B C
      PARENT B C CHILD D
      # END DAG FILE diamond.dag

The top level DAG incorporates the diamond-shaped splice:

.. code-block:: condor-dagman

      # BEGIN DAG FILE toplevel.dag
      JOB X simple-job.sub
      VARS X jobname="$(JOB)"

      JOB Y simple-job.sub
      VARS Y jobname="$(JOB)"

      # This is an instance of diamond.dag, given the symbolic name DIAMOND
      SPLICE DIAMOND diamond.dag

      # Set up a relationship between the nodes in this dag and the splice

      PARENT X CHILD DIAMOND
      PARENT DIAMOND CHILD Y

      # END DAG FILE toplevel.dag

The following example illustrates the resulting top level DAG
and the dependencies produced. Notice the naming of nodes scoped with
the splice name. This hierarchy of splice names assures unique names
associated with all nodes.

.. mermaid::
   :caption: The diamond-shaped DAG spliced between two nodes.
   :align: center

   flowchart TD 
     X --> Diamond+A  
     Diamond+A --> Diamond+B & Diamond+C
     Diamond+B & Diamond+C --> Diamond+D
     Diamond+D --> Y  

SPLICING one DAG Twice Example
''''''''''''''''''''''''''''''

The next example illustrates the starting point for a
more complex example. The DAG input file ``X.dag`` describes this
X-shaped DAG. The completed example displays more of the spatial
constructs provided by splices. Pay particular attention to the notion
that each named splice creates a new graph, even when the same DAG input
file is specified.

.. code-block:: condor-dagman

      # BEGIN DAG FILE X.dag

      JOB A simple-job.sub
      VARS A jobname="$(JOB)"

      JOB B simple-job.sub
      VARS B jobname="$(JOB)"

      JOB C simple-job.sub
      VARS C jobname="$(JOB)"

      JOB D simple-job.sub
      VARS D jobname="$(JOB)"

      JOB E simple-job.sub
      VARS E jobname="$(JOB)"

      JOB F simple-job.sub
      VARS F jobname="$(JOB)"

      JOB G simple-job.sub
      VARS G jobname="$(JOB)"

      # Make an X-shaped dependency graph
      PARENT A B C CHILD D
      PARENT D CHILD E F G

      # END DAG FILE X.dag

.. mermaid::
   :caption: The X-shaped DAG.
   :align: center

   flowchart TD 
     A & B & C  --> D
     D --> E & F & G

File ``s1.dag`` continues the example, presenting the DAG input file
that incorporates two separate splices of the X-shaped DAG.
The next description illustrates the resulting DAG.

.. code-block:: condor-dagman

      # BEGIN DAG FILE s1.dag

      JOB A simple-job.sub
      VARS A jobname="$(JOB)"

      JOB B simple-job.sub
      VARS B jobname="$(JOB)"

      # name two individual splices of the X-shaped DAG
      SPLICE X1 X.dag
      SPLICE X2 X.dag

      # Define dependencies
      # A must complete before the initial nodes in X1 can start
      PARENT A CHILD X1
      # All final nodes in X1 must finish before
      # the initial nodes in X2 can begin
      PARENT X1 CHILD X2
      # All final nodes in X2 must finish before B may begin.
      PARENT X2 CHILD B

      # END DAG FILE s1.dag

.. mermaid::
   :caption: The DAG described by s1.dag.
   :align: center

   flowchart TD
    A((A)) --> X1+A & X1+B & X1+C
    X1+A & X1+B & X1+C --> X1+D
    X1+D --> X1+E & X1+F & X1+G
    X1+E & X1+F & X1+G --> X2+A
    X1+E & X1+F & X1+G --> X2+B
    X1+E & X1+F & X1+G --> X2+C
    X2+A & X2+B & X2+C --> X2+D
    X2+D --> X2+E & X2+F & X2+G
    X2+E & X2+F & X2+G --> B

Disjointed SPLICE Example
'''''''''''''''''''''''''

The top level DAG in the hierarchy of this complex example is described
by the DAG input file ``toplevel.dag``, which illustrates the final DAG. 
Notice that the DAG has two disjoint graphs in it as a result of splice S3 not
having any dependencies associated with it in this top level DAG.

.. code-block:: condor-dagman

      # BEGIN DAG FILE toplevel.dag

      JOB A simple-job.sub
      VARS A jobname="$(JOB)"

      JOB B simple-job.sub
      VARS B jobname="$(JOB)"

      JOB C simple-job.sub
      VARS C jobname="$(JOB)"

      JOB D simple-job.sub
      VARS D jobname="$(JOB)"

      # a diamond-shaped DAG
      PARENT A CHILD B C
      PARENT B C CHILD D

      # This splice of the X-shaped DAG can only run after
      # the diamond dag finishes
      SPLICE S2 X.dag
      PARENT D CHILD S2

      # Since there are no dependencies for S3,
      # the following splice is disjoint
      SPLICE S3 s1.dag

      # END DAG FILE toplevel.dag

.. mermaid::
   :caption: The complex splice example DAG.
   :align: center

   flowchart TD
    A --> B & C
    B & C --> D
    D --> S2+A & S2+B & S2+C
    S2+A & S2+B & S2+C --> S2+D
    S2+D --> S2+E & S2+F & S2+G

    S3+A --> S3+X1+A & S3+X1+B & S3+X1+C
    S3+X1+A & S3+X1+B & S3+X1+C --> S3+X1+D
    S3+X1+D --> S3+X1+E & S3+X1+F & S3+X1+G
    S3+X1+E & S3+X1+F & S3+X1+G --> S3+X2+A & S3+X2+B & S3+X2+C
    S3+X2+A & S3+X2+B & S3+X2+C --> S3+X2+D
    S3+X2+D --> S3+X2+E & S3+X2+F & S3+X3+G
    S3+X2+E & S3+X2+F & S3+X3+G --> S3+B

Splice DIR option
'''''''''''''''''

The *DIR* option specifies a working directory for a splice, from which
the splice will be parsed and the jobs within the splice submitted. The
directory associated with the splice's *DIR* specification will be
propagated as a prefix to all nodes in the splice and any included
splices. If a node already has a *DIR* specification, then the splice's
*DIR* specification will be a prefix to the node's, separated by a
directory separator character. Jobs in included splices with an absolute
path for their *DIR* specification will have their *DIR* specification
untouched. Note that a DAG containing *DIR* specifications cannot be run
in conjunction with the *-usedagdir* command-line argument to
*condor_submit_dag*.

A "full" rescue DAG generated by a DAG run with the *-usedagdir*
argument will contain DIR specifications, so such a rescue DAG must be
run without the *-usedagdir* argument. (Note that "full" rescue DAGs are
no longer the default.)

Splice Limitations
''''''''''''''''''

**Limitation: splice DAGS do not produce rescue DAGs**

Because the nodes of a splice are directly incorporated into the DAG
containing the SPLICE command, splices do not generate their own rescue
DAGs, unlike SUBDAG EXTERNALs. However, all progress for nodes in the splice
DAG will be written in the parent DAGs rescue DAG file.

**Limitation: splice DAGs must exist at submit time**

Unlike the DAG files referenced in a SUBDAG EXTERNAL command, DAG files
referenced in a SPLICE command must exist when the DAG containing the
SPLICE command is submitted. (Note that, if a SPLICE is contained within
a sub-DAG, the splice DAG must exist at the time that the sub-DAG is
submitted, not when the top-most DAG is submitted, so the splice DAG can
be created by a part of the workflow that runs before the relevant
sub-DAG.)

**Limitation: Splices and PRE or POST Scripts**

A PRE or POST script may not be specified for a splice (however, nodes
within a spliced DAG can have PRE and POST scripts). The reason for
this is that, when the DAG is parsed, the splices are also parsed and
the splice nodes are directly incorporated into the DAG containing the
SPLICE command. Therefore, once parsing is complete, there are no actual
nodes corresponding to the splice itself to which to "attach" the PRE or
POST scripts.

To achieve the desired effect of having a PRE script associated with a
splice, introduce a new NOOP node into the DAG with the splice as a
dependency. Attach the PRE script to the NOOP node.

.. code-block:: condor-dagman

    # BEGIN DAG FILE example1.dag

    # Names a node with no associated node job, a NOOP node
    # Note that the file noop.submit does not need to exist
    JOB OnlyPreNode noop.sub NOOP

    # Attach a PRE script to the NOOP node
    SCRIPT PRE OnlyPreNode prescript.sh

    # Define the splice
    SPLICE TheSplice thenode.dag

    # Define the dependency
    PARENT OnlyPreNode CHILD TheSplice

    # END DAG FILE example1.dag

The same technique is used to achieve the effect of having a POST script
associated with a splice. Introduce a new NOOP node into the DAG as a
child of the splice, and attach the POST script to the NOOP node.

.. code-block:: condor-dagman

    # BEGIN DAG FILE example2.dag

    # Names a node with no associated node job, a NOOP node
    # Note that the file noop.submit does not need to exist.
    JOB OnlyPostNode noop.sub NOOP

    # Attach a POST script to the NOOP node
    SCRIPT POST OnlyPostNode postscript.sh

    # Define the splice
    SPLICE TheSplice thenode.dag

    # Define the dependency
    PARENT TheSplice CHILD OnlyPostNode

    # END DAG FILE example2.dag

**Limitation: Splices and the RETRY of a Node, use of VARS, or use of PRIORITY**

A RETRY, VARS or PRIORITY command cannot be specified for a SPLICE;
however, individual nodes within a spliced DAG can have a RETRY, VARS or
PRIORITY specified.

Here is an example showing a DAG that will not be parsed successfully:

.. code-block:: condor-dagman

      # top level DAG input file
      JOB    A a.sub
      SPLICE B b.dag
      PARENT A CHILD B

      # cannot work, as B is not a node in the DAG once
      # splice B is incorporated
      RETRY B 3
      VARS  B dataset="10"
      PRIORITY B 20

The following example will work:

.. code-block:: condor-dagman

      # top level DAG input file
      JOB    A a.sub
      SPLICE B b.dag
      PARENT A  CHILD B

      # file: b.dag
      JOB   X x.sub
      RETRY X 3
      VARS  X dataset="10"
      PRIORITY X 20

When RETRY is desired on an entire subgraph of a workflow, sub-DAGs (see
above) must be used instead of splices.

Here is the same example, now defining job B as a SUBDAG, and effecting
RETRY on that SUBDAG.

.. code-block:: condor-dagman

      # top level DAG input file
      JOB    A a.sub
      SUBDAG EXTERNAL B b.dag
      PARENT A  CHILD B

      RETRY B 3

**Limitation: The Interaction of Categories and MAXJOBS with Splices**

Categories normally refer only to nodes within a given splice. All of
the assignments of nodes to a category, and the setting of the category
throttle, should be done within a single DAG file. However, it is now
possible to have categories include nodes from within more than one
splice. To do this, the category name is prefixed with the ``+`` (plus)
character. This tells DAGMan that the category is a cross-splice
category. Towards deeper understanding, what this really does is prevent
renaming of the category when the splice is incorporated into the
upper-level DAG. The MAXJOBS specification for the category can appear
in either the upper-level DAG file or one of the splice DAG files. It
probably makes the most sense to put it in the upper-level DAG file.

Here is an example which applies a single limitation on submitted jobs,
identifying the category with ``+init``.

.. code-block:: condor-dagman

    # relevant portion of file name: upper.dag

    SPLICE A splice1.dag
    SPLICE B splice2.dag

    MAXJOBS +init 2

.. code-block:: condor-dagman

    # relevant portion of file name: splice1.dag

    JOB C C.sub
    CATEGORY C +init
    JOB D D.sub
    CATEGORY D +init

.. code-block:: condor-dagman

    # relevant portion of file name: splice2.dag

    JOB X X.sub
    CATEGORY X +init
    JOB Y Y.sub
    CATEGORY Y +init

For both global and non-global category throttles, settings at a higher
level in the DAG override settings at a lower level. In this example:

.. code-block:: condor-dagman

    # relevant portion of file name: upper.dag

    SPLICE A lower.dag

    MAXJOBS A+catX 10
    MAXJOBS +catY 2


    # relevant portion of file name: lower.dag

    MAXJOBS catX 5
    MAXJOBS +catY 1

the resulting throttle settings are 2 for the ``+catY`` category and 10
for the ``A+catX`` category in splice. Note that non-global category
names are prefixed with their splice name(s), so to refer to a
non-global category at a higher level, the splice name must be included.

