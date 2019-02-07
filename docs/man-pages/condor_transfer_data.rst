      

condor\_transfer\_data
======================

transfer spooled data

Synopsis
--------

**condor\_transfer\_data** [**-help \| -version**\ ]

**condor\_transfer\_data** [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*]
*cluster… \| cluster.process… \| user…* \|
**-constraint **\ *expression* …

**condor\_transfer\_data** [
**-pool **\ *centralmanagerhostname[:portnumber]* \|
**-name **\ *scheddname* ] \| [**-addr  **\ *"<a.b.c.d:port>"*] **-all**

Description
-----------

*condor\_transfer\_data* causes HTCondor to transfer spooled data. It is
meant to be used in conjunction with the **-spool** option of
*condor\_submit*, as in

::

    condor_submit -spool mysubmitfile

Submission of a job with the **-spool** option causes HTCondor to spool
all input files, the job event log, and any proxy across a connection to
the machine where the *condor\_schedd* daemon is running. After spooling
these files, the machine from which the job is submitted may disconnect
from the network or modify its local copies of the spooled files.

When the job finishes, the job has ``JobStatus`` = 4, meaning that the
job has completed. The output of the job is spooled, and
*condor\_transfer\_data* retrieves the output of the completed job.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-pool **\ *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager’s host name and an
    optional port number
 **-name **\ *scheddname*
    Send the command to a machine identified by *scheddname*
 **-addr **\ *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 *cluster*
    Transfer spooled data belonging to the specified cluster
 *cluster.process*
    Transfer spooled data belonging to a specific job in the cluster
 *user*
    Transfer spooled data belonging to the specified user
 **-constraint **\ *expression*
    Transfer spooled data for jobs which match the job ClassAd
    expression constraint
 **-all**
    Transfer all spooled data

Exit Status
-----------

*condor\_transfer\_data* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
