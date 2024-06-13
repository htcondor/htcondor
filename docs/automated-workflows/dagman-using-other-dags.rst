Composing Workflows from DAGs
=============================

:index:`Composing workflows<single: DAGMan; Composing workflows>`

HTCondor DAGMan can compose large DAG workflows from various smaller sub-workflows
defined in individual DAG files. This may be beneficial for the following reasons:

- Easily incorporate pre-existing DAGs
- Easily reuse a specific DAG sub-structure multiple times within a larger workflow
- Encapsulate parts of DAG workflow into easier managed sub-parts
- Dynamically create parts of the workflow (Sub-DAGs Only)
- Retry multiple nodes as a unit (Sub-DAGs Only)
- Short-circuit part of the workflow (Sub-DAGs Only)

There are two ways that DAGs can be nested within other DAGs:

#. **Sub-DAGs:**
    A Sub-DAG is a DAG that is submitted via :tool:`condor_submit_dag` and
    managed by a parent DAG as a single node. Each Sub-DAG has its own
    :tool:`condor_dagman` job submitted to the HTCondor queue that executes
    on the Access Point. As a result, scalability is a concern because a
    DAG comprised of hundreds to thousands of Sub-DAGs can overload the
    Access Point and its resources (i.e. memory, disk, cpu, etc.).
#. **Splices:**
    A Splice is a DAG that has all of its nodes directly incorporated to
    its parent DAG. Meaning all splices get merged into a single
    :tool:`condor_dagman` job reducing the stress and resource consumption
    on the Access Point.

.. note::

    Sub-DAGs and Splices can be combined in a single workflow, and to any depth,
    but be careful to avoid recursion which will cause problems.

.. sidebar:: Splice Dependencies

    When DAGMan incorporates a Splice into its workflow, any :dag-cmd:`PARENT/CHILD`
    relationships declared on a Splice is passed on to the Splices initial and
    terminal nodes (see definitions below).

    For example if the following DAGs Splice sub-workflow (``sub-workflow.dag``)
    has 1,000 initial nodes and 1,000 terminal nodes then 2,000 dependencies are
    created when a single :dag-cmd:`PARENT/CHILD` relationship is declared between
    two instances of this Splice. If :macro:`DAGMAN_USE_JOIN_NODES` = ``False``
    then 1 million dependencies would be created.

    .. code-block:: condor-dagman
        :caption: Example DAG description of spliced DAGs having declared dependency

        # Example DAG with Splices
        SPLICE A sub-workflow.dag
        SPLICE B sub-workflow.dag

        PARENT A CHILD B

    *Initial Node*: A node in a DAG with no Parent node dependencies

    *Terminal Node*: A node in a DAG with no Child node dependencies

It is recommended to use Splices if the workflow doesn't require special functionality
because splices don't produce the same scaling issue as Sub-DAGs. When determining how
to incorporate DAGs into larger workflows consider the following pros and cons list:

+----------------------------------------------------------+--------------+-------------+
|                        Feature                           |   Sub-DAGs   |   Splices   |
+==========================================================+==============+=============+
| Ability to incorporate separate sub-workflow files       |      Yes     |     Yes     |
+----------------------------------------------------------+--------------+-------------+
| Rescue DAG(s) created upon failure                       |      Yes     |     Yes     |
+----------------------------------------------------------+--------------+-------------+
| DAG Recovery (e.g. from AP host crash)                   |      Yes     |     Yes     |
+----------------------------------------------------------+--------------+-------------+
| Creates multiple :tool:`condor_dagman` instances         |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Possible explosion of dependencies (see right)           |      No      |     Yes     |
+----------------------------------------------------------+--------------+-------------+
| Sub-workflow DAG file must exist at submission           |      No      |     Yes     |
+----------------------------------------------------------+--------------+-------------+
| PRE/POST Script allowed for sub-workflow                 |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Ability to retry sub-workflow                            |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Job/Script throttling applies across entire workflow     |      No      |     Yes     |
+----------------------------------------------------------+--------------+-------------+
| Separate job/script throttling for sub-workflows         |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Node categories apply across entire workflow             |      No      |     Yes     |
+----------------------------------------------------------+--------------+-------------+
| Ability to set priority for sub-workflows                |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Ability to have unique final nodes for sub-workflows     |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Ability to abort sub-workflows individually              |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Ability to associate variables with sub-workflow nodes   |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Ability to configure sub-workflows individually          |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Separate DAGMan files for sub-workflow                   |      Yes     |     No      |
+----------------------------------------------------------+--------------+-------------+
| Halt file or :tool:`condor_hold` suspends entire workflow|      No      |     Yes     |
+----------------------------------------------------------+--------------+-------------+

Terminology
-----------

.. sidebar:: Terminology Example

    In the common case of DAG A incorporates DAG B, DAG A can be referred to as the
    top-level, high-level, outer, and/or parent DAG while DAG B is the low-level/inner
    DAG.

    .. note::

        Even with the provided terminology, once multiple DAGs are incorporated at
        various nested depths it can become difficult to keep track of which DAG is
        being referenced.

To help distinguish which DAG is being discussed in a workflow comprised of sub-workflows,
the following terminology is used:

#. **Top-level/Root DAG:**
    The highest level DAG that was manually submitted by the user.
#. **High-level/Outer DAG:**
    A DAG that is abstractly higher in the nest of DAGs. This refers to the DAG
    that includes other DAG sub-workflows.
#. **Low-level/Inner DAG:**
    A DAG that is abstractly lower in the nest of DAGs. This refers to the DAG
    that is incorporated into another DAG workflow.
#. **Parent DAG:**
    The specific DAG that incorporates/declared the current DAG the workflow.

:index:`DAGs within DAGs<single: DAGMan; DAGs within DAGs>`

.. _subdag-external:

A DAG Within a DAG Is a SUBDAG
------------------------------

To declare a Sub-DAG simply use the following syntax for the :dag-cmd:`SUBDAG[Usage]` command:

.. code-block:: condor-dagman

    SUBDAG EXTERNAL NodeName DagFileName [DIR directory] [NOOP] [DONE]

Since a Sub-DAG is run as a separate :tool:`condor_dagman` job, the parent DAG
views the entire sub-workflow as a single node in its workflow. For this reason,
the **DIR**, **NOOP**, and **DONE** keywords work exactly the same the regular
node :dag-cmd:`NODE` command. The main difference is instead of an HTCondor submit
description the Sub-DAG takes DAG description file.

.. note::

    The **EXTERNAL** keyword is required, and represents that the DAG is run
    externally as its own :tool:`condor_dagman` job. This is the only option
    for Sub-DAGs currently.

Example SUBDAG
^^^^^^^^^^^^^^

As stated earlier, DAGMan views a Sub-DAG as just another node. So, when the
Sub-DAG is ready to run, DAGMan submits the DAG via :tool:`condor_submit_dag`
and watches for the :tool:`condor_dagman` job to complete and exit the queue.

In the following example DAG files, the outer DAG is submitted by the user while
the inner DAG is submitted automatically once Node Y is ready to start.

.. code-block:: condor-dagman
    :caption: Example Line DAG description containing a sub-DAG

    # Outer DAG: line.dag
    NODE X job.sub
    SUBDAG EXTERNAL Y diamond.dag
    NODE Z job.sub

    PARENT X CHILD Y
    PARENT Y CHILD Z

.. code-block:: condor-dagman
    :caption: Example Diamond DAG description utilized as a sub-DAG

    # Inner DAG: diamond.dag
    NODE A job.sub
    NODE B job.sub
    NODE C job.sub
    NODE D job.sub

    PARENT A CHILD B C
    PARENT B C CHILD D

.. code-block:: console
    :caption: Submission of a DAG containing a sub-DAG

    $ condor_submit_dag line.dag

SUBDAG Submit Description Generation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Since a Sub-DAG is another :tool:`condor_dagman` job, a submit description file
needs to be generated. By default this will get generated at Sub-DAG submission
time when DAGMan executes :tool:`condor_submit_dag`. This has the added benefit
in the fact that the DAG description file can be created/modified dynamically during
the life of a higher-level DAGs lifetime; although the Sub-DAG description file
needs to be defined at the submission time of the top-level DAG, the inner DAG
description file only needs to exist just before node submission time.

.. note::

    Sub-DAG submit files can be pre-generated before workflow submission via
    :tool:`condor_submit_dag`\ s options **-no_submit** and **-do_recurse**.

.. sidebar:: Special Case Option Preservation

    If a Sub-DAG submit file is pre-generated then the following
    :tool:`condor_submit_dag` and **-update_submit** is set then
    the following are preserved for the specific DAG:

    +--------------+--------------+
    | **-MaxJobs** | **-MaxIdle** |
    +--------------+--------------+
    | **-MaxPre**  | **-MaxPost** |
    +--------------+--------------+

    .. note::

        If **-Force** is specified then the above listed options are not preserved.

Preserved DAGMan Options
^^^^^^^^^^^^^^^^^^^^^^^^

The following options for :tool:`condor_submit_dag[deep DAG options]` specified
at submission time of the top-level DAG are preserved and passed down to all
Sub-DAGs in the workflow:

+---------------------------------+---------------------------------+---------------------------------+
| **-Force**                      | **-UseDagDir**                  | **-BatchName**                  |
+---------------------------------+---------------------------------+---------------------------------+
| **-AutoRescue**                 | **-DoRescueFrom**               | **-Verbose**                    |
+---------------------------------+---------------------------------+---------------------------------+
| **-import_env**                 | **-include_env**                | **-insert_env**                 |
+---------------------------------+---------------------------------+---------------------------------+
| **-Notification**               | **-suppress_notification**      | **dont_suppress_notification**  |
+---------------------------------+---------------------------------+---------------------------------+
| **-outfile_dir**                | **-update_submit**              | **-AllowVersionMismatch**       |
+---------------------------------+---------------------------------+---------------------------------+
| **-DAGMan**                     | **-do_recurse**                 | **-no_recurse**                 |
+---------------------------------+---------------------------------+---------------------------------+
| **-SubmitMethod**               |                                 |                                 |
+---------------------------------+---------------------------------+---------------------------------+

SUBDAGs and Rescue
^^^^^^^^^^^^^^^^^^

Each Sub-DAG in the workflow will produce its own rescue DAG file upon failure.
Once the Sub-DAG has failed, written a rescue DAG, and exited, the failure will
cascade upwards to the top-level DAG. The final result is each DAG having a
unique rescue DAG file that will be automatically detected upon re-run.

SUBDAG Working Directory
^^^^^^^^^^^^^^^^^^^^^^^^

Unless the **DIR** keyword is specified when declaring a Sub-DAG, the low-level
DAG utilizes the current working directory of its parent DAG. Otherwise, the
specified directory is the Sub-DAGs working directory.

.. sidebar:: Nested Splice Node Naming

    Each level of splice is added to the hierarchal scope from highest
    to lowest level. Meaning node ``TOP+HIGH+MIDDLE+BOTTOM+NODE`` was
    spliced multiple times as such:

    .. mermaid::
        :align: center

        flowchart TD
            subgraph TOP
              subgraph HIGH
                subgraph MIDDLE
                  subgraph BOTTOM
                    NODE((NODE))
                  end
                end
              end
            end

:index:`splicing DAGs<single: DAGMan; Splicing DAGs>`

.. _DAG splicing:

DAG Splicing
------------

To Splice a DAG into the current DAG being described simply follow
the syntax for the :dag-cmd:`SPLICE[Usage]` command:

.. code-block:: condor-dagman

    SPLICE SpliceName DagFileName [DIR directory]

A splice is a named instance of a subgraph which is specified in a
separate DAG file. The splice is treated as an entity for dependency
specification in the including DAG. Although a splice can have dependencies,
it is not required. If no dependencies are specified then the splice
will become a disjointed graph.

The same DAG file can be reused as differently named splices, each one
incorporating a copy of the same DAG structure.

To prevent name collisions of nodes being spliced into a DAG, DAGMan
adds hierarchal scopes to the name of the node using the splice name.
This scope is delimited with ``+``. For example, if a DAG containing
``NodeY`` was spliced into another DAG as ``SpliceX`` then the resulting
node added to the top-level DAG will be named ``SpliceX+NodeY``.

.. warning::

    HTCondor does not detect nor support splices that form a cycle within
    the DAG. A DAGMan job that causes a cyclic inclusion of splices will
    eventually exhaust available memory and crash.

The following series of examples illustrate potential uses of splicing.
To simplify the examples, presume that each and every job uses the same,
simple HTCondor submit description file:

.. code-block:: condor-submit
    :caption: Example simple echo job submit description

    # File simple-job.sub
    executable   = /bin/echo
    arguments    = OK
    output       = $(NODE_NAME).out
    error        = $(NODE_NAME).err
    log          = submit.log
    notification = NEVER

    request_cpus   = 1
    request_memory = 1024M
    request_disk   = 10240K

    queue

Splice DIR Option
^^^^^^^^^^^^^^^^^

When the **DIR** keyword is specified for a splice, the splice will be
parsed from that directory and all nodes in the spliced DAG will be
submitted from. If the nodes in the spliced DAG specify their own working
directory as a relative path then DAGMan will use the splice directory as
a prefix to the node's directory. Absolute paths are untouched.

.. sidebar:: Diamond DAG spliced between two nodes

    .. mermaid::
       :align: center

       flowchart TD
         X --> Diamond+A
         Diamond+A --> Diamond+B & Diamond+C
         Diamond+B & Diamond+C --> Diamond+D
         Diamond+D --> Y

Simple SPLICE Example
^^^^^^^^^^^^^^^^^^^^^

This first simple example splices a diamond-shaped DAG in between the
two nodes of a top level DAG. Given the following DAG description files, a
single DAGMan workflow will be created as shown on the right.

.. code-block:: condor-dagman
    :caption: Example Diamond DAG description

    # Inner DAG: diamond.dag
    NODE A simple-job.sub
    NODE B simple-job.sub
    NODE C simple-job.sub
    NODE D simple-job.sub

    PARENT A CHILD B C
    PARENT B C CHILD D

.. code-block:: condor-dagman
    :caption: Example top level DAG description splicing in Diamond DAG

    # Outer DAG: topLevel.dag
    NODE X simple-job.sub
    NODE Y simple-job.sub

    # This is an instance of diamond.dag, given the symbolic name DIAMOND
    SPLICE DIAMOND diamond.dag

    # Set up a relationship between the nodes in this DAG and the splice
    PARENT X CHILD DIAMOND
    PARENT DIAMOND CHILD Y

.. sidebar:: X-shaped DAG

    .. mermaid::
       :align: center

       flowchart TD
         A & B & C  --> D
         D --> E & F & G

SPLICING one DAG Twice Example
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This next example illustrates the reuse of a DAG in multiple splices
for a single workflow. Below we have the X-shaped DAG description file
which can be visualized on the right.

.. code-block:: condor-dagman
    :caption: Example X shaped DAG description

    # Example: X.dag
    NODE A simple-job.sub
    NODE B simple-job.sub
    NODE C simple-job.sub
    NODE D simple-job.sub
    NODE E simple-job.sub
    NODE F simple-job.sub
    NODE G simple-job.sub

    # Make an X-shaped dependency graph
    PARENT A B C CHILD D
    PARENT D CHILD E F G

.. sidebar:: Splicing one DAG Multiple Times

    .. mermaid::
       :caption: The DAG described by s1.dag
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
        X2+E & X2+F & X2+G --> B((B))

Described below is a top-level DAG (``s1.dag``) that uses
the above described X-shaped DAG for two unique splice instances. The
full workflow is visualized on the right. Pay particular attention to the notion
that each named splice creates a new graph, even when the same DAG input
file is specified.

.. code-block:: condor-dagman
    :caption: Example top level DAG splicing in X shaped DAG twice

    # Top-level DAG: s1.dag
    NODE A simple-job.sub
    NODE B simple-job.sub

    # name two individual splices of the X-shaped DAG
    SPLICE X1 X.dag
    SPLICE X2 X.dag

    # Define dependencies
    # A must complete before the initial nodes in X1 can start
    PARENT A CHILD X1
    # All terminal nodes in X1 must finish before
    # the initial nodes in X2 can begin
    PARENT X1 CHILD X2
    # All terminal nodes in X2 must finish before B may begin.
    PARENT X2 CHILD B


Disjointed SPLICE Example
^^^^^^^^^^^^^^^^^^^^^^^^^

For this final example, the top level DAG in the hierarchy (``toplevel.dag``)
contains a self defined diamond structure that leads into a spliced X-shaped
DAG and a disjointed splice ``s1.dag`` as described in the previous example.
This ``S3`` splice is considered disjointed due to its lack of declared dependencies.

This shows how three simple DAG structures (Diamond, X-shaped, and line) can be
spliced together to create a more complex workflow. Notice how the hierarchal
scoped naming scheme is applied to the various nodes in the workflow especially
in the disjointed ``S3`` splice.

.. code-block:: condor-dagman
    :caption: Example top level DAG description splicing multiple disjointed DAGs

    # Outer DAG: toplevel.dag
    NODE A simple-job.sub
    NODE B simple-job.sub
    NODE C simple-job.sub
    NODE D simple-job.sub

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

.. mermaid::
   :caption: Disjointed Splice Example Visualized
   :align: center

   flowchart TD
    subgraph TOP[Top Level DAG]
     subgraph DG1[Diamond DAG to X-shaped splice]
      direction TB
      A --> B & C
      B & C --> D
      D --> S2+A & S2+B & S2+C
      S2+A & S2+B & S2+C --> S2+D
      S2+D --> S2+E & S2+F & S2+G
     end

     subgraph DG2[Disjoint s1.dag splice]
      direction TB
      S3+A --> S3+X1+A & S3+X1+B & S3+X1+C
      S3+X1+A & S3+X1+B & S3+X1+C --> S3+X1+D
      S3+X1+D --> S3+X1+E & S3+X1+F & S3+X1+G
      S3+X1+E & S3+X1+F & S3+X1+G --> S3+X2+A & S3+X2+B & S3+X2+C
      S3+X2+A & S3+X2+B & S3+X2+C --> S3+X2+D
      S3+X2+D --> S3+X2+E & S3+X2+F & S3+X3+G
      S3+X2+E & S3+X2+F & S3+X3+G --> S3+B
     end
    end

    style TOP fill:#FFF,stroke:#000
    style DG1 fill:#FFF,stroke:#000
    style DG2 fill:#FFF,stroke:#000

.. _DAG Splice Limitations:

Splice Limitations
^^^^^^^^^^^^^^^^^^

#. **Spliced DAGs do not produce Rescue DAGs**
    Because the nodes of a splice are directly incorporated into the DAG
    containing the :dag-cmd:`SPLICE` command, splices do not generate their own rescue
    DAGs, unlike :dag-cmd:`SUBDAG`\ s. However, all progress for nodes in the splice
    DAG will be written in the parent DAGs rescue DAG file.
#. **Spliced DAGs must exist at submit time**
    DAG files referenced as splices must exist at the submit time of its parent
    DAG since DAGMan needs to know the whole DAG structure at parse time.

    .. note::

        If the splice is part of a Sub-DAG it doesn't have to exist at submit
        time of the top-level DAG, but rather of the Sub-DAG that declares the
        splice.

#. **Splices and Scripts (PRE/POST)**
    Although splices are considered an entity in the parent DAG, they do not
    contain the ability to have PRE and POST scripts applied to the entire
    sub-workflow . This is because once all the splice nodes are parsed and
    and incorporated into the parent DAG, there is no one node that represents
    the entire sub-workflow like a Sub-DAG. Nodes within the spliced DAG can
    contain scripts.

    A work around to this problem is to add *NOOP* nodes with the desired
    PRE/POST scripts before and after the spliced DAG.

    .. code-block:: condor-dagman
        :caption: Example DAG description adding PRE & POST script to spliced DAG via NOOP nodes

        # Outer DAG: example.dag
        # Names a node with no associated node job, a NOOP node
        # Note that the file noop.sub does not need to exist
        NODE OnlyPreNode noop.sub NOOP
        NODE OnlyPostNode noop.sub NOOP

        # Attach Scripts to NOOP Nodes
        SCRIPT PRE OnlyPreNode prescript.sh
        SCRIPT POST OnlyPostNode postscript.sh

        # Define the splice
        SPLICE TheSplice thenode.dag

        # Define the dependency
        PARENT OnlyPreNode CHILD TheSplice
        PARENT TheSplice CHILD OnlyPostNode

#. **Splices and various DAG commands**
    For the same reason as why PRE and POST scripts can't be applied to an
    entire spliced sub-workflow (see above limitation), the following DAG
    commands can't be applied to a spliced DAG, but the nodes described in a
    splice can use all available commands.

    #. :dag-cmd:`RETRY`
    #. :dag-cmd:`VARS`
    #. :dag-cmd:`PRIORITY`
    #. :dag-cmd:`SAVE_POINT_FILE`

    The following commands in a spliced DAG do not take effect since they
    are processed at :tool:`condor_submit_dag` time.

    #. :dag-cmd:`SET_JOB_ATTR`
    #. :dag-cmd:`CONFIG`
    #. :dag-cmd:`ENV`

#. **Splice Interaction with Categories and MAXJOBS**
    While a :dag-cmd:`CATEGORY` can be set up to refer only to nodes internal to a
    splice, DAGMan has the ability for categories to include nodes from
    more than one splice. This is done by prefixing the category name
    with a ``+`` to make it a global category. The :dag-cmd:`MAXJOBS` declaration
    using a cross-splice category can be specified in either the parent
    DAG or the spliced DAG, but is recommended to be put in the parent DAG.

    Here is an example which applies a single limitation on submitted jobs,
    identifying the category with ``+init``.

    .. code-block:: condor-dagman
        :caption: Example DAG description with spliced DAGs and global categories

        # relevant portion of file name: upper.dag
        SPLICE A splice1.dag
        SPLICE B splice2.dag

        MAXJOBS +init 2

    .. code-block:: condor-dagman
        :caption: Example SPLICE A DAG description adding nodes to global category

        # relevant portion of file name: splice1.dag
        NODE C C.sub
        CATEGORY C +init
        NODE D D.sub
        CATEGORY D +init

    .. code-block:: condor-dagman
        :caption: Example SPLICE B DAG description adding nodes to global category

        # relevant portion of file name: splice2.dag
        NODE X X.sub
        CATEGORY X +init
        NODE Y Y.sub
        CATEGORY Y +init

    For both global and non-global category throttles, settings at a higher
    level in the DAG overrides settings at a lower level. For example, the
    following will result in the throttle settings of 2 for the ``+catY``
    category and 10 for the ``A+catX`` category in splice.

    .. code-block:: condor-dagman
        :caption: Example DAG descriptions setting multiple MAXJOBS throttles for global categories

        # relevant portion of file name: upper.dag
        SPLICE A lower.dag
        MAXJOBS A+catX 10
        MAXJOBS +catY 2

        # relevant portion of file name: lower.dag
        MAXJOBS catX 5
        MAXJOBS +catY 1

    .. note::

        Non-global category names are prefixed with their splice name(s), so
        to refer to a non-global category at a higher level, the splice name
        must be included.

