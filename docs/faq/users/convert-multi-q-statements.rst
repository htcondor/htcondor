Example: Convert Multi-Queue Statements into One
================================================

The old method of setting multiple :subcom:`queue` statements into a
single submit description to create variance between submitted jobs is
deprecated. However, the ability to easily submit multiple jobs with
variance is still possible with a single queue statement. The following
examples will show how one can convert the older syntax to the improved
single queue statement.

.. note::

    The following example submit files are not designed to be useable but rather
    a demonstration of how the old syntax can be converted.

A Job Per File
--------------

The following example shows a submit file using multiple queue statements to
submit jobs with differing input files which can easily be done with a single
queue statement. This example assumes the working directory contains the files
``example1.in`` and ``example2.in``.

.. code-block:: condor-submit

    executable = /bin/cat
    arguments  = $(filename)

    transfer_input_files = $(filename)

    #==========Replace=========
    # Old Syntax
    filename = example1.in
    queue

    filename = example2.in
    queue
    #==========================

.. code-block:: condor-submit

    # Converted Syntax
    # Submit one job per file matching *.in
    queue filename matching files *.in

Jobs with multiple variables
----------------------------

.. sidebar:: External Item Data

    The queue statement can also have the per job item data
    stored in an external file that is referenced such as
    the following ``external.dat`` contents:

    .. code-block:: text

        3, 1, 10
        2, 8, 4

    .. code-block:: condor-submit

        queue A,B,C from external.dat

The following two examples show how multiple variables can be given
variance in a single queue statement.

.. code-block:: condor-submit

    executable = quadratic.sh
    arguments  = "$(A) $(B) $(C)"

    #==========Replace=========
    # Old Syntax
    A = 3
    B = 1
    C = 10
    queue

    A = 2
    B = 8
    C = 4
    queue
    #==========================

.. code-block:: condor-submit

    # Converted Syntax
    # Submit one job for each row of data
    queue A,B,C from (
        3, 1, 10
        2, 8, 4
    )

Shared Variance
^^^^^^^^^^^^^^^

In this example the ``tolerance`` variable is the same for two jobs
and different for the last. Despite only having two values in the set
of jobs, the ``tolerance`` must be set for each job in the converted
syntax unlike the old.

.. code-block:: condor-submit

    executable = word_count.py
    arguments  = "$(filename) $(tolerance)"

    transfer_input_files = $(filename)

    #==========Replace=========
    # Old Syntax
    tolerance = 50
    filename = first.txt
    queue

    filename = second.txt
    queue

    tolerance = 25
    filename = third.txt
    queue
    #==========================

.. code-block:: condor-submit

    # Converted Syntax
    # Submit one job per each row of data
    # Note: Each row needs each variable data defined
    queue tolerance, filename from (
        50, first.txt
        50, second.txt
        25, third.txt
    )

Variable Cadence
^^^^^^^^^^^^^^^^

In the following example, the ``capacity`` variable uses each assigned value for
two jobs with a varying ``type`` variable.

.. code-block:: condor-submit

    executable = science.py
    arguments  = "$(capacity) $(type)"

    #==========Replace=========
    # Old Syntax
    capacity = 100
    type = "soft"
    queue

    type = "hard"
    queue

    capacity = 42
    type = "soft"
    queue

    type = "hard"
    queue
    #==========================

.. code-block:: condor-submit

    # Converted Syntax
    type = $Choice(STEP,"soft","hard")
    queue 2 capacity from (
        100
        42
    )

