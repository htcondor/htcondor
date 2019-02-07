      

Java Support Installation
=========================

Compiled Java programs may be executed (under HTCondor) on any execution
site with a Java Virtual Machine (JVM). To do this, HTCondor must be
informed of some details of the JVM installation.

Begin by installing a Java distribution according to the vendor’s
instructions. Your machine may have been delivered with a JVM already
installed – installed code is frequently found in ``/usr/bin/java``.

HTCondor’s configuration includes the location of the installed JVM.
Edit the configuration file. Modify the ``JAVA`` entry to point to the
JVM binary, typically ``/usr/bin/java``. Restart the *condor\_startd*
daemon on that host. For example,

::

    % condor_restart -startd bluejay

The *condor\_startd* daemon takes a few moments to exercise the Java
capabilities of the *condor\_starter*, query its properties, and then
advertise the machine to the pool as Java-capable. If the set up
succeeded, then *condor\_status* will tell you the host is now
Java-capable by printing the Java vendor and the version number:

::

    % condor_status -java bluejay

After a suitable amount of time, if this command does not give any
output, then the *condor\_starter* is having difficulty executing the
JVM. The exact cause of the problem depends on the details of the JVM,
the local installation, and a variety of other factors. We can offer
only limited advice on these matters, but here is an approach to solving
the problem.

To reproduce the test that the *condor\_starter* is attempting, try
running the Java *condor\_starter* directly. To find where the
*condor\_starter* is installed, run this command:

::

    % condor_config_val STARTER

This command prints out the path to the *condor\_starter*, perhaps
something like this:

::

    /usr/condor/sbin/condor_starter

Use this path to execute the *condor\_starter* directly with the
*-classad* argument. This tells the starter to run its tests and display
its properties.

::

    /usr/condor/sbin/condor_starter -classad

This command will display a short list of cryptic properties, such as:

::

    IsDaemonCore = True 
    HasFileTransfer = True 
    HasMPI = True 
    CondorVersion = "$CondorVersion: 7.1.0 Mar 26 2008 BuildID: 80210 $"

If the Java configuration is correct, there will also be a short list of
Java properties, such as:

::

    JavaVendor = "Sun Microsystems Inc." 
    JavaVersion = "1.2.2" 
    JavaMFlops = 9.279696 
    HasJava = True

If the Java installation is incorrect, then any error messages from the
shell or Java will be printed on the error stream instead.

Many implementations of the JVM set a value of the Java maximum heap
size that is too small for particular applications. HTCondor uses this
value. The administrator can change this value through configuration by
setting a different value for ``JAVA_EXTRA_ARGUMENTS`` .

::

    JAVA_EXTRA_ARGUMENTS = -Xmx1024m

Note that if a specific job sets the value in the submit description
file, using the submit command **java\_vm\_args**, the job’s value takes
precedence over a configured value.

      
