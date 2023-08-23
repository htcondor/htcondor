*condor_remote_cluster*
=======================

Manage and configure the clusters to be accessed.
:index:`condor_remote_cluster<single: condor_remote_cluster; HTCondor commands>`
:index:`condor_remote_cluster command`

Synopsis
--------

**condor_remote_cluster** [-**h** || --**help**]

**condor_remote_cluster** [-**l** || --**list**] [-**a** || --**add <host>
[schedd]**] [-**r** || --**remove <host>**] [-**s** || --**status
<host>**] [-**t** || --**test <host>**]

Description
-----------

*condor_remote_cluster* is part of a feature for accessing high
throughput computing resources from a local desktop using only an SSH
connection.

*condor_remote_cluster* enables management and configuration of the
access point of the remote computing resource.
After initial setup, jobs can be submitted to the local job queue,
which are then forwarded to the remote system.

A **<host>** is of the form ``[user@]fqdn.example.com[:22]``.

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

