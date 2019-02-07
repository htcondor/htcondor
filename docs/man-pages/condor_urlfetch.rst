      

condor\_urlfetch
================

fetch configuration given a URL

Synopsis
--------

**condor\_urlfetch** [**-<daemon>**\ ] *url* *local-url-cache-file*

Description
-----------

Depending on the command line arguments, *condor\_urlfetch* sends the
result of a query from the *url* to both standard output and to a file
specified by *local-url-cache-file*, or it sends the contents of the
file specified by *local-url-cache-file* to standard output.

*condor\_urlfetch* is intended to be used as the program to run when
defining configuration, such as in the nonfunctional example:

::

    LOCAL_CONFIG_FILE = $(LIBEXEC)/condor_urlfetch -$(SUBSYSTEM) \ 
      http://www.example.com/htcondor-baseconfig  local.config |

The pipe character (\|) at the end of this definition of the location of
a configuration file changes the use of the definition. It causes the
command listed on the right hand side of this assignment statement to be
invoked, and standard output becomes the configuration. The value of
``$(SUBSYSTEM)`` becomes the daemon that caused this configuration to be
read. If ``$(SUBSYSTEM)`` evaluates to ``MASTER``, then the URL query
always occurs, and the result is sent to standard output as well as
written to the file specified by argument *local-url-cache-file*. When
``$(SUBSYSTEM)`` evaluates to a daemon other than ``MASTER``, then the
URL query only occurs if the file specified by *local-url-cache-file*
does not exist. If the file specified by *local-url-cache-file* does
exist, then the contents of this file is sent to standard output.

Note that if the configuration kept at the URL site changes, and
reconfiguration is requested, the **-<daemon>** argument needs to be
``-MASTER``. This is the only way to guarantee that there will be a
query of the changed URL contents, such that they will make their way
into the configuration.

Options
-------

 **-<daemon>**
    The upper case name of the daemon issuing the request for the
    configuration output. If ``-MASTER``, then the URL query always
    occurs. If a daemon other than ``-MASTER``, for example ``STARTD``
    or ``SCHEDD``, then the URL query only occurs if the file defined by
    *local-url-cache-file* does not exist.

Exit Status
-----------

*condor\_urlfetch* will exit with a status value of 0 (zero) upon
success and non zero otherwise.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
