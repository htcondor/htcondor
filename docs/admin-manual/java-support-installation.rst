Java Support Installation
=========================

:index:`Java<single: Java; installation>` :index:`Java`

Compiled Java programs may be executed (under HTCondor) on any execution
site with a :index:`Java Virtual Machine`\ :index:`JVM`
Java Virtual Machine (JVM). To do this, HTCondor must be informed of
some details of the JVM installation.

Begin by installing a Java distribution according to the vendor's
instructions. Your machine may have been delivered with a JVM already
installed - installed code is frequently found in ``/usr/bin/java``.

HTCondor's configuration includes the location of the installed JVM.
Edit the configuration file. Modify the ``JAVA`` :index:`JAVA`
entry to point to the JVM binary, typically ``/usr/bin/java``. Restart
the *condor_startd* daemon on that host. For example,

.. code-block:: console

    $ condor_restart -startd bluejay

The *condor_startd* daemon takes a few moments to exercise the Java
capabilities of the *condor_starter*, query its properties, and then
advertise the machine to the pool as Java-capable. If the set up
succeeded, then *condor_status* will tell you the host is now
Java-capable by printing the Java vendor and the version number:

.. code-block:: console

    $ condor_status -java bluejay

After a suitable amount of time, if this command does not give any
output, then the *condor_starter* is having difficulty executing the
JVM. The exact cause of the problem depends on the details of the JVM,
the local installation, and a variety of other factors. We can offer
only limited advice on these matters, but here is an approach to solving
the problem.

To reproduce the test that the *condor_starter* is attempting, try
running the Java *condor_starter* directly. To find where the
*condor_starter* is installed, run this command:

.. code-block:: console

    $ condor_config_val STARTER

This command prints out the path to the *condor_starter*, perhaps
something like this:

.. code-block:: console

    $ /usr/condor/sbin/condor_starter

Use this path to execute the *condor_starter* directly with the
*-classad* argument. This tells the starter to run its tests and display
its properties.

.. code-block:: console

    $ /usr/condor/sbin/condor_starter -classad

This command will display a short list of cryptic properties, such as:

.. code-block:: condor-classad

    IsDaemonCore = True
    HasFileTransfer = True
    HasMPI = True
    CondorVersion = "$CondorVersion: 7.1.0 Mar 26 2008 BuildID: 80210 $"

If the Java configuration is correct, there will also be a short list of
Java properties, such as:

.. code-block:: condor-classad

    JavaVendor = "Sun Microsystems Inc."
    JavaVersion = "1.2.2"
    HasJava = True

If the Java installation is incorrect, then any error messages from the
shell or Java will be printed on the error stream instead.

Many implementations of the JVM set a value of the Java maximum heap
size that is too small for particular applications. HTCondor uses this
value. The administrator can change this value through configuration by
setting a different value for ``JAVA_EXTRA_ARGUMENTS``
:index:`JAVA_EXTRA_ARGUMENTS`.

.. code-block:: condor-config

    JAVA_EXTRA_ARGUMENTS = -Xmx1024m

Note that if a specific job sets the value in the submit description
file, using the submit command
**java_vm_args** :index:`java_vm_args<single: java_vm_args; submit commands>`, the
job's value takes precedence over a configured value.


