Configuration Macros
====================

:index:`configuration-macros<single: configuration-macros; HTCondor>`
:index:`configuration: macros`

The section contains a list of the individual configuration macros for
HTCondor. Before attempting to set up HTCondor configuration, you should
probably read the :doc:`/admin-manual/introduction-to-configuration` section
and possibly the 
:ref:`admin-manual/introduction-to-configuration:configuration templates`
section.

The settings that control the policy under which HTCondor will start,
suspend, resume, vacate or kill jobs are described in
:ref:`admin-manual/ep-policy-configuration:*condor_startd* policy configuration`,
not in this section.

General Configuration
---------------------

.. toctree::
    :maxdepth: 2
    :glob:

    global.rst
    security.rst


Management Daemon Configuration
-------------------------------

.. toctree::
    :maxdepth: 2
    :glob:

    master.rst
    shared_port.rst
    procd.rst
    preen.rst

Central Manager Configuration
-----------------------------

.. toctree::
    :maxdepth: 2
    :glob:

    collector.rst
    negotiator.rst
    credd.rst
    defrag.rst
    gangliad.rst

Access Point Configuration
--------------------------

.. toctree::
    :maxdepth: 2
    :glob:

    schedd.rst
    shadow.rst
    submit.rst
    dagman.rst
    gridmanager.rst
    job_router.rst

Execution Point Configuration
-----------------------------

.. toctree::
    :maxdepth: 2
    :glob:

    startd.rst
    starter.rst
    rooster.rst
    virtual_machines.rst


Other Configuration
-------------------

.. toctree::
    :maxdepth: 2
    :glob:

    high_availability.rst
    hooks.rst
    ssh_to_job.rst

    windows.rst
    annex.rst

