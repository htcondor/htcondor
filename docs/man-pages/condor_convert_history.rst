      

*condor_convert_history*
==========================

Convert the history file to the new format

Synopsis
--------

**condor_convert_history** [**-help** ]

**condor_convert_history** *history-file1* [*history-file2...* ]
:index:`condor_convert_history<single: condor_convert_history; Condor commands>`
:index:`condor_convert_history command`

Description
-----------

As of Condor version 6.7.19, the Condor history file has a new format to
allow fast searches backwards through the file. Not all queries can take
advantage of the speed increase, but the ones that can are significantly
faster.

Entries placed in the history file after upgrade to Condor 6.7.19 will
automatically be saved in the new format. The new format adds
information to the string which distinguishes and separates job entries.
In order to search within this new format, no changes are necessary.
However, to be able to search the entire history, the history file must
be converted to the updated format. *condor_convert_history* does
this.

The *condor_convert_history* command can also be used to reconstruct
the new format in a history file that has been corrupted or
concantenated with another history file.

Turn the *condor_schedd* daemon off while converting history files.
Turn it back on after conversion is completed.

Arguments to *condor_convert_history* are the history files to
convert. The history file is normally in the Condor spool directory; it
is named ``history``. Since the history file is rotated, there may be
multiple history files, and all of them should be converted. On Unix
platform variants, the easiest way to do this is:

.. code-block:: console

    $ cd `condor_config_val SPOOL`
    condor_convert_history history*

*condor_convert_history* makes a back up of each original history
files in case of a problem. The names of these back up files are listed;
names are formed by appending the suffix .oldver to the original file
name. Move these back up files to a directory other than the spool
directory. If kept in the spool directory, *condor_history* will find
the back ups, and will appear to have duplicate jobs.

Exit Status
-----------

*condor_convert_history* will exit with a status value of 0 (zero)
upon success, and it will exit with the value 1 (one) upon failure.

