Recipe: Setting up a Collector Tree
===================================

As an HTCondor pool grows in size by adding more Execution Points (EP), you may
notice the Central Manager (CM), particularly the *condor_collector*, becoming
overwhelmed. This is likely due to the handling of updates from all EPs
in the pool. A solution to preventing this overload is to create a
Collector Tree.

Changes at the Central Manager
------------------------------

To create a Collector Tree you will define a number of child Collectors to
handle updates from the EPs and forwarding of ClassAds to the top level
Collector. Only information needed for daemon lookup and proper matchmaking
should forwarded to the top level Collector.

.. code-block:: condor-config

    # Setup ClassAd forwarding with filtering of necessary ClassAds only
    COLLECTOR_FORWARD_FILTERING = True
    CONDOR_VIEW_CLASSAD_TYPES = Master,Machine,Submitter,Scheduler,StartD

    # OPTIONAL: Filter out Dynamic Slot Ads
    COLLECTOR_REQUIREMENTS = DynamicSlot =!= true

    # Increase UDP packet size
    UDP_NETWORK_FRAGMENT_SIZE = 60000

    # Allow EPs to send updates to child Collectors
    $(LOCALNAME).ALLOW_ADVERTISE_MASTER = *
    $(LOCALNAME).ALLOW_ADVERTISE_STARTD = *

    # Prevent EPs from taking directly to top level Collector
    COLLECTOR.ALLOW_ADVERTISE_MASTER = */$(HOSTNAME)
    COLLECTOR.ALLOW_ADVERTISE_STARTD = */$(HOSTNAME)

    # Create N child Collectors

    # Child Collector 1
    use FEATURE:ChildCollector(1)
    COLLECTOR1_LOG = $(LOG)/Collector1Log

    # Child Collector 2
    use FEATURE:ChildCollector(2)
    COLLECTOR2_LOG = $(LOG)/Collector2Log

    # Child Collector 3
    use FEATURE:ChildCollector(3)
    COLLECTOR3_LOG = $(LOG)/Collector3Log

Considerations for Access Points
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can choose whether or not Access Points (AP) report directly to the
top level Collector or one of the child Collectors. For allowing AP updates
directly to the top level Collector make sure to add ``*/<ap.host.name>``
for all APs to :macro:`COLLECTOR.ALLOW_ADVERTISE_MASTER` in the CM configuration.
Otherwise, apply the same configuration as the EPs to each AP in order to
choose a specific child Collector to send updates.

Changes at the Execution Points
-------------------------------

Once the child Collectors have been set up, you will need to update all the
EP configuration to point the EPs at one of the desired child Collectors.
You are in charge of deciding how to distribute the update load from EPs
across the child Collectors.

.. code-block:: condor-config

    # Inform EP to send updates to specific child Collector N
    # For this example N = 1
    COLLECTOR_HOST = $(CONDOR_HOST)?sock=collector1

.. note::

    It is possible to have all EPs choose a random child Collector via
    ``$(CONDOR_HOST)?sock=collector$RANDOM_INTEGER(1,N)``. However,
    if the EP is reconfigured then the selected child Collector will
    change. This can cause issues with job reconnects.

Contacting Child Collectors
---------------------------

To contact a child Collector you first need to acquire the desired child
Collectors Sinful address via the following command.

.. code-block:: console

    $ condor_status -collector -af Name MyAddress

Once you have an address, simply use the **-pool** option to communication
with the desired child Collector.

.. code-block:: console

    $ condor_status -pool "<Child Collector Sinful Address>"
