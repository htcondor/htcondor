      

*condor_cod*
=============

manage COD machines and jobs
:index:`condor_cod<single: condor_cod; HTCondor commands>`
:index:`condor_cod command`

Synopsis
--------

**condor_cod** [**-help | -version** ]

**condor_cod** *request* [
**-pool** *centralmanagerhostname[:portnumber]* |
**-name** *scheddname* ] | [**-addr** *"<a.b.c.d:port>"*] [
[**-help | -version** ] | [**-debug | -timeout N | -classad
file** ] ] [**-requirements expr** ] [**-lease N** ]

**condor_cod** *release* **-id** *ClaimID* [ [**-help |
-version** ] | [**-debug | -timeout N | -classad file** ] ]
[**-fast** ]

**condor_cod** *activate* **-id** *ClaimID* [ [**-help |
-version** ] | [**-debug | -timeout N | -classad file** ] ]
[**-keyword string | -jobad filename | -cluster N | -proc N |
-requirements expr** ]

**condor_cod** *deactivate* **-id** *ClaimID* [ [**-help |
-version** ] | [**-debug | -timeout N | -classad file** ] ]
[**-fast** ]

**condor_cod** *suspend* **-id** *ClaimID* [ [**-help |
-version** ] | [**-debug | -timeout N | -classad file** ] ]

**condor_cod** *renew* **-id** *ClaimID* [ [**-help | -version** ]
| [**-debug | -timeout N | -classad file** ] ]

**condor_cod** *resume* **-id** *ClaimID* [ [**-help | -version** ]
| [**-debug | -timeout N | -classad file** ] ]

**condor_cod** *delegate_proxy* **-id** *ClaimID* [ [**-help |
-version** ] | [**-debug | -timeout N | -classad file** ] ]
[**-x509proxy** *ProxyFile*]

Description
-----------

*condor_cod* issues commands that manage and use COD claims on
machines, given proper authorization.

Instead of specifying an argument of *request*, *release*, *activate*,
*deactivate*, *suspend*, *renew*, or *resume*, the user may invoke the
*condor_cod* tool by appending an underscore followed by one of these
arguments. As an example, the following two commands are equivalent:

::

        condor_cod release -id "<128.105.121.21:49973>#1073352104#4"

::

        condor_cod_release -id "<128.105.121.21:49973>#1073352104#4"

To make these extended-name commands work, hard link the extended name
to the *condor_cod* executable. For example on a Unix machine:

::

    ln condor_cod_request condor_cod

The *request* argument gives a claim ID, and the other commands
(*release*, *activate*, *deactivate*, *suspend*, and *resume*) use the
claim ID. The claim ID is given as the last line of output for a
*request*, and the output appears of the form:

::

    ID of new claim is: "<a.b.c.d:portnumber>#x#y"

An actual example of this line of output is

::

    ID of new claim is: "<128.105.121.21:49973>#1073352104#4"

The HTCondor manual has a complete description of COD.

Options
-------

 **-help**
    Display usage information
 **-version**
    Display version information
 **-pool** *centralmanagerhostname[:portnumber]*
    Specify a pool by giving the central manager's host name and an
    optional port number
 **-name** *scheddname*
    Send the command to a machine identified by *scheddname*
 **-addr** *"<a.b.c.d:port>"*
    Send the command to a machine located at *"<a.b.c.d:port>"*
 **-lease** *N*
    For the **request** of a new claim, automatically release the claim
    after *N* seconds.
 **request**
    Create a new COD claim
 **release**
    Relinquish a claim and kill any running job
 **activate**
    Start a job on a given claim
 **deactivate**
    Kill the current job, but keep the claim
 **suspend**
    Suspend the job on a given claim
 **renew**
    Renew the lease to the COD claim
 **resume**
    Resume the job on a given claim
 **delegate_proxy**
    Delegate an X509 proxy for the given claim

General Remarks
---------------

Examples
--------

Exit Status
-----------

*condor_cod* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
