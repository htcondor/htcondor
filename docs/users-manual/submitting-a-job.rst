Submitting a Job
================

:index:`submitting<single: submitting; job>`

The *condor_submit* command takes a job description file as input
and submits the job to HTCondor.
:index:`submit description file`\ :index:`submit description<single: submit description; file>`
In the submit description file, HTCondor finds everything it needs to
know about the job. Items such as the name of the executable to run, the
initial working directory, and command-line arguments to the program all
go into the submit description file. *condor_submit* creates a job
ClassAd based upon the information, and HTCondor works toward running
the job. :index:`contents of<single: contents of; submit description file>`

It is easy to submit multiple runs of a program
to HTCondor with a single submit description file. To run the same
program many times with different input data sets, arrange the data files
accordingly so that each run reads its own input, and each run writes
its own output. Each individual run may have its own initial working
directory, files mapped for ``stdin``, ``stdout``, ``stderr``,
command-line arguments, and shell environment.

The :doc:`/man-pages/condor_submit` manual page contains a complete and full
description of how to use *condor_submit*. It also includes descriptions of
all of the many commands that may be placed into a submit description
file. In addition, the index lists entries for each command under the
heading of Submit Commands.

Sample submit description files
-------------------------------

In addition to the examples of submit description files given here,
there are more in the :doc:`/man-pages/condor_submit` manual page.
:index:`examples<single: examples; submit description file>`


**Example 1**

Example 1 is one of the simplest submit description files possible. It
queues the program *myexe* for execution somewhere in the pool.
As this submit description file does not request a specific operating
system to run on, HTCondor will use the default, which is to run the job
on a machine which has the same architecture and operating system 
it was submitted from.

Before submitting a job to HTCondor, it is a good idea to test it
first locally, by running it from a command shell.  This example job
might look like this when run from the shell prompt.

.. code-block:: console

      $ ./myexe SomeArgument

The corresponding submit description file might look like the following

.. code-block:: condor-submit

    # Example 1
    # Simple HTCondor submit description file
    # Everything with a leading # is a comment

    executable   = myexe
    arguments    = SomeArgument

    output       = outputfile
    error        = errorfile
    log          = myexe.log

    request_cpus   = 1
    request_memory = 1024
    request_disk   = 10240

    should_transfer_files = yes

    queue

The standard output for this job will go to the file
``outputfile``, as specified by the
**output** :index:`output<single: output; submit commands>` command. Likewise,
the standard error output will go to ``errorfile``. 

HTCondor will append events about the job to a log file wih the 
requested name``myexe.log``. When the job
finishes, its exit conditions and resource usage will also be noted in the log file. 
This file's contents are an excellent way to figure out what happened to jobs.

HTCondor needs to know how many machine resources to allocate to this job.
The ``request_`` lines describe that this job should be allocated 1 cpu core, 1024 
megabytes of memory and 10240 kilobytes of scratch disk space.

Finally, the queue statement tells HTCondor that you are done describing the
job, and to send it to the queue for processing.

**Example 2**

The submit description file for Example 2 queues 150
:index:`running multiple programs`\ runs of program *foo*. 
This job requires machines which have at least
4 GiB of physical memory, one cpu core and 16 Gb of scratch disk.
Each of the 150 runs of the program is given its own HTCondor process number, 
starting with 0. $(Process) is expanded by HTCondor to the actual number
used by each instance of the job. So, ``stdout``, and ``stderr`` will refer to
``out.0``, and ``err.0`` for the first run of the program,
``out.1``, and ``err.1`` for the second run of the program,
and so forth. A log file containing entries about when and where
HTCondor runs, checkpoints, and migrates processes for all the 150
queued programs will be written into the single file ``foo.log``.
If there are 150 or more available slots in your pool, all 150 instances
might be run at the same time, otherwise, HTCondor will run as many as
it can concurrently.

Each instance of this program works on one input file.  The name of this
input file is passed to the program as the only argument.  We prepare
150 copies of this input file in the current directory, and name them
input_file.0, input_file.1 ... up to input_file.149.  Using transfer_input_files,
we tell HTCondor which input file to send to each instance of the program.

.. code-block:: condor-submit

    # Example 2: Show off some fancy features,
    # including the use of pre-defined macros.

    executable     = foo
    arguments      = input_file.$(Process)

    request_memory = 4096
    request_cpus   = 1
    request_disk   = 16383

    error   = err.$(Process)
    output  = out.$(Process)
    log     = foo.log

    should_transfer_files = yes
    transfer_input_files = input_file.$(Process)

    # submit 150 instances of this job
    queue 150

:index:`examples<single: examples; submit description file>`

Submitting many similar jobs with one queue command
---------------------------------------------------

A wide variety of job submissions can be specified with extra
information to the **queue** :index:`queue<single: queue; submit commands>`
submit command. This flexibility eliminates the need for a job wrapper
or Perl script for many submissions.

The form of the **queue** command defines variables and expands values,
identifying a set of jobs. Square brackets identify an optional item.

**queue** [**<int expr>** ]

**queue** [**<int expr>** ] [**<varname>** ] **in** [**slice** ]
**<list of items>**

**queue** [**<int expr>** ] [**<varname>** ] **matching** [**files |
dirs** ] [**slice** ] **<list of items with file globbing>**

**queue** [**<int expr>** ] [**<list of varnames>** ] **from**
[**slice** ] **<file name> | <list of items>**

All optional items have defaults:

-  If ``<int expr>`` is not specified, it defaults to the value 1.
-  If ``<varname>`` or ``<list of varnames>`` is not specified, it
   defaults to the single variable called ``ITEM``.
-  If ``slice`` is not specified, it defaults to all elements within the
   list. This is the Python slice ``[::]``, with a step value of 1.
-  If neither ``files`` nor ``dirs`` is specified in a specification
   using the **from** key word, then both files and directories are
   considered when globbing.

The list of items uses syntax in one of two forms. One form is a comma
and/or space separated list; the items are placed on the same line as
the **queue** command. The second form separates items by placing each
list item on its own line, and delimits the list with parentheses. The
opening parenthesis goes on the same line as the **queue** command. The
closing parenthesis goes on its own line. The **queue** command
specified with the key word **from** will always use the second form of
this syntax. Example 3 below uses this second form of syntax. Finally,
the key word **from** accepts a shell command in place of file name, 
followed by a pipe ``|`` (example 4).

The optional ``slice`` specifies a subset of the list of items using the
Python syntax for a slice. Negative step values are not permitted.

Here are a set of examples.


**Example 1**

.. code-block:: condor-submit

      transfer_input_files = $(filename)
      arguments            = -infile $(filename)
      queue filename matching files *.dat

The use of file globbing expands the list of items to be all files in
the current directory that end in ``.dat``. Only files, and not
directories are considered due to the specification of ``files``. One
job is queued for each file in the list of items. For this example,
assume that the three files ``initial.dat``, ``middle.dat``, and
``ending.dat`` form the list of items after expansion; macro
``filename`` is assigned the value of one of these file names for each
job queued. That macro value is then substituted into the **arguments**
and **transfer_input_files** commands. The **queue** command expands
to

.. code-block:: condor-submit

      transfer_input_files = initial.dat
      arguments            = -infile initial.dat
      queue
      transfer_input_files = middle.dat
      arguments            = -infile middle.dat
      queue
      transfer_input_files = ending.dat
      arguments            = -infile ending.dat
      queue

**Example 2**

.. code-block:: condor-submit

      queue 1 input in A, B, C

Variable ``input`` is set to each of the 3 items in the list, and one
job is queued for each. For this example the **queue** command expands
to

.. code-block:: condor-submit

      input = A
      queue
      input = B
      queue
      input = C
      queue


**Example 3**

.. code-block:: condor-submit

      queue input, arguments from (
        file1, -a -b 26
        file2, -c -d 92
      )

Using the ``from`` form of the options, each of the two variables
specified is given a value from the list of items. For this example the
**queue** command expands to

.. code-block:: condor-submit

      input = file1
      arguments = -a -b 26
      queue
      input = file2
      arguments = -c -d 92
      queue

**Example 4**

.. code-block:: condor-submit

      queue from seq 7 9 |
      
feeds the list of items to queue with the output of ``seq 7 9``:

.. code-block:: condor-submit

      item = 7
      queue
      item = 8
      queue
      item = 9
      queue

Variables in the Submit Description File
----------------------------------------

:index:`automatic variables<single: automatic variables; submit description file>`
:index:`in submit description file<single: in submit description file; automatic variables>`

There are automatic variables for use within the submit description
file.

``$(Cluster)`` or ``$(ClusterId)``
    Each set of queued jobs from a specific user, submitted from a
    single submit host, sharing an executable have the same value of
    ``$(Cluster)`` or ``$(ClusterId)``. The first cluster of jobs are
    assigned to cluster 0, and the value is incremented by one for each
    new cluster of jobs. ``$(Cluster)`` or ``$(ClusterId)`` will have
    the same value as the job ClassAd attribute ``ClusterId``.

``$(Process)`` or ``$(ProcId)``
    Within a cluster of jobs, each takes on its own unique
    ``$(Process)`` or ``$(ProcId)`` value. The first job has value 0.
    ``$(Process)`` or ``$(ProcId)`` will have the same value as the job
    ClassAd attribute ``ProcId``.

``$$(a_machine_classad_attribute)``
    When the machine is matched to this job for it to run on, any
    dollar-dollar expressions are looked up from the machine ad, and then
    expanded.  This lets you put the value of some machine ad attribute
    into your job.  For example, if you to pass the actual amount of
    memory a slot has provisioned as an argument to the job, you
    could add ``arguments = --mem $$(Memory)``

    .. code-block:: condor-submit

      arguments = --mem $$(Memory)

    or, if you wanted to put the name of the machine the job ran on
    into the output file name, you could add

    .. code-block: condor-submit

      output = output_file.$$(Name)

``$$([ an_evaluated_classad_expression ])``
    This dollar-dollar-bracket syntax is useful when you need to
    perform some math on a value before passing it to your job.
    For example, if want to pass 90% of the allocated memory as an
    argument to your job, the submit file can have

    .. code-block: condor-submit

        arguments = --mem $$([ Memory * 0.9 ])

    and when the job is matched to a machine, condor will evaluate
    this expression in the context of both the job and machine ad

``$(ARCH)``
    The Architecture that HTCondor is running on, or the ARCH variable
    in the config file.  Example might be X86_64.

``$(OPSYS)`` ``$(OPSYSVER)`` ``$(OPSYSANDVER)`` ``$(OPSYSMAJORVER)``
    These submit file macros are availle at submit time, and mimic
    the classad attributes of the same names.

``$(SUBMIT_FILE)``
    The name of the submit_file as passed to the ``condor_submit`` command.

``$(SUBMIT_TIME)``
    The Unix epoch time submit was run.  Note, this may be useful for
    naming output files.

``$(Year)`` ``$(Month)`` ``$(Day)``
    These integer values are derived from the `$(SUBMIT_FILE)` macro above.

``$(Item)``
    The default name of the variable when no ``<varname>`` is provided
    in a **queue** command.

``$(ItemIndex)``
    Represents an index within a list of items. When no slice is
    specified, the first ``$(ItemIndex)`` is 0. When a slice is
    specified, ``$(ItemIndex)`` is the index of the item within the
    original list.

``$(Step)``
    For the ``<int expr>`` specified, ``$(Step)`` counts, starting at 0.

``$(Row)``
    When a list of items is specified by placing each item on its own
    line in the submit description file, ``$(Row)`` identifies which
    line the item is on. The first item (first line of the list) is
    ``$(Row)`` 0. The second item (second line of the list) is
    ``$(Row)`` 1. When a list of items are specified with all items on
    the same line, ``$(Row)`` is the same as ``$(ItemIndex)``.

Here is an example of a **queue** command for which the values of these
automatic variables are identified.


**Example 1**

This example queues six jobs.

.. code-block:: condor-submit

    queue 3 in (A, B)

-  ``$(Process)`` takes on the six values 0, 1, 2, 3, 4, and 5.
-  Because there is no specification for the ``<varname>`` within this
   **queue** command, variable ``$(Item)`` is defined. It has the value
   ``A`` for the first three jobs queued, and it has the value ``B`` for
   the second three jobs queued.
-  ``$(Step)`` takes on the three values 0, 1, and 2 for the three jobs
   with ``$(Item)=A``, and it takes on the same three values 0, 1, and 2
   for the three jobs with ``$(Item)=B``.
-  ``$(ItemIndex)`` is 0 for all three jobs with ``$(Item)=A``, and it
   is 1 for all three jobs with ``$(Item)=B``.
-  ``$(Row)`` has the same value as ``$(ItemIndex)`` for this example.


Including Submit Commands Defined Elsewhere
-------------------------------------------

:index:`including commands from elsewhere<single: including commands from elsewhere; submit description file>`

Externally defined submit commands can be incorporated into the submit
description file using the syntax

.. code-block:: condor-submit

      include : <what-to-include>

The <what-to-include> specification may specify a single file, where the
contents of the file will be incorporated into the submit description
file at the point within the file where the **include** is. Or,
<what-to-include> may cause a program to be executed, where the output
of the program is incorporated into the submit description file. The
specification of <what-to-include> has the bar character (``|``)
following the name of the program to be executed.

The **include** key word is case insensitive. There are no requirements
for white space characters surrounding the colon character.

Included submit commands may contain further nested **include**
specifications, which are also parsed, evaluated, and incorporated.
Levels of nesting on included files are limited, such that infinite
nesting is discovered and thwarted, while still permitting nesting.

Consider the example

.. code-block:: condor-submit

      include : ./list-infiles.sh |

In this example, the bar character at the end of the line causes the
script ``list-infiles.sh`` to be invoked, and the output of the script
is parsed and incorporated into the submit description file. If this
bash script is in the PATH when submit is run, and contains

.. code-block:: bash

      #!/bin/sh

      echo "transfer_input_files = `ls -m infiles/*.dat`"
      exit 0

then the output of this script has specified the set of input files to
transfer to the execute host. For example, if directory ``infiles``
contains the three files ``A.dat``, ``B.dat``, and ``C.dat``, then the
submit command

.. code-block:: condor-submit

      transfer_input_files = infiles/A.dat, infiles/B.dat, infiles/C.dat

is incorporated into the submit description file.


Using Conditionals in the Submit Description File
-------------------------------------------------

:index:`IF/ELSE syntax<single: IF/ELSE syntax; submit commands>`
:index:`IF/ELSE submit commands syntax`

Conditional if/else semantics are available in a limited form. The
syntax:

.. code-block:: condor-submit

      if <simple condition>
         <statement>
         . . .
         <statement>
      else
         <statement>
         . . .
         <statement>
      endif

An else key word and statements are not required, such that simple if
semantics are implemented. The <simple condition> does not permit
compound conditions. It optionally contains the exclamation point
character (!) to represent the not operation, followed by

-  the defined keyword followed by the name of a variable. If the
   variable is defined, the statement(s) are incorporated into the
   expanded input. If the variable is not defined, the statement(s) are
   not incorporated into the expanded input. As an example,

   .. code-block:: condor-submit

         if defined MY_UNDEFINED_VARIABLE
            X = 12
         else
            X = -1
         endif

   results in ``X = -1``, when ``MY_UNDEFINED_VARIABLE`` is not yet
   defined.

-  the version keyword, representing the version number of of the daemon
   or tool currently reading this conditional. This keyword is followed
   by an HTCondor version number. That version number can be of the form
   x.y.z or x.y. The version of the daemon or tool is compared to the
   specified version number. The comparison operators are

   -  == for equality. Current version 8.2.3 is equal to 8.2.
   -  >= to see if the current version number is greater than or equal
      to. Current version 8.2.3 is greater than 8.2.2, and current
      version 8.2.3 is greater than or equal to 8.2.
   -  <= to see if the current version number is less than or equal to.
      Current version 8.2.0 is less than 8.2.2, and current version
      8.2.3 is less than or equal to 8.2.

   As an example,

   .. code-block:: condor-submit

         if version >= 8.1.6
            DO_X = True
         else
            DO_Y = True
         endif

   results in defining ``DO_X`` as ``True`` if the current version of
   the daemon or tool reading this if statement is 8.1.6 or a more
   recent version.

-  True or yes or the value 1. The statement(s) are incorporated.
-  False or no or the value 0 The statement(s) are not incorporated.
-  $(<variable>) may be used where the immediately evaluated value is a
   simple boolean value. A value that evaluates to the empty string is
   considered False, otherwise a value that does not evaluate to a
   simple boolean value is a syntax error.

The syntax

.. code-block:: condor-submit

      if <simple condition>
         <statement>
         . . .
         <statement>
      elif <simple condition>
         <statement>
         . . .
         <statement>
      endif

is the same as syntax

.. code-block:: condor-submit

      if <simple condition>
         <statement>
         . . .
         <statement>
      else
         if <simple condition>
            <statement>
            . . .
            <statement>
         endif
      endif

Here is an example use of a conditional in the submit description file.
A portion of the ``sample.sub`` submit description file uses the if/else
syntax to define command line arguments in one of two ways:

.. code-block:: condor-submit

    if defined X
      arguments = -n $(X)
    else
      arguments = -n 1 -debug
    endif

Submit variable ``X`` is defined on the *condor_submit* command line
with

.. code-block:: console

    $ condor_submit  X=3  sample.sub

This command line incorporates the submit command ``X = 3`` into the
submission before parsing the submit description file. For this
submission, the command line arguments of the submitted job become

.. code-block:: condor-submit

    arguments = -n 3

If the job were instead submitted with the command line

.. code-block:: console

    $ condor_submit  sample.sub

then the command line arguments of the submitted job become

.. code-block:: condor-submit

    arguments = -n 1 -debug


Function Macros in the Submit Description File
----------------------------------------------

:index:`function macros<single: function macros; submit description file>`

A set of predefined functions increase flexibility. Both submit
description files and configuration files are read using the same
parser, so these functions may be used in both submit description files
and configuration files.

Case is significant in the function's name, so use the same letter case
as given in these definitions.

``$CHOICE(index, listname)`` or ``$CHOICE(index, item1, item2, ...)``
    An item within the list is returned. The list is represented by a
    parameter name, or the list items are the parameters. The ``index``
    parameter determines which item. The first item in the list is at
    index 0. If the index is out of bounds for the list contents, an
    error occurs.

``$ENV(environment-variable-name[:default-value])``
    Evaluates to the value of environment variable
    ``environment-variable-name``. If there is no environment variable
    with that name, Evaluates to UNDEFINED unless the optional
    :default-value is used; in which case it evaluates to default-value.
    For example,

    .. code-block:: condor-submit

        A = $ENV(HOME)

    binds ``A`` to the value of the ``HOME`` environment variable.

``$F[fpduwnxbqa](filename)``
    One or more of the lower case letters may be combined to form the
    function name and thus, its functionality. Each letter operates on
    the ``filename`` in its own way.

    -  ``f`` convert relative path to full path by prefixing the current
       working directory to it. This option works only in
       *condor_submit* files.
    -  ``p`` refers to the entire directory portion of ``filename``,
       with a trailing slash or backslash character. Whether a slash or
       backslash is used depends on the platform of the machine. The
       slash will be recognized on Linux platforms; either a slash or
       backslash will be recognized on Windows platforms, and the parser
       will use the same character specified.
    -  ``d`` refers to the last portion of the directory within the
       path, if specified. It will have a trailing slash or backslash,
       as appropriate to the platform of the machine. The slash will be
       recognized on Linux platforms; either a slash or backslash will
       be recognized on Windows platforms, and the parser will use the
       same character specified unless u or w is used. if b is used the
       trailing slash or backslash will be omitted.
    -  ``u`` convert path separators to Unix style slash characters
    -  ``w`` convert path separators to Windows style backslash
       characters
    -  ``n`` refers to the file name at the end of any path, but without
       any file name extension. As an example, the return value from
       ``$Fn(/tmp/simulate.exe)`` will be ``simulate`` (without the
       ``.exe`` extension).
    -  ``x`` refers to a file name extension, with the associated period
       (``.``). As an example, the return value from
       ``$Fn(/tmp/simulate.exe)`` will be ``.exe``.
    -  ``b`` when combined with the d option, causes the trailing slash
       or backslash to be omitted. When combined with the x option,
       causes the leading period (``.``) to be omitted.
    -  ``q`` causes the return value to be enclosed within quotes.
       Double quote marks are used unless a is also specified.
    -  ``a`` When combined with the q option, causes the return value to
       be enclosed within single quotes.

``$DIRNAME(filename)`` is the same as ``$Fp(filename)``

``$BASENAME(filename)`` is the same as ``$Fnx(filename)``

``$INT(item-to-convert)`` or ``$INT(item-to-convert, format-specifier)``
    Expands, evaluates, and returns a string version of
    ``item-to-convert``. The ``format-specifier`` has the same syntax as
    a C language or Perl format specifier. If no ``format-specifier`` is
    specified, "%d" is used as the format specifier.

``$RANDOM_CHOICE(choice1, choice2, choice3, ...)``
    :index:`$RANDOM_CHOICE() function macro` A random choice
    of one of the parameters in the list of parameters is made. For
    example, if one of the integers 0-8 (inclusive) should be randomly
    chosen:

    .. code-block:: text

        $RANDOM_CHOICE(0,1,2,3,4,5,6,7,8)

``$RANDOM_INTEGER(min, max [, step])``
    :index:`in configuration<single: in configuration; $RANDOM_INTEGER()>` A random integer
    within the range min and max, inclusive, is selected. The optional
    step parameter controls the stride within the range, and it defaults
    to the value 1. For example, to randomly chose an even integer in
    the range 0-8 (inclusive):

    .. code-block:: text

        $RANDOM_INTEGER(0, 8, 2)

``$REAL(item-to-convert)`` or ``$REAL(item-to-convert, format-specifier)``
    Expands, evaluates, and returns a string version of
    ``item-to-convert`` for a floating point type. The
    ``format-specifier`` is a C language or Perl format specifier. If no
    ``format-specifier`` is specified, "%16G" is used as a format
    specifier.

``$SUBSTR(name, start-index)`` or ``$SUBSTR(name, start-index, length)``
    Expands name and returns a substring of it. The first character of
    the string is at index 0. The first character of the substring is at
    index start-index. If the optional length is not specified, then the
    substring includes characters up to the end of the string. A
    negative value of start-index works back from the end of the string.
    A negative value of length eliminates use of characters from the end
    of the string. Here are some examples that all assume

    .. code-block:: condor-submit

        Name = abcdef

    -  ``$SUBSTR(Name, 2)`` is ``cdef``.
    -  ``$SUBSTR(Name, 0, -2)`` is ``abcd``.
    -  ``$SUBSTR(Name, 1, 3)`` is ``bcd``.
    -  ``$SUBSTR(Name, -1)`` is ``f``.
    -  ``$SUBSTR(Name, 4, -3)`` is the empty string, as there are no
       characters in the substring for this request.

Here are example uses of the function macros in a submit description
file. Note that these are not complete submit description files, but
only the portions that promote understanding of use cases of the
function macros.


**Example 1**

Generate a range of numerical values for a set of jobs, where values
other than those given by $(Process) are desired.

.. code-block:: condor-submit

    MyIndex     = $(Process) + 1
    initial_dir = run-$INT(MyIndex,%04d)

Assuming that there are three jobs queued, such that $(Process) becomes
0, 1, and 2, ``initial_dir`` will evaluate to the directories
``run-0001``, ``run-0002``, and ``run-0003``.


**Example 2**

This variation on Example 1 generates a file name extension which is a
3-digit integer value.

.. code-block:: condor-submit

    Values     = $(Process) * 10
    Extension  = $INT(Values,%03d)
    input      = X.$(Extension)

Assuming that there are four jobs queued, such that $(Process) becomes
0, 1, 2, and 3, ``Extension`` will evaluate to 000, 010, 020, and 030,
leading to files defined for **input** of ``X.000``, ``X.010``,
``X.020``, and ``X.030``.


**Example 3**

This example uses both the file globbing of the
**queue** :index:`queue<single: queue; submit commands>` command and a macro
function to specify a job input file that is within a subdirectory on
the submit host, but will be placed into a single, flat directory on the
execute host.

.. code-block:: condor-submit

    arguments            = $Fnx(FILE)
    transfer_input_files = $(FILE)
    queue FILE matching (
        samplerun/*.dat
    )

Assume that two files that end in ``.dat``, ``A.dat`` and ``B.dat``, are
within the directory ``samplerun``. Macro ``FILE`` expands to
``samplerun/A.dat`` and ``samplerun/B.dat`` for the two jobs queued. The
input files transferred are ``samplerun/A.dat`` and ``samplerun/B.dat``
on the submit host. The ``$Fnx()`` function macro expands to the
complete file name with any leading directory specification stripped,
such that the command line argument for one of the jobs will be
``A.dat`` and the command line argument for the other job will be
``B.dat``.


About Requirements and Rank
---------------------------

The ``requirements`` and ``rank`` commands in the submit description
file are powerful and flexible.
:index:`requirements<single: requirements; submit commands>`\ :index:`requirements attribute`
:index:`rank attribute`\ :index:`requirements<single: requirements; ClassAd attribute>`
:index:`rank<single: rank; ClassAd attribute>`\ Using them effectively requires
care, and this section presents those details.

Both ``requirements`` and ``rank`` need to be specified as valid
HTCondor ClassAd expressions, however, default values are set by the
*condor_submit* program if these are not defined in the submit
description file. From the *condor_submit* manual page and the above
examples, you see that writing ClassAd expressions is intuitive,
especially if you are familiar with the programming language C. There
are some pretty nifty expressions you can write with ClassAds. A
complete description of ClassAds and their expressions can be found in
the :doc:`/misc-concepts/classad-mechanism` section.

All of the commands in the submit description file are case insensitive,
except for the ClassAd attribute string values. ClassAd attribute names
are case insensitive, but ClassAd string values are case preserving.

Note that the comparison operators (<, >, <=, >=, and ==) compare
strings case insensitively. The special comparison operators =?= and =!=
compare strings case sensitively.

A **requirements** :index:`requirements<single: requirements; submit commands>` or
**rank** :index:`rank<single: rank; submit commands>` command in the submit
description file may utilize attributes that appear in a machine or a
job ClassAd. Within the submit description file (for a job) the prefix
MY. (on a ClassAd attribute name) causes a reference to the job ClassAd
attribute, and the prefix TARGET. causes a reference to a potential
machine or matched machine ClassAd attribute.

The *condor_status* command displays
:index:`condor_status<single: condor_status; HTCondor commands>`\ statistics about
machines within the pool. The **-l** option displays the machine ClassAd
attributes for all machines in the HTCondor pool. The job ClassAds, if
there are jobs in the queue, can be seen with the *condor_q -l*
command. This shows all the defined attributes for current jobs in the
queue.

A list of defined ClassAd attributes for job ClassAds is given in the
Appendix on the :doc:`/classad-attributes/job-classad-attributes` page. A
list of defined ClassAd attributes for machine ClassAds is given in the
Appendix on the :doc:`/classad-attributes/machine-classad-attributes` page.

Rank Expression Examples
''''''''''''''''''''''''

:index:`examples<single: examples; rank attribute>`
:index:`rank examples<single: rank examples; ClassAd attribute>`
:index:`rank<single: rank; submit commands>`

When considering the match between a job and a machine, rank is used to
choose a match from among all machines that satisfy the job's
requirements and are available to the user, after accounting for the
user's priority and the machine's rank of the job. The rank expressions,
simple or complex, define a numerical value that expresses preferences.

The job's ``Rank`` expression evaluates to one of three values. It can
be UNDEFINED, ERROR, or a floating point value. If ``Rank`` evaluates to
a floating point value, the best match will be the one with the largest,
positive value. If no ``Rank`` is given in the submit description file,
then HTCondor substitutes a default value of 0.0 when considering
machines to match. If the job's ``Rank`` of a given machine evaluates to
UNDEFINED or ERROR, this same value of 0.0 is used. Therefore, the
machine is still considered for a match, but has no ranking above any
other.

A boolean expression evaluates to the numerical value of 1.0 if true,
and 0.0 if false.

The following ``Rank`` expressions provide examples to follow.

For a job that desires the machine with the most available memory:

.. code-block:: condor-submit

    Rank = memory

For a job that prefers to run on a friend's machine on Saturdays and
Sundays:

.. code-block:: condor-submit

    Rank = ( (clockday == 0) || (clockday == 6) )
           && (machine == "friend.cs.wisc.edu")

For a job that prefers to run on one of three specific machines:

.. code-block:: condor-submit

    Rank = (machine == "friend1.cs.wisc.edu") ||
           (machine == "friend2.cs.wisc.edu") ||
           (machine == "friend3.cs.wisc.edu")

For a job that wants the machine with the best floating point
performance (on Linpack benchmarks):

.. code-block:: condor-submit

    Rank = kflops

This particular example highlights a difficulty with ``Rank`` expression
evaluation as currently defined. While all machines have floating point
processing ability, not all machines will have the ``kflops`` attribute
defined. For machines where this attribute is not defined, ``Rank`` will
evaluate to the value UNDEFINED, and HTCondor will use a default rank of
the machine of 0.0. The ``Rank`` attribute will only rank machines where
the attribute is defined. Therefore, the machine with the highest
floating point performance may not be the one given the highest rank.

So, it is wise when writing a ``Rank`` expression to check if the
expression's evaluation will lead to the expected resulting ranking of
machines. This can be accomplished using the *condor_status* command
with the *-constraint* argument. This allows the user to see a list of
machines that fit a constraint. To see which machines in the pool have
``kflops`` defined, use

.. code-block:: console

    $ condor_status -constraint kflops

Alternatively, to see a list of machines where ``kflops`` is not
defined, use

.. code-block:: console

    $ condor_status -constraint "kflops=?=undefined"

For a job that prefers specific machines in a specific order:

.. code-block:: condor-submit

    Rank = ((machine == "friend1.cs.wisc.edu")*3) +
           ((machine == "friend2.cs.wisc.edu")*2) +
            (machine == "friend3.cs.wisc.edu")

If the machine being ranked is ``friend1.cs.wisc.edu``, then the
expression

.. code-block:: condor-classad-expr

    (machine == "friend1.cs.wisc.edu")

is true, and gives the value 1.0. The expressions

.. code-block:: condor-classad-expr

    (machine == "friend2.cs.wisc.edu")

and

.. code-block:: condor-classad-expr

    (machine == "friend3.cs.wisc.edu")

are false, and give the value 0.0. Therefore, ``Rank`` evaluates to the
value 3.0. In this way, machine ``friend1.cs.wisc.edu`` is ranked higher
than machine ``friend2.cs.wisc.edu``, machine ``friend2.cs.wisc.edu`` is
ranked higher than machine ``friend3.cs.wisc.edu``, and all three of
these machines are ranked higher than others.

Submitting Jobs Using a Shared File System
------------------------------------------

:index:`submission using a shared file system<single: submission using a shared file system; job>`
:index:`submission of jobs<single: submission of jobs; shared file system>`

If vanilla, java, or parallel universe jobs are submitted without using
the File Transfer mechanism, HTCondor must use a shared file system to
access input and output files. In this case, the job must be able to
access the data files from any machine on which it could potentially
run.

As an example, suppose a job is submitted from blackbird.cs.wisc.edu,
and the job requires a particular data file called
``/u/p/s/psilord/data.txt``. If the job were to run on
cardinal.cs.wisc.edu, the file ``/u/p/s/psilord/data.txt`` must be
available through either NFS or AFS for the job to run correctly.

HTCondor allows users to ensure their jobs have access to the right
shared files by using the ``FileSystemDomain`` and ``UidDomain`` machine
ClassAd attributes. These attributes specify which machines have access
to the same shared file systems. All machines that mount the same shared
directories in the same locations are considered to belong to the same
file system domain. Similarly, all machines that share the same user
information (in particular, the same UID, which is important for file
systems like NFS) are considered part of the same UID domain.

The default configuration for HTCondor places each machine in its own
UID domain and file system domain, using the full host name of the
machine as the name of the domains. So, if a pool does have access to a
shared file system, the pool administrator must correctly configure
HTCondor such that all the machines mounting the same files have the
same ``FileSystemDomain`` configuration. Similarly, all machines that
share common user information must be configured to have the same
``UidDomain`` configuration.

When a job relies on a shared file system, HTCondor uses the
``requirements`` expression to ensure that the job runs on a machine in
the correct ``UidDomain`` and ``FileSystemDomain``. In this case, the
default ``requirements`` expression specifies that the job must run on a
machine with the same ``UidDomain`` and ``FileSystemDomain`` as the
machine from which the job is submitted. This default is almost always
correct. However, in a pool spanning multiple ``UidDomain``\ s and/or
``FileSystemDomain``\ s, the user may need to specify a different
``requirements`` expression to have the job run on the correct machines.

For example, imagine a pool made up of both desktop workstations and a
dedicated compute cluster. Most of the pool, including the compute
cluster, has access to a shared file system, but some of the desktop
machines do not. In this case, the administrators would probably define
the ``FileSystemDomain`` to be ``cs.wisc.edu`` for all the machines that
mounted the shared files, and to the full host name for each machine
that did not. An example is ``jimi.cs.wisc.edu``.

In this example, a user wants to submit vanilla universe jobs from her
own desktop machine (jimi.cs.wisc.edu) which does not mount the shared
file system (and is therefore in its own file system domain, in its own
world). But, she wants the jobs to be able to run on more than just her
own machine (in particular, the compute cluster), so she puts the
program and input files onto the shared file system. When she submits
the jobs, she needs to tell HTCondor to send them to machines that have
access to that shared data, so she specifies a different
``requirements`` expression than the default:

.. code-block:: condor-submit

    Requirements = TARGET.UidDomain == "cs.wisc.edu" && \
                   TARGET.FileSystemDomain == "cs.wisc.edu"

WARNING: If there is no shared file system, or the HTCondor pool
administrator does not configure the ``FileSystemDomain`` setting
correctly (the default is that each machine in a pool is in its own file
system and UID domain), a user submits a job that cannot use remote
system calls (for example, a vanilla universe job), and the user does
not enable HTCondor's File Transfer mechanism, the job will only run on
the machine from which it was submitted.

.. _jobs_that_require_credentials:

Jobs That Require Credentials
-----------------------------

:index:`requesting OAuth credentials for a job<single: requesting OAuth credentials for a job; OAuth>`

If the HTCondor pool administrator has configured the submit machine
with one or more credential monitors,
jobs submitted on that machine may automatically be provided with credentials
and/or it may be possible for users to request and obtain credentials for their jobs.

Suppose the administrator has configured the submit machine
such that users may obtain credentials from a storage service called "CloudBoxDrive."
A job that needs credentials from CloudBoxDrive
should contain the submit command

.. code-block:: condor-submit

    use_oauth_services = cloudboxdrive

Upon submitting this job for the first time,
the user will be directed to a webpage hosted on the submit machine
which will guide the user through the process of obtaining a CloudBoxDrive credential.
The credential is then stored securely on the submit machine.
(**Note: depending on which credential monitor is used, the original
job may have to be re-submitted at this point.**)
(Also note that at no point is the user's *password* stored on the submit machine.)
Once a credential is stored on the submit machine,
as long as it remains valid,
it is transferred securely to all subsequently submitted jobs that contain ``use_oauth_services = cloudboxdrive``.

When a job that contains credentials runs on an execute machine,
the job's executable will have the environment variable ``_CONDOR_CREDS`` set,
which points to the location of all of the credentials inside the job's sandbox.
For credentials obtained via the ``use_oauth_services`` submit file command,
the "access token" is stored under ``$_CONDOR_CREDS``
in a JSON-encoded file
named with the name of the service provider and with the extension ``.use``.
For the "CloudBoxDrive" example,
the access token would be located in ``$_CONDOR_CREDS/cloudboxdrive.use``.

The HTCondor file transfer mechanism has built-in plugins
for using user-obtained credentials
to transfer files from some specific storage providers,
see :ref:`file_transfer_using_a_url`.

Some credential providers may require the user to provide
a description of the permissions (often called "scopes") a user needs for a specific credential.
Credential permission scoping is possible using the ``<service name>_oauth_permissions``
submit file command.
For example, suppose our CloudBoxDrive service has a ``/public`` directory,
and the documentation for the service said that users must specify a ``read:<directory>`` scope
in order to be able to read data out of ``<directory>``.
The submit file would need to contain

.. code-block:: condor-submit

    use_oauth_services = cloudboxdrive
    cloudboxdrive_oauth_permissions = read:/public

Some credential providers may also require the user to provide
the name of the resource (or "audience") that a credential should allow access to.
Resource naming is done using the ``<service name>_oauth_resource`` submit file command.
For example, if our CloudBoxDrive service has servers located at some unversities
and the documentation says that we should pick one near us and specify it as the audience,
the submit file might look like

.. code-block:: condor-submit

    use_oauth_services = cloudboxdrive
    cloudboxdrive_oauth_permissions = read:/public
    cloudboxdrive_oauth_resource = https://cloudboxdrive.myuni.edu

It is possible for a single job to request and/or use credentials from multiple services
by listing each service in the ``use_oauth_services`` command.
Suppose the nearby university has a SciTokens service that provides credentials to access the ``localstorage.myuni.edu`` machine,
and the HTCondor pool administrator has configured the submit machine to allow users to obtain credentials from this service,
and that a user has write access to the `/foo` directory on the storage machine.
A submit file that would result in a job that contains credentials
that can read from CloudBoxDrive and write to the local university storage might look like

.. code-block:: condor-submit

    use_oauth_services = cloudboxdrive, myuni

    cloudboxdrive_oauth_permissions = read:/public
    cloudboxdrive_oauth_resource = https://cloudboxdrive.myuni.edu

    myuni_oauth_permissions = write:/foo
    myuni_oauth_resource = https://localstorage.myuni.edu

A single job can also request multiple credentials from the same service provider
by affixing handles to the ``<service>_oauth_permissions`` and (if necessary)
``<service>_oauth_resource`` commands.
For example, if a user wants separate read and write credentials for CloudBoxDrive

.. code-block:: condor-submit

    use_oauth_services = cloudboxdrive
    cloudboxdrive_oauth_permissions_readpublic = read:/public
    cloudboxdrive_oauth_permissions_writeprivate = write:/private

    cloudboxdrive_oauth_resource_readpublic = https://cloudboxdrive.myuni.edu
    cloudboxdrive_oauth_resource_writeprivate = https://cloudboxdrive.myuni.edu

Submitting the above would result in a job with respective access tokens located in
``$_CONDOR_CREDS/cloudboxdrive_readpublic.use`` and
``$_CONDOR_CREDS/cloudboxdrive_writeprivate.use``.

Note that the permissions and resource settings for each handle (and for
no handle) are stored separately from the job so multiple jobs from the
same user running at the same time or for a period of time consecutively
may not use a different set of permissions and resource settings for the
same service and handle.  If that is attempted, a new job submission
will fail with instructions on how to resolve the conflict, but the
safest thing is to choose a unique handle.

If a service provider does not require permissions or resources to be specified,
a user can still request multiple credentials by affixing handles to
``<service>_oauth_permissions`` commands with empty values

.. code-block:: condor-submit

    use_oauth_services = cloudboxdrive
    cloudboxdrive_oauth_permissions_personal =
    cloudboxdrive_oauth_permissions_public =

.. only:: Vault

    When the Vault credential monitor is configured, the service name may
    optionally be split into two parts with an underscore between them,
    where the first part is the issuer and the second part is the role.  In
    this example the issuer is "dune" and the role is "production", both
    as configured by the administrator of the Vault server:

    .. code-block:: condor-submit

        use_oauth_services = dune_production

    Vault server.  Vault does not require permissions or resources to be
    set, but they may be set to reduce the default permissions or restrict
    the resources that may use the credential.  The full service name
    including an underscore may be used in an ``oauth_permissions`` or
    ``oauth_resource``.  Avoid using handles that might be confused as
    role names.  For example, the following will result in a conflict
    between two credentials called ``dune_production.use``:

    .. code-block:: condor-submit

        use_oauth_services = dune, dune_production
        dune_oauth_permissions_production =
        dune_production_oauth_permissions =


Jobs That Require GPUs
----------------------

:index:`requesting GPUs for a job<single: requesting GPUs for a job; GPUs>`

A job that needs GPUs to run identifies the number of GPUs needed in the
submit description file by adding the submit command

.. code-block:: condor-submit

    request_GPUs = <n>

where ``<n>`` is replaced by the integer quantity of GPUs required for
the job. For example, a job that needs 1 GPU uses

.. code-block:: condor-submit

    request_GPUs = 1

Because there are different capabilities among GPUs, the job might need
to further qualify which GPU of available ones is required. Do this by
specifying or adding a clause to an existing
**Requirements** :index:`Requirements<single: Requirements; submit commands>` submit
command. As an example, assume that the job needs a speed and capacity
of a CUDA GPU that meets or exceeds the value 1.2. In the submit
description file, place

.. code-block:: condor-submit

    request_GPUs = 1
    requirements = (CUDACapability >= 1.2) && $(requirements:True)

Access to GPU resources by an HTCondor job needs special configuration
of the machines that offer GPUs. Details of how to set up the
configuration are in the :doc:`/admin-manual/policy-configuration` section.

Interactive Jobs
----------------

:index:`interactive<single: interactive; job>` :index:`interactive jobs`

An interactive job is a Condor job that is provisioned and scheduled
like any other vanilla universe Condor job onto an execute machine
within the pool. The result of a running interactive job is a shell
prompt issued on the execute machine where the job runs. The user that
submitted the interactive job may then use the shell as desired, perhaps
to interactively run an instance of what is to become a Condor job. This
might aid in checking that the set up and execution environment are
correct, or it might provide information on the RAM or disk space
needed. This job (shell) continues until the user logs out or any other
policy implementation causes the job to stop running. A useful feature
of the interactive job is that the users and jobs are accounted for
within Condor's scheduling and priority system.

Neither the submit nor the execute host for interactive jobs may be on
Windows platforms.

The current working directory of the shell will be the initial working
directory of the running job. The shell type will be the default for the
user that submits the job. At the shell prompt, X11 forwarding is
enabled.

Each interactive job will have a job ClassAd attribute of

.. code-block:: condor-classad

    InteractiveJob = True

Submission of an interactive job specifies the option **-interactive**
on the *condor_submit* command line.

A submit description file may be specified for this interactive job.
Within this submit description file, a specification of these 5 commands
will be either ignored or altered:

#. **executable** :index:`executable<single: executable; submit commands>`
#. **transfer_executable** :index:`transfer_executable<single: transfer_executable; submit commands>`
#. **arguments** :index:`arguments<single: arguments; submit commands>`
#. **universe** :index:`universe<single: universe; submit commands>`. The
   interactive job is a vanilla universe job.
#. **queue** :index:`queue<single: queue; submit commands>` **<n>**. In this
   case the value of **<n>** is ignored; exactly one interactive job is
   queued.

The submit description file may specify anything else needed for the
interactive job, such as files to transfer.

If no submit description file is specified for the job, a default one is
utilized as identified by the value of the configuration variable
``INTERACTIVE_SUBMIT_FILE`` :index:`INTERACTIVE_SUBMIT_FILE`.

Here are examples of situations where interactive jobs may be of
benefit.

-  An application that cannot be batch processed might be run as an
   interactive job. Where input or output cannot be captured in a file
   and the executable may not be modified, the interactive nature of the
   job may still be run on a pool machine, and within the purview of
   Condor.
-  A pool machine with specialized hardware that requires interactive
   handling can be scheduled with an interactive job that utilizes the
   hardware.
-  The debugging and set up of complex jobs or environments may benefit
   from an interactive session. This interactive session provides the
   opportunity to run scripts or applications, and as errors are
   identified, they can be corrected on the spot.
-  Development may have an interactive nature, and proceed more quickly
   when done on a pool machine. It may also be that the development
   platforms required reside within Condor's purview as execute hosts.


Submitting Lots of Jobs
-----------------------

:index:`late materialization<single: late materialization; lots of jobs>` :index:`late materialization`

When submitting a lot of jobs with a single submit file, you can dramatically speed up submission
and reduce the load on the *condor_schedd* by submitting the jobs as a late materialization job factory.

A submission of this form sends a single ClassAd, called the Cluster ad, to the *condor_schedd*, as
well as instructions to create the individual jobs as variations on that Cluster ad. These instructions
are sent as a *submit digest* and optional *itemdata*.  The *submit digest* is the submit file stripped down
to just the statements that vary between jobs.  The *itemdata* is the arguments to the ``Queue`` statement
when the arguments are more than just a count of jobs.

The *condor_schedd* will use the *submit digest* and the *itemdata* to create the individual job ClassAds
when they are needed.  Materialization is controlled by two values stored in the Cluster classad, and
by optional limits configured in the *condor_schedd*.

The ``max_idle`` limit specifies the maximum number of non-running jobs that should be materialized in the 
*condor_schedd* at any one time. One or more jobs will materialize whenever a job enters the Run state
and the number of non-running jobs that are still in the *condor_schedd* is less than this limit. This
limit is stored in the Cluster ad in the `JobMaterializeMaxIdle` attribute.

The ``max_materialize`` limit specifies an overall limit on the number of jobs that can be materialized in
the *condor_schedd* at any one time.  One or more jobs will materialize when a job leaves the *condor_schedd*
and the number of materialized jobs remaining is less than this limit. This limit is stored in the Cluster
ad in the `JobMaterializeLimit` attribute.

Late materialization can be used as a way for a user to submit millions of jobs without hitting the 
:macro:`MAX_JOBS_PER_OWNER` or :macro:`MAX_JOBS_PER_SUBMISSION` limits in the *condor_schedd*, since
the *condor_schedd* will enforce these limits by applying them to the ``max_materialize`` and ``max_idle``
values specified in the Cluster ad.

To give an example, the following submit file:

.. code-block:: condor-submit

    executable     = foo
    arguments      = input_file.$(Process)

    request_memory = 4096
    request_cpus   = 1
    request_disk   = 16383

    error   = err.$(Process)
    output  = out.$(Process)
    log     = foo.log

    should_transfer_files = yes
    transfer_input_files = input_file.$(Process)

    # submit as a factory with an idle jobs limit
    max_idle = 100

    # submit 15,000 instances of this job
    queue 15*1000


When submitted as a late materialization factory, the *submit digest* for this factory
will contain only the submit statments that vary between jobs, and the collapsed queue statement
like this:

.. code-block:: condor-submit

    arguments = input_file.$(Process)
    error = err.$(Process)
    output = out.$(Process)
    transfer_input_files = input_file.$(Process)

    queue 15000

Materialization log events
''''''''''''''''''''''''''

When a Late Materialization job factory is submitted to the *condor_schedd*, a ``Cluster submitted`` event
will be written to the UserLog of the Cluster ad.  This will be the same log file used by the first job
materialized by the factory.  To avoid confustion,
it is recommended that you use the same log file for all jobs in the factory.

When the Late Materialization job factory is removed from the *condor_schedd*, a ``Cluster removed`` event
will be written to the UserLog of the Cluster ad.  This event will indicate how many jobs were materialized
before the factory was removed.

If Late Materialization of jobs is paused due to an error in materialization or because condor_hold 
was used to hold the cluster id, a ``Job Materialization Paused`` event will be written to the UserLog of the
Cluster ad. This event will indicate the reason for the pause.

When ``condor_release`` is used to release the the cluster id of a Late Materialization job factory,
and materialization  was paused because of a previous use of *condor_hold*, a ``Job Materialization Resumed``
event will be written to the UserLog of the Cluster ad.

Limitations
'''''''''''

Currently, not all features of *condor_submit* will work with late materialization.
The following limitations apply:

- Only a single ``Queue`` statement is allowed, lines from the submit file after the 
  first ``Queue`` statement will be ignored.
- the ``$RANDOM_INTEGER`` and ``$RANDOM_CHOICE`` macro functions will expand at submit
  time to produce the Cluster ad, but these macro functions will not be included in
  the *submit digest* and so will have the same value for all jobs.
- Spooling of input files does not work with late materialization.
- :macro:`SUBMIT_REQUIREMENT_*` and :macro:`JOB_TRANSFORM_*` configuration parameters in
  the *condor_schedd* are applied to jobs as they are materialized,
  but not to the Cluster ad as it is submitted.  So a :macro:`SUBMIT_REQUIREMENT` might not fail
  at submit time, causing the user to think that they had met the submit requirements when in 
  fact the jobs would fail to materialize at some time in the future.  This can be 
  confusing because a factory that has no materialized jobs is not visible in the normal
  *condor_q* output. The only way to see late materialization job factories is to use the
  ``-factory`` option with *condor_q*

Displaying the Factory
''''''''''''''''''''''

*condor_q* can be use to show late materialization job factories in the *condor_schedd* by
using the ``-factory`` option.

.. code-block::

    > condor_q -factory
    -- Schedd: submit.example.org : <192.168.101.101:9618?... @ 12/01/20 13:35:00
    ID     OWNER          SUBMITTED  LIMIT PRESNT   RUN    IDLE   HOLD NEXTID MODE DIGEST
    77.    bob         12/01  13:30  15000    130     30     80     20   1230      /var/lib/condor/spool/77/condor_submit.77.digest

The factory above shows that 30 jobs are currently running,
80 are idle, 20 are held and that the next job to materialize will
be job ``77.1230``.  The total of Idle + Held jobs is 100,
which is equal to the ``max_idle`` value specified in the submit file.

The path to the *submit digest* file is shown. This file is used to reload the factory
when the *condor_schedd* is restarted.  If the factory is unable to materialize jobs
because of an error, the ``MODE`` field will show ``Held`` or ``Errs`` to indicate
there is a problem. ``Errs`` indicates a problem reloading the factory, ``Held``
indicates a problem materializing jobs.

In case of a factory problem, use ``condor_q -factory -long`` to see the the factory information
and the ``JobMaterializePauseReason`` attribute.

Removing a Factory
''''''''''''''''''

The Late materialization job factory will be remove from the schedd automatically once all of the
jobs have materialized and completed.  To remove the factory without first completing all of the jobs
use *condor_rm* with the ClusterId of the factory as the argument.

Editing a Factory
'''''''''''''''''

The *submit digest* for a Late Materialization job factory cannot be changed after submission, but the Cluster ad
for the factory can be edited using *condor_qedit*.  Any *condor_qedit* command that has the ClusterId as a edit
target will edit all currently materialized jobs, as well as editing the Cluster ad so that all jobs that materialize
in the future will also be edited.
