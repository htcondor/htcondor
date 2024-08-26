*condor_pool_job_report*
===========================

generate report about all jobs that have run in the last 24 hours on all
execute hosts
:index:`condor_pool_job_report<single: condor_pool_job_report; HTCondor commands>`\ :index:`condor_pool_job_report command`

Synopsis
--------

**condor_pool_job_report**

Description
-----------

*condor_pool_job_report* is a Linux-only tool that is designed to be
run nightly using *cron*. It is intended to be run on the central
manager, or another machine that has administrative permissions, and is
able to fetch the *condor_startd* history logs from all of the
*condor_startd* daemons in the pool. After fetching these logs,
*condor_pool_job_report* then generates a report about job run times
and mails it to administrators, as defined by configuration variable
:macro:`CONDOR_ADMIN`.

Exit Status
-----------

*condor_pool_job_report* will exit with a status value of 0 (zero)
upon success, and it will exit with the value 1 (one) upon failure.

