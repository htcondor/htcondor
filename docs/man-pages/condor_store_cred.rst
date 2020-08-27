      

*condor_store_cred*
=====================

securely stash a password
:index:`condor_store_cred<single: condor_store_cred; HTCondor commands>`\ :index:`condor_store_cred command`

Synopsis
--------

**condor_store_cred** [**-help** ]

**condor_store_cred** *add* [**-c** | **-u** *username*
][\ **-p** *password*] [**-n** *machinename*]
[**-f** *filename*]

**condor_store_cred** *delete* [**-c** | **-u** *username*
][\ **-n** *machinename*]

**condor_store_cred** *query* [**-c** | **-u** *username*
][\ **-n** *machinename*]

Description
-----------

*condor_store_cred* stores passwords in a secure manner. There are two
separate uses of *condor_store_cred*:

#. A shared pool password is needed in order to implement the
   ``PASSWORD`` authentication method. *condor_store_cred* using the
   **-c** option deals with the password for the implied
   condor_pool@$(UID_DOMAIN) user name.

   On a Unix machine, *condor_store_cred* with the **-f** option is
   used to set the pool password, as needed when used with the
   ``PASSWORD`` authentication method. The pool password is placed in a
   file specified by the ``SEC_PASSWORD_FILE`` configuration variable.

#. In order to submit a job from a Windows platform machine, or to
   execute a job on a Windows platform machine utilizing the
   **run_as_owner** :index:`run_as_owner<single: run_as_owner; submit commands>`
   functionality, *condor_store_cred* stores the password of a
   user/domain pair securely in the Windows registry. Using this stored
   password, HTCondor may act on behalf of the submitting user to access
   files, such as writing output or log files. HTCondor is able to run
   jobs with the user ID of the submitting user. The password is stored
   in the same manner as the system does when setting or changing
   account passwords.

Passwords are stashed in a persistent manner; they are maintained across
system reboots.

The *add* argument on the Windows platform stores the password securely
in the registry. The user is prompted to enter the password twice for
confirmation, and characters are not echoed. If there is already a
password stashed, the old password will be overwritten by the new
password.

The *delete* argument deletes the current password, if it exists.

The *query* reports whether the password is stored or not.

Options
-------

 **-c**
    Operations refer to the pool password, as used in the ``PASSWORD``
    authentication method.
 **-f** *filename*
    For Unix machines only, generates a pool password file named
    *filename* that may be used with the ``PASSWORD`` authentication
    method.
 **-help**
    Displays a brief summary of command options.
 **-n** *machinename*
    Apply the command on the given machine.
 **-p** *password*
    Stores *password*, rather than prompting the user to enter a
    password.
 **-u** *username*
    Specify the user name.

Exit Status
-----------

*condor_store_cred* will exit with a status value of 0 (zero) upon
success, and it will exit with the value 1 (one) upon failure.

