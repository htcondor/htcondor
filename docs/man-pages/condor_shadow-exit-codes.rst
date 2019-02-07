      

*condor\_shadow* Exit Codes
===========================

When a *condor\_shadow* daemon exits, the *condor\_shadow* exit code is
recorded in the *condor\_schedd* log, and it identifies why the job
exited. Prose in the log appears of the form

::

    Shadow pid XXXXX for job XX.X exited with status YYY

where ``YYY`` is the exit code, or

::

    Shadow pid XXXXX for job XX.X reports job exit reason 100.

where the exit code is the value 100. Table \ `B.1 <#x181-12450021>`__
lists these codes.

--------------

Table B.1: *condor\_shadow* Exit Codes

Value

Error Name

Description

4

JOB\_EXCEPTION

the job exited with an exception

44

DPRINTF\_ERROR

there was a fatal error with dprintf()

100

JOB\_EXITED

the job exited (not killed)

101

JOB\_CKPTED

the job did produce a checkpoint

102

JOB\_KILLED

the job was killed

103

JOB\_COREDUMPED

the job was killed and a core file was produced

105

JOB\_NO\_MEM

not enough memory to start the condor\_shadow

106

JOB\_SHADOW\_USAGE

incorrect arguments to condor\_shadow

107

JOB\_NOT\_CKPTED

the job vacated without a checkpoint

107

JOB\_SHOULD\_REQUEUE

same number as JOB\_NOT\_CKPTED,

to achieve the same behavior.

This exit code implies that we want

the job to be put back in the job queue

and run again.

108

JOB\_NOT\_STARTED

can not connect to the *condor\_startd* or request refused

109

JOB\_BAD\_STATUS

job status != RUNNING on start up

110

JOB\_EXEC\_FAILED

exec failed for some reason other than ENOMEM

111

JOB\_NO\_CKPT\_FILE

there is no checkpoint file (as it was lost)

112

JOB\_SHOULD\_HOLD

the job should be put on hold

113

JOB\_SHOULD\_REMOVE

the job should be removed

114

JOB\_MISSED\_DEFERRAL\_TIME

the job goes on hold, because it did not run within the

specified window of time

115

JOB\_EXITED\_AND\_CLAIM\_CLOSING

the job exited (not killed) but the *condor\_startd*

is not accepting any more jobs on this claim

--------------

--------------

--------------

--------------

      
