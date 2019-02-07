      

condor\_advertise
=================

Send a ClassAd to the *condor\_collector* daemon

Synopsis
--------

**condor\_advertise** [**-help \| -version**\ ]

**condor\_advertise** [**-pool  **\ *centralmanagerhostname[:portname]*]
[**-debug**\ ] [**-tcp**\ ] [**-udp**\ ] [**-multiple**\ ] [**\ ]

Description
-----------

*condor\_advertise* sends one or more ClassAds to the
*condor\_collector* daemon on the central manager machine. The optional
argument *update-command* says what daemon type’s ClassAd is to be
updated; if it is absent, it assumed to be the update command
corresponding to the type of the (first) ClassAd. The optional argument
*classad-filename* is the file from which the ClassAd(s) should be read.
If *classad-filename* is omitted or is the dash character (’-’), then
the ClassAd(s) are read from standard input. You must specify
*update-command* if you do not want to read from standard input.

When **-multiple** is specified, multiple ClassAds may be published.
Publishing many ClassAds in a single invocation of *condor\_advertise*
is more efficient than invoking *condor\_advertise* once per ClassAd.
The ClassAds are expected to be separated by one or more blank lines.
When **-multiple** is not specified, blank lines are ignored (for
backward compatibility). It is best not to rely on blank lines being
ignored, as this may change in the future.

The *update-command* may be one of the following strings:

 UPDATE\_STARTD\_AD
 UPDATE\_SCHEDD\_AD
 UPDATE\_MASTER\_AD
 UPDATE\_GATEWAY\_AD
 UPDATE\_CKPT\_SRVR\_AD
 UPDATE\_NEGOTIATOR\_AD
 UPDATE\_HAD\_AD
 UPDATE\_AD\_GENERIC
 UPDATE\_SUBMITTOR\_AD
 UPDATE\_COLLECTOR\_AD
 UPDATE\_LICENSE\_AD
 UPDATE\_STORAGE\_AD

*condor\_advertise* can also be used to invalidate and delete ClassAds
currently held by the *condor\_collector* daemon. In this case the
*update-command* will be one of the following strings:

 INVALIDATE\_STARTD\_ADS
 INVALIDATE\_SCHEDD\_ADS
 INVALIDATE\_MASTER\_ADS
 INVALIDATE\_GATEWAY\_ADS
 INVALIDATE\_CKPT\_SRVR\_ADS
 INVALIDATE\_NEGOTIATOR\_ADS
 INVALIDATE\_HAD\_ADS
 INVALIDATE\_ADS\_GENERIC
 INVALIDATE\_SUBMITTOR\_ADS
 INVALIDATE\_COLLECTOR\_ADS
 INVALIDATE\_LICENSE\_ADS
 INVALIDATE\_STORAGE\_ADS

For any of these INVALIDATE commands, the ClassAd in the required file
consists of three entries. The file contents will be similar to:

::

    MyType = "Query" 
    TargetType = "Machine" 
    Requirements = Name == "condor.example.com"

The definition for ``MyType`` is always ``Query``. ``TargetType`` is set
to the ``MyType`` of the ad to be deleted. This ``MyType`` is
``DaemonMaster`` for the *condor\_master* ClassAd, ``Machine`` for the
*condor\_startd* ClassAd, ``Scheduler`` for the *condor\_schedd*
ClassAd, and ``Negotiator`` for the *condor\_negotiator* ClassAd.
``Requirements`` is an expression evaluated within the context of ads of
``TargetType``. When ``Requirements`` evaluates to ``True``, the
matching ad is invalidated. A full example is given below.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-debug**
    Print debugging information as the command executes.
 **-multiple**
    Send more than one ClassAd, where the boundary between ClassAds is
    one or more blank lines.
 **-pool **\ *centralmanagerhostname[:portname]*
    Specify a pool by giving the central manager’s host name and an
    optional port number. The default is the ``COLLECTOR_HOST``
    specified in the configuration file.
 **-tcp**
    Use TCP for communication. Used by default if
    ``UPDATE_COLLECTOR_WITH_TCP`` is true.
 **-udp**
    Use UDP for communication.

General Remarks
---------------

The job and machine ClassAds are regularly updated. Therefore, the
result of *condor\_advertise* is likely to be overwritten in a very
short time. It is unlikely that either HTCondor users (those who submit
jobs) or administrators will ever have a use for this command. If it is
desired to update or set a ClassAd attribute, the *condor\_config\_val*
command is the proper command to use.

Attributes are defined in Appendix A of the HTCondor manual.

For those administrators who do need *condor\_advertise*, the following
attributes may be included:

 ``DaemonStartTime``
 ``UpdateSequenceNumber``

If both of the above are included, the *condor\_collector* will
automatically include the following attributes:

 ``UpdatesTotal``
 ``UpdatesLost``
 ``UpdatesSequenced``
 ``UpdatesHistory``
    Affected by ``COLLECTOR_DAEMON_HISTORY_SIZE`` .

Examples
--------

Assume that a machine called condor.example.com is turned off, yet its
*condor\_startd* ClassAd does not expire for another 20 minutes. To
avoid this machine being matched, an administrator chooses to delete the
machine’s *condor\_startd* ClassAd. Create a file (called
``remove_file`` in this example) with the three required attributes:

::

    MyType = "Query" 
    TargetType = "Machine" 
    Requirements = Name == "condor.example.com"

This file is used with the command:

::

    % condor_advertise INVALIDATE_STARTD_ADS remove_file

Exit Status
-----------

*condor\_advertise* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure. Success
means that all ClassAds were successfully sent to all
*condor\_collector* daemons. When there are multiple ClassAds or
multiple *condor\_collector* daemons, it is possible that some but not
all publications succeed; in this case, the exit status is 1, indicating
failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
