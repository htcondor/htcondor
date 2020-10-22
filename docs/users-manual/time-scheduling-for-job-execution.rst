Time Scheduling for Job Execution
=================================

:index:`to execute at a specific time<single: to execute at a specific time; scheduling jobs>`
:index:`at a specific time<single: at a specific time; job execution>`

Jobs may be scheduled to begin execution at a specified time in the
future with HTCondor's job deferral functionality. All specifications
are in a job's submit description file. Job deferral functionality is
expanded to provide for the periodic execution of a job, known as the
CronTab scheduling.

Job Deferral
------------

:index:`job deferral time`
:index:`of a job<single: of a job; deferral time>`

Job deferral allows the specification of the exact date and time at
which a job is to begin executing. HTCondor attempts to match the job to
an execution machine just like any other job, however, the job will wait
until the exact time to begin execution. A user can define the job to
allow some flexibility in the execution of jobs that miss their
execution time.

Deferred Execution Time
'''''''''''''''''''''''

:index:`of a job<single: of a job; deferral time>`
:index:`DeferralTime<single: DeferralTime; ClassAd job attribute>`

A job's deferral time is the exact time that HTCondor should attempt to
execute the job. The deferral time attribute is defined as an expression
that evaluates to a Unix Epoch timestamp (the number of seconds elapsed
since 00:00:00 on January 1, 1970, Coordinated Universal Time). This is
the time that HTCondor will begin to execute the job.

After a job is matched and all of its files have been transferred to an
execution machine, HTCondor checks to see if the job's ClassAd contains
a deferral time. If it does, HTCondor calculates the number of seconds
between the execution machine's current system time and the job's
deferral time. If the deferral time is in the future, the job waits to
begin execution. While a job waits, its job ClassAd attribute
``JobStatus`` indicates the job is in the Running state. As the deferral
time arrives, the job begins to execute. If a job misses its execution
time, that is, if the deferral time is in the past, the job is evicted
from the execution machine and put on hold in the queue.

The specification of a deferral time does not interfere with HTCondor's
behavior. For example, if a job is waiting to begin execution when a
*condor_hold* command is issued, the job is removed from the execution
machine and is put on hold. If a job is waiting to begin execution when
a *condor_suspend* command is issued, the job continues to wait. When
the deferral time arrives, HTCondor begins execution for the job, but
immediately suspends it.

The deferral time is specified in the job's submit description file with
the command
**deferral_time** :index:`deferral_time<single: deferral_time; submit commands>`.

Deferral Window
'''''''''''''''

:index:`DeferralWindow<single: DeferralWindow; ClassAd job attribute>`
:index:`deferral_window<single: deferral_window; submit commands>`

If a job arrives at its execution machine after the deferral time has
passed, the job is evicted from the machine and put on hold in the job
queue. This may occur, for example, because the transfer of needed files
took too long due to a slow network connection. A deferral window
permits the execution of a job that misses its deferral time by
specifying a window of time within which the job may begin.

The deferral window is the number of seconds after the deferral time,
within which the job may begin. When a job arrives too late, HTCondor
calculates the difference in seconds between the execution machine's
current time and the job's deferral time. If this difference is less
than or equal to the deferral window, the job immediately begins
execution. If this difference is greater than the deferral window, the
job is evicted from the execution machine and is put on hold in the job
queue.

The deferral window is specified in the job's submit description file
with the command
**deferral_window** :index:`deferral_window<single: deferral_window; submit commands>`.

Preparation Time
''''''''''''''''

:index:`DeferralPrepTime<single: DeferralPrepTime; ClassAd job attribute>`

When a job defines a deferral time far in the future and then is matched
to an execution machine, potential computation cycles are lost because
the deferred job has claimed the machine, but is not actually executing.
Other jobs could execute during the interval when the job waits for its
deferral time. To make use of the wasted time,
:index:`deferral_prep_time<single: deferral_prep_time; submit commands>`\ a job defines a
**deferral_prep_time** :index:`deferral_prep_time<single: deferral_prep_time; submit commands>`
with an integer expression that evaluates to a number of seconds. At
this number of seconds before the deferral time, the job may be matched
with a machine.

Deferral Usage Examples
'''''''''''''''''''''''

:index:`deferral_time<single: deferral_time; submit commands>`

Here are examples of how the job deferral time, deferral window, and the
preparation time may be used.

The job's submit description file specifies that the job is to begin
execution on January 1st, 2006 at 12:00 pm:

.. code-block:: condor-submit

       deferral_time = 1136138400

The Unix *date* program may be used to calculate a Unix epoch time. The
syntax of the command to do this depends on the options provided within
that flavor of Unix. In some, it appears as

.. code-block:: console

    $ date --date "MM/DD/YYYY HH:MM:SS" +%s

and in others, it appears as

.. code-block:: console

    $ date -d "YYYY-MM-DD HH:MM:SS" +%s

MM is a 2-digit month number, DD is a 2-digit day of the month number,
and YYYY is a 4-digit year. HH is the 2-digit hour of the day, MM is the
2-digit minute of the hour, and SS are the 2-digit seconds within the
minute. The characters +%s tell the *date* program to give the output as
a Unix epoch time.

The job always waits 60 seconds after submission before beginning
execution:

.. code-block:: condor-submit

       deferral_time = (QDate + 60)

In this example, assume that the deferral time is 45 seconds in the past
as the job is available. The job begins execution, because 75 seconds
remain in the deferral window:

.. code-block:: condor-submit

       deferral_window = 120

In this example, a job is scheduled to execute far in the future, on
January 1st, 2010 at 12:00 pm. The
**deferral_prep_time** :index:`deferral_prep_time<single: deferral_prep_time; submit commands>`
attribute delays the job from being matched until 60 seconds before the
job is to begin execution.

.. code-block:: condor-submit

       deferral_time      = 1262368800
       deferral_prep_time = 60

Deferral Limitations
''''''''''''''''''''

There are some limitations to HTCondor's job deferral feature.

-  Job deferral is not available for scheduler universe jobs. A
   scheduler universe job defining the ``deferral_time`` produces a
   fatal error when submitted.
-  The time that the job begins to execute is based on the execution
   machine's system clock, and not the submission machine's system
   clock. Be mindful of the ramifications when the two clocks show
   dramatically different times.
-  A job's ``JobStatus`` attribute is always in the Running state when
   job deferral is used. There is currently no way to distinguish
   between a job that is executing and a job that is waiting for its
   deferral time.

CronTab Scheduling
------------------

:index:`CronTab job scheduling`
:index:`periodic<single: periodic; job scheduling>`
:index:`to execute periodically<single: to execute periodically; scheduling jobs>`

HTCondor's CronTab scheduling functionality allows jobs to be scheduled
to execute periodically. A job's execution schedule is defined by
commands within the submit description file. The notation is much like
that used by the Unix *cron* daemon. As such, HTCondor developers are
fond of referring to CronTab :index:`Crondor`\ scheduling as
Crondor. The scheduling of jobs using HTCondor's CronTab feature
calculates and utilizes the ``DeferralTime`` ClassAd attribute.

Also, unlike the Unix *cron* daemon, HTCondor never runs more than one
instance of a job at the same time.

The capability for repetitive or periodic execution of the job is
enabled by specifying an
**on_exit_remove** :index:`on_exit_remove<single: on_exit_remove; submit commands>`
command for the job, such that the job does not leave the queue until
desired.

Semantics for CronTab Specification
'''''''''''''''''''''''''''''''''''

A job's execution schedule is defined by a set of specifications within
the submit description file. HTCondor uses these to calculate a
``DeferralTime`` for the job.

Table 2.3 lists the submit commands and acceptable
values for these commands. At least one of these must be defined in
order for HTCondor to calculate a ``DeferralTime`` for the job. Once one
CronTab value is defined, the default for all the others uses all the
values in the allowed values ranges.
:index:`cron_minute<single: cron_minute; submit commands>`
:index:`cron_hour<single: cron_hour; submit commands>`
:index:`cron_day_of_month<single: cron_day_of_month; submit commands>`
:index:`cron_month<single: cron_month; submit commands>`
:index:`cron_day_of_week<single: cron_day_of_week; submit commands>`

+----------------------------+----------------------------+
| **cron_minute**            | 0 - 59                     |
+----------------------------+----------------------------+
| **cron_hour**              | 0 - 23                     |
+----------------------------+----------------------------+
| **cron_day_of_month**      | 1 - 31                     |
+----------------------------+----------------------------+
| **cron_month**             | 1 - 12                     |
+----------------------------+----------------------------+
| **cron_day_of_week**       | 0 - 7 (Sunday is 0 or 7)   |
+----------------------------+----------------------------+

Table 2.3: The list of submit commands and their value ranges.


The day of a job's execution can be specified by both the
**cron_day_of_month** and the **cron_day_of_week** attributes. The
day will be the logical or of both.

The semantics allow more than one value to be specified by using the \*
operator, ranges, lists, and steps (strides) within ranges.

 The asterisk operator
    The \* (asterisk) operator specifies that all of the allowed values
    are used for scheduling. For example,

    .. code-block:: condor-submit

              cron_month = *


    becomes any and all of the list of possible months:
    (1,2,3,4,5,6,7,8,9,10,11,12). Thus, a job runs any month in the
    year.

 Ranges
    A range creates a set of integers from all the allowed values
    between two integers separated by a hyphen. The specified range is
    inclusive, and the integer to the left of the hyphen must be less
    than the right hand integer. For example,

    .. code-block:: condor-submit

              cron_hour = 0-4


    represents the set of hours from 12:00 am (midnight) to 4:00 am, or
    (0,1,2,3,4).

 Lists
    A list is the union of the values or ranges separated by commas.
    Multiple entries of the same value are ignored. For example,

    .. code-block:: condor-submit

              cron_minute = 15,20,25,30
              cron_hour   = 0-3,9-12,15


    where this **cron_minute** example represents (15,20,25,30) and
    **cron_hour** represents (0,1,2,3,9,10,11,12,15).

 Steps
    Steps select specific numbers from a range, based on an interval. A
    step is specified by appending a range or the asterisk operator with
    a slash character (/), followed by an integer value. For example,

    .. code-block:: condor-submit

              cron_minute = 10-30/5
              cron_hour = */3


    where this **cron_minute** example specifies every five minutes
    within the specified range to represent (10,15,20,25,30), and
    **cron_hour** specifies every three hours of the day to represent
    (0,3,6,9,12,15,18,21).

Preparation Time and Execution Window
'''''''''''''''''''''''''''''''''''''

The
**cron_prep_time** :index:`cron_prep_time<single: cron_prep_time; submit commands>`
command is analogous to the deferral time's
**deferral_prep_time** :index:`deferral_prep_time<single: deferral_prep_time; submit commands>`
command. It specifies the number of seconds before the deferral time
that the job is to be matched and sent to the execution machine. This
permits HTCondor to make necessary preparations before the deferral time
occurs.

Consider the submit description file example that includes

.. code-block:: condor-submit

       cron_minute = 0
       cron_hour = *
       cron_prep_time = 300

The job is scheduled to begin execution at the top of every hour. Note
that the setting of **cron_hour** in this example is not required, as
the default value will be \*, specifying any and every hour of the day.
The job will be matched and sent to an execution machine no more than
five minutes before the next deferral time. For example, if a job is
submitted at 9:30am, then the next deferral time will be calculated to
be 10:00am. HTCondor may attempt to match the job to a machine and send
the job once it is 9:55am.

As the CronTab scheduling calculates and uses deferral time, jobs may
also make use of the deferral window. The submit command
**cron_window** :index:`cron_window<single: cron_window; submit commands>` is
analogous to the submit command
**deferral_window** :index:`deferral_window<single: deferral_window; submit commands>`.
Consider the submit description file example that includes

.. code-block:: condor-submit

       cron_minute = 0
       cron_hour = *
       cron_window = 360

As the previous example, the job is scheduled to begin execution at the
top of every hour. Yet with no preparation time, the job is likely to
miss its deferral time. The 6-minute window allows the job to begin
execution, as long as it arrives and can begin within 6 minutes of the
deferral time, as seen by the time kept on the execution machine.

Scheduling
''''''''''

When a job using the CronTab functionality is submitted to HTCondor, use
of at least one of the submit description file commands beginning with
**cron_** causes HTCondor to calculate and set a deferral time for when
the job should run. A deferral time is determined based on the current
time rounded later in time to the next minute. The deferral time is the
job's ``DeferralTime`` attribute. A new deferral time is calculated when
the job first enters the job queue, when the job is re-queued, or when
the job is released from the hold state. New deferral times for all jobs
in the job queue using the CronTab functionality are recalculated when a
*condor_reconfig* or a *condor_restart* command that affects the job
queue is issued.

A job's deferral time is not always the same time that a job will
receive a match and be sent to the execution machine. This is because
HTCondor operates on the job queue at times that are independent of job
events, such as when job execution completes. Therefore, HTCondor may
operate on the job queue just after a job's deferral time states that it
is to begin execution. HTCondor attempts to start a job when the
following pseudo-code boolean expression evaluates to ``True``:

.. code-block:: text

       ( time() + SCHEDD_INTERVAL ) >= ( DeferralTime - CronPrepTime )

If the ``time()`` plus the number of seconds until the next time
HTCondor checks the job queue is greater than or equal to the time that
the job should be submitted to the execution machine, then the job is to
be matched and sent now.

Jobs using the CronTab functionality are not automatically re-queued by
HTCondor after their execution is complete. The submit description file
for a job must specify an appropriate
**on_exit_remove** :index:`on_exit_remove<single: on_exit_remove; submit commands>`
command to ensure that a job remains in the queue. This job maintains
its original ``ClusterId`` and ``ProcId``.

Submit Commands Usage Examples
''''''''''''''''''''''''''''''

Here are some examples of the submit commands necessary to schedule jobs
to run at multifarious times. Please note that it is not necessary to
explicitly define each attribute; the default value is \*.

Run 23 minutes after every two hours, every day of the week:

.. code-block:: condor-submit

       on_exit_remove = false
       cron_minute = 23
       cron_hour = 0-23/2
       cron_day_of_month = *
       cron_month = *
       cron_day_of_week = *

Run at 10:30pm on each of May 10th to May 20th, as well as every
remaining Monday within the month of May:

.. code-block:: condor-submit

       on_exit_remove = false
       cron_minute = 30
       cron_hour = 20
       cron_day_of_month = 10-20
       cron_month = 5
       cron_day_of_week = 2

Run every 10 minutes and every 6 minutes before noon on January 18th
with a 2-minute preparation time:

.. code-block:: condor-submit

       on_exit_remove = false
       cron_minute = */10,*/6
       cron_hour = 0-11
       cron_day_of_month = 18
       cron_month = 1
       cron_day_of_week = *
       cron_prep_time = 120

Submit Commands Limitations
'''''''''''''''''''''''''''

The use of the CronTab functionality has all of the same limitations of
deferral times, because the mechanism is based upon deferral times.

-  It is impossible to schedule vanilla universe jobs at
   intervals that are smaller than the interval at which HTCondor
   evaluates jobs. This interval is determined by the configuration
   variable ``SCHEDD_INTERVAL`` :index:`SCHEDD_INTERVAL`. As a
   vanilla universe job completes execution and is placed
   back into the job queue, it may not be placed in the idle state in
   time. This problem does not afflict local universe jobs.
-  HTCondor cannot guarantee that a job will be matched in order to make
   its scheduled deferral time. A job must be matched with an execution
   machine just as any other HTCondor job; if HTCondor is unable to find
   a match, then the job will miss its chance for executing and must
   wait for the next execution time specified by the CronTab schedule.


