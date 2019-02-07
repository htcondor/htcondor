      

Submitting a Job
================

A job is submitted for execution to HTCondor using the *condor\_submit*
command. *condor\_submit* takes as an argument the name of a file called
a submit description file. This file contains commands and keywords to
direct the queuing of jobs. In the submit description file, HTCondor
finds everything it needs to know about the job. Items such as the name
of the executable to run, the initial working directory, and
command-line arguments to the program all go into the submit description
file. *condor\_submit* creates a job ClassAd based upon the information,
and HTCondor works toward running the job.

The contents of a submit description file have been designed to save
time for HTCondor users. It is easy to submit multiple runs of a program
to HTCondor with a single submit description file. To run the same
program many times on different input data sets, arrange the data files
accordingly so that each run reads its own input, and each run writes
its own output. Each individual run may have its own initial working
directory, files mapped for ``stdin``, ``stdout``, ``stderr``,
command-line arguments, and shell environment; these are all specified
in the submit description file. A program that directly opens its own
files will read the file names to use either from ``stdin`` or from the
command line. A program that opens a static file, given by file name,
every time will need to use a separate subdirectory for the output of
each run.

The *condor\_submit* manual page is on
page \ `2135 <Condorsubmit.html#x149-108000012>`__ and contains a
complete and full description of how to use *condor\_submit*. It also
includes descriptions \ `2139 <Condorsubmit.html#x149-108400012>`__ of
all of the many commands that may be placed into a submit description
file. In addition, the index lists entries for each command under the
heading of Submit Commands.

Note that job ClassAd attributes can be set directly in a submit file
using the **+<attribute> = <value>** syntax (see
 `2206 <Condorsubmit.html#x149-108400012>`__ for details.)

Sample submit description files
-------------------------------

In addition to the examples of submit description files given here,
there are more in the *condor\_submit* manual page (see
 `2135 <Condorsubmit.html#x149-108000012>`__).

 Example 1

Example 1 is one of the simplest submit description files possible. It
queues up the program *myexe* for execution somewhere in the pool. Use
of the vanilla universe is implied, as that is the default when not
specified in the submit description file.

An executable is compiled to run on a specific platform. Since this
submit description file does not specify a platform, HTCondor will use
its default, which is to run the job on a machine which has the same
architecture and operating system as the machine where *condor\_submit*
is run to submit the job.

Standard input for this job will come from the file ``inputfile``, as
specified by the **input** command, and standard output for this job
will go to the file ``outputfile``, as specified by the **output**
command. HTCondor expects to find ``inputfile`` in the current working
directory when this job is submitted, and the system will take care of
getting the input file to where it needs to be when the job is executed,
as well as bringing back the output results (to the current working
directory) after job execution.

A log file, ``myexe.log``, will also be produced that contains events
the job had during its lifetime inside of HTCondor. When the job
finishes, its exit conditions will be noted in the log file. This file’s
contents are an excellent way to figure out what happened to submitted
jobs.

::

      #################### 
      # 
      # Example 1 
      # Simple HTCondor submit description file 
      # 
      #################### 
     
      Executable   = myexe 
      Log          = myexe.log 
      Input        = inputfile 
      Output       = outputfile 
      Queue

 Example 2

Example 2 queues up one copy of the program *foo* (which had been
created by *condor\_compile*) for execution by HTCondor. No **input**,
**output**, or **error** commands are given in the submit description
file, so ``stdin``, ``stdout``, and ``stderr`` will all refer to
``/dev/null``. The program may produce output by explicitly opening a
file and writing to it.

::

      #################### 
      # 
      # Example 2 
      # Standard universe submit description file 
      # 
      #################### 
     
      Executable   = foo 
      Universe     = standard 
      Log          = foo.log 
      Queue

 Example 3

Example 3 queues two copies of the program *mathematica*. The first copy
will run in directory ``run_1``, and the second will run in directory
``run_2`` due to the **initialdir** command. For each copy, ``stdin``
will be ``test.data``, ``stdout`` will be ``loop.out``, and ``stderr``
will be ``loop.error``. Each run will read input and write output files
within its own directory. Placing data files in separate directories is
a convenient way to organize data when a large group of HTCondor jobs is
to run. The example file shows program submission of *mathematica* as a
vanilla universe job. The vanilla universe is most often the right
choice of universe when the source and/or object code is not available.

The **request\_memory** command is included to ensure that the
*mathematica* jobs match with and then execute on pool machines that
provide at least 1 GByte of memory.

::

      #################### 
      # 
      # Example 3: demonstrate use of multiple 
      # directories for data organization. 
      # 
      #################### 
     
      executable     = mathematica 
      universe       = vanilla 
      input          = test.data 
      output         = loop.out 
      error          = loop.error 
      log            = loop.log 
      request_memory = 1 GB 
     
      initialdir     = run_1 
      queue 
     
      initialdir     = run_2 
      queue

 Example 4

The submit description file for Example 4 queues 150 runs of program
*foo* which has been compiled and linked for Linux running on a 32-bit
Intel processor. This job requires HTCondor to run the program on
machines which have greater than 32 MiB of physical memory, and the
**rank** command expresses a preference to run each instance of the
program on machines with more than 64 MiB. It also advises HTCondor that
this standard universe job will use up to 28000 KiB of memory when
running. Each of the 150 runs of the program is given its own process
number, starting with process number 0. So, files ``stdin``, ``stdout``,
and ``stderr`` will refer to ``in.0``, ``out.0``, and ``err.0`` for the
first run of the program, ``in.1``, ``out.1``, and ``err.1`` for the
second run of the program, and so forth. A log file containing entries
about when and where HTCondor runs, checkpoints, and migrates processes
for all the 150 queued programs will be written into the single file
``foo.log``.

::

      #################### 
      # 
      # Example 4: Show off some fancy features including 
      # the use of pre-defined macros. 
      # 
      #################### 
     
      Executable     = foo 
      Universe       = standard 
      requirements   = OpSys == "LINUX" && Arch =="INTEL" 
      rank           = Memory >= 64 
      image_size     = 28000 
      request_memory = 32 
     
      error   = err.$(Process) 
      input   = in.$(Process) 
      output  = out.$(Process) 
      log     = foo.log 
     
      queue 150

Using the Power and Flexibility of the Queue Command
----------------------------------------------------

A wide variety of job submissions can be specified with extra
information to the **queue** submit command. This flexibility eliminates
the need for a job wrapper or Perl script for many submissions.

The form of the **queue** command defines variables and expands values,
identifying a set of jobs. Square brackets identify an optional item.

**queue** [**<int expr>**\ ]

**queue** [**<int expr>**\ ] [**<varname>**\ ] **in** [**slice**\ ]
**<list of items>**

**queue** [**<int expr>**\ ] [**<varname>**\ ] **matching** [**files \|
dirs**\ ] [**slice**\ ] **<list of items with file globbing>**

**queue** [**<int expr>**\ ] [**<list of varnames>**\ ] **from**
[**slice**\ ] **<file name> \| <list of items>**

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
this syntax. Example 3 below uses this second form of syntax.

The optional ``slice`` specifies a subset of the list of items using the
Python syntax for a slice. Negative step values are not permitted.

Here are a set of examples.

 Example 1

::

      transfer_input_files = $(filename) 
      arguments            = -infile $(filename) 
      queue filename matching files *.dat 

The use of file globbing expands the list of items to be all files in
the current directory that end in ``.dat``. Only files, and not
directories are considered due to the specification of ``files``. One
job is queued for each file in the list of items. For this example,
assume that the three files ``initial.dat``, ``middle.dat``, and
``ending.dat`` form the list of items after expansion; macro
``filename`` is assigned the value of one of these file names for each
job queued. That macro value is then substituted into the **arguments**
and **transfer\_input\_files** commands. The **queue** command expands
to

::

      transfer_input_files = initial.dat 
      arguments            = -infile initial.dat 
      queue 
      transfer_input_files = middle.dat 
      arguments            = -infile middle.dat 
      queue 
      transfer_input_files = ending.dat 
      arguments            = -infile ending.dat 
      queue

 Example 2

::

      queue 1 input in A, B, C

Variable ``input`` is set to each of the 3 items in the list, and one
job is queued for each. For this example the **queue** command expands
to

::

      input = A 
      queue 
      input = B 
      queue 
      input = C 
      queue

 Example 3

::

      queue input,arguments from ( 
        file1, -a -b 26 
        file2, -c -d 92 
      )

Using the ``from`` form of the options, each of the two variables
specified is given a value from the list of items. For this example the
**queue** command expands to

::

      input = file1 
      arguments = -a -b 26 
      queue 
      input = file2 
      arguments = -c -d 92 
      queue

Variables in the Submit Description File
----------------------------------------

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

 Example 1

This example queues six jobs.

::

      queue 3 in (A, B)

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

Externally defined submit commands can be incorporated into the submit
description file using the syntax

::

      include : <what-to-include>

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

::

      include : list-infiles.sh |

In this example, the bar character at the end of the line causes the
script ``list-infiles.sh`` to be invoked, and the output of the script
is parsed and incorporated into the submit description file. If this
bash script contains

::

      echo "transfer_input_files = `ls -m infiles/*.dat`"

then the output of this script has specified the set of input files to
transfer to the execute host. For example, if directory ``infiles``
contains the three files ``A.dat``, ``B.dat``, and ``C.dat``, then the
submit command

::

      transfer_input_files = infiles/A.dat, infiles/B.dat, infiles/C.dat

is incorporated into the submit description file.

Using Conditionals in the Submit Description File
-------------------------------------------------

Conditional if/else semantics are available in a limited form. The
syntax:

::

      if <simple condition> 
         <statement> 
         . . . 
         <statement> 
      else 
         <statement> 
         . . . 
         <statement> 
      endif

An else key word and statements are not required, such that simple if
semantics are implemented. The <simple condition> does not permit
compound conditions. It optionally contains the exclamation point
character (!) to represent the not operation, followed by

-  the defined keyword followed by the name of a variable. If the
   variable is defined, the statement(s) are incorporated into the
   expanded input. If the variable is not defined, the statement(s) are
   not incorporated into the expanded input. As an example,

   ::

         if defined MY_UNDEFINED_VARIABLE 
            X = 12 
         else 
            X = -1 
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

   ::

         if version >= 8.1.6 
            DO_X = True 
         else 
            DO_Y = True 
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

::

      if <simple condition> 
         <statement> 
         . . . 
         <statement> 
      elif <simple condition> 
         <statement> 
         . . . 
         <statement> 
      endif

is the same as syntax

::

      if <simple condition> 
         <statement> 
         . . . 
         <statement> 
      else 
         if <simple condition> 
            <statement> 
            . . . 
            <statement> 
         endif 
      endif

Here is an example use of a conditional in the submit description file.
A portion of the ``sample.sub`` submit description file uses the if/else
syntax to define command line arguments in one of two ways:

::

      if defined X 
        arguments = -n $(X) 
      else 
        arguments = -n 1 -debug 
      endif

Submit variable ``X`` is defined on the *condor\_submit* command line
with

::

      condor_submit  X=3  sample.sub

This command line incorporates the submit command ``X = 3`` into the
submission before parsing the submit description file. For this
submission, the command line arguments of the submitted job become

::

        -n 3

If the job were instead submitted with the command line

::

      condor_submit  sample.sub

then the command line arguments of the submitted job become

::

        -n 1 -debug

Function Macros in the Submit Description File
----------------------------------------------

A set of predefined functions increase flexibility. Both submit
description files and configuration files are read using the same
parser, so these functions may be used in both submit description files
and configuration files.

Case is significant in the function’s name, so use the same letter case
as given in these definitions.

 ``$CHOICE(index, listname)`` or ``$CHOICE(index, item1, item2, …)``
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

    ::

          A = $ENV(HOME)

    binds ``A`` to the value of the ``HOME`` environment variable.

 ``$F[fpduwnxbqa](filename)``
    One or more of the lower case letters may be combined to form the
    function name and thus, its functionality. Each letter operates on
    the ``filename`` in its own way.

    -  ``f`` convert relative path to full path by prefixing the current
       working directory to it. This option works only in
       *condor\_submit* files.
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
 ``$INT(item-to-convert)`` or
``$INT(item-to-convert, format-specifier)``
    Expands, evaluates, and returns a string version of
    ``item-to-convert``. The ``format-specifier`` has the same syntax as
    a C language or Perl format specifier. If no ``format-specifier`` is
    specified, "%d" is used as the format specifier.
 ``$RANDOM_CHOICE(choice1, choice2, choice3, …)``
    A random choice of one of the parameters in the list of parameters
    is made. For example, if one of the integers 0-8 (inclusive) should
    be randomly chosen:

    ::

          $RANDOM_CHOICE(0,1,2,3,4,5,6,7,8)

 ``$RANDOM_INTEGER(min, max [, step])``
    A random integer within the range min and max, inclusive, is
    selected. The optional step parameter controls the stride within the
    range, and it defaults to the value 1. For example, to randomly
    chose an even integer in the range 0-8 (inclusive):

    ::

          $RANDOM_INTEGER(0, 8, 2)

 ``$REAL(item-to-convert)`` or
``$REAL(item-to-convert, format-specifier)``
    Expands, evaluates, and returns a string version of
    ``item-to-convert`` for a floating point type. The
    ``format-specifier`` is a C language or Perl format specifier. If no
    ``format-specifier`` is specified, "%16G" is used as a format
    specifier.
 ``$SUBSTR(name, start-index)`` or
``$SUBSTR(name, start-index, length)``
    Expands name and returns a substring of it. The first character of
    the string is at index 0. The first character of the substring is at
    index start-index. If the optional length is not specified, then the
    substring includes characters up to the end of the string. A
    negative value of start-index works back from the end of the string.
    A negative value of length eliminates use of characters from the end
    of the string. Here are some examples that all assume

    ::

          Name = abcdef

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

 Example 1

Generate a range of numerical values for a set of jobs, where values
other than those given by $(Process) are desired.

::

      MyIndex     = $(Process) + 1 
      initial_dir = run-$INT(MyIndex, %04d)

Assuming that there are three jobs queued, such that $(Process) becomes
0, 1, and 2, ``initial_dir`` will evaluate to the directories
``run-0001``, ``run-0002``, and ``run-0003``.

 Example 2

This variation on Example 1 generates a file name extension which is a
3-digit integer value.

::

      Values     = $(Process) * 10 
      Extension  = $INT(Values, %03d) 
      input      = X.$(Extension)

Assuming that there are four jobs queued, such that $(Process) becomes
0, 1, 2, and 3, ``Extension`` will evaluate to 000, 010, 020, and 030,
leading to files defined for **input** of ``X.000``, ``X.010``,
``X.020``, and ``X.030``.

 Example 3

This example uses both the file globbing of the **queue** command and a
macro function to specify a job input file that is within a subdirectory
on the submit host, but will be placed into a single, flat directory on
the execute host.

::

      arguments            = $Fnx(FILE) 
      transfer_input_files = $(FILE) 
      queue  FILE  MATCHING ( 
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
file are powerful and flexible. Using them effectively requires care,
and this section presents those details.

Both ``requirements`` and ``rank`` need to be specified as valid
HTCondor ClassAd expressions, however, default values are set by the
*condor\_submit* program if these are not defined in the submit
description file. From the *condor\_submit* manual page and the above
examples, you see that writing ClassAd expressions is intuitive,
especially if you are familiar with the programming language C. There
are some pretty nifty expressions you can write with ClassAds. A
complete description of ClassAds and their expressions can be found in
section \ `4.1 <HTCondorsClassAdMechanism.html#x48-3980004.1>`__ on
page \ `1277 <HTCondorsClassAdMechanism.html#x48-3980004.1>`__.

All of the commands in the submit description file are case insensitive,
except for the ClassAd attribute string values. ClassAd attribute names
are case insensitive, but ClassAd string values are case preserving.

Note that the comparison operators (<, >, <=, >=, and ==) compare
strings case insensitively. The special comparison operators =?= and =!=
compare strings case sensitively.

A **requirements** or **rank** command in the submit description file
may utilize attributes that appear in a machine or a job ClassAd. Within
the submit description file (for a job) the prefix MY. (on a ClassAd
attribute name) causes a reference to the job ClassAd attribute, and the
prefix TARGET. causes a reference to a potential machine or matched
machine ClassAd attribute.

The *condor\_status* command displays statistics about machines within
the pool. The **-l** option displays the machine ClassAd attributes for
all machines in the HTCondor pool. The job ClassAds, if there are jobs
in the queue, can be seen with the *condor\_q -l* command. This shows
all the defined attributes for current jobs in the queue.

A list of defined ClassAd attributes for job ClassAds is given in the
unnumbered Appendix on
page \ `2351 <JobClassAdAttributes.html#x170-1234000A.2>`__. A list of
defined ClassAd attributes for machine ClassAds is given in the
unnumbered Appendix on
page \ `2397 <MachineClassAdAttributes.html#x171-1235000A.3>`__.

Rank Expression Examples
~~~~~~~~~~~~~~~~~~~~~~~~

When considering the match between a job and a machine, rank is used to
choose a match from among all machines that satisfy the job’s
requirements and are available to the user, after accounting for the
user’s priority and the machine’s rank of the job. The rank expressions,
simple or complex, define a numerical value that expresses preferences.

The job’s ``Rank`` expression evaluates to one of three values. It can
be UNDEFINED, ERROR, or a floating point value. If ``Rank`` evaluates to
a floating point value, the best match will be the one with the largest,
positive value. If no ``Rank`` is given in the submit description file,
then HTCondor substitutes a default value of 0.0 when considering
machines to match. If the job’s ``Rank`` of a given machine evaluates to
UNDEFINED or ERROR, this same value of 0.0 is used. Therefore, the
machine is still considered for a match, but has no ranking above any
other.

A boolean expression evaluates to the numerical value of 1.0 if true,
and 0.0 if false.

The following ``Rank`` expressions provide examples to follow.

For a job that desires the machine with the most available memory:

::

       Rank = memory

For a job that prefers to run on a friend’s machine on Saturdays and
Sundays:

::

       Rank = ( (clockday == 0) || (clockday == 6) ) 
              && (machine == "friend.cs.wisc.edu")

For a job that prefers to run on one of three specific machines:

::

       Rank = (machine == "friend1.cs.wisc.edu") || 
              (machine == "friend2.cs.wisc.edu") || 
              (machine == "friend3.cs.wisc.edu")

For a job that wants the machine with the best floating point
performance (on Linpack benchmarks):

::

       Rank = kflops

This particular example highlights a difficulty with ``Rank`` expression
evaluation as currently defined. While all machines have floating point
processing ability, not all machines will have the ``kflops`` attribute
defined. For machines where this attribute is not defined, ``Rank`` will
evaluate to the value UNDEFINED, and HTCondor will use a default rank of
the machine of 0.0. The ``Rank`` attribute will only rank machines where
the attribute is defined. Therefore, the machine with the highest
floating point performance may not be the one given the highest rank.

So, it is wise when writing a ``Rank`` expression to check if the
expression’s evaluation will lead to the expected resulting ranking of
machines. This can be accomplished using the *condor\_status* command
with the *-constraint* argument. This allows the user to see a list of
machines that fit a constraint. To see which machines in the pool have
``kflops`` defined, use

::

    condor_status -constraint kflops

Alternatively, to see a list of machines where ``kflops`` is not
defined, use

::

    condor_status -constraint "kflops=?=undefined"

For a job that prefers specific machines in a specific order:

::

       Rank = ((machine == "friend1.cs.wisc.edu")*3) + 
              ((machine == "friend2.cs.wisc.edu")*2) + 
               (machine == "friend3.cs.wisc.edu")

If the machine being ranked is ``friend1.cs.wisc.edu``, then the
expression

::

       (machine == "friend1.cs.wisc.edu")

is true, and gives the value 1.0. The expressions

::

       (machine == "friend2.cs.wisc.edu")

and

::

       (machine == "friend3.cs.wisc.edu")

are false, and give the value 0.0. Therefore, ``Rank`` evaluates to the
value 3.0. In this way, machine ``friend1.cs.wisc.edu`` is ranked higher
than machine ``friend2.cs.wisc.edu``, machine ``friend2.cs.wisc.edu`` is
ranked higher than machine ``friend3.cs.wisc.edu``, and all three of
these machines are ranked higher than others.

Submitting Jobs Using a Shared File System
------------------------------------------

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

::

       Requirements = TARGET.UidDomain == "cs.wisc.edu" && \ 
                      TARGET.FileSystemDomain == "cs.wisc.edu"

WARNING: If there is no shared file system, or the HTCondor pool
administrator does not configure the ``FileSystemDomain`` setting
correctly (the default is that each machine in a pool is in its own file
system and UID domain), a user submits a job that cannot use remote
system calls (for example, a vanilla universe job), and the user does
not enable HTCondor’s File Transfer mechanism, the job will only run on
the machine from which it was submitted.

Submitting Jobs Without a Shared File System: HTCondor’s File Transfer Mechanism
--------------------------------------------------------------------------------

HTCondor works well without a shared file system. The HTCondor file
transfer mechanism permits the user to select which files are
transferred and under which circumstances. HTCondor can transfer any
files needed by a job from the machine where the job was submitted into
a remote scratch directory on the machine where the job is to be
executed. HTCondor executes the job and transfers output back to the
submitting machine. The user specifies which files and directories to
transfer, and at what point the output files should be copied back to
the submitting machine. This specification is done within the job’s
submit description file.

Specifying If and When to Transfer Files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To enable the file transfer mechanism, place two commands in the job’s
submit description file: **should\_transfer\_files** and
**when\_to\_transfer\_output**. By default, they will be:

::

      should_transfer_files = IF_NEEDED 
      when_to_transfer_output = ON_EXIT

Setting the **should\_transfer\_files** command explicitly enables or
disables the file transfer mechanism. The command takes on one of three
possible values:

#. YES: HTCondor transfers both the executable and the file defined by
   the **input** command from the machine where the job is submitted to
   the remote machine where the job is to be executed. The file defined
   by the **output** command as well as any files created by the
   execution of the job are transferred back to the machine where the
   job was submitted. When they are transferred and the directory
   location of the files is determined by the command
   **when\_to\_transfer\_output**.
#. IF\_NEEDED: HTCondor transfers files if the job is matched with and
   to be executed on a machine in a different ``FileSystemDomain`` than
   the one the submit machine belongs to, the same as if
   should\_transfer\_files = YES. If the job is matched with a machine
   in the local ``FileSystemDomain``, HTCondor will not transfer files
   and relies on the shared file system.
#. NO: HTCondor’s file transfer mechanism is disabled.

The **when\_to\_transfer\_output** command tells HTCondor when output
files are to be transferred back to the submit machine. The command
takes on one of two possible values:

#. ON\_EXIT: HTCondor transfers the file defined by the **output**
   command, as well as any other files in the remote scratch directory
   created by the job, back to the submit machine only when the job
   exits on its own.
#. ON\_EXIT\_OR\_EVICT: HTCondor behaves the same as described for the
   value ON\_EXIT when the job exits on its own. However, if, and each
   time the job is evicted from a machine, files are transferred back at
   eviction time. The files that are transferred back at eviction time
   may include intermediate files that are not part of the final output
   of the job. When **transfer\_output\_files** is specified, its list
   governs which are transferred back at eviction time. Before the job
   starts running again, all of the files that were stored when the job
   was last evicted are copied to the job’s new remote scratch
   directory.

   The purpose of saving files at eviction time is to allow the job to
   resume from where it left off. This is similar to using the
   checkpoint feature of the standard universe, but just specifying
   ON\_EXIT\_OR\_EVICT is not enough to make a job capable of producing
   or utilizing checkpoints. The job must be designed to save and
   restore its state using the files that are saved at eviction time.

   The files that are transferred back at eviction time are not stored
   in the location where the job’s final output will be written when the
   job exits. HTCondor manages these files automatically, so usually the
   only reason for a user to worry about them is to make sure that there
   is enough space to store them. The files are stored on the submit
   machine in a temporary directory within the directory defined by the
   configuration variable ``SPOOL``. The directory is named using the
   ``ClusterId`` and ``ProcId`` job ClassAd attributes. The directory
   name takes the form:

   ::

          <X mod 10000>/<Y mod 10000>/cluster<X>.proc<Y>.subproc0

   where <X> is the value of ``ClusterId``, and <Y> is the value of
   ``ProcId``. As an example, if job 735.0 is evicted, it will produce
   the directory

   ::

          $(SPOOL)/735/0/cluster735.proc0.subproc0

The default values for these two submit commands make sense as used
together. If only **should\_transfer\_files** is set, and set to the
value ``NO``, then no output files will be transferred, and the value of
**when\_to\_transfer\_output** is irrelevant. If only
**when\_to\_transfer\_output** is set, and set to the value
``ON_EXIT_OR_EVICT``, then the default value for an unspecified
**should\_transfer\_files** will be ``YES``.

Note that the combination of

::

      should_transfer_files = IF_NEEDED 
      when_to_transfer_output = ON_EXIT_OR_EVICT

would produce undefined file access semantics. Therefore, this
combination is prohibited by *condor\_submit*.

Specifying What Files to Transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If the file transfer mechanism is enabled, HTCondor will transfer the
following files before the job is run on a remote machine.

#. the executable, as defined with the **executable** command
#. the input, as defined with the **input** command
#. any jar files, for the **java** universe, as defined with the
   **jar\_files** command

If the job requires other input files, the submit description file
should utilize the **transfer\_input\_files** command. This
comma-separated list specifies any other files or directories that
HTCondor is to transfer to the remote scratch directory, to set up the
execution environment for the job before it is run. These files are
placed in the same directory as the job’s executable. For example:

::

      should_transfer_files = YES 
      when_to_transfer_output = ON_EXIT 
      transfer_input_files = file1,file2

This example explicitly enables the file transfer mechanism, and it
transfers the executable, the file specified by the **input** command,
any jar files specified by the **jar\_files** command, and files
``file1`` and ``file2``.

If the file transfer mechanism is enabled, HTCondor will transfer the
following files from the execute machine back to the submit machine
after the job exits.

#. the output file, as defined with the **output** command
#. the error file, as defined with the **error** command
#. any files created by the job in the remote scratch directory; this
   only occurs for jobs other than **grid** universe, and for HTCondor-C
   **grid** universe jobs; directories created by the job within the
   remote scratch directory are ignored for this automatic detection of
   files to be transferred.

A path given for **output** and **error** commands represents a path on
the submit machine. If no path is specified, the directory specified
with **initialdir** is used, and if that is not specified, the directory
from which the job was submitted is used. At the time the job is
submitted, zero-length files are created on the submit machine, at the
given path for the files defined by the **output** and **error**
commands. This permits job submission failure, if these files cannot be
written by HTCondor.

To restrict the output files or permit entire directory contents to be
transferred, specify the exact list with **transfer\_output\_files**.
Delimit the list of file names, directory names, or paths with commas.
When this list is defined, and any of the files or directories do not
exist as the job exits, HTCondor considers this an error, and places the
job on hold. Setting **transfer\_output\_files** to the empty string
("") means no files are to be transferred. When this list is defined,
automatic detection of output files created by the job is disabled.
Paths specified in this list refer to locations on the execute machine.
The naming and placement of files and directories relies on the term
base name. By example, the path ``a/b/c`` has the base name ``c``. It is
the file name or directory name with all directories leading up to that
name stripped off. On the submit machine, the transferred files or
directories are named using only the base name. Therefore, each output
file or directory must have a different name, even if they originate
from different paths.

For **grid** universe jobs other than than HTCondor-C grid jobs, files
to be transferred (other than standard output and standard error) must
be specified using **transfer\_output\_files** in the submit description
file, because automatic detection of new files created by the job does
not take place.

Here are examples to promote understanding of what files and directories
are transferred, and how they are named after transfer. Assume that the
job produces the following structure within the remote scratch
directory:

::

          o1 
          o2 
          d1 (directory) 
              o3 
              o4

If the submit description file sets

::

       transfer_output_files = o1,o2,d1

then transferred back to the submit machine will be

::

          o1 
          o2 
          d1 (directory) 
              o3 
              o4

Note that the directory ``d1`` and all its contents are specified, and
therefore transferred. If the directory ``d1`` is not created by the job
before exit, then the job is placed on hold. If the directory ``d1`` is
created by the job before exit, but is empty, this is not an error.

If, instead, the submit description file sets

::

       transfer_output_files = o1,o2,d1/o3

then transferred back to the submit machine will be

::

          o1 
          o2 
          o3

Note that only the base name is used in the naming and placement of the
file specified with ``d1/o3``.

File Paths for File Transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The file transfer mechanism specifies file names and/or paths on both
the file system of the submit machine and on the file system of the
execute machine. Care must be taken to know which machine, submit or
execute, is utilizing the file name and/or path.

Files in the **transfer\_input\_files** command are specified as they
are accessed on the submit machine. The job, as it executes, accesses
files as they are found on the execute machine.

There are three ways to specify files and paths for
**transfer\_input\_files**:

#. Relative to the current working directory as the job is submitted, if
   the submit command **initialdir** is not specified.
#. Relative to the initial directory, if the submit command
   **initialdir** is specified.
#. Absolute.

Before executing the program, HTCondor copies the executable, an input
file as specified by the submit command **input**, along with any input
files specified by **transfer\_input\_files**. All these files are
placed into a remote scratch directory on the execute machine, in which
the program runs. Therefore, the executing program must access input
files relative to its working directory. Because all files and
directories listed for transfer are placed into a single, flat
directory, inputs must be uniquely named to avoid collision when
transferred. A collision causes the last file in the list to overwrite
the earlier one.

Both relative and absolute paths may be used in
**transfer\_output\_files**. Relative paths are relative to the job’s
remote scratch directory on the execute machine. When the files and
directories are copied back to the submit machine, they are placed in
the job’s initial working directory as the base name of the original
path. An alternate name or path may be specified by using
**transfer\_output\_remaps**.

A job may create files outside the remote scratch directory but within
the file system of the execute machine, in a directory such as ``/tmp``,
if this directory is guaranteed to exist and be accessible on all
possible execute machines. However, HTCondor will not automatically
transfer such files back after execution completes, nor will it clean up
these files.

Here are several examples to illustrate the use of file transfer. The
program executable is called *my\_program*, and it uses three
command-line arguments as it executes: two input file names and an
output file name. The program executable and the submit description file
for this job are located in directory ``/scratch/test``.

Here is the directory tree as it exists on the submit machine, for all
the examples:

::

    /scratch/test (directory) 
          my_program.condor (the submit description file) 
          my_program (the executable) 
          files (directory) 
              logs2 (directory) 
              in1 (file) 
              in2 (file) 
          logs (directory)

 Example 1
    This first example explicitly transfers input files. These input
    files to be transferred are specified relative to the directory
    where the job is submitted. An output file specified in the
    **arguments** command, ``out1``, is created when the job is
    executed. It will be transferred back into the directory
    ``/scratch/test``.

    ::

        # file name:  my_program.condor 
        # HTCondor submit description file for my_program 
        Executable      = my_program 
        Universe        = vanilla 
        Error           = logs/err.$(cluster) 
        Output          = logs/out.$(cluster) 
        Log             = logs/log.$(cluster) 
         
        should_transfer_files = YES 
        when_to_transfer_output = ON_EXIT 
        transfer_input_files = files/in1,files/in2 
         
        Arguments       = in1 in2 out1 
        Queue

    The log file is written on the submit machine, and is not involved
    with the file transfer mechanism.

 Example 2
    This second example is identical to Example 1, except that absolute
    paths to the input files are specified, instead of relative paths to
    the input files.

    ::

        # file name:  my_program.condor 
        # HTCondor submit description file for my_program 
        Executable      = my_program 
        Universe        = vanilla 
        Error           = logs/err.$(cluster) 
        Output          = logs/out.$(cluster) 
        Log             = logs/log.$(cluster) 
         
        should_transfer_files = YES 
        when_to_transfer_output = ON_EXIT 
        transfer_input_files = /scratch/test/files/in1,/scratch/test/files/in2 
         
        Arguments       = in1 in2 out1 
        Queue

 Example 3
    This third example illustrates the use of the submit command
    **initialdir**, and its effect on the paths used for the various
    files. The expected location of the executable is not affected by
    the **initialdir** command. All other files (specified by **input**,
    **output**, **error**, **transfer\_input\_files**, as well as files
    modified or created by the job and automatically transferred back)
    are located relative to the specified **initialdir**. Therefore, the
    output file, ``out1``, will be placed in the files directory. Note
    that the ``logs2`` directory exists to make this example work
    correctly.

    ::

        # file name:  my_program.condor 
        # HTCondor submit description file for my_program 
        Executable      = my_program 
        Universe        = vanilla 
        Error           = logs2/err.$(cluster) 
        Output          = logs2/out.$(cluster) 
        Log             = logs2/log.$(cluster) 
         
        initialdir      = files 
         
        should_transfer_files = YES 
        when_to_transfer_output = ON_EXIT 
        transfer_input_files = in1,in2 
         
        Arguments       = in1 in2 out1 
        Queue

 Example 4 – Illustrates an Error
    This example illustrates a job that will fail. The files specified
    using the **transfer\_input\_files** command work correctly (see
    Example 1). However, relative paths to files in the **arguments**
    command cause the executing program to fail. The file system on the
    submission side may utilize relative paths to files, however those
    files are placed into the single, flat, remote scratch directory on
    the execute machine.

    ::

        # file name:  my_program.condor 
        # HTCondor submit description file for my_program 
        Executable      = my_program 
        Universe        = vanilla 
        Error           = logs/err.$(cluster) 
        Output          = logs/out.$(cluster) 
        Log             = logs/log.$(cluster) 
         
        should_transfer_files = YES 
        when_to_transfer_output = ON_EXIT 
        transfer_input_files = files/in1,files/in2 
         
        Arguments       = files/in1 files/in2 files/out1 
        Queue

    This example fails with the following error:

    ::

        err: files/out1: No such file or directory.

 Example 5 – Illustrates an Error
    As with Example 4, this example illustrates a job that will fail.
    The executing program’s use of absolute paths cannot work.

    ::

        # file name:  my_program.condor 
        # HTCondor submit description file for my_program 
        Executable      = my_program 
        Universe        = vanilla 
        Error           = logs/err.$(cluster) 
        Output          = logs/out.$(cluster) 
        Log             = logs/log.$(cluster) 
         
        should_transfer_files = YES 
        when_to_transfer_output = ON_EXIT 
        transfer_input_files = /scratch/test/files/in1, /scratch/test/files/in2 
         
        Arguments = /scratch/test/files/in1 /scratch/test/files/in2 /scratch/test/files/out1 
        Queue

    The job fails with the following error:

    ::

        err: /scratch/test/files/out1: No such file or directory.

 Example 6
    This example illustrates a case where the executing program creates
    an output file in a directory other than within the remote scratch
    directory that the program executes within. The file creation may or
    may not cause an error, depending on the existence and permissions
    of the directories on the remote file system.

    The output file ``/tmp/out1`` is transferred back to the job’s
    initial working directory as ``/scratch/test/out1``.

    ::

        # file name:  my_program.condor 
        # HTCondor submit description file for my_program 
        Executable      = my_program 
        Universe        = vanilla 
        Error           = logs/err.$(cluster) 
        Output          = logs/out.$(cluster) 
        Log             = logs/log.$(cluster) 
         
        should_transfer_files = YES 
        when_to_transfer_output = ON_EXIT 
        transfer_input_files = files/in1,files/in2 
        transfer_output_files = /tmp/out1 
         
        Arguments       = in1 in2 /tmp/out1 
        Queue

Public Input Files
~~~~~~~~~~~~~~~~~~

There are some cases where HTCondor’s file transfer mechanism is
inefficient. For jobs that need to run a large number of times, the
input files need to get transferred for every job, even if those files
are identical. This wastes resources on both the submit machine and the
network, slowing overall job execution time.

Public input files allow a user to specify files to be transferred over
a publicly-available HTTP web service. A system administrator can then
configure caching proxies, load balancers, and other tools to
dramatically improve performance. Public input files are not available
by default, and need to be explicitly enabled by a system administrator.

To specify files that use this feature, the submit file should include a
**public\_input\_files** command. This comma-separated list specifies
files which HTCondor will transfer using the HTTP mechanism. For
example:

::

      should_transfer_files = YES 
      when_to_transfer_output = ON_EXIT 
      transfer_input_files = file1,file2 
      public_input_files = public_data1,public_data2

Similar to the regular **transfer\_input\_files**, the files specified
in **public\_input\_files** can be relative to the submit directory, or
absolute paths. You can also specify an **initialDir**, and
*condor\_submit* will look for files relative to that directory. The
files must be world-readable on the file system (files with permissions
set to 0644, directories with permissions set to 0755).

Lastly, all files transferred using this method will be publicly
available and world-readable, so this feature should not be used for any
sensitive data.

Behavior for Error Cases
~~~~~~~~~~~~~~~~~~~~~~~~

This section describes HTCondor’s behavior for some error cases in
dealing with the transfer of files.

 Disk Full on Execute Machine
    When transferring any files from the submit machine to the remote
    scratch directory, if the disk is full on the execute machine, then
    the job is place on hold.
 Error Creating Zero-Length Files on Submit Machine
    As a job is submitted, HTCondor creates zero-length files as
    placeholders on the submit machine for the files defined by
    **output** and **error**. If these files cannot be created, then job
    submission fails.

    This job submission failure avoids having the job run to completion,
    only to be unable to transfer the job’s output due to permission
    errors.

 Error When Transferring Files from Execute Machine to Submit Machine
    When a job exits, or potentially when a job is evicted from an
    execute machine, one or more files may be transferred from the
    execute machine back to the machine on which the job was submitted.

    During transfer, if any of the following three similar types of
    errors occur, the job is put on hold as the error occurs.

    #. If the file cannot be opened on the submit machine, for example
       because the system is out of inodes.
    #. If the file cannot be written on the submit machine, for example
       because the permissions do not permit it.
    #. If the write of the file on the submit machine fails, for example
       because the system is out of disk space.

File Transfer Using a URL
~~~~~~~~~~~~~~~~~~~~~~~~~

Instead of file transfer that goes only between the submit machine and
the execute machine, HTCondor has the ability to transfer files from a
location specified by a URL for a job’s input file, or from the execute
machine to a location specified by a URL for a job’s output file(s).
This capability requires administrative set up, as described in
section \ `3.14.2 <SettingUpforSpecialEnvironments.html#x42-3480003.14.2>`__.

The transfer of an input file is restricted to vanilla and vm universe
jobs only. HTCondor’s file transfer mechanism must be enabled.
Therefore, the submit description file for the job will define both
**should\_transfer\_files** and **when\_to\_transfer\_output**. In
addition, the URL for any files specified with a URL are given in the
**transfer\_input\_files** command. An example portion of the submit
description file for a job that has a single file specified with a URL:

::

    should_transfer_files = YES 
    when_to_transfer_output = ON_EXIT 
    transfer_input_files = http://www.full.url/path/to/filename

The destination file is given by the file name within the URL.

For the transfer of the entire contents of the output sandbox, which are
all files that the job creates or modifies, HTCondor’s file transfer
mechanism must be enabled. In this sample portion of the submit
description file, the first two commands explicitly enable file
transfer, and the added **output\_destination** command provides both
the protocol to be used and the destination of the transfer.

::

    should_transfer_files = YES 
    when_to_transfer_output = ON_EXIT 
    output_destination = urltype://path/to/destination/directory

Note that with this feature, no files are transferred back to the submit
machine. This does not interfere with the streaming of output.

If only a subset of the output sandbox should be transferred, the subset
is specified by further adding a submit command of the form:

::

    transfer_output_files = file1, file2

Requirements and Rank for File Transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``requirements`` expression for a job must depend on the
should\_transfer\_files command. The job must specify the correct logic
to ensure that the job is matched with a resource that meets the file
transfer needs. If no ``requirements`` expression is in the submit
description file, or if the expression specified does not refer to the
attributes listed below, *condor\_submit* adds an appropriate clause to
the ``requirements`` expression for the job. *condor\_submit* appends
these clauses with a logical AND, &&, to ensure that the proper
conditions are met. Here are the default clauses corresponding to the
different values of should\_transfer\_files:

#. should\_transfer\_files = YES

   results in the addition of the clause (HasFileTransfer). If the job
   is always going to transfer files, it is required to match with a
   machine that has the capability to transfer files.

#. should\_transfer\_files = NO

   results in the addition of
   (TARGET.FileSystemDomain == MY.FileSystemDomain). In addition,
   HTCondor automatically adds the ``FileSystemDomain`` attribute to the
   job ClassAd, with whatever string is defined for the *condor\_schedd*
   to which the job is submitted. If the job is not using the file
   transfer mechanism, HTCondor assumes it will need a shared file
   system, and therefore, a machine in the same ``FileSystemDomain`` as
   the submit machine.

#. should\_transfer\_files = IF\_NEEDED results in the addition of

   ::

         (HasFileTransfer || (TARGET.FileSystemDomain == MY.FileSystemDomain))

   If HTCondor will optionally transfer files, it must require that the
   machine is either capable of transferring files or in the same file
   system domain.

To ensure that the job is matched to a machine with enough local disk
space to hold all the transferred files, HTCondor automatically adds the
``DiskUsage`` job attribute. This attribute includes the total size of
the job’s executable and all input files to be transferred. HTCondor
then adds an additional clause to the ``Requirements`` expression that
states that the remote machine must have at least enough available disk
space to hold all these files:

::

      && (Disk >= DiskUsage)

If should\_transfer\_files = IF\_NEEDED and the job prefers to run on a
machine in the local file system domain over transferring files, but is
still willing to allow the job to run remotely and transfer files, the
``Rank`` expression works well. Use:

::

    rank = (TARGET.FileSystemDomain == MY.FileSystemDomain)

The ``Rank`` expression is a floating point value, so if other items are
considered in ranking the possible machines this job may run on, add the
items:

::

    Rank = kflops + (TARGET.FileSystemDomain == MY.FileSystemDomain)

The value of ``kflops`` can vary widely among machines, so this ``Rank``
expression will likely not do as it intends. To place emphasis on the
job running in the same file system domain, but still consider floating
point speed among the machines in the file system domain, weight the
part of the expression that is matching the file system domains. For
example:

::

    Rank = kflops + (10000 * (TARGET.FileSystemDomain == MY.FileSystemDomain))

Environment Variables
---------------------

The environment under which a job executes often contains information
that is potentially useful to the job. HTCondor allows a user to both
set and reference environment variables for a job or job cluster.

Within a submit description file, the user may define environment
variables for the job’s environment by using the **environment**
command. See within the *condor\_submit* manual page at
section \ `12 <Condorsubmit.html#x149-108400012>`__ for more details
about this command.

The submitter’s entire environment can be copied into the job ClassAd
for the job at job submission. The **getenv** command within the submit
description file does this, as described at
section \ `12 <Condorsubmit.html#x149-108400012>`__.

If the environment is set with the **environment** command and
**getenv** is also set to true, values specified with **environment**
override values in the submitter’s environment, regardless of the order
of the **environment** and **getenv** commands.

Commands within the submit description file may reference the
environment variables of the submitter as a job is submitted. Submit
description file commands use $ENV(EnvironmentVariableName) to reference
the value of an environment variable.

HTCondor sets several additional environment variables for each
executing job that may be useful for the job to reference.

-  ``_CONDOR_SCRATCH_DIR`` gives the directory where the job may place
   temporary data files. This directory is unique for every job that is
   run, and its contents are deleted by HTCondor when the job stops
   running on a machine, no matter how the job completes.
-  ``_CONDOR_SLOT`` gives the name of the slot (for SMP machines), on
   which the job is run. On machines with only a single slot, the value
   of this variable will be 1, just like the ``SlotID`` attribute in the
   machine’s ClassAd. This setting is available in all universes. See
   section \ `3.7.1 <PolicyConfigurationforExecuteHostsandforSubmitHosts.html#x35-2530003.7.1>`__
   for more details about SMP machines and their configuration.
-  ``X509_USER_PROXY`` gives the full path to the X.509 user proxy file
   if one is associated with the job. Typically, a user will specify
   **x509userproxy** in the submit description file. This setting is
   currently available in the local, java, and vanilla universes.
-  ``_CONDOR_JOB_AD`` is the path to a file in the job’s scratch
   directory which contains the job ad for the currently running job.
   The job ad is current as of the start of the job, but is not updated
   during the running of the job. The job may read attributes and their
   values out of this file as it runs, but any changes will not be acted
   on in any way by HTCondor. The format is the same as the output of
   the *condor\_q* **-l** command. This environment variable may be
   particularly useful in a USER\_JOB\_WRAPPER.
-  ``_CONDOR_MACHINE_AD`` is the path to a file in the job’s scratch
   directory which contains the machine ad for the slot the currently
   running job is using. The machine ad is current as of the start of
   the job, but is not updated during the running of the job. The format
   is the same as the output of the *condor\_status* **-l** command.
-  ``_CONDOR_JOB_IWD`` is the path to the initial working directory the
   job was born with.
-  ``_CONDOR_WRAPPER_ERROR_FILE`` is only set when the administrator has
   installed a USER\_JOB\_WRAPPER. If this file exists, HTCondor assumes
   that the job wrapper has failed and copies the contents of the file
   to the StarterLog for the administrator to debug the problem.
-  ``CONDOR_IDS`` overrides the value of configuration variable
   ``CONDOR_IDS``, when set in the environment.
-  ``CONDOR_ID`` is set for scheduler universe jobs to be the same as
   the ``ClusterId`` attribute.

Heterogeneous Submit: Execution on Differing Architectures
----------------------------------------------------------

If executables are available for the different platforms of machines in
the HTCondor pool, HTCondor can be allowed the choice of a larger number
of machines when allocating a machine for a job. Modifications to the
submit description file allow this choice of platforms.

A simplified example is a cross submission. An executable is available
for one platform, but the submission is done from a different platform.
Given the correct executable, the ``requirements`` command in the submit
description file specifies the target architecture. For example, an
executable compiled for a 32-bit Intel processor running Windows Vista,
submitted from an Intel architecture running Linux would add the
``requirement``

::

      requirements = Arch == "INTEL" && OpSys == "WINDOWS"

Without this ``requirement``, *condor\_submit* will assume that the
program is to be executed on a machine with the same platform as the
machine where the job is submitted.

Cross submission works for all universes except ``scheduler`` and
``local``. See
section \ `5.3.11 <TheGridUniverse.html#x56-4870005.3.11>`__ for how
matchmaking works in the ``grid`` universe. The burden is on the user to
both obtain and specify the correct executable for the target
architecture. To list the architecture and operating systems of the
machines in a pool, run *condor\_status*.

Vanilla Universe Example for Execution on Differing Architectures
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A more complex example of a heterogeneous submission occurs when a job
may be executed on many different architectures to gain full use of a
diverse architecture and operating system pool. If the executables are
available for the different architectures, then a modification to the
submit description file will allow HTCondor to choose an executable
after an available machine is chosen.

A special-purpose Machine Ad substitution macro can be used in string
attributes in the submit description file. The macro has the form

::

      $$(MachineAdAttribute)

The $$() informs HTCondor to substitute the requested
``MachineAdAttribute`` from the machine where the job will be executed.

An example of the heterogeneous job submission has executables available
for two platforms: RHEL 3 on both 32-bit and 64-bit Intel processors.
This example uses *povray* to render images using a popular free
rendering engine.

The substitution macro chooses a specific executable after a platform
for running the job is chosen. These executables must therefore be named
based on the machine attributes that describe a platform. The
executables named

::

      povray.LINUX.INTEL 
      povray.LINUX.X86_64

will work correctly for the macro

::

      povray.$$(OpSys).$$(Arch)

The executables or links to executables with this name are placed into
the initial working directory so that they may be found by HTCondor. A
submit description file that queues three jobs for this example:

::

      #################### 
      # 
      # Example of heterogeneous submission 
      # 
      #################### 
     
      universe     = vanilla 
      Executable   = povray.$$(OpSys).$$(Arch) 
      Log          = povray.log 
      Output       = povray.out.$(Process) 
      Error        = povray.err.$(Process) 
     
      Requirements = (Arch == "INTEL" && OpSys == "LINUX") || \ 
                     (Arch == "X86_64" && OpSys =="LINUX") 
     
      Arguments    = +W1024 +H768 +Iimage1.pov 
      Queue 
     
      Arguments    = +W1024 +H768 +Iimage2.pov 
      Queue 
     
      Arguments    = +W1024 +H768 +Iimage3.pov 
      Queue

These jobs are submitted to the vanilla universe to assure that once a
job is started on a specific platform, it will finish running on that
platform. Switching platforms in the middle of job execution cannot work
correctly.

There are two common errors made with the substitution macro. The first
is the use of a non-existent ``MachineAdAttribute``. If the specified
``MachineAdAttribute`` does not exist in the machine’s ClassAd, then
HTCondor will place the job in the held state until the problem is
resolved.

The second common error occurs due to an incomplete job set up. For
example, the submit description file given above specifies three
available executables. If one is missing, HTCondor reports back that an
executable is missing when it happens to match the job with a resource
that requires the missing binary.

Standard Universe Example for Execution on Differing Architectures
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Jobs submitted to the standard universe may produce checkpoints. A
checkpoint can then be used to start up and continue execution of a
partially completed job. For a partially completed job, the checkpoint
and the job are specific to a platform. If migrated to a different
machine, correct execution requires that the platform must remain the
same.

In previous versions of HTCondor, the author of the heterogeneous
submission file would need to write extra policy expressions in the
``requirements`` expression to force HTCondor to choose the same type of
platform when continuing a checkpointed job. However, since it is needed
in the common case, this additional policy is now automatically added to
the ``requirements`` expression. The additional expression is added
provided the user does not use ``CkptArch`` in the ``requirements``
expression. HTCondor will remain backward compatible for those users who
have explicitly specified ``CkptRequirements``–implying use of
``CkptArch``, in their ``requirements`` expression.

The expression added when the attribute ``CkptArch`` is not specified
will default to

::

      # Added by HTCondor 
      CkptRequirements = ((CkptArch == Arch) || (CkptArch =?= UNDEFINED)) && \ 
                          ((CkptOpSys == OpSys) || (CkptOpSys =?= UNDEFINED)) 
     
      Requirements = (<user specified policy>) && $(CkptRequirements)

The behavior of the ``CkptRequirements`` expressions and its addition to
``requirements`` is as follows. The ``CkptRequirements`` expression
guarantees correct operation in the two possible cases for a job. In the
first case, the job has not produced a checkpoint. The ClassAd
attributes ``CkptArch`` and ``CkptOpSys`` will be undefined, and
therefore the meta operator (=?=) evaluates to true. In the second case,
the job has produced a checkpoint. The Machine ClassAd is restricted to
require further execution only on a machine of the same platform. The
attributes ``CkptArch`` and ``CkptOpSys`` will be defined, ensuring that
the platform chosen for further execution will be the same as the one
used just before the checkpoint.

Note that this restriction of platforms also applies to platforms where
the executables are binary compatible.

The complete submit description file for this example:

::

      #################### 
      # 
      # Example of heterogeneous submission 
      # 
      #################### 
     
      universe     = standard 
      Executable   = povray.$$(OpSys).$$(Arch) 
      Log          = povray.log 
      Output       = povray.out.$(Process) 
      Error        = povray.err.$(Process) 
     
      # HTCondor automatically adds the correct expressions to insure that the 
      # checkpointed jobs will restart on the correct platform types. 
      Requirements = ( (Arch == "INTEL" && OpSys == "LINUX") || \ 
                     (Arch == "X86_64" && OpSys == "LINUX") ) 
     
      Arguments    = +W1024 +H768 +Iimage1.pov 
      Queue 
     
      Arguments    = +W1024 +H768 +Iimage2.pov 
      Queue 
     
      Arguments    = +W1024 +H768 +Iimage3.pov 
      Queue

Vanilla Universe Example for Execution on Differing Operating Systems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The addition of several related OpSys attributes assists in selection of
specific operating systems and versions in heterogeneous pools.

::

      #################### 
      # 
      # Example targeting only RedHat platforms 
      # 
      #################### 
     
      universe     = vanilla 
      Executable   = /bin/date 
      Log          = distro.log 
      Output       = distro.out 
      Error        = distro.err 
     
      Requirements = (OpSysName == "RedHat") 
     
      Queue

::

      #################### 
      # 
      # Example targeting RedHat 6 platforms in a heterogeneous Linux pool 
      # 
      #################### 
     
      universe     = vanilla 
      Executable   = /bin/date 
      Log          = distro.log 
      Output       = distro.out 
      Error        = distro.err 
     
      Requirements = ( OpSysName == "RedHat" && OpSysMajorVer == 6) 
     
      Queue

Here is a more compact way to specify a RedHat 6 platform.

::

      #################### 
      # 
      # Example targeting RedHat 6 platforms in a heterogeneous Linux pool 
      # 
      #################### 
     
      universe     = vanilla 
      Executable   = /bin/date 
      Log          = distro.log 
      Output       = distro.out 
      Error        = distro.err 
     
      Requirements = ( OpSysAndVer == "RedHat6") 
     
      Queue

Jobs That Require GPUs
----------------------

A job that needs GPUs to run identifies the number of GPUs needed in the
submit description file by adding the submit command

::

      request_GPUs = <n>

where ``<n>`` is replaced by the integer quantity of GPUs required for
the job. For example, a job that needs 1 GPU uses

::

      request_GPUs = 1

Because there are different capabilities among GPUs, the job might need
to further qualify which GPU of available ones is required. Do this by
specifying or adding a clause to an existing **Requirements** submit
command. As an example, assume that the job needs a speed and capacity
of a CUDA GPU that meets or exceeds the value 1.2. In the submit
description file, place

::

      request_GPUs = 1 
      requirements = (CUDACapability >= 1.2) && $(requirements:True)

Access to GPU resources by an HTCondor job needs special configuration
of the machines that offer GPUs. Details of how to set up the
configuration are in
section \ `3.7.1 <PolicyConfigurationforExecuteHostsandforSubmitHosts.html#x35-2580003.7.1>`__.

Interactive Jobs
----------------

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
within Condor’s scheduling and priority system.

Neither the submit nor the execute host for interactive jobs may be on
Windows platforms.

The current working directory of the shell will be the initial working
directory of the running job. The shell type will be the default for the
user that submits the job. At the shell prompt, X11 forwarding is
enabled.

Each interactive job will have a job ClassAd attribute of

::

      InteractiveJob = True

Submission of an interactive job specifies the option **-interactive**
on the *condor\_submit* command line.

A submit description file may be specified for this interactive job.
Within this submit description file, a specification of these 5 commands
will be either ignored or altered:

#. **executable**
#. **transfer\_executable**
#. **arguments**
#. **universe**. The interactive job is a vanilla universe job.
#. **queue** **<n>**. In this case the value of **<n>** is ignored;
   exactly one interactive job is queued.

The submit description file may specify anything else needed for the
interactive job, such as files to transfer.

If no submit description file is specified for the job, a default one is
utilized as identified by the value of the configuration variable
``INTERACTIVE_SUBMIT_FILE`` .

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
   platforms required reside within Condor’s purview as execute hosts.

      
