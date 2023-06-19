Custom Variables for Nodes
==========================

Jobs may be set up in a way that require a submit time ``key=value`` macros
of information to be used in the jobs submit description dictating various
behaviors for how the job can run. This allows a single submit description
file for a job to be versatile for many different job runs. However, for a
normal job submission (one not automated) the user must pass this extra
information at job submit time. To mimic this behavior of passing information
at job submit time within a DAGMan workflow, the *VARS* command can be utilized.

:index:`VARS command<single: DAG input file; VARS command>`
:index:`VARS (macro for submit description file)<single: DAGMan; VARS (macro for submit description file)>`

Variable Values Associated with Nodes
-------------------------------------

Macros defined for DAG nodes can be used within the submit description
file of the node job. The *VARS* command provides a method for defining
a macro. Macros are defined on a per-node basis, using the syntax

.. code-block:: condor-dagman

    VARS <JobName | ALL_NODES> [PREPEND | APPEND] macroname="string" [macroname2="string2" ... ]

The macro may be used within the submit description file of the relevant
node. A *macroname* may contain alphanumeric characters (a-z, A-Z, and
0-9) and the underscore character. The space character delimits macros,
such that there may be more than one macro defined on a single line.
Multiple lines defining macros for the same node are permitted.

Correct syntax requires that the *string* must be enclosed in double
quotes. To use a double quote mark within a *string*, escape the double
quote mark with the backslash character (\\). To add the backslash
character itself, use two backslashes (\\\\).

A restriction is that the *macroname* itself cannot begin with the
string ``queue``, in any combination of upper or lower case letters.

**Examples**

If the DAG input file contains

.. code-block:: condor-dagman

    # File name: diamond.dag

    JOB  A  A.submit
    JOB  B  B.submit
    JOB  C  C.submit
    JOB  D  D.submit
    VARS A state="Wisconsin"
    PARENT A CHILD B C
    PARENT B C CHILD D

then the submit description file ``A.submit`` may use the macro state.
Consider this submit description file ``A.submit``:

.. code-block:: condor-submit

    # file name: A.submit
    executable = A.exe
    log        = A.log
    arguments  = "$(state)"
    queue

The macro value expands to become a command-line argument in the
invocation of the job. The job is invoked with

.. code-block:: text

    A.exe Wisconsin

The use of macros may allow a reduction in the number of distinct submit
description files. A separate example shows this intended use of *VARS*.
In the case where the submit description file for each node varies only
in file naming, macros reduce the number of submit description files to
one.

This example references a single submit description file for each of the
nodes in the DAG input file, and it uses the *VARS* entry to name files
used by each job.

The relevant portion of the DAG input file appears as

.. code-block:: condor-dagman

    JOB A theonefile.sub
    JOB B theonefile.sub
    JOB C theonefile.sub

    VARS A filename="A"
    VARS B filename="B"
    VARS C filename="C"

The submit description file appears as

.. code-block:: condor-submit

    # submit description file called:  theonefile.sub
    executable   = progX
    output       = $(filename)
    error        = error.$(filename)
    log          = $(filename).log
    queue

For a DAG such as this one, but with thousands of nodes, the ability to
write and maintain a single submit description file together with a
single, yet more complex, DAG input file is worthwhile.

Prepend or Append Variables to Node
-----------------------------------

After *JobName* the word *PREPEND* or *APPEND* can be added to specify how
a variable is passed to a node at job submission time. *APPEND* will add
the variable after the submit description file is read. Resulting in the
passed variable being added as a macro or overwriting any already existing
variable values. *PREPEND* will add the variable before the submit
description file is read. This allows the variable to be used in submit
description file conditionals.

The relevant portion of the DAG input file appears as

.. code-block:: condor-dagman

     JOB A theotherfile.sub

     VARS A PREPEND var1="A"
     VARS A APPEND  var2="B"

The submit description file appears as

.. code-block:: condor-submit

     # submit description file called:   theotherfile.sub
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

For a DAG such as this one, ``Arguments`` will become "A was prepended" and the
output file will be named ``results-B.out``. If instead var1 used *APPEND*
and var2 used *PREPEND* then ``Arguments`` will become "No variables prepended"
and the output file will be named ``results-C.out``.

If neither *PREPEND* nor *APPEND* is used in the *VARS* line then the variable
will either be prepended or appended based on the configuration variable
:macro:`DAGMAN_DEFAULT_APPEND_VARS`.

Multiple macroname definitions
------------------------------

If a macro name for a specific node in a DAG is defined more than once,
as it would be with the partial file contents

.. code-block:: condor-dagman

    JOB job1 job1.submit
    VARS job1 a="foo"
    VARS job1 a="bar"

a warning is written to the log, of the format

.. code-block:: text

    Warning: VAR <macroname> is already defined in job <JobName>
    Discovered at file "<DAG input file name>", line <line number>

The behavior of DAGMan is such that all definitions for the macro exist,
but only the last one defined is used as the variable's value. Using
this example, if the ``job1.submit`` submit description file contains

.. code-block:: condor-submit

    arguments = "$(a)"

then the argument will be ``bar``.

:index:`VARS (use of special characters)<single: DAGMan; VARS (use of special characters)>`

Special characters within VARS string definitions
-------------------------------------------------

The value defined for a macro may contain spaces and tabs. It is also
possible to have double quote marks and backslashes within a value. In
order to have spaces or tabs within a value specified for a command line
argument, use the New Syntax format for the **arguments** submit
command, as described in :doc:`/man-pages/condor_submit`. Escapes for double
quote marks depend on whether the New Syntax or Old Syntax format is
used for the **arguments** submit command. Note that in both syntaxes,
double quote marks require two levels of escaping: one level is for the
parsing of the DAG input file, and the other level is for passing the
resulting value through *condor_submit*.

As of HTCondor version 8.3.7, single quotes are permitted within the
value specification. For the specification of command line
**arguments**, single quotes can be used in three ways:

-  in Old Syntax, within a macro's value specification
-  in New Syntax, within a macro's value specification
-  in New Syntax only, to delimit an argument containing white space

There are examples of all three cases below. In New Syntax, to pass a
single quote as part of an argument, escape it with another single quote
for *condor_submit* parsing as in the example's NodeA ``fourth`` macro.

As an example that shows uses of all special characters, here are only
the relevant parts of a DAG input file. Note that the NodeA value for
the macro ``second`` contains a tab.

.. code-block:: condor-dagman

    VARS NodeA first="Alberto Contador"
    VARS NodeA second="\"\"Andy Schleck\"\""
    VARS NodeA third="Lance\\ Armstrong"
    VARS NodeA fourth="Vincenzo ''The Shark'' Nibali"
    VARS NodeA misc="!@#$%^&*()_-=+=[]{}?/"

    VARS NodeB first="Lance_Armstrong"
    VARS NodeB second="\\\"Andreas_Kloden\\\""
    VARS NodeB third="Ivan_Basso"
    VARS NodeB fourth="Bernard_'The_Badger'_Hinault"
    VARS NodeB misc="!@#$%^&*()_-=+=[]{}?/"

    VARS NodeC args="'Nairo Quintana' 'Chris Froome'"

Consider an example in which the submit description file for NodeA uses
the New Syntax for the **arguments** command:

.. code-block:: condor-submit

    arguments = "'$(first)' '$(second)' '$(third)' '($fourth)' '$(misc)'"

The single quotes around each variable reference are only necessary if
the variable value may contain spaces or tabs. The resulting values
passed to the NodeA executable are:

.. code-block:: text

    Alberto Contador
    "Andy Schleck"
    Lance\ Armstrong
    Vincenzo 'The Shark' Nibali
    !@#$%^&*()_-=+=[]{}?/

Consider an example in which the submit description file for NodeB uses
the Old Syntax for the **arguments** command:

.. code-block:: condor-submit

      arguments = $(first) $(second) $(third) $(fourth) $(misc)

The resulting values passed to the NodeB executable are:

.. code-block:: text

      Lance_Armstrong
      "Andreas_Kloden"
      Ivan_Basso
      Bernard_'The_Badger'_Hinault
      !@#$%^&*()_-=+=[]{}?/

Consider an example in which the submit description file for NodeC uses
the New Syntax for the **arguments** command:

.. code-block:: condor-submit

      arguments = "$(args)"

The resulting values passed to the NodeC executable are:

.. code-block:: text

      Nairo Quintana
      Chris Froome

Using special macros within a definition
----------------------------------------

The $(JOB) and $(RETRY) macros may be used within a definition of the
*string* that defines a variable. This usage requires parentheses, such
that proper macro substitution may take place when the macro's value is
only a portion of the string.

-  $(JOB) expands to the node *JobName*. If the *VARS* line appears in a
   DAG file used as a splice file, then $(JOB) will be the fully scoped
   name of the node.

   For example, the DAG input file lines

   .. code-block:: condor-dagman

         JOB  NodeC NodeC.submit
         VARS NodeC nodename="$(JOB)"

   set ``nodename`` to ``NodeC``, and the DAG input file lines

   .. code-block:: condor-dagman

         JOB  NodeD NodeD.submit
         VARS NodeD outfilename="$(JOB)-output"

   set ``outfilename`` to ``NodeD-output``.

-  $(RETRY) expands to 0 the first time a node is run; the value is
   incremented each time the node is retried. For example:

   .. code-block:: condor-dagman

         VARS NodeE noderetry="$(RETRY)"

Using VARS to define ClassAd attributes
---------------------------------------

The *macroname* may also begin with a ``My.``, in which case it
names a ClassAd attribute. For example, the VARS specification

.. code-block:: condor-dagman

    VARS NodeF My.A="\"bob\""

results in the the ``NodeF`` job ClassAd attribute

.. code-block:: condor-classad

    A = "bob"

Continuing this example, it allows the HTCondor submit description file
for NodeF to use the following line:

.. code-block:: condor-submit

    arguments = "$$([My.A])"

Note that while the old behavior of using the ``+`` character to signify classad
attributes does work, it is not recommended over using ``My.``

.. code-block:: condor-dagman

    VARS NodeF +A="\"bob\""

will also result in

.. code-block:: condor-classad

    A = "bob"
