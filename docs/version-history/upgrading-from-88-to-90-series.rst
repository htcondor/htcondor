Upgrading from the 8.8 series to the 9.0 series of HTCondor
===========================================================

:index:`items to be aware of<single: items to be aware of; upgrading>`

HTCondor 9.0 introduces many security improvements.  As a result, **we expect
that many 8.8 pools will require explicit administrator intervention after
the upgrade.**

**If you’re upgrading from an 8.9 release**, read the
`specific instructions for that <https://htcondor-wiki.cs.wisc.edu/index.cgi/wiki?p=UpgradingFromEightNineToNineZero>`_, instead.

The following steps will help you determine if you need to make changes,
and if so, which ones.

Step 1
------

The default HTCondor security configuration is no longer host-based.
Specifically, to allow HTCondor 9.0 to be secure by default, we have
commented out the line ``use security:host_based`` from the default
``/etc/condor/condor_config``, and have added a new configuration file,
``/etc/condor/config.d/00-htcondor-9.0.config``.  (This file will not
be overwritten by subsequent upgrades, so it is safe to modify.)  This
file adds the line

.. code-block:: condor-config

   use security:recommended_v9_0

which configures user-based security and requires encryption, authentication,
and integrity.  If you have already configured another daemon authentication
method (e.g. pool ``PASSWORD``, ``SSL``, ``GSI``, ``KERBEROS``, etc) at some
point in the past, you can comment out the above line in the file
``00-htcondor-9.0.config`` and skip to `Step 2`_ below.

If you have not already configured some other daemon authentication method
and thus are relying solely on host-based authentication (i.e. a list of
allowed hostnames or IP addresses), you have three options:

- **Option A**.  Use `get_htcondor` to reinstall your pool with a fresh
  installation; see the :doc:`instructions </getting-htcondor/index>`.
  The `get_htcondor` tool will configure your pool with our recommended
  security configuration for you.  Once it's done, you can copy your
  site-specific configuration from your old installation to the new
  installation by placing configuration files into ``/etc/condor/config.d``.

  Continue with `Step 4`_ below.

- **Option B**.  Run two commands (as root) on every machine in your pool to
  enable the recommended security configuration appropriate for v9.0.  When
  prompted, type the same password for every machine. (*Note:* If typing a
  password is problematic, see the
  `man page <https://htcondor.readthedocs.io/en/latest/man-pages/condor_store_cred.html>`_
  for other options such as reading the password from a file or command-line).

  .. code-block:: shell

        # condor_store_cred -c add
        # umask 0077; condor_token_create -identity condor@mypool > /etc/condor/tokens.d/condor@mypool

  Continue with `Step 4`_ below.

- **Option C**.  Revert to the previous host-based security configuration that
  was the default before v9.0.  This is the most expedient way to get your
  pool running again as it did before upgrading, but realize that a host-based
  security model is not recommended.  If you go for this option, please
  consider it a temporary measure.  To configure HTCondor to function as it
  did before the upgrade, see the instructions in
  ``/etc/condor/config.d/00-htcondor-9.0.config``.

Step 2
------

If you chose option **A** or option **B** in step 1, skip this step.

If you did not previously set ``ALLOW_DAEMON`` explicitly, you will now
need to do so.  To duplicate the 8.8 behavior, set
``ALLOW_DAEMON = $(ALLOW_WRITE)``.

Step 3
------

If you chose option **A** or option **B** in step 1, skip this step.

The deprecated configuration settings beginning with ``HOSTALLOW`` and
``HOSTDENY`` have been removed.  If your 8.8 configuration was still
using either, add their entries to the corresponding ``ALLOW`` or ``DENY`` list.

If you run the *condor_check_config* tool it will detect a couple of the
most common configuration values that should be changed after an upgrade.

Step 4
------

The following changes may affect but your pool, but are not security
improvements.  You may have to take action to continue using certain
features of HTCondor.  If you don't use the feature, you may ignore
its entry.

- Singularity jobs no longer mount the user's home directory by default.
  To re-enable this, set the knob ``SINGULARITY_MOUNT_HOME = true``.
  :ticket:`6676`

- The ``SHARED_PORT_PORT`` setting is now honored.  If you are using
  a non-standard port on machines other than the Central Manager, this
  bug fix will a require configuration change in order to specify
  the non-standard port.
  :ticket:`7697`

- If ``EXECUTE`` and/or ``LOCAL_UNIV_EXECUTE`` are on NFS with root squash,
  permissions on these subdirectories will need to be changed from the
  default of ``0755`` to ``1777``.

- Users of *bosco_cluster* will have to re-run ``bosco_cluster --add`` for
  all remote clusters they are using.
  :jira:`274`

- *condor_gpu_discovery* will now report short-UUID-based stable GPU IDs by
  default.  Add ``-by-index`` to ``GPU_DISCOVERY_EXTRA`` to go back to the
  8.8-compatible index-based GPU IDs.
  :jira:`145`

- Many API changes in the Python bindings: many new features, new packages,
  and many interfaces have been deprecated.  In particular, see
  :ticket:`7808`, :ticket:`7607`, :ticket:`7337`, :ticket:`7261`,
  :ticket:`7109`, and :ticket:`6983` for potentially-breaking changes.
  (Too many other tickets to list.)

New Features
------------

Upgrading from the 8.8 series of HTCondor to the 9.0 series will bring
new features introduced in the 8.9 series of HTCondor. These new
features include the following (note that this list contains only the
most significant changes; a full list of changes can be found in the
version history: \ `Development Release Series
8.9 <../version-history/development-release-series-89.html>`_):

- Absent any configuration, a new HTCondor installation denies authorization to all users

- AES encryption is used for all communication and file transfers by default (Hardware accelerated when available)

- New IDTOKEN authentication method enables fine-grained authorization control designed to replace GSI authentication

- Improved support for GPUs, including machines with multiple GPUs

- New condor_watch_q tool that efficiently provides live job status updates

- Many improvements to the Python bindings, including new bindings for DAGMan and chirp

- Improved curl, https, box, Amazon S3, and google drive file transfer plugins supporting uploads and authentication

- File transfer times are now recorded in the job log

- Added support for jobs that need to acquire and use OAUTH tokens

- Many memory footprint and performance improvements in DAGMan

- Submitter ceilings allow administrators to set limits on the number of running jobs per user across the pool

Other Changes
-------------

The following items are strictly informative.

- Any negotiator trusted by the collector is now trusted by schedds which
  trust the collector.  This may inform or simplify your (new) security
  configuration.
  :ticket:`6956`

- The packages have changed.  The ``condor-externals`` package is now empty,
  and the blahp is packaged in the ``blahp`` package.  The 9.0 release RPMs
  of HTCondor require additional packages from EPEL.
  :ticket:`7681`

- GPU monitoring is now on by default.
  :ticket:`7201`

- HTCondor now creates a number of directories on start-up, rather than
  fail later on when it needs them to exist.  See the ticket for details.
  :jira:`73`

- Kerberos and OAuth credentials may now be enabled on the same machine.
  :ticket:`7462`

- Added a new tool, *classad_eval*, that can evaluate a ClassAd expression in
  the context of ClassAd attributes, and print the result in ClassAd format.
  :ticket:`7339`

- Added a new authentication method, ``IDTOKENS``, which we recommend over
  ``PASSWORD`` unconditionally.
  :ticket:`6947`

.. Apparently SciTokens still aren't -- perhaps deliberately -- documented.
.. - `SciTokens <https://scitokens.org>`_ may now be used for authentication.
..  :ticket:`7011`
