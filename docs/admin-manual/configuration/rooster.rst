:index:`Rooster Daemon Options<single: Configuration; Rooster Daemon Options>`

.. _rooster_config_options:

Rooster Daemon Configuration Options
====================================

*condor_rooster* is an optional daemon that may be added to the
:tool:`condor_master` daemon's :macro:`DAEMON_LIST`. It is responsible for waking
up hibernating machines when their :macro:`UNHIBERNATE`
:index:`UNHIBERNATE` expression becomes ``True``. In the typical
case, a pool runs a single instance of *condor_rooster* on the central
manager. However, if the network topology requires that Wake On LAN
packets be sent to specific machines from different locations,
*condor_rooster* can be run on any machine(s) that can read from the
pool's *condor_collector* daemon.

For *condor_rooster* to wake up hibernating machines, the collecting of
offline machine ClassAds must be enabled. See variable
:macro:`COLLECTOR_PERSISTENT_AD_LOG` for details on how to do this.

:macro-def:`ROOSTER_INTERVAL`
    The integer number of seconds between checks for offline machines
    that should be woken. The default value is 300.

:macro-def:`ROOSTER_MAX_UNHIBERNATE`
    An integer specifying the maximum number of machines to wake up per
    cycle. The default value of 0 means no limit.

:macro-def:`ROOSTER_UNHIBERNATE`
    A boolean expression that specifies which machines should be woken
    up. The default expression is ``Offline && Unhibernate``. If network
    topology or other considerations demand that some machines in a pool
    be woken up by one instance of *condor_rooster*, while others be
    woken up by a different instance, :macro:`ROOSTER_UNHIBERNATE` may
    be set locally such that it is different for the two instances of
    *condor_rooster*. In this way, the different instances will only
    try to wake up their respective subset of the pool.

:macro-def:`ROOSTER_UNHIBERNATE_RANK`
    A ClassAd expression specifying which machines should be woken up
    first in a given cycle. Higher ranked machines are woken first. If
    the number of machines to be woken up is limited by
    :macro:`ROOSTER_MAX_UNHIBERNATE`, the rank may be used for determining
    which machines are woken before reaching the limit.

:macro-def:`ROOSTER_WAKEUP_CMD`
    A string representing the command line invoked by *condor_rooster*
    that is to wake up a machine. The command and any arguments should
    be enclosed in double quote marks, the same as
    :subcom:`arguments` syntax in
    an HTCondor submit description file. The default value is
    "$(BIN)/condor_power -d -i". The command is expected to read from
    its standard input a ClassAd representing the offline machine.
