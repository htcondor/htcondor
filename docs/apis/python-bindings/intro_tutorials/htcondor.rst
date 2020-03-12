HTCondor Introduction
=====================

Let's start interacting with the HTCondor daemons!

We'll cover the basics of two daemons, the *Collector* and the *Schedd*:

-  The **Collector** maintains an inventory of all the pieces of the
   HTCondor pool. For example, each machine that can run jobs will
   advertise a ClassAd describing its resources and state. In this
   module, we'll learn the basics of querying the collector for
   information and displaying results.
-  The **Schedd** maintains a queue of jobs and is responsible for
   managing their execution. We'll learn the basics of querying the
   schedd.

There are several other daemons - particularly, the *Startd* and the
*Negotiator* - the Python bindings can interact with. We'll cover those
in the advanced modules.

To better demonstrate how HTCondor works, we have launched a personal
instance for you to use. Your private HTCondor instance runs a minature
HTCondor pool and can run a single job at a time.

To start, let's import the ``htcondor`` modules.

.. code:: ipython3

    import htcondor
    import classad

Collector
---------

We'll start with the *Collector*, which gathers descriptions of the
states of all the daemons in your HTCondor pool. The collector provides
both **service discovery** and **monitoring** for these daemons.

Let's try to find the Schedd information for your HTCondor pool. First,
we'll create a ``Collector`` object, then use the ``locate`` method:

.. code:: ipython3

    coll = htcondor.Collector() # Create the object representing the collector.
    schedd_ad = coll.locate(htcondor.DaemonTypes.Schedd) # Locate the default schedd.
    print(schedd_ad['MyAddress']) # Prints the location of the schedd, using HTCondor's internal addressing scheme.


.. parsed-literal::

    <172.17.0.2:9618?addrs=172.17.0.2-9618&alias=c096c3d74375&noUDP&sock=schedd_20_9c63_4>


The ``locate`` method takes a type of daemon and (optionall) a name,
returning a ClassAd. Here, we print out the resulting ``MyAddress`` key.

A few minor points about the above example: - Because we didn't provide
the collector with a constructor, we used the default collector in the
container's configuration file. If we wanted to instead query a
non-default collector, we could have done
``htcondor.Collector("collector.example.com")``. - We used the
``DaemonTypes`` enumeration to pick the kind of daemon to return. - If
there were multiple schedds in the pool, the ``locate`` query would have
failed. In such a case, we need to provide an explicit name to the
method. E.g.,
``coll.locate(htcondor.DaemonTypes.Schedd, "schedd.example.com")``. -
The final output prints the schedd's location. You may be surprised that
this is not simply a ``hostname:port``; to help manage addressing in the
today's complicated Internet (full of NATs, private networks, and
firewalls), a more flexible structure was needed. - HTCondor developers
sometimes refer to this as the *sinful string*; here, *sinful* is a play
on a Unix data structure, not a moral judgement.

The ``locate`` method often returns only enough data to contact a remote
daemon. Typically, a ClassAd records significantly more attributes. For
example, if we wanted to query for a few specific attributes, we would
use the ``query`` method instead:

.. code:: ipython3

    coll.query(htcondor.AdTypes.Schedd, projection=["Name", "MyAddress", "DaemonCoreDutyCycle"])




.. parsed-literal::

    [[ DaemonCoreDutyCycle = 1.221765499040184E-03; Name = "jovyan@c096c3d74375"; MyAddress = "<172.17.0.2:9618?addrs=172.17.0.2-9618&alias=c096c3d74375&noUDP&sock=schedd_20_9c63_4>" ]]



Here, ``query`` takes an ``AdType`` (slightly more generic than the
``DaemonTypes``, as many kinds of ads are in the collector) and several
optional arguments, then returns a list of ClassAds.

We used the ``projection`` keyword argument; this indicates what
attributes you want returned. The collector may automatically insert
additional attributes (here, only ``MyType``); if an ad is missing a
requested attribute, it is simply not set in the returned ClassAd
object. If no projection is specified, then all attributes are returned.

**WARNING**: when possible, utilize the projection to limit the data
returned. Some ads may have hundreds of attributes, making returning the
entire ad an expensive operation.

The projection filters the returned *keys*; to filter out unwanted
*ads*, utilize the ``constraint`` option. Let's do the same query again,
but specify our hostname explicitly:

.. code:: ipython3

    import socket # We'll use this to automatically fill in our hostname
    coll.query(htcondor.AdTypes.Schedd, constraint='Name=?=%s' % classad.quote("jovyan@%s" % socket.getfqdn()), projection=["Name", 
    "MyAddress", "DaemonCoreDutyCycle"])




.. parsed-literal::

    [[ DaemonCoreDutyCycle = 1.221765499040184E-03; Name = "jovyan@c096c3d74375"; MyAddress = "<172.17.0.2:9618?addrs=172.17.0.2-9618&alias=c096c3d74375&noUDP&sock=schedd_20_9c63_4>" ]]



Notes: - ``constraint`` accepts either an ``ExprTree`` or ``string``
object; the latter is automatically parsed as an expression. - We used
the ``classad.quote`` function to properly quote the hostname string. In
this example, we're relatively certain the hostname won't contain
quotes. However, it is good practice to use the ``quote`` function to
avoid possible SQL-injection-type attacks. - Consider what would happen
if the host's FQDN contained spaces and doublequotes, such as
``foo.example.com" || true``.

Schedd
------

Let's try our hand at querying the ``schedd``!

First, we'll need a schedd object. You may either create one out of the
ad returned by ``locate`` above or use the default in the configuration
file:

.. code:: ipython3

    schedd = htcondor.Schedd()
    schedd = htcondor.Schedd(schedd_ad)
    print(schedd)


.. parsed-literal::

    <htcondor._htcondor.Schedd object at 0x7f06c009e490>


Unfortunately, as there are no jobs in our personal HTCondor pool,
querying the ``schedd`` will be boring. Let's submit a few jobs
(**note** the API used below will be covered by the next module; it's OK
if you don't understand it now):

.. code:: ipython3

    sub = htcondor.Submit()
    sub['executable'] = '/bin/sleep'
    sub['arguments'] = '5m'
    with schedd.transaction() as txn:
        sub.queue(txn, 10)

We should now have 10 jobs in queue, each of which should take 5 minutes
to complete.

Let's query for the jobs, paying attention to the jobs' ID and status:

.. code:: ipython3

    for job in schedd.xquery(projection=['ClusterId', 'ProcId', 'JobStatus']):
        print(repr(job))


.. parsed-literal::

    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 0; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 1; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 2; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 3; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 4; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 5; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 6; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 7; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 8; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 5; ProcId = 9; ClusterId = 1 ]
    [ ServerTime = 1574110154; JobStatus = 4; ProcId = 0; ClusterId = 4 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 0; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 1; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 2; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 3; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 4; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 5; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 6; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 7; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 8; ClusterId = 5 ]
    [ ServerTime = 1574110154; JobStatus = 1; ProcId = 9; ClusterId = 5 ]


The ``JobStatus`` is an integer; the integers map into the following
states: - ``1``: Idle (``I``) - ``2``: Running (``R``) - ``3``: Removed
(``X``) - ``4``: Completed (``C``) - ``5``: Held (``H``) - ``6``:
Transferring Output - ``7``: Suspended

Depending on how quickly you executed the notebook, you might see all
jobs idle (``JobStatus = 1``) or one job running (``JobStatus = 2``)
above. It is rare to see the other codes.

As with the Collector's ``query`` method, we can also filter out jobs
using ``xquery``:

.. code:: ipython3

    for job in schedd.xquery(requirements = 'ProcId >= 5', projection=['ProcId']):
        print(job.get('ProcId'))


.. parsed-literal::

    5
    6
    7
    8
    9
    5
    6
    7
    8
    9


Astute readers may notice that the ``Schedd`` object has both ``xquery``
and ``query`` methods. The difference between the two mimics the
difference between ``xreadlines`` and ``readlines`` call in the standard
Python library: - ``query`` returns a *list* of ClassAds, meaning all
objects are held in memory at once. This utilizes more memory and , but
the size of the results is immediately available. It utilizes an older,
heavyweight protocol to communicate with the Schedd. - ``xquery``
returns an *iterator* that produces ClassAds. This only requires one
ClassAd to be in memory at once. It is much more lightweight, both on
the client and server side.

When in doubt, utilize ``xquery``.

Now that we have a running job, it may be useful to check the status of
the machine in our HTCondor pool:

.. code:: ipython3

    print(coll.query(htcondor.AdTypes.Startd, projection=['Name', 'Status', 'Activity', 'JobId', 'RemoteOwner'])[0])


.. parsed-literal::

    
        [
            Activity = "Idle"; 
            Name = "slot2@c096c3d74375"
        ]


On Job Submission
-----------------

Congratulations - you can now perform simple queries against the
collector for worker and submit hosts, as well as simple job queries
against the submit host!

It is now time to move on to `submitting and managing
jobs <Submitting-and-Managing-Jobs.ipynb>`__.
