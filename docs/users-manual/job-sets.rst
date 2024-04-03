.. _job_sets:

Job Sets
================

:index:`Job Sets`

.. warning::
   Job sets are an experimental feature that is currently disabled by default.
   To check to see if it is enabled, run `$ condor_config_val USE_JOBSETS`
   which will return true or false.  If false, you will need your administrator
   to set the config knob :macro:`USE_JOBSETS` to true for this to work.

Multiple jobs that share
a common set of input files and/or arguments and/or index values, etc.,
can be organized and submitted as a **job set**
:index:`job set<single: job_set; definition>`).
For example, if you have 10 sets of measurements
that you are using as input to two different models,
you might consider submitting a job set
containing two different modeling jobs
that use the same set of input measurement data.


:index:`Job Sets<single: Job Sets; example submit>`

Submitting a job set
--------------------

Submitting a job set involves creating a job set description file
and then using the :doc:`/man-pages/htcondor` command-line tool
to submit the jobs described in the job set description file
to the job queue.
For example, if your jobs are described in a file named *my-jobs.set*:

.. code-block:: condor-submit
   :caption: my-jobs.set file

   # Jobset example from https://htcondor.readthedocs.io/en/latest/users-manual/job-sets.html
   name = MyJobSet

   iterator = table my_var {
       input_A.txt
       input_B.txt
   }
   
   job {
       executable     = /bin/echo
       arguments = $(my_var)

       Request_cpus   = 1
       Request_memory = 1024M
       Request_disk   = 1024M

       output         = out
       error          = err
       log            = log
   }


Then you can submit this set using the following command from the shell:

.. code-block:: console
   :caption: Command line to submit a simple job set

   $ htcondor jobset submit my-jobs.set

A **job set description file** must contain:

1. A **name**,
2. An **iterator**, and
3. At least one **job**.

The **name** of a job set is used to identify the set.
Job set names are used to check the status of sets or to remove sets.

The **iterator** of a job set is used to describe the shared values
and the values' associated variable names
that are used by the jobs in the job set.
Multiple iterator types are planned to be supported by HTCondor.
As of HTCondor 9.4.0, only the *table* iterator type is available.

The *table* iterator type works similar
to the ``queue <list of varnames> from <file name or list of items>`` syntax
used by :tool:`condor_submit` description files.
A table contains comma-separated columns (one per named variable)
and line-separated rows.
The table data can either be stored in a separate file
and referenced by file name,
or it can be stored inside the job set description file itself
inside curly brackets (``{ ... }``, see example below).

The job set description file syntax for a *table iterator* is:

.. code-block:: condor-submit

   iterator = table <list of variable names> <table file name>

   or

   iterator = table <list of variable names> {
       <list of items>
   }

Suppose you have four *input files*,
and each input file is associated with two parameters, *foo* and *bar*,
needed by your jobs.
An example table in this case could be:

.. code-block:: condor-submit

    input_A.txt,0,0
    input_B.txt,0,1
    input_C.txt,1,0
    input_D.txt,1,1

If this table is stored in *input_description.txt*,
your iterator would be:

.. code-block:: condor-submit

    iterator = table inputfile,foo,bar input_description.txt

Or you could put this table directly inside in the job set description file:

.. code-block:: condor-submit

    iterator = table inputfile,foo,bar {
        input_A.txt,0,0
        input_B.txt,0,1
        input_C.txt,1,0
        input_D.txt,1,1
    }

Each **job** in a job set is a HTCondor job
and is described using the :tool:`condor_submit` submit description syntax.
A job description can reference one or more
of the variables described by the job set iterator.
Furthermore, each job description in a job set
can have its variables mapped
(e.g. ``foo=bar`` will replace ``$(foo)`` with ``$(bar)``).
A job description can either be stored in a separate file
and referenced by file name,
or it can be stored inside the job set description file itself
inside curly brackets (``{ ... }``, see example below). 

The job set description file syntax for a *job* is:

.. code-block:: condor-submit

   job [<list of mapped variable names>] <submit file name>

   or

   job [<list of mapped variable names>] {
       <submit file description>
   }

Suppose you have two jobs
that you want to have use the *inputfile*, *foo*, and *bar* values
defined in the *table iterator* example above.
And suppose that one of these jobs already has an existing submit description
in a file named ``my-job.sub``,
and this submit file *doesn't* use the *foo* and *bar* variable names
but instead uses *x* and *y*.
Your *job* descriptions could look like:

.. code-block:: condor-submit

   job x=foo,y=bar my-job.sub

   job {
       executable = a.out
       arguments = $(inputfile) $(foo) $(bar)
       transfer_input_files = $(inputfile)
   }

Note how in the second job above that there is no ``queue`` statement.
Job description queue statements
are disregarded when using job sets.
Instead, the number of jobs queued
are based on the *iterator* of the job set.
For the *table iterator*, the number of jobs queued
will be the number of rows in the table.

Putting together the examples above,
an entire example job set might look like:

.. code-block:: condor-submit

    name = MyJobSet

    iterator = table inputfile,foo,bar {
        input_A.txt,0,0
        input_B.txt,0,1
        input_C.txt,1,0
        input_D.txt,1,1
    }
          
    job x=foo,y=bar my-job.sub

    job {
        executable = a.out
        arguments = $(inputfile) $(foo) $(bar)
        transfer_input_files = $(inputfile)
    }

Based on this job set description,
with two job descriptions
(which become two job clusters),
you would expect the following output
when submitting this job set:

.. code-block:: console

      $ htcondor jobset submit my-jobs.set
      Submitted job set MyJobSet containing 2 job clusters.


:index:`Job Sets<single: Job Sets; listing>`

Listing job sets
--------------------------------

You can get a list of your active job sets
(i.e. job sets with jobs that are idle, executing, or held)
with the command ``htcondor jobset list``:

.. code-block:: console

    $ htcondor jobset list
    JOB_SET_NAME
    MyJobSet

The argument ``--allusers`` will list active job sets
for all users on the current access point:

.. code-block:: console

    $ htcondor jobset list --allusers
    OWNER  JOB_SET_NAME
    alice  MyJobSet
    bob    AnotherJobSet


:index:`Job Sets<single: Job Sets; checking status of>`

Checking on the progress of job sets
------------------------------------

You can check on your job set with the
``htcondor jobset status <job set name>`` command.

.. code-block:: console

    $ htcondor jobset status MyJobSet

    MyJobSet currently has 3 jobs idle, 5 jobs running, and 0 jobs completed.
    MyJobSet contains:
        Job cluster 1234 with 4 total jobs
        Job cluster 1235 with 4 total jobs

:index:`Job Sets<single: Job Sets; removing>`

Removing a job set
--------------------------------

If you realize that there is a problem with a job set
or you just do not need the job set to finish computing
for whatever reason,
you can remove an entire job set with the
``htcondor jobset remove <job set name>`` command:

.. code-block:: console

    $ htcondor jobset remove MyJobSet
    Removed 8 jobs matching job set MyJobSet for user alice.
