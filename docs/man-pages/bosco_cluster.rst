*bosco_cluster*
================

Manage and configure the clusters to be accessed.
:index:`bosco_cluster<single: bosco_cluster; Bosco commands>`
:index:`bosco_cluster command`

Synopsis
--------

**bosco_cluster** [-**h** || --**help**]

**bosco_cluster** [-**l** || --**list**] [-**a** || --**add <host>
[schedd]**] [-**r** || --**remove <host>**] [-**s** || --**status
<host>**] [-**t** || --**test <host>**]

Description
-----------

*bosco_cluster* is part of the Bosco system for accessing high
throughput computing resources from a local desktop. For detailed
information, please see the Bosco web site:
`https://osg-bosco.github.io/docs/ <https://osg-bosco.github.io/docs/>`_

*bosco_cluster* enables management and configuration of the computing
resources the Bosco tools access; these are called clusters.

A **<host>** is of the form ``user@fqdn.example.com``.

Options
-------

 **-help**
    Print usage information and exit.
 **-list**
    List all installed clusters.
 **-remove** *<host>*
    Remove an already installed cluster, where the cluster is identified
    by *<host>*.
 **-add** *<host> [scheduler]*
    Install and add a cluster defined by *<host>*. The optional
    *scheduler* specifies the scheduler on the cluster. Valid values are
    ``pbs``, ``lsf``, ``condor``, ``sge`` or ``slurm``. If not given,
    the default will be ``pbs``.
 **-status** *<host>*
    Query and print the status of an already installed cluster, where
    the cluster is identified by *<host>*.
 **-test** *<host>*
    Attempt to submit a test job to an already installed cluster, where
    the cluster is identified by *<host>*.

