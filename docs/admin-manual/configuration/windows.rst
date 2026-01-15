:index:`Windows Specific Options<single: Configuration; Windows Specific Options>`

.. _windows_config_options:

Windows Specific Configuration Options
======================================

These macros are utilized only on Windows platforms.

:macro-def:`WINDOWS_RMDIR`
    The complete path and executable name of the HTCondor version of the
    built-in *rmdir* program. The HTCondor version will not fail when
    the directory contains files that have ACLs that deny the SYSTEM
    process delete access. If not defined, the built-in Windows *rmdir*
    program is invoked, and a value defined for :macro:`WINDOWS_RMDIR_OPTIONS`
    is ignored.

:macro-def:`WINDOWS_RMDIR_OPTIONS`
    Command line options to be specified when configuration variable
    :macro:`WINDOWS_RMDIR` is defined. Defaults to **/S** **/C** when
    configuration variable :macro:`WINDOWS_RMDIR` is defined and its
    definition contains the string ``"condor_rmdir.exe"``.
