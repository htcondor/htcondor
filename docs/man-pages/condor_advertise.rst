      

*condor_advertise*
===================

Send a ClassAd to the *condor_collector* daemon
:index:`condor_advertise<single: condor_advertise; HTCondor commands>`\ :index:`condor_advertise command`

Synopsis
--------

**condor_advertise** [**-help | -version** ]

**condor_advertise** [**-pool** *centralmanagerhostname[:portname]*]
[**-debug** ] [**-tcp** ] [**-udp** ] [**-multiple** ]
[*update-command [classad-filename]*]

Description
-----------

*condor_advertise* sends one or more ClassAds to the
*condor_collector* daemon on the central manager machine. The optional
argument *update-command* says what daemon type's ClassAd is to be
updated; if it is absent, it assumed to be the update command
corresponding to the type of the (first) ClassAd. The optional argument
*classad-filename* is the file from which the ClassAd(s) should be read.
If *classad-filename* is omitted or is the dash character ('-'), then
the ClassAd(s) are read from standard input. You must specify
*update-command* if you do not want to read from standard input.

When **-multiple** is specified, multiple ClassAds may be published.
Publishing many ClassAds in a single invocation of *condor_advertise*
is more efficient than invoking *condor_advertise* once per ClassAd.
The ClassAds are expected to be separated by one or more blank lines.
When **-multiple** is not specified, blank lines are ignored (for
backward compatibility). It is best not to rely on blank lines being
ignored, as this may change in the future.

The *update-command* may be one of the following strings:

 UPDATE_STARTD_AD
 UPDATE_SCHEDD_AD
 UPDATE_MASTER_AD
 UPDATE_GATEWAY_AD
 UPDATE_CKPT_SRVR_AD
 UPDATE_NEGOTIATOR_AD
 UPDATE_HAD_AD
 UPDATE_AD_GENERIC
 UPDATE_SUBMITTOR_AD
 UPDATE_COLLECTOR_AD
 UPDATE_LICENSE_AD
 UPDATE_STORAGE_AD

*condor_advertise* can also be used to invalidate and delete ClassAds
currently held by the *condor_collector* daemon. In this case the
*update-command* will be one of the following strings:

 INVALIDATE_STARTD_ADS
 INVALIDATE_SCHEDD_ADS
 INVALIDATE_MASTER_ADS
 INVALIDATE_GATEWAY_ADS
 INVALIDATE_CKPT_SRVR_ADS
 INVALIDATE_NEGOTIATOR_ADS
 INVALIDATE_HAD_ADS
 INVALIDATE_ADS_GENERIC
 INVALIDATE_SUBMITTOR_ADS
 INVALIDATE_COLLECTOR_ADS
 INVALIDATE_LICENSE_ADS
 INVALIDATE_STORAGE_ADS

For any of these INVALIDATE commands, the ClassAd in the required file
consists of three entries. The file contents will be similar to:

.. code-block:: condor-classad-expr

    MyType = "Query"
    TargetType = "Machine" 
    Requirements = Name == "condor.example.com"

The definition for ``MyType`` is always ``Query``. ``TargetType`` is set
to the ``MyType`` of the ad to be deleted. This ``MyType`` is
``DaemonMaster`` for the *condor_master* ClassAd, ``Machine`` for the
*condor_startd* ClassAd, ``Scheduler`` for the *condor_schedd*
ClassAd, and ``Negotiator`` for the *condor_negotiator* ClassAd.

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
 **-pool** *centralmanagerhostname[:portname]*
    Specify a pool by giving the central manager's host name and an
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
result of *condor_advertise* is likely to be overwritten in a very
short time. It is unlikely that either HTCondor users (those who submit
jobs) or administrators will ever have a use for this command. If it is
desired to update or set a ClassAd attribute, the *condor_config_val*
command is the proper command to use.

Attributes are defined in Appendix A of the HTCondor manual.

For those administrators who do need *condor_advertise*, the following
attributes may be included:

 ``DaemonStartTime``
 ``UpdateSequenceNumber``

If both of the above are included, the *condor_collector* will
automatically include the following attributes:

 ``UpdatesTotal``
 ``UpdatesLost``
 ``UpdatesSequenced``
 ``UpdatesHistory``

    Affected by ``COLLECTOR_DAEMON_HISTORY_SIZE`` :index:`COLLECTOR_DAEMON_HISTORY_SIZE`.

Examples
--------

Assume that a machine called condor.example.com is turned off, yet its
*condor_startd* ClassAd does not expire for another 20 minutes. To
avoid this machine being matched, an administrator chooses to delete the
machine's *condor_startd* ClassAd. Create a file (called
``remove_file`` in this example) with the three required attributes:

.. code-block:: condor-classad

    MyType = "Query"
    TargetType = "Machine" 
    Requirements = Name == "condor.example.com"

This file is used with the command:

.. code-block:: console

    $ condor_advertise INVALIDATE_STARTD_ADS remove_file

Exit Status
-----------

*condor_advertise* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure. Success
means that all ClassAds were successfully sent to all
*condor_collector* daemons. When there are multiple ClassAds or
multiple *condor_collector* daemons, it is possible that some but not
all publications succeed; in this case, the exit status is 1, indicating
failure.

