:mod:`htcondor` -- HTCondor Reference
=====================================

.. module:: htcondor
   :platform: Unix, Windows, Mac OS X
   :synopsis: Interact with the HTCondor daemons
.. moduleauthor:: Brian Bockelman <bbockelm@cse.unl.edu>

This page is an exhaustive reference of the API exposed by the :mod:`htcondor`
module.  It is not meant to be a tutorial for new users but rather a helpful
guide for those who already understand the basic usage of the module.

This reference covers the following:

* :ref:`common_module_functions`: The more commonly-used :mod:`htcondor` functions.
* :class:`Schedd`: Interacting with the ``condor_schedd``.
* :class:`Collector`: Interacting with the ``condor_collector``.
* :class:`Submit`: Submitting to HTCondor.
* :class:`Claim`: Working with HTCondor claims.
* :class:`_Param`: Working with the parameter objects.
* :class:`JobEventLog`: Working with user event logs.
* :class:`JobEvent`: An event in a user event log.
* :ref:`esoteric_module_functions`: Less-commonly used :mod:`htcondor` functions.
* :ref:`useful_enums`: Useful enumerations.

.. _common_module_functions:

Common Module-Level Functions and Objects
-----------------------------------------

.. function:: platform()

   Returns the platform of HTCondor this module is running on.

.. function:: version()

   Returns the version of HTCondor this module is linked against.

.. function:: reload_config()

   Reload the HTCondor configuration from disk.

.. function:: enable_debug()

   Enable debugging output from HTCondor; output is sent to ``stderr``.
   The logging level is controlled by the HTCondor configuration variable
   ``TOOL_DEBUG``.

.. function:: enable_log()

   Enable debugging output from HTCondor; output is sent to a file.

   The log level and the file used are controlled by the HTCondor configuration
   variables ``TOOL_DEBUG`` and ``TOOL_LOG``, respectively.

.. function:: read_events( file_obj, is_xml = True )

   Read and parse an HTCondor event log file.

   :param file_obj: A file object corresponding to an HTCondor event log.
   :param bool is_xml: Specifies whether the event log is XML-formatted.
   :return: A Python iterator which produces objects of type :class:`ClassAd`.
   :rtype: :class:`EventIterator`

.. function:: poll( active_queries )

   Wait on the results of multiple query iteratories.

   This function returns an iterator which yields the next ready query iterator.
   The returned iterator stops when all results have been consumed for all iterators.

   :param active_queries: Query iterators as returned by xquery().
   :type active_queries: list[:class:`QueryIterator`]
   :return: An iterator producing the ready :class:`QueryIterator`.
   :rtype: :class:`BulkQueryIterator`

.. data:: param

   Provides dictionary-like access the HTCondor configuration.

   An instance of :class:`_Param`.  Upon importing the :mod:`htcondor` module, the
   HTCondor configuration files are parsed and populate this dictionary-like object.


.. _schedd_class:

Module Classes
--------------

.. class:: Schedd

   Client object for a remote ``condor_schedd``.

   .. method:: __init__( location_ad=None )

      Create an instance of the :class:`Schedd` class.

      :param location_ad: describes the location of the remote ``condor_schedd``
         daemon, as returned by the :meth:`Collector.locate` method. If the parameter is omitted,
         the local ``condor_schedd`` daemon is used.
      :type location_ad: :class:`~classad.ClassAd`

   .. method:: transaction(flags=0, continue_txn=False)

      Start a transaction with the ``condor_schedd``.

      Starting a new transaction while one is ongoing is an error unless the ``continue_txn``
      flag is set.

      :param flags: Flags controlling the behavior of the transaction, defaulting to 0.
      :type flags: :class:`TransactionFlags`
      :param bool continue_txn: Set to ``True`` if you would like this transaction to extend any
         pre-existing transaction; defaults to ``False``.  If this is not set, starting a transaction
         inside a pre-existing transaction will cause an exception to be thrown.
      :return: A transaction context manager object.

   .. method:: query( constraint='true', attr_list=[], callback=None, limit=-1, opts=QueryOpts.Default )

      Query the ``condor_schedd`` daemon for jobs.

      .. note:: This returns a *list* of :class:`~classad.ClassAd` objects, meaning all results must
         be buffered in memory.  This may be memory-intensive for large responses; we strongly recommend
         to utilize the :meth:`xquery`

      :param constraint: Query constraint; only jobs matching this constraint will be returned; defaults to ``'true'``.
      :type constraint: str or :class:`~classad.ExprTree`
      :param attr_list: Attributes for the ``condor_schedd`` daemon to project along.
         At least the attributes in this list will be returned.
         The default behavior is to return all attributes.
      :type attr_list: list[str]
      :param callback: A callable object; if provided, it will be invoked for each ClassAd.
         The return value (if note ``None``) will be added to the returned list instead of the
         ad.
      :param int limit: The maximum number of ads to return; the default (``-1``) is to return
         all ads.
      :param opts: Additional flags for the query; these may affect the behavior of the ``condor_schedd``.
      :type opts: :class:`QueryOpts`.
      :return: ClassAds representing the matching jobs.
      :rtype: list[:class:`~classad.ClassAd`]

   .. method:: xquery( requirements='true', projection=[] , limit=-1 , opts=QueryOpts.Default , name=None)

      Query the condor_schedd daemon for jobs.

      As opposed to :meth:`query`, this returns an *iterator*, meaning only one ad is buffered in memory at a time.

      :param requirements: provides a constraint for filtering out jobs. It defaults to ``'true'``.
      :type requirements: str or :class:`~classad.ExprTree`
      :param projection: The attributes to return; an empty list (the default) signifies all attributes.
      :type projection: list[str]
      :param int limit: A limit on the number of matches to return.  The default (``-1``) indicates all
         matching jobs should be returned.
      :param opts: Additional flags for the query, from :class:`QueryOpts`.
      :type opts: :class:`QueryOpts`
      :param str name: A tag name for the returned query iterator. This string will always be
         returned from the :meth:`QueryIterator.tag` method of the returned iterator.
         The default value is the ``condor_schedd``'s name. This tag is useful to identify
         different queries when using the :func:`poll` function.
      :return: An iterator for the matching job ads
      :rtype: :class:`~htcondor.QueryIterator`

   .. method:: act( action, job_spec )

      Change status of job(s) in the ``condor_schedd`` daemon. The return value is a ClassAd object
      describing the number of jobs changed.

      This will throw an exception if no jobs are matched by the constraint.

      :param action: The action to perform; must be of the enum JobAction.
      :type action: :class:`JobAction`
      :param job_spec: The job specification. It can either be a list of job IDs or a string specifying a constraint.
         Only jobs matching this description will be acted upon.
      :type job_spec: list[str] or str

   .. method:: edit( job_spec, attr, value )

      Edit one or more jobs in the queue.

      This will throw an exception if no jobs are matched by the ``job_spec`` constraint.

      :param job_spec: The job specification. It can either be a list of job IDs or a string specifying a constraint.
         Only jobs matching this description will be acted upon.
      :type job_spec: list[str] or str
      :param str attr: The name of the attribute to edit.
      :param value: The new value of the attribute.  It should be a string, which will
         be converted to a ClassAd expression, or an ExprTree object.  Be mindful of quoting
         issues; to set the value to the string ``foo``, one would set the value to ``'"foo"'``
      :type value: str or :class:`~classad.ExprTree`

   .. method:: history( requirements, projection, match=1 )

      Fetch history records from the ``condor_schedd`` daemon.

      :param requirements: Query constraint; only jobs matching this constraint will be returned;
         defaults to ``'true'``.
      :type constraint: str or :class:`class.ExprTree`
      :param projection: Attributes that are to be included for each returned job.
         The empty list causes all attributes to be included.
      :type projection: list[str]
      :param int match: An limit on the number of jobs to include; the default (``-1``)
         indicates to return all matching jobs.
      :return: All matching ads in the Schedd history, with attributes according to the
         ``projection`` keyword.
      :rtype: :class:`HistoryIterator`

   .. method:: submit( ad, count = 1, spool = false, ad_results = None )

      Submit one or more jobs to the ``condor_schedd`` daemon.

      This method requires the invoker to provide a ClassAd for the new job cluster;
      such a ClassAd contains attributes with different names than the commands in a
      submit description file. As an example, the stdout file is referred to as ``output``
      in the submit description file, but ``Out`` in the ClassAd.

      .. hint:: To generate an example ClassAd, take a sample submit description
         file and invoke::

            condor_submit -dump <filename> [cmdfile]

         Then, load the resulting contents of ``<filename>`` into Python.

      :param ad: The ClassAd describing the job cluster.
      :type ad: :class:`~classad.ClassAd`
      :param int count: The number of jobs to submit to the job cluster. Defaults to ``1``.
      :param bool spool: If ``True``, the clinent inserts the necessary attributes
         into the job for it to have the input files spooled to a remote
         ``condor_schedd`` daemon. This parameter is necessary for jobs submitted
         to a remote ``condor_schedd`` that use HTCondor file transfer.
      :param ad_results: If set to a list, the list object will contain the job ads
         resulting from the job submission.
         These are needed for interacting with the job spool after submission.
      :type ad_results: list[:class:`~classad.ClassAd`]
      :return: The newly created cluster ID.
      :rtype: int

   .. method:: submitMany( cluster_ad, proc_ads, spool = false, ad_results = None )

      Submit multiple jobs to the ``condor_schedd`` daemon, possibly including
      several distinct processes.

      :param cluster_ad: The base ad for the new job cluster; this is the same format
         as in the :meth:`submit` method.
      :type cluster_ad: :class:`~classad.ClassAd`
      :param list proc_ads: A list of 2-tuples; each tuple has the format of ``(proc_ad, count)``.
         For each list entry, this will result in count jobs being submitted inheriting from
         both ``cluster_ad`` and ``proc_ad``.
      :param bool spool: If ``True``, the clinent inserts the necessary attributes
         into the job for it to have the input files spooled to a remote
         ``condor_schedd`` daemon. This parameter is necessary for jobs submitted
         to a remote ``condor_schedd`` that use HTCondor file transfer.
      :param ad_results: If set to a list, the list object will contain the job ads
         resulting from the job submission.
         These are needed for interacting with the job spool after submission.
      :type ad_results: list[:class:`~classad.ClassAd`]
      :return: The newly created cluster ID.
      :rtype: int

   .. method:: spool(ad_list)

      Spools the files specified in a list of job ClassAds
      to the ``condor_schedd``.

      :param ad_list: A list of job descriptions; typically, this is the list
         filled by the ``ad_results`` argument of the :meth:`submit` method call.
      :type ad_list: list[:class:`~classad.ClassAds`]
      :raises RuntimeError: if there are any errors.

   .. method:: retrieve(job_spec)

      Retrieve the output sandbox from one or more jobs.

      :param job_spec: An expression matching the list of job output sandboxes
         to retrieve.
      :type job_spec: list[:class:`~classad.ClassAd`]

   .. method:: refreshGSIProxy(cluster, proc, filename, lifetime)

      Refresh the GSI proxy of a job; the job's proxy will be replaced the contents
      of the provided ``filename``.

      .. note:: Depending on the lifetime of the proxy in filename, the resulting lifetime
         may be shorter than the desired lifetime.

      :param int cluster: Cluster ID of the job to alter.
      :param int proc: Process ID of the job to alter.
      :param int lifetime: Indicates the desired lifetime (in seconds) of the delegated proxy.
         A value of ``0`` specifies to not shorten the proxy lifetime.
         A value of ``-1`` specifies to use the value of configuration variable
         ``DELEGATE_JOB_GSI_CREDENTIALS_LIFETIME``.

   .. method:: negotiate( (str)accounting_name )

      Begin a negotiation cycle with the remote schedd for a given user.

      .. note:: The returned :class:`ScheddNegotiate` additionally serves as a context manager,
         automatically destroying the negotiation session when the context is left.

      :param str accounting_name: Determines which user the client will start negotiating with.
      :return: An iterator which yields resource request ClassAds from the ``condor_schedd``.
         Each resource request represents a set of jobs that are next in queue for the schedd
         for this user.
      :rtype: :class:`ScheddNegotiate`

   .. method:: reschedule()

      Send reschedule command to the schedd.


.. class:: Collector

   Client object for a remote ``condor_collector``.  The interaction with the
   collector broadly has three aspects:

   * Locating a daemon.
   * Query the collector for one or more specific ClassAds.
   * Advertise a new ad to the ``condor_collector``.

   .. method:: __init__( pool = None )

      Create an instance of the :class:`Collector` class.

      :param pool: A ``host:port`` pair specified for the remote collector
         (or a list of pairs for HA setups). If omitted, the value of
         configuration parameter ``COLLECTOR_HOST`` is used.
      :type pool: str or list[str]

   .. method:: locate( daemon_type, name )

      Query the ``condor_collector`` for a particular daemon.

      :param daemon_type: The type of daemon to locate.
      :type daemon_type: :class:`DaemonTypes`
      :param str name: The name of daemon to locate. If not specified, it searches for the local daemon.
      :return: a minimal ClassAd of the requested daemon, sufficient only to contact the daemon;
         typically, this limits to the ``MyAddress`` attribute.
      :rtype: :class:`~classad.ClassAd`

   .. method:: locateAll( daemon_type )

      Query the condor_collector daemon for all ClassAds of a particular type. Returns a list of matching ClassAds.

      :param daemon_type: The type of daemon to locate.
      :type daemon_type: :class:`DaemonTypes`
      :return: Matching ClassAds
      :rtype: list[:class:`~classad.ClassAd`]

   .. method:: query( ad_type, constraint='true', attrs=[], statistics='' )

      Query the contents of a condor_collector daemon. Returns a list of ClassAds that match the constraint parameter.

      :param ad_type: The type of ClassAd to return. If not specified, the type will be ANY_AD.
      :type ad_type: :class:`AdTypes`
      :param constraint: A constraint for the collector query; only ads matching this constraint are returned.
         If not specified, all matching ads of the given type are returned.
      :type constraint: str or :class:`~classad.ExprTree`
      :param attrs: A list of attributes to use for the projection.  Only these attributes, plus a few server-managed,
         are returned in each :class:`~classad.ClassAd`.
      :type attrs: list[str]
      :param list[str] statistics: Statistics attributes to include, if they exist for the specified daemon.
      :return: A list of matching ads.
      :rtype: list[:class:`~classad.ClassAd`]

   .. directQuery( daemon_type, name = '', projection = [], statistics = '' )

      Query the specified daemon directly for a ClassAd, instead of using the ClassAd from the ``condor_collector`` daemon.
      Requires the client library to first locate the daemon in the collector, then querying the remote daemon.

      :param daemon_type: Specifies the type of the remote daemon to query.
      :type daemon_type: :class:`DaemonTypes`
      :param str name: Specifies the daemon's name. If not specified, the local daemon is used.
      :param projection: is a list of attributes requested, to obtain only a subset of the attributes from the daemon's :class:`~classad.ClassAd`.
      :type projection: list[str]
      :param statistics: Statistics attributes to include, if they exist for the specified daemon.
      :type statistics: str
      :return: The ad of the specified daemon.
      :rtype: :class:`~classad.ClassAd`

   .. method:: advertise( ad_list, command="UPDATE_AD_GENERIC", use_tcp=True )

      Advertise a list of ClassAds into the condor_collector.

      :param ad_list: :class:`~classad.ClassAds` to advertise.
      :type ad_list: list[:class:`~classad.ClassAds`]
      :param str command: An advertise command for the remote ``condor_collector``. It defaults to ``UPDATE_AD_GENERIC``.
         Other commands, such as ``UPDATE_STARTD_AD``, may require different authorization levels with the remote daemon.
      :param bool use_tcp: When set to true, updates are sent via TCP.  Defaults to ``True``.


.. class:: Submit

   An object representing a job submit description.  This uses the same submit
   language as ``condor_submit``.

   The submit description contains ``key = value`` pairs and implements the python
   dictionary protocol, including the ``get``, ``setdefault``, ``update``, ``keys``,
   ``items``, and ``values`` methods.

   .. method:: __init__( input = None )

      Create an instance of the Submit class.

      :param input: ``Key = value`` pairs for initializing the submit description.
         If omitted, the submit class is initially empty.
      :type input: dict

   .. method:: expand( attr )

      Expand all macros for the given attribute.

      :param str attr: The name of the relevant attribute.
      :return: The value of the given attribute; all macros are expanded.
      :rtype: str

   .. method:: queue( (object)txn, (int)count = 1, (object)ad_results = None )

      Submit the current object to a remote queue.

      :param txn: An active transaction object (see :meth:`Schedd.transaction`).
      :type txn: :class:`Transaction`
      :param int count: The number of jobs to create (defaults to ``1``).
      :param ad_results: A list to receive the ClassAd resulting from this submit.
         As with :meth:`Schedd.submit`, this is often used to later spool the input
         files.
      :return: The ClusterID of the submitted job(s).
      :rtype: int
      :raises RuntimeError: if the submission fails.


.. class:: Negotiator

   This class provides a query interface to the ``condor_negotiator``; primarily,
   it allows one to query and set various parameters in the fair-share accounting.

   .. method:: __init__( ad = None )

     Create an instance of the Negotiator class.

     :param ad: A ClassAd describing the claim and the ``condor_negotiator``
        location.  If omitted, the default pool negotiator is assumed.
     :type ad: :class:`~classad.ClassAd`

   .. method:: deleteUser( user )

      Delete all records of a user from the Negotiator's fair-share accounting.

      :param str user: A fully-qualified user name, i.e., ``USER@DOMAIN``.

   .. method:: getPriorities( [(bool)rollup = False ] )

      Retrieve the pool accounting information, one per entry.Returns a list of accounting ClassAds.

      :param bool rollup: Set to ``True`` if accounting information, as applied to hierarchical group quotas, should be summed for groups and subgroups.
      :return: A list of accounting ads, one per entity.
      :rtype: list[:class:`~classad.ClassAd`]

   .. method:: getResourceUsage( (str)user )

      Get the resources (slots) used by a specified user.

      :param str user: A fully-qualified user name, ``USER@DOMAIN``.
      :return: List of ads describing the resources (slots) in use.
      :rtype: list[:class:`~classad.ClassAd`]

   .. method:: resetAllUsage( )

      Reset all usage accounting.  All known user records in the negotiator are deleted.

   .. method:: resetUsage( user )

      Reset all usage accounting of the specified user.

      :param str user: A fully-qualified user name, ``USER@DOMAIN``.

   .. method:: setBeginUsage( user, value )

      Manually set the time that a user begins using the pool.

      :param str user: A fully-qualified user name, ``USER@DOMAIN``.
      :param int value: The Unix timestamp of initial usage.

   .. method:: setLastUsage( user, value )

      Manually set the time that a user last used the pool.

      :param str user: A fully-qualified user name, ``USER@DOMAIN``.
      :param int value: The Unix timestamp of last usage.

   .. method:: setFactor( user, factor )

      Set the priority factor of a specified user.

      :param str user: A fully-qualified user name, ``USER@DOMAIN``.
      :param float factor: The priority factor to be set for the user; must be greater-than or equal-to 1.0.

   .. method:: setPriority( user, prio )

      Set the real priority of a specified user.

      :param str user: A fully-qualified user name, ``USER@DOMAIN``.
      :param float prio: The priority to be set for the user; must be greater-than 0.0.

   .. method:: setUsage( user, usage )

      Set the accumulated usage of a specified user.

      :param str user: A fully-qualified user name, ``USER@DOMAIN``.
      :param float usage: The usage, in hours, to be set for the user.


.. class:: Startd

   .. method:: __init__( ad = None )

      Create an instance of the Startd class.

      :param ad: A ClassAd describing the claim and the startd location.
         If omitted, the local startd is assumed.
      :type ad: :class:`~classad.ClassAd`

   .. drainJobs( drain_type = Graceful, (bool)resume_on_completion = false, (expr)check_expr = true )

      Begin draining jobs from the startd.

      :param drain_type: How fast to drain the jobs.  Defaults to Graceful if not specified.
      :type drain_type: :class:`DrainTypes`
      :param bool resume_on_completion: Whether the startd should start accepting jobs again
         once draining is complete.  Otherwise, it will remain in the drained state.
         Defaults to False.
      :param str check_expr: An expression string that must evaluate to ``true`` for all slots for
         draining to begin. Defaults to ``"true"`` if not specified.
      :return: An opaque request ID that can be used to cancel draining.
      :rtype: str

   .. method:: cancelDrainJobs( request_id = None )

      Cancel a draining request.

      :param str request_id: Specifies a draining request to cancel.  If not specified, all
         draining requests for this startd are canceled.


.. class:: SecMan

   A class, representing the internal HTCondor security state.

   If a security session becomes invalid, for example, because the remote daemon restarts,
   reuses the same port, and the client continues to use the session, then all future
   commands will fail with strange connection errors. This is the only mechanism to
   invalidate in-memory sessions.

   The :class:`SecMan` can also behave as a context manager; when created, the object can
   be used to set temporary security configurations that only last during the lifetime
   of the security object.

   .. method:: __init__( )

      Create a SecMan object.

   .. method:: invalidateAllSessions( )

      Invalidate all security sessions. Any future connections to a daemon will
      cause a new security session to be created.

   .. method:: ping ( ad, command='DC_NOP' )

      Perform a test authorization against a remote daemon for a given command.

      :param ad: The ClassAd of the daemon as returned by :meth:`Collector.locate`;
         alternately, the sinful string can be given directly as the first parameter.
      :type ad: str or :class:`~classad.ClassAd`
      :param command: The DaemonCore command to try; if not given, ``'DC_NOP'`` will be used.
      :return: An ad describing the results of the test security negotiation.
      :rtype: :class:`~classad.ClassAd`

   .. method:: getCommandString(commandInt)

      Return the string name corresponding to a given integer command.

   .. method:: setConfig(key, value)

      Set a temporary configuration variable; this will be kept for all security
      sessions in this thread for as long as the :class:`SecMan` object is alive.

      :param str key: Configuration key to set.
      :param str value: Temporary value to set.

   .. method:: setGSICredential(filename)

      Set the GSI credential to be used for security negotiation.

      :param str filename: File name of the GSI credential.

   .. method:: setPoolPassword(new_pass)

      Set the pool password

      :param str new_pass: Updated pool password to use for new
         security negotiations.

   .. method:: setTag(tag)

      Set the authentication context tag for the current thread.

      All security sessions negotiated with the same tag will only
      be utilized when that tag is active.

      For example, if thread A has a tag set to ``Joe`` and thread B
      has a tag set to ``Jane``, then all security sessions negotiated
      for thread A will not be used for thread B.

      :param str tag: New tag to set.


.. class:: Claim

   The :class:`Claim` class provides access to HTCondor's Compute-on-Demand
   facilities.  The class represents a claim of a remote resource; it allows
   the user to manually activate a claim (start a job) or release the associated
   resources.

   The claim comes with a finite lifetime - the *lease*.  The lease may be
   extended for as long as the remote resource (the Startd) allows.

   .. method:: __init__( ad )

      Create a :class:`Claim` object of a given remote resource.
      The ad provides a description of the resource, as returned
      by :meth:`Collector.locate`.

      This only stores the remote resource's location; it is not
      contacted until :meth:`requestCOD` is invoked.

      :param ad: Location of the Startd to claim.
      :type ad: :class:`~classad.ClassAd`

   .. method:: requestCOD( constraint, lease_duration )

      Request a claim from the condor_startd represented by this object.

      On success, the :class:`Claim` object will represent a valid claim on the
      remote startd; other methods, such as :meth:`activate` should now function.

      :param str constraint:  ClassAd expression that pecifies which slot in
         the startd should be claimed.  Defaults to ``'true'``, which will
         result in the first slot becoming claimed.
      :param int lease_duration: Indicates how long the claim should be valid.
         Defaults to ``-1``, which indicates to lease the resource for as long
         as the Startd allows.

   .. method:: activate( ad )

      Activate a claim using a given job ad.

      :param ad: Description of the job to launch; this uses similar, *but not identical*
         attribute names as ``condor_submit``.  See
         `the HTCondor manual <http://research.cs.wisc.edu/htcondor/manual/v8.5/4_3Computing_On.html#SECTION00533100000000000000>`_
         for a description of the job language.

   .. method:: release( vacate_type )

      Release the remote ``condor_startd`` from this claim; shut down any running job.

      :param vacate_type: Indicates the type of vacate to perform for the
         running job.
      :type vacate_type: :class:`VacateTypes`

   .. method:: suspend( )

      Temporarily suspend the remote execution of the COD application.
      On Unix systems, this is done using ``SIGSTOP``.

   .. method:: resume( )

      Resume the temporarily suspended execution.
      On Unix systems, this is done using ``SIGCONT``.

   .. method:: renew()

      Renew the lease on an existing claim.
      The renewal should last for the value of ``lease_duration`` provided to
      :meth:`__init__`.

   .. method:: deactivate()

      Deactivate a claim; shuts down the currently running job,
      but holds onto the claim for future activation.

   .. method:: delegateGSIProxy(fname)

      Send an X509 proxy credential to an activated claim.

      :param str fname: Filename of the X509 proxy to send to the active claim.


.. class:: ScheddNegotiate

   The :class:`ScheddNegotiate` class represents an ongoing negotiation session
   with a schedd.  It is a context manager, returned by the :meth:`~htcondor.Schedd.negotiate`
   method.

   .. method:: sendClaim( claim, offer, request )

      Send a claim to the schedd; if possible, the schedd will activate this and run
      one or more jobs.

      :param str claim: The claim ID, typically from the ``Capability`` attribute in the
         corresponding Startd's private ad.
      :param offer: A description of the resource claimed (typically, the machine's ClassAd).
      :type offer: :class:`~classad.ClassAd`
      :param request: The resource request this claim is responding to; if not provided
         (default), the Schedd will decide which job receives this resource.
      :type request: :class:`~classad.ClassAd`

   .. method:: disconnect()

      Disconnect from this negotiation session.  This can also be achieved by exiting
      the context.


.. class:: _Param

   A dictionary-like object for the local HTCondor configuration; the keys and
   values of this object are the keys and values of the HTCondor configuration.

   The  ``get``, ``setdefault``, ``update``, ``keys``, ``items``, and ``values``
   methods of this class have the same semantics as a python dictionary.

   Writing to a ``_Param`` object will update the in-memory HTCondor configuration.

.. class:: JobEventLog

   An iterable object (and iterable context manager) corresponding to a
   specific file on disk containing a user event log.  By default, it
   waits for new events, but it may be used to poll for them, as follows: ::

     import htcondor
     jel = htcondor.JobEventLog("file.log")
     # Read all currently-available events without blocking.
     for event in jel.events(0):
         print(event)
     else:
         print("We found the the end of file")

   .. method:: __init__( filename )

   Create an instance of the :class:`JobEventLog` class.

   :param filename: Filename of the job event log.

   .. method:: events( stop_after=None )

   Return an iterator (self), which yields :class:`JobEvent` objects.  The
   iterator may yield any number of events, including zero, before
   throwing ``StopIteration``, which signals end-of-file.  You may iterate
   again with the same ``JobEventLog`` to check for new events.

   :param stop_after: Stop waiting for new events after this many seconds.
      If ``None``, never stop waiting for new events.  If ``0``, do not wait
      for new events.

   .. method:: close()

   Closes any open underlying file.  Subsequent iterations on this
   :class:`JobEventLog` object will immediately terminate (will never return
   another :class:`JobEvent`).

.. class:: JobEvent

   An immutable dictionary-like object corresponding to a particular event
   in the user log.  All events define the following attributes.  Other
   type-specific attributes are keys of the dictionary.  :class:`JobEvent`
   objects support both ``in`` operators (``if "attribute" in jobEvent`` and
   ``for attributeName in jobEvent``) and may be passed as arguments to
   ``len``.

   .. note:: Although the attribute `type` is a :class:`JobEventType` type,
      when acting as dictionary, a :class:`JobEvent` object returns types
      as if it were a :class:`~classad.ClassAd`, so comparisons to enumerated
      values must use the `==` operator.  (No current event type has
      :class:`~classad.ExprTree` values.)

   .. attribute:: type

      :type: :class:`htcondor.JobEventType`

      The event type.

   .. attribute:: cluster

      The cluster ID.

   .. attribute:: proc

      The proc ID.

   .. attribute:: timestamp

      When the event was recorded.

.. _esoteric_module_functions:

Esoteric Module-Level Functions
-------------------------------

.. function:: send_command( ad, dc, target = None)

   Send a command to an HTCondor daemon specified by a location ClassAd.

   :param ad: Specifies the location of the daemon (typically, found by using :meth:`Collector.locate`.
   :type ad: :class:`~classad.ClassAd`
   :param dc: A command type
   :type dc: :class:`DaemonCommands`
   :param str target: An additional command to send to a daemon. Some commands
      require additional arguments; for example, sending ``DaemonOff`` to a
      ``condor_master`` requires one to specify which subsystem to turn off.

.. function:: send_alive( ad, pid = None, timeout = -1 )

   Send a keep alive message to an HTCondor daemon.

   This is used when the python process is run as a child daemon under
   the ``condor_master``.

   :param ad: A :class:`~classad.ClassAd` specifying the location of the daemon.
      This ad is typically found by using :meth:`Collector.locate`.
   :type ad: :class:`~classad.ClassAd`
   :param int pid: The process identifier for the keep alive. The default value of
      ``None`` uses the value from :func:`os.getpid`.
   :param int timeout: The number of seconds that this keep alive is valid. If a
      new keep alive is not received by the condor_master in time, then the
      process will be terminated. The default value is controlled by configuration
      variable ``NOT_RESPONDING_TIMEOUT``.

.. function:: set_subsystem( name, daemon_type = Auto )

   Set the subsystem name for the object.

   The subsystem is primarily used for the parsing of the HTCondor configuration file.

   :param str name: The subsystem name.
   :param daemon_type: The HTCondor daemon type. The default value of Auto infers the type from the name parameter.
   :type daemon_type: :class:`SubsystemType`

.. function:: lock( file_obj, lock_type )

   Take a lock on a file object using the HTCondor locking protocol
   (distinct from typical POSIX locks).

   :param file file_obj: is a file object corresponding to the file which should be locked.
   :param lock_type: The kind of lock to acquire.
   :type lock_type: :class:`LockType`
   :return: A context manager object; the lock is released when the context manager object is exited.
   :rtype: FileLock

.. function:: log( level, msg )

   Log a message using the HTCondor logging subsystem.

   :param level: The Log category and formatting indicator. Multiple LogLevel enum attributes may be OR'd together.
   :type level: :class:`LogLevel`
   :param str msg: A message to log.


Iterator and Helper Classes
---------------------------

.. class:: HistoryIterator

   An iterator class for managing results of the :meth:`Schedd.history` method.

   .. method:: next()

      :return: the next available history ad.
      :rtype: :class:`~classad.ClassAd`
      :raises StopIteration: when no additional ads are available.

.. class:: QueryIterator

   An iterator class for managing results of the :meth:`Schedd.query` and
   :meth:`Schedd.xquery` methods.

   .. method:: next(mode=BlockingMode.Blocking)

      :param mode: The blocking mode for this call to :meth:`next`; defaults
         to :attr:`~BlockingMode.Blocking`.
      :type mode: :class:`BlockingMode`
      :return: the next available job ad.
      :rtype: :class:`~classad.ClassAd`
      :raises StopIteration: when no additional ads are available.

   .. method:: nextAdsNonBlocking()

      Retrieve as many ads are available to the iterator object.

      If no ads are available, returns an empty list.  Does not throw
      an exception if no ads are available or the iterator is finished.

      :return: Zero-or-more job ads.
      :rtype: list[:class:`~classad.ClassAd`]

   .. method:: tag()

      Retrieve the tag associated with this iterator; when using the :func:`poll` method,
      this is useful to distinguish multiple iterators.

      :return: the query's tag.

   .. method:: done()

      :return: ``True`` if the iterator is finished; ``False`` otherwise.

   .. method:: watch()

      Returns an ``inotify``-based file descriptor; if this descriptor is given
      to a ``select()`` instance, ``select`` will indicate this file descriptor is ready
      to read whenever there are more jobs ready on the iterator.

      If ``inotify`` is not available on this platform, this will return ``-1``.

      :return: A file descriptor associated with this query.
      :rtype: int

.. class:: BulkQueryIterator

   Returned by :func:`poll`, this iterator produces a sequence of :class:`QueryIterator`
   objects that have ads ready to be read in a non-blocking manner.

   Once there are no additional available iterators, :func:`poll` must be called again.

   .. method:: next()

      :return: The next available :class:`QueryIterator` that can be read without
         blocking.
      :rtype: :class:`QueryIterator`
      :raises StopIteration: if no more iterators are ready.

.. class:: FileLock

   A context manager object created by the :func:`lock` function; upon exit from the
   context, it will release the lock.


.. _useful_enums:

Useful Enumerations
-------------------

.. class:: DaemonTypes

   An enumeration of different types of daemons available to HTCondor.

   .. attribute:: Collector

      Ads representing the ``condor_collector``.

   .. attribute:: Negotiator

      Ads representing the ``condor_negotiator``.

   .. attribute:: Schedd

      Ads representing the ``condor_schedd``.

   .. attribute:: Startd

      Ads representing the resources on a worker node.

   .. attribute:: HAD

      Ads representing the high-availability daemons (``condor_had``).

   .. attribute:: Master

      Ads representing the ``condor_master``.

   .. attribute:: Generic

      All other ads that are not categorized as above.

   .. attribute:: Any

      Any type of daemon; useful when specifying queries where all matching
      daemons should be returned.

.. class:: AdTypes

   A list of different types of ads that may be kept in the ``condor_collector``.

   .. attribute:: Any

      Type representing any matching ad.  Useful for queries that match everything
      in the collector.

   .. attribute:: Collector

      Ads from the ``condor_collector`` daemon.

   .. attribute:: Generic

      Generic ads, associated with no particular daemon.

   .. attribute:: Grid

      Ads associated with the grid universe.

   .. attribute:: HAD

      Ads produced by the ``condor_had``.

   .. attribute:: License

      License ads.  These do not appear to be used by any modern HTCondor daemon.

   .. attribute:: Master

      Master ads, produced by the ``condor_master`` daemon.

   .. attribute:: Negotiator

      Negotiator ads, produced by the ``condor_negotiator`` daemon.

   .. attribute:: Schedd

      Schedd ads, produced by the ``condor_schedd`` daemon.

   .. attribute:: Startd

      Startd ads, produced by the ``condor_startd`` daemon.  Represents the
      available slots managed by the startd.

   .. attribute:: StartdPrivate

      The "private" ads, containing the claim IDs associated with a particular
      slot.  These require additional authorization to read as the claim ID
      may be used to run jobs on the slot.

   .. attribute:: Submitter

      Ads describing the submitters with available jobs to run; produced by
      the ``condor_schedd`` and read by the ``condor_negotiator`` to determine
      which users need a new negotiation cycle.

.. class:: JobAction

   Different actions that may be performed on a job in queue.

   .. attribute:: Hold

      Put a job on hold, vacating a running job if necessary.  A job will stay in the hold state
      until explicitly acted upon by the admin or owner.

   .. attribute:: Release

      Release a job from the hold state, returning it to ``Idle``.

   .. attribute:: Suspend

      Suspend the processes of a running job (on Unix platforms, this triggers a ``SIGSTOP``).
      The job's processes stay in memory but no longer get scheduled on the CPU.

   .. attribute:: Continue

      Continue a suspended jobs (on Unix, ``SIGCONT``).
      The processes in a previously suspended job will be scheduled to get CPU time again.

   .. attribute:: Remove

      Remove a job from the Schedd's queue, cleaning it up first on the remote host (if running).
      This requires the remote host to acknowledge it has successfully vacated the job, meaning ``Remove`` may not be instantaneous.

   .. attribute:: RemoveX

      Immediately remove a job from the schedd queue, even if it means the job is left running on the remote resource.

   .. attribute:: Vacate

      Cause a running job to be killed on the remote resource and return to idle state.
      With ``Vacate``, jobs may be given significant time to cleanly shut down.

   .. attribute:: VacateFast

      Vacate a running job as quickly as possible, without providing time for the job to cleanly terminate.

.. class:: DaemonCommands

   Various state-changing commands that can be sent to to a HTCondor daemon using :func:`send_command`.

   .. attribute:: DaemonOff

   .. attribute:: DaemonOffFast

   .. attribute:: DaemonOffPeaceful

   .. attribute:: DaemonsOff

   .. attribute:: DaemonsOffFast

   .. attribute:: DaemonsOffPeaceful

   .. attribute:: OffFast

   .. attribute:: OffForce

   .. attribute:: OffGraceful

   .. attribute:: OffPeaceful

   .. attribute:: Reconfig

   .. attribute:: Restart

   .. attribute:: RestartPeacful

   .. attribute:: SetForceShutdown

   .. attribute:: SetPeacefulShutdown

.. class:: TransactionFlags

   Flags affecting the characteristics of a transaction.

   .. attribute:: NonDurable

      Non-durable transactions are changes that may be lost when the ``condor_schedd``
      crashes.  ``NonDurable`` is used for performance, as it eliminates extra ``fsync()`` calls.

   .. attribute:: SetDirty

      This marks the changed ClassAds as dirty, causing an update notification to be sent
      to the ``condor_shadow`` and the ``condor_gridmanager``, if they are managing the job.

  .. attribute:: ShouldLog

     Causes any changes to the job queue to be logged in the relevant job event log.

.. class:: QueryOpts

   Flags sent to the ``condor_schedd`` during a query to alter its behavior.

   .. attribute:: Default

      Queries should use all default behaviors.

   .. attribute:: AutoCluster

      Instead of returning job ads, return an ad per auto-cluster.

.. class:: BlockingMode

   Controls the behavior of query iterators once they are out of data.

   .. attribute:: Blocking

      Sets the iterator to block until more data is available.

   .. attribute:: NonBlocking

      Sets the iterator to return immediately if additional data is not available.

.. class:: DrainTypes

   Draining policies that can be sent to a ``condor_startd``.

   .. attribute:: Fast

   .. attribute:: Graceful

   .. attribute:: Quick

.. class:: VacateTypes

   Vacate policies that can be sent to a ``condor_startd``.

   .. attribute:: Fast

   .. attribute:: Graceful

.. class:: LockType

   Lock policies that may be taken.

   .. attribute:: ReadLock

   .. attribute:: WriteLock

.. class:: SubsystemType

   An enumeration of known subsystem names.

   .. attribute:: Collector

   .. attribute:: Daemon

   .. attribute:: Dagman

   .. attribute:: GAHP

   .. attribute:: Job

   .. attribute:: Master

   .. attribute:: Negotiator

   .. attribute:: Schedd

   .. attribute:: Shadow

   .. attribute:: SharedPort

   .. attribute:: Startd

   .. attribute:: Starter

   .. attribute:: Submit

   .. attribute:: Tool

.. class:: LogLevel

   The log level attribute to use with :func:`log`.  Note that HTCondor
   mixes both a class (debug, network, all) and the header format (Timestamp,
   PID, NoHeader) within this enumeration.

   .. attribute:: Always

   .. attribute:: Audit

   .. attribute:: Config

   .. attribute:: DaemonCore

   .. attribute:: Error

   .. attribute:: FullDebug

   .. attribute:: Hostname

   .. attribute:: Job

   .. attribute:: Machine

   .. attribute:: Network

   .. attribute:: NoHeader

   .. attribute:: PID

   .. attribute:: Priv

   .. attribute:: Protocol

   .. attribute:: Security

   .. attribute:: Status

   .. attribute:: SubSecond

   .. attribute:: Terse

   .. attribute:: Timestamp

   .. attribute:: Verbose

.. class:: JobEventType

   The type event of a user log event; corresponds to ``ULogEventNumber``
   in the C++ source.

   .. attribute:: SUBMIT

   .. attribute:: EXECUTE

   .. attribute:: EXECUTABLE_ERROR

   .. attribute:: CHECKPOINTED

   .. attribute:: JOB_EVICTED

   .. attribute:: JOB_TERMINATED

   .. attribute:: IMAGE_SIZE

   .. attribute:: SHADOW_EXCEPTION

   .. attribute:: GENERIC

   .. attribute:: JOB_ABORTED

   .. attribute:: JOB_SUSPENDED

   .. attribute:: JOB_UNSUSPENDED

   .. attribute:: JOB_HELD

   .. attribute:: JOB_RELEASED

   .. attribute:: NODE_EXECUTE

   .. attribute:: NODE_TERMINATED

   .. attribute:: POST_SCRIPT_TERMINATED

   .. attribute:: GLOBUS_SUBMIT

   .. attribute:: GLOBUS_SUBMIT_FAILED

   .. attribute:: GLOBUS_RESOURCE_UP

   .. attribute:: GLOBUS_RESOURCE_DOWN

   .. attribute:: REMOTE_ERROR

   .. attribute:: JOB_DISCONNECTED

   .. attribute:: JOB_RECONNECTED

   .. attribute:: JOB_RECONNECT_FAILED

   .. attribute:: GRID_RESOURCE_UP

   .. attribute:: GRID_RESOURCE_DOWN

   .. attribute:: GRID_SUBMIT

   .. attribute:: JOB_AD_INFORMATION

   .. attribute:: JOB_STATUS_UNKNOWN

   .. attribute:: JOB_STATUS_KNOWN

   .. attribute:: JOB_STAGE_IN


   .. attribute:: JOB_STAGE_OUT

   .. attribute:: ATTRIBUTE_UPDATE

   .. attribute:: PRESKIP

   .. attribute:: CLUSTER_SUBMIT

   .. attribute:: CLUSTER_REMOVE

   .. attribute:: FACTORY_PAUSED

   .. attribute:: FACTORY_RESUMED

   .. attribute:: NONE

   .. attribute:: FILE_TRANSFER
