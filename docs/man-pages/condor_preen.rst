*condor_preen*
===============

remove extraneous files from HTCondor directories
:index:`condor_preen<single: condor_preen; HTCondor commands>`\ :index:`condor_preen command`

Synopsis
--------

**condor_preen** [**-mail** ] [**-remove** ] [**-verbose** ]
[**-debug** ] [**-log <filename>**]

Description
-----------

*condor_preen* examines the directories belonging to HTCondor, and
removes extraneous files and directories which may be left over from
HTCondor processes which terminated abnormally either due to internal
errors or a system crash. The directories checked are the :macro:`LOG`,
:macro:`EXECUTE`, and :macro:`SPOOL` directories as defined in the HTCondor
configuration files. *condor_preen* is intended to be run as user root
or user condor periodically as a backup method to ensure reasonable file
system cleanliness in the face of errors. This is done automatically by
default by the *condor_master* daemon. It may also be explicitly
invoked on an as needed basis.

When *condor_preen* cleans the :macro:`SPOOL` directory, it always leaves
behind the files specified in the configuration variables
:macro:`VALID_SPOOL_FILES` and :macro:`SYSTEM_VALID_SPOOL_FILES`, as
given by the configuration. For the :macro:`LOG` directory, the only files
removed or reported are those listed within the configuration variable
:macro:`INVALID_LOG_FILES` list. The reason
for this difference is that, in general, the files in the :macro:`LOG`
directory ought to be left alone, with few exceptions. An example of
exceptions are core files. As there are new log files introduced
regularly, it is less effort to specify those that ought to be removed
than those that are not to be removed.

Options
-------

 **-mail**
    Send mail to the user defined in the :macro:`PREEN_ADMIN` configuration
    variable, instead of writing to the standard output.
 **-remove**
    Remove the offending files and directories rather than reporting on
    them.
 **-verbose**
    List all files or directories found in the Condor directories and considered
    for deletion, even those which are not extraneous. This option also modifies the output produced by
    the **-debug** and **-log** options
 **-debug**
    Print extra debugging information to stderr as the command executes.
 **-log <filename>**
    Write extra debugging information to <filename> as the command executes.

Exit Status
-----------

*condor_preen* will exit with a status value of 0 (zero) upon success,
and it will exit with a non-zero value upon failure.  An exit status
of 2 indicates that *condor_preen* attempted to send email about deleted
files but was unable to. This usually indicates an error in the configuration
for sending email.  An exit status of 1 indicates a general failure.

