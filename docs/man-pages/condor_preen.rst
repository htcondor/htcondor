      

condor\_preen
=============

remove extraneous files from HTCondor directories

Synopsis
--------

**condor\_preen** [**-mail**\ ] [**-remove**\ ] [**-verbose**\ ]
[**-debug**\ ]

Description
-----------

*condor\_preen* examines the directories belonging to HTCondor, and
removes extraneous files and directories which may be left over from
HTCondor processes which terminated abnormally either due to internal
errors or a system crash. The directories checked are the ``LOG``,
``EXECUTE``, and ``SPOOL`` directories as defined in the HTCondor
configuration files. *condor\_preen* is intended to be run as user root
or user condor periodically as a backup method to ensure reasonable file
system cleanliness in the face of errors. This is done automatically by
default by the *condor\_master* daemon. It may also be explicitly
invoked on an as needed basis.

When *condor\_preen* cleans the ``SPOOL`` directory, it always leaves
behind the files specified in the configuration variables
``VALID_SPOOL_FILES`` and ``SYSTEM_VALID_SPOOL_FILES`` , as given by the
configuration. For the ``LOG`` directory, the only files removed or
reported are those listed within the configuration variable
``INVALID_LOG_FILES`` list. The reason for this difference is that, in
general, the files in the ``LOG`` directory ought to be left alone, with
few exceptions. An example of exceptions are core files. As there are
new log files introduced regularly, it is less effort to specify those
that ought to be removed than those that are not to be removed.

Options
-------

 **-mail**
    Send mail to the user defined in the ``PREEN_ADMIN`` configuration
    variable, instead of writing to the standard output.
 **-remove**
    Remove the offending files and directories rather than reporting on
    them.
 **-verbose**
    List all files found in the Condor directories, even those which are
    not considered extraneous.
 **-debug**
    Print extra debugging information as the command executes.

Exit Status
-----------

*condor\_preen* will exit with a status value of 0 (zero) upon success,
and it will exit with the value 1 (one) upon failure.

Author
------

Center for High Throughput Computing, University of Wisconsin–Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

      
