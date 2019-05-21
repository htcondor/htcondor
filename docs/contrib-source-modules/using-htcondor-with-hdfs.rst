Using HTCondor with the Hadoop File System
==========================================

The Hadoop project is an Apache project, headquartered at
`http://hadoop.apache.org <http://hadoop.apache.org/>`_, which
implements an open-source, distributed file system across a large set of
machines. The file system proper is called the Hadoop File System, or
HDFS, and there are several Hadoop-provided tools which use the file
system, most notably databases and tools which use the map-reduce
distributed programming style.

Distributed with the HTCondor source code, HTCondor provides a way to
manage the daemons which implement an HDFS, but no direct support for
the high-level tools which run atop this file system. There are two
types of daemons, which together create an instance of a Hadoop File
System. The first is called the Name node, which is like the central
manager for a Hadoop cluster. There is only one active Name node per
HDFS. If the Name node is not running, no files can be accessed. The
HDFS does not support fail over of the Name node, but it does support a
hot-spare for the Name node, called the Backup node. HTCondor can
configure one node to be running as a Backup node. The second kind of
daemon is the Data node, and there is one Data node per machine in the
distributed file system. As these are both implemented in Java, HTCondor
cannot directly manage these daemons. Rather, HTCondor provides a small
DaemonCore daemon, called *condor_hdfs*, which reads the HTCondor
configuration file, responds to HTCondor commands like *condor_on* and
*condor_off*, and runs the Hadoop Java code. It translates entries in
the HTCondor configuration file to an XML format native to HDFS. These
configuration items are listed with the *condor_hdfs* daemon in
section :ref:`contrib-source-modules/using-htcondor-with-hdfs:condor_hdfs
configuration file entries`. So, to configure HDFS in HTCondor, the HTCondor
configuration file should specify one machine in the pool to be the HDFS Name node, and
others to be the Data nodes.

Once an HDFS is deployed, HTCondor jobs can directly use it in a vanilla
universe job, by transferring input files directly from the HDFS by
specifying a URL within the job's submit description file command
**transfer_input_files**. See
:ref:`admin-manual/setting-up-special-environments:enabling the transfer of
files specified by a url` for the administrative details to set up transfers
specified by a URL. It requires that a plug-in is accessible and defined to
handle ``hdfs`` protocol transfers.

condor_hdfs Configuration File Entries
---------------------------------------

These macros affect the *condor_hdfs* daemon. Many of these variables
determine how the *condor_hdfs* daemon sets the HDFS XML configuration.

``HDFS_HOME``
    The directory path for the Hadoop file system installation
    directory. Defaults to ``$(RELEASE_DIR)``/libexec. This directory is
    required to contain

    -  directory ``lib``, containing all necessary jar files for the
       execution of a Name node and Data nodes.
    -  directory ``conf``, containing default Hadoop file system
       configuration files with names that conform to ``*-site.xml``.
    -  directory ``webapps``, containing JavaServer pages (jsp) files
       for the Hadoop file system's embedded server.

``HDFS_NAMENODE``
    The host and port number for the HDFS Name node. There is no default
    value for this required variable. Defines the value of
    ``fs.default.name`` in the HDFS XML configuration.

``HDFS_NAMENODE_WEB``
    The IP address and port number for the HDFS embedded web server
    within the Name node with the syntax of ``a.b.c.d:portnumber``.
    There is no default value for this required variable. Defines the
    value of ``dfs.http.address`` in the HDFS XML configuration.

``HDFS_DATANODE_WEB``
    The IP address and port number for the HDFS embedded web server
    within the Data node with the syntax of ``a.b.c.d:portnumber``. The
    default value for this optional variable is 0.0.0.0:0, which means
    bind to the default interface on a dynamic port. Defines the value
    of ``dfs.datanode.http.address`` in the HDFS XML configuration.

``HDFS_NAMENODE_DIR``
    The path to the directory on a local file system where the Name node
    will store its meta-data for file blocks. There is no default value
    for this variable; it is required to be defined for the Name node
    machine. Defines the value of ``dfs.name.dir`` in the HDFS XML
    configuration.

``HDFS_DATANODE_DIR``
    The path to the directory on a local file system where the Data node
    will store file blocks. There is no default value for this variable;
    it is required to be defined for a Data node machine. Defines the
    value of ``dfs.data.dir`` in the HDFS XML configuration.

``HDFS_DATANODE_ADDRESS``
    The IP address and port number of this machine's Data node. There is
    no default value for this variable; it is required to be defined for
    a Data node machine, and may be given the value ``0.0.0.0:0`` as a
    Data node need not be running on a known port. Defines the value of
    ``dfs.datanode.address`` in the HDFS XML configuration.

``HDFS_NODETYPE``
    This parameter specifies the type of HDFS service provided by this
    machine. Possible values are ``HDFS_NAMENODE`` and
    ``HDFS_DATANODE``. The default value is ``HDFS_DATANODE``.

``HDFS_BACKUPNODE``
    The host address and port number for the HDFS Backup node. There is
    no default value. It defines the value of the HDFS
    dfs.namenode.backup.address field in the HDFS XML configuration
    file.

``HDFS_BACKUPNODE_WEB``
    The address and port number for the HDFS embedded web server within
    the Backup node, with the syntax of
    hdfs://<host_address>:<portnumber>. There is no default value for
    this required variable. It defines the value of
    dfs.namenode.backup.http-address in the HDFS XML configuration.

``HDFS_NAMENODE_ROLE``
    If this machine is selected to be the Name node, then the role must
    be defined. Possible values are ``ACTIVE``, ``BACKUP``,
    ``CHECKPOINT``, and ``STANDBY``. The default value is ``ACTIVE``.
    The ``STANDBY`` value exists for future expansion. If
    ``HDFS_NODETYPE`` is selected to be Data node (``HDFS_DATANODE``),
    then this variable is ignored.

``HDFS_LOG4J``
    Used to set the configuration for the HDFS debugging level.
    Currently one of ``OFF``, ``FATAL``, ``ERROR``, ``WARN``,
    ``INFODEBUG``, ``ALL`` or ``INFO``. Debugging output is written to
    ``$(LOG)``/hdfs.log. The default value is ``INFO``.

``HDFS_ALLOW``
    A comma separated list of hosts that are authorized with read and
    write access to the invoked HDFS. Note that this configuration
    variable name is likely to change to ``HOSTALLOW_HDFS``.

``HDFS_DENY``
    A comma separated list of hosts that are denied access to the
    invoked HDFS. Note that this configuration variable name is likely
    to change to ``HOSTDENY_HDFS``.

``HDFS_NAMENODE_CLASS``
    An optional value that specifies the class to invoke. The default
    value is org.apache.hadoop.hdfs.server.namenode.NameNode.

``HDFS_DATANODE_CLASS``
    An optional value that specifies the class to invoke. The default
    value is org.apache.hadoop.hdfs.server.datanode.DataNode.

``HDFS_SITE_FILE``
    The optional value that specifies the HDFS XML configuration file to
    generate. The default value is ``hdfs-site.xml``.
 
``HDFS_REPLICATION``
    An integer value that facilitates setting the replication factor of
    an HDFS, defining the value of ``dfs.replication`` in the HDFS XML
    configuration. This configuration variable is optional, as the HDFS
    has its own default value of 3 when not set through configuration.