Power Management
================

:index:`power management` :index:`green computing`
:index:`offline machine`

HTCondor supports placing machines in low power states. A machine in the
low power state is identified as being offline. Power setting decisions
are based upon HTCondor configuration.

Power conservation is relevant when machines are not in heavy use, or
when there are known periods of low activity within the pool.

Entering a Low Power State
--------------------------

:index:`entering a low power state<single: entering a low power state; power management>`

By default, HTCondor does not do power management. When desired, the
ability to place a machine into a low power state is accomplished
through configuration. This occurs when all slots on a machine agree
that a low power state is desired.

A slot's readiness to hibernate is determined by the evaluating the
``HIBERNATE`` :index:`HIBERNATE` configuration variable (see
the :ref:`admin-manual/configuration-macros:condor_startd configuration file
macros` section) within the context of the slot. Readiness is evaluated at
fixed intervals, as determined by the ``HIBERNATE_CHECK_INTERVAL``
:index:`HIBERNATE_CHECK_INTERVAL` configuration variable. A
non-zero value of this variable enables the power management facility.
It is an integer value representing seconds, and it need not be a small
value. There is a trade off between the extra time not at a low power
state and the unnecessary computation of readiness.

To put the machine in a low power state rapidly after it has become
idle, consider checking each slot's state frequently, as in the example
configuration:

.. code-block:: condor-config

    HIBERNATE_CHECK_INTERVAL = 20

This checks each slot's readiness every 20 seconds. A more common value
for frequency of checks is 300 (5 minutes). A value of 300 loses some
degree of granularity, but it is more reasonable as machines are likely
to be put in to a low power state after a few hours, rather than
minutes.

A slot's readiness or willingness to enter a low power state is
determined by the ``HIBERNATE`` expression. Because this expression is
evaluated in the context of each slot, and not on the machine as a
whole, any one slot can veto a change of power state. The ``HIBERNATE``
expression may reference a wide array of variables. Possibilities
include the change in power state if none of the slots are claimed, or
if the slots are not in the Owner state.

Here is a concrete example. Assume that the ``START`` expression is not
set to always be ``True``. This permits an easy determination whether or
not the machine is in an Unclaimed state through the use of an auxiliary
macro called ``ShouldHibernate``.

.. code-block:: condor-config

    TimeToWait  = (2 * $(HOUR))
    ShouldHibernate = ( (KeyboardIdle > $(StartIdleTime)) \
                        && $(CPUIdle) \
                        && ($(StateTimer) > $(TimeToWait)) )

This macro evaluates to ``True`` if the following are all ``True``:

-  The keyboard has been idle long enough.
-  The CPU is idle.
-  The slot has been Unclaimed for more than 2 hours.

The sample ``HIBERNATE`` expression that enters the power state called
"RAM", if ``ShouldHibernate`` evaluates to ``True``, and remains in its
current state otherwise is

.. code-block:: condor-config

    HibernateState  = "RAM"
    HIBERNATE = ifThenElse($(ShouldHibernate), $(HibernateState), "NONE" )

If any slot returns "NONE", that slot vetoes the decision to enter a low
power state. Only when values returned by all slots are all non-zero is
there a decision to enter a low power state. If all agree to enter the
low power state, but differ in which state to enter, then the largest
magnitude value is chosen.

Returning From a Low Power State
--------------------------------

:index:`leaving a low power state<single: leaving a low power state; power management>`

The HTCondor command line tool *condor_power* may wake a machine from a
low power state by sending a UDP Wake On LAN (WOL) packet. See the
:doc:`/man-pages/condor_power` manual page.
:index:`condor_rooster daemon`

To automatically call *condor_power* under specific conditions,
*condor_rooster* may be used. The configuration options for
*condor_rooster* are described in the 
:ref:`admin-manual/configuration-macros:condor_rooster configuration file
macros` section.

Keeping a ClassAd for a Hibernating Machine
-------------------------------------------

A pool's *condor_collector* daemon can be configured to keep a
persistent ClassAd entry for each machine, once it has entered
hibernation. This is required by *condor_rooster* so that it can
evaluate the ``UNHIBERNATE`` :index:`UNHIBERNATE` expression of
the offline machines.

To do this, define a log file using the ``OFFLINE_LOG``
:index:`OFFLINE_LOG` configuration variable. See
the :ref:`admin-manual/configuration-macros:condor_startd configuration file
macros` section for the definition. An optional expiration time for each
ClassAd can be specified with ``OFFLINE_EXPIRE_ADS_AFTER``
:index:`OFFLINE_EXPIRE_ADS_AFTER`. The timing begins from the time
the hibernating machine's ClassAd enters the *condor_collector* daemon.
See the :ref:`admin-manual/configuration-macros:condor_startd configuration
file macros` section for the definition.

Linux Platform Details
----------------------

:index:`Linux platform details<single: Linux platform details; power management>`

Depending on the Linux distribution and version, there are three methods
for controlling a machine's power state. The methods:

#. *pm-utils* is a set of command line tools which can be used to detect
   and switch power states. In HTCondor, this is defined by the string
   "pm-utils".
#. The directory in the virtual file system ``/sys/power`` contains
   virtual files that can be used to detect and set the power states. In
   HTCondor, this is defined by the string "/sys".
#. The directory in the virtual file system ``/proc/acpi`` contains
   virtual files that can be used to detect and set the power states. In
   HTCondor, this is defined by the string "/proc".

By default, the HTCondor attempts to detect the method to use in the
order shown. The first method detected as usable on the system is
chosen.

This ordered detection may be bypassed, to use a specified method
instead by setting the configuration variable
``LINUX_HIBERNATION_METHOD`` with one of the defined strings. This
variable is defined in the :ref:`admin-manual/configuration-macros:condor_startd
configuration file macros` section. If no usable methods are detected or the
method specified by ``LINUX_HIBERNATION_METHOD`` is either not detected or
invalid, hibernation is disabled.

The details of this selection process, and the final method selected can
be logged via enabling ``D_FULLDEBUG`` in the relevant subsystem's log
configuration.

Windows Platform Details
------------------------

:index:`Windows platform troubleshooting<single: Windows platform troubleshooting; power management>`

If after a suitable amount of time, a Windows machine has not entered
the expected power state, then HTCondor is having difficulty exercising
the operating system's low power capabilities. While the cause will be
specific to the machine's hardware, it may also be due to improperly
configured software. For hardware difficulties, the likely culprit is
the configuration within the machine's BIOS, for which HTCondor can
offer little guidance. For operating system difficulties, the *powercfg*
tool can be used to discover the available power states on the machine.
The following command demonstrates how to list all of the supported
power states of the machine:

.. code-block:: doscon

    > powercfg -A
    The following sleep states are available on this system:
    Standby (S3) Hibernate Hybrid Sleep
    The following sleep states are not available on this system:
    Standby (S1)
            The system firmware does not support this standby state.
    Standby (S2)
            The system firmware does not support this standby state.

Note that the ``HIBERNATE`` expression is written in terms of the Sn
state, where n is the value evaluated from the expression.

This tool can also be used to enable and disable other sleep states.
This example turns hibernation on.

.. code-block:: doscon

    > powercfg -h on

If this tool is insufficient for configuring the machine in the manner
required, the *Power Options* control panel application offers the full
extent of the machine's power management abilities. Windows 2000 and XP
lack the *powercfg* program, so all configuration must be done via the
*Power Options* control panel application.
:index:`green computing` :index:`power management`
:index:`administrators manual`


