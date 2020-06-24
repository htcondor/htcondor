      

*condor_check_userlogs*
=========================

Check job event log files for errors
:index:`condor_check_userlogs<single: condor_check_userlogs; HTCondor commands>`
:index:`condor_check_userlogs command`

Synopsis
--------

**condor_check_userlogs** *UserLogFile1* [*UserLogFile2
...UserLogFileN* ]

Description
-----------

*condor_check_userlogs* is a program for checking a job event log or a
set of job event logs for errors. Output includes an indication that no
errors were found within a log file, or a list of errors such as an
execute or terminate event without a corresponding submit event, or
multiple terminated events for the same job.

*condor_check_userlogs* is especially useful for debugging
*condor_dagman* problems. If *condor_dagman* reports an error it is
often useful to run *condor_check_userlogs* on the relevant log files.

Exit Status
-----------

*condor_check_userlogs* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

