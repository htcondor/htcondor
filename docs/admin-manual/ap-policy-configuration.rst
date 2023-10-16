Configuration for Access Points
===============================

*condor_schedd* Policy Configuration
-------------------------------------

:index:`condor_schedd policy<single: condor_schedd policy; configuration>`
:index:`policy configuration<single: policy configuration; submit host>`

There are two types of schedd policy: job transforms (which change the
ClassAd of a job at submission) and submit requirements (which prevent
some jobs from entering the queue). These policies are explained below.

Job Transforms
''''''''''''''

:index:`job transforms`

The *condor_schedd* can transform jobs as they are submitted.
Transformations can be used to guarantee the presence of required job
attributes, to set defaults for job attributes the user does not supply,
or to modify job attributes so that they conform to schedd policy; an
example of this might be to automatically set accounting attributes
based on the owner of the job while letting the job owner indicate a
preference.

There can be multiple job transforms. Each transform can have a
Requirements expression to indicate which jobs it should transform and
which it should ignore. Transforms without a Requirements expression
apply to all jobs. Job transforms are applied in order. The set of
transforms and their order are configured using the Configuration
variable :macro:`JOB_TRANSFORM_NAMES`.

For each entry in this list there must be a corresponding
:macro:`JOB_TRANSFORM_<name>`
configuration variable that specifies the transform rules. Transforms
can use the same syntax as *condor_job_router* transforms; although unlike
the *condor_job_router* there is no default transform, and all
matching transforms are applied - not just the first one. (See the
:doc:`/grid-computing/job-router` section for information on the
*condor_job_router*.)

When a submission is a late materialization job factory,
transforms that would match the first factory job will be applied to the Cluster ad at submit time.
When job ads are later materialized, attribute values set by the transform
will override values set by the job factory for those attributes.

The following example shows a set of two transforms: one that
automatically assigns an accounting group to jobs based on the
submitting user, and one that shows one possible way to transform
Vanilla jobs to Docker jobs.

.. code-block:: text

    JOB_TRANSFORM_NAMES = AssignGroup, SL6ToDocker

    JOB_TRANSFORM_AssignGroup @=end
       # map Owner to group using the existing accounting group attribute as requested group
       EVALSET AcctGroup = userMap("Groups",Owner,AcctGroup)
       EVALSET AccountingGroup = join(".",AcctGroup,Owner)
    @end

    JOB_TRANSFORM_SL6ToDocker @=end
       # match only vanilla jobs that have WantSL6 and do not already have a DockerImage
       REQUIREMENTS JobUniverse==5 && WantSL6 && DockerImage =?= undefined
       SET  WantDocker = true
       SET  DockerImage = "SL6"
       SET  Requirements = TARGET.HasDocker && $(MY.Requirements)
    @end

The AssignGroup transform above assumes that a mapfile that can map an
owner to one or more accounting groups has been configured via
:macro:`SCHEDD_CLASSAD_USER_MAP_NAMES`, and given the name "Groups".

The SL6ToDocker transform above is most likely incomplete, as it assumes
a custom attribute (``WantSL6``) that your pool may or may not use.

Submit Requirements
'''''''''''''''''''

:index:`submit requirements`

The *condor_schedd* may reject job submissions, such that rejected jobs
never enter the queue. Rejection may be best for the case in which there
are jobs that will never be able to run; for instance, a job specifying
an obsolete universe, like standard.
Another appropriate example might be to reject all jobs that
do not request a minimum amount of memory. Or, it may be appropriate to
prevent certain users from using a specific submit host.

Rejection criteria are configured. Configuration variable
:macro:`SUBMIT_REQUIREMENT_NAMES`
lists criteria, where each criterion is given a name. The chosen name is
a major component of the default error message output if a user attempts
to submit a job which fails to meet the requirements. Therefore, choose
a descriptive name. For the three example submit requirements described:

.. code-block:: text

    SUBMIT_REQUIREMENT_NAMES = NotStandardUniverse, MinimalRequestMemory, NotChris

The criterion for each submit requirement is then specified in
configuration variable 
:macro:`SUBMIT_REQUIREMENT_<Name>`, where ``<Name>`` matches the
chosen name listed in ``SUBMIT_REQUIREMENT_NAMES``. The value is a
boolean ClassAd expression. The three example criterion result in these
configuration variable definitions:

.. code-block:: text

    SUBMIT_REQUIREMENT_NotStandardUniverse = JobUniverse != 1
    SUBMIT_REQUIREMENT_MinimalRequestMemory = RequestMemory > 512
    SUBMIT_REQUIREMENT_NotChris = Owner != "chris"

Submit requirements are evaluated in the listed order; the first
requirement that evaluates to ``False`` causes rejection of the job,
terminates further evaluation of other submit requirements, and is the
only requirement reported. Each submit requirement is evaluated in the
context of the *condor_schedd* ClassAd, which is the ``MY.`` name space
and the job ClassAd, which is the ``TARGET.`` name space. Note that
``JobUniverse`` and ``RequestMemory`` are both job ClassAd attributes.

Further configuration may associate a rejection reason with a submit
requirement with the :macro:`SUBMIT_REQUIREMENT_<Name>_REASON`.

.. code-block:: text

    SUBMIT_REQUIREMENT_NotStandardUniverse_REASON = "This pool does not accept standard universe jobs."
    SUBMIT_REQUIREMENT_MinimalRequestMemory_REASON = strcat( "The job only requested ", \
      RequestMemory, " Megabytes.  If that small amount is really enough, please contact ..." )
    SUBMIT_REQUIREMENT_NotChris_REASON = "Chris, you may only submit jobs to the instructional pool."

The value must be a ClassAd expression which evaluates to a string.
Thus, double quotes were required to make strings for both
``SUBMIT_REQUIREMENT_NotStandardUniverse_REASON`` and
``SUBMIT_REQUIREMENT_NotChris_REASON``. The ClassAd function strcat()
produces a string in the definition of
``SUBMIT_REQUIREMENT_MinimalRequestMemory_REASON``.

Rejection reasons are sent back to the submitting program and will
typically be immediately presented to the user. If an optional
:macro:`SUBMIT_REQUIREMENT_<Name>_REASON` is not defined, a default reason
will include the ``<Name>`` chosen for the submit requirement.
Completing the presentation of the example submit requirements, upon an
attempt to submit a standard universe job, *condor_submit* would print

.. code-block:: text

    Submitting job(s).
    ERROR: Failed to commit job submission into the queue.
    ERROR: This pool does not accept standard universe jobs.

Where there are multiple jobs in a cluster, if any job within the
cluster is rejected due to a submit requirement, the entire cluster of
jobs will be rejected.

Submit Warnings
'''''''''''''''

:index:`submit warnings`

Starting in HTCondor 8.7.4, you may instead configure submit warnings. A
submit warning is a submit requirement for which
:macro:`SUBMIT_REQUIREMENT_<Name>_IS_WARNING` is true. A submit
warning does not cause the submission to fail; instead, it returns a
warning to the user's console (when triggered via *condor_submit*) or
writes a message to the user log (always). Submit warnings are intended
to allow HTCondor administrators to provide their users with advance
warning of new submit requirements. For example, if you want to increase
the minimum request memory, you could use the following configuration.

.. code-block:: text

    SUBMIT_REQUIREMENT_NAMES = OneGig $(SUBMIT_REQUIREMENT_NAMES)
    SUBMIT_REQUIREMENT_OneGig = RequestMemory > 1024
    SUBMIT_REQUIREMENT_OneGig_REASON = "As of <date>, the minimum requested memory will be 1024."
    SUBMIT_REQUIREMENT_OneGig_IS_WARNING = TRUE

When a user runs *condor_submit* to submit a job with ``RequestMemory``
between 512 and 1024, they will see (something like) the following,
assuming that the job meets all the other requirements.

.. code-block:: text

    Submitting job(s).
    WARNING: Committed job submission into the queue with the following warning:
    WARNING: As of <date>, the minimum requested memory will be 1024.

    1 job(s) submitted to cluster 452.

The job will contain (something like) the following:

.. code-block:: text

    000 (452.000.000) 10/06 13:40:45 Job submitted from host: <128.105.136.53:37317?addrs=128.105.136.53-37317+[fc00--1]-37317&noUDP&sock=19966_e869_5>
        WARNING: Committed job submission into the queue with the following warning: As of <date>, the minimum requested memory will be 1024.
    ...

Marking a submit requirement as a warning does not change when or how it
is evaluated, only the result of doing so. In particular, failing a
submit warning does not terminate further evaluation of the submit
requirements list. Currently, only one (the most recent) problem is
reported for each submit attempt. This means users will see (as they
previously did) only the first failed requirement; if all requirements
passed, they will see the last failed warning, if any.
