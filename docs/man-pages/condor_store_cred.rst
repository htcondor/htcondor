      

*condor_store_cred*
=====================

securely stash a credential
:index:`condor_store_cred<single: condor_store_cred; HTCondor commands>`\ :index:`condor_store_cred command`

Synopsis
--------

**condor_store_cred** **-h**

**condor_store_cred** action [ *options* ]

Description
-----------

*condor_store_cred* stores credentials in a secure manner.  There are
three actions, each of which can optionally be followed by a hyphen and
one of three types.

The actions are:

 **add[-type]**
    Add credential to secure storage
 **delete[-type]**
    Remove credential from secure storage
 **query[-type]**
    Check if a credential has been stored

The types are:

 **-pwd**
    Credential is a password (default)
 **-krb**
    Credential is a Kerberos/AFS token
 **-oauth**
    Credential is Scitoken or Oauth2 token

Credentials are stashed in a persistent manner; they are maintained
across system reboots.  When adding a credential, if there is already a
credential stashed, the old credential will be overwritten by the new
one.

There are two separate uses of the password actions of 
*condor_store_cred*:

#. A shared pool password is needed in order to implement the
   ``PASSWORD`` authentication method. *condor_store_cred* using the
   **-c** option deals with the password for the implied
   condor_pool@$(UID_DOMAIN) user name.

   On a Unix machine, *condor_store_cred add[-pwd]* with the **-f** option
   is used to set the pool password, as needed when used with the
   ``PASSWORD`` authentication method. The pool password is placed in a
   file specified by the ``SEC_PASSWORD_FILE`` configuration variable.

#. In order to submit a job from a Windows platform machine, or to
   execute a job on a Windows platform machine utilizing the
   **run_as_owner** :index:`run_as_owner<single: run_as_owner; submit commands>`
   functionality, *condor_store_cred add[-pwd]* stores the password of a
   user/domain pair securely in the Windows registry. Using this stored
   password, HTCondor may act on behalf of the submitting user to access
   files, such as writing output or log files. HTCondor is able to run
   jobs with the user ID of the submitting user. The password is stored
   in the same manner as the system does when setting or changing
   account passwords.

Unless the *-p* argument is used with the *add* or *add-pwd* action, the
user is prompted to enter the password twice for confirmation, and
characters are not echoed. 

The *add-krb* and *add-oauth* actions must be used with the *-i* argument
to specify a filename to read from.

The *-oauth* actions require a *-s* service name argument.  The *-S*
and *-A* options may be used with *add-oauth* to add scopes and/or
audience to the credentials or with *query-oauth* to make sure that
the scopes or audience match the previously stored credentials.  If
either *-S* or *-A* are used then the credentials must be in JSON
format.

Options
-------

 **-h**
    Displays a brief summary of command options.
 **-c**
    *[-pwd]* actions refer to the pool password, as used in the ``PASSWORD``
    authentication method.
 **-f** *filename*
    For Unix machines only, generates a pool password file named
    *filename* that may be used with the ``PASSWORD`` authentication
    method.
 **-i** *filename*
    Read credential from *filename*.  If *filename* is *-*, read from
    stdin.  Required for *add-krb* and *add-oauth*.
 **-s** *service*
    The Oauth2 service.  Required for all *-oauth* actions.
 **-H** *handle*
    Specify a handle for the given OAuth2 service.
 **-S** *scopes*
    Optional comma-separated list of scopes to request for *add-oauth*
    action.  If used with the *query-oauth* action, makes sure that
    the same scopes were requested in the original credential.
    Requires credentials to be in JSON format.
 **-A** *audience*
    Optional audience to request for *add-oauth*
    action.  If used with the *query-oauth* action, makes sure that
    the same audience was requested in the original credential.
    Requires credentials to be in JSON format.
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
success.  If the *query-oauth* action finds a credential but the
scopes or audience don't match, *condor_store_cred* will exit
with a status value 2 (two).  Otherwise, it will exit with the value 1
(one) upon failure.

