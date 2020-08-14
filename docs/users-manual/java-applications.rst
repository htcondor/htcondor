Java Applications
=================

:index:`Java`

HTCondor allows users to access a wide variety of machines distributed
around the world. The Java Virtual Machine (JVM)
:index:`Java Virtual Machine` :index:`JVM` provides a
uniform platform on any machine, regardless of the machine's
architecture or operating system. The HTCondor Java universe brings
together these two features to create a distributed, homogeneous
computing environment.

Compiled Java programs can be submitted to HTCondor, and HTCondor can
execute the programs on any machine in the pool that will run the Java
Virtual Machine.

The *condor_status* command can be used to see a list of machines in
the pool for which HTCondor can use the Java Virtual Machine.

.. code-block:: console

    $ condor_status -java

    Name               JavaVendor Ver    State     Activity LoadAv  Mem  ActvtyTime

    adelie01.cs.wisc.e Sun Micros 1.6.0_ Claimed   Busy     0.090   873  0+00:02:46
    adelie02.cs.wisc.e Sun Micros 1.6.0_ Owner     Idle     0.210   873  0+03:19:32
    slot10@bio.cs.wisc Sun Micros 1.6.0_ Unclaimed Idle     0.000   118  7+03:13:28
    slot2@bio.cs.wisc. Sun Micros 1.6.0_ Unclaimed Idle     0.000   118  7+03:13:28
    ...

If there is no output from the *condor_status* command, then HTCondor
does not know the location details of the Java Virtual Machine on
machines in the pool, or no machines have Java correctly installed. In
this case, contact your system administrator or see the 
:doc:`/admin-manual/java-support-installation` section
for more information on getting HTCondor to work together with Java.

A Simple Example Java Application
---------------------------------

:index:`job example<single: job example; Java>`

Here is a complete, if simple, example. Start with a simple Java
program, ``Hello.java``:

.. code-block:: java

    public class Hello {
        public static void main( String [] args ) {
            System.out.println("Hello, world!\n");
        }
    }

Build this program using your Java compiler. On most platforms, this is
accomplished with the command

.. code-block:: console

    $ javac Hello.java

Submission to HTCondor requires a submit description file. If submitting
where files are accessible using a shared file system, this simple
submit description file works:

.. code-block:: condor-submit

      ####################
      #
      # Example 1
      # Execute a single Java class
      #
      ####################

      universe       = java
      executable     = Hello.class
      arguments      = Hello
      output         = Hello.output
      error          = Hello.error
      queue

The Java universe must be explicitly selected.

The main class of the program is given in the
**executable** :index:`executable<single: executable; submit commands>` statement.
This is a file name which contains the entry point of the program. The
name of the main class (not a file name) must be specified as the first
argument to the program.

If submitting the job where a shared file system is not accessible, the
submit description file becomes:

.. code-block:: condor-submit

      ####################
      #
      # Example 2
      # Execute a single Java class,
      # not on a shared file system
      #
      ####################

      universe       = java
      executable     = Hello.class
      arguments      = Hello
      output         = Hello.output
      error          = Hello.error
      should_transfer_files = YES
      when_to_transfer_output = ON_EXIT
      queue

For more information about using HTCondor's file transfer mechanisms,
see the :doc:`/users-manual/submitting-a-job` section.

To submit the job, where the submit description file is named
``Hello.cmd``, execute

.. code-block:: console

    $ condor_submit Hello.cmd

To monitor the job, the commands *condor_q* and *condor_rm* are used
as with all jobs.

Less Simple Java Specifications
-------------------------------

 Specifying more than 1 class file.
    :index:`multiple class files<single: multiple class files; Java>` For programs that
    consist of more than one ``.class`` file, identify the files in the
    submit description file:

    .. code-block:: condor-submit

        executable = Stooges.class
        transfer_input_files = Larry.class,Curly.class,Moe.class

    The **executable** :index:`executable<single: executable; submit commands>`
    command does not change. It still identifies the class file that
    contains the program's entry point.

 JAR files.
    :index:`using JAR files<single: using JAR files; Java>` If the program consists of a
    large number of class files, it may be easier to collect them all
    together into a single Java Archive (JAR) file. A JAR can be created
    with:

    .. code-block:: console

        $ jar cvf Library.jar Larry.class Curly.class Moe.class Stooges.class

    HTCondor must then be told where to find the JAR as well as to use
    the JAR. The JAR file that contains the entry point is specified
    with the **executable** :index:`executable<single: executable; submit commands>`
    command. All JAR files are specified with the
    **jar_files** :index:`jar_files<single: jar_files; submit commands>` command.
    For this example that collected all the class files into a single
    JAR file, the submit description file contains:

    .. code-block:: condor-submit

        executable = Library.jar
        jar_files = Library.jar

    Note that the JVM must know whether it is receiving JAR files or
    class files. Therefore, HTCondor must also be informed, in order to
    pass the information on to the JVM. That is why there is a
    difference in submit description file commands for the two ways of
    specifying files
    (**transfer_input_files** :index:`transfer_input_files<single: transfer_input_files; submit commands>`
    and **jar_files** :index:`jar_files<single: jar_files; submit commands>`).

    If there are multiple JAR files, the **executable** command
    specifies the JAR file that contains the program's entry point. This
    file is also listed with the **jar_files** command:

    .. code-block:: condor-submit

        executable = sortmerge.jar
        jar_files = sortmerge.jar,statemap.jar

 Using a third-party JAR file.
    As HTCondor requires that all JAR files (third-party or not) be
    available, specification of a third-party JAR file is no different
    than other JAR files. If the sortmerge example above also relies on
    version 2.1 from http://jakarta.apache.org/commons/lang/, and this
    JAR file has been placed in the same directory with the other JAR
    files, then the submit description file contains

    .. code-block:: condor-submit

        executable = sortmerge.jar
        jar_files = sortmerge.jar,statemap.jar,commons-lang-2.1.jar

 An executable JAR file.
    When the JAR file is an executable, specify the program's entry
    point in the
    **arguments** :index:`arguments<single: arguments; submit commands>` command:

    .. code-block:: condor-submit

        executable = anexecutable.jar
        jar_files  = anexecutable.jar
        arguments  = some.main.ClassFile

 Discovering the main class within a JAR file.
    As of Java version 1.4, Java virtual machines have a **-jar**
    option, which takes a single JAR file as an argument. With this
    option, the Java virtual machine discovers the main class to run
    from the contents of the Manifest file, which is bundled within the
    JAR file. HTCondor's **java** universe does not support this
    discovery, so before submitting the job, the name of the main class
    must be identified.

    For a Java application which is run on the command line with

    .. code-block:: console

        $ java -jar OneJarFile.jar

    the equivalent version after discovery might look like

    .. code-block:: console

        $ java -classpath OneJarFile.jar TheMainClass

    The specified value for TheMainClass can be discovered by unjarring
    the JAR file, and looking for the MainClass definition in the
    Manifest file. Use that definition in the HTCondor submit
    description file. Partial contents of that file Java universe submit
    file will appear as

    .. code-block:: condor-submit

          universe   = java
          executable =  OneJarFile.jar
          jar_files  = OneJarFile.jar
          Arguments  = TheMainClass More-Arguments
          queue

 Packages.
    :index:`using packages<single: using packages; Java>` An example of a Java class that
    is declared in a non-default package is

    .. code-block:: java

        package hpc;

        public class CondorDriver
        {
         // class definition here
        }

    The JVM needs to know the location of this package. It is passed as
    a command-line argument, implying the use of the naming convention
    and directory structure.

    Therefore, the submit description file for this example will contain

    .. code-block:: condor-submit

        arguments = hpc.CondorDriver

 JVM-version specific features.
    If the program uses Java features found only in certain JVMs, then
    the Java application submitted to HTCondor must only run on those
    machines within the pool that run the needed JVM. Inform HTCondor by
    adding a ``requirements`` statement to the submit description file.
    For example, to require version 3.2, add to the submit description
    file:

    .. code-block:: condor-submit

        requirements = (JavaVersion=="3.2")

 JVM options.
    Options to the JVM itself are specified in the submit description
    file:

    .. code-block:: condor-submit

        java_vm_args = -DMyProperty=Value -verbose:gc -Xmx1024m

    These options are those which go after the java command, but before
    the user's main class. Do not use this to set the classpath, as
    HTCondor handles that itself. Setting these options is useful for
    setting system properties, system assertions and debugging certain
    kinds of problems.

Chirp I/O
---------

:index:`Chirp`

If a job has more sophisticated I/O requirements that cannot be met by
HTCondor's file transfer mechanism, then the Chirp facility may provide
a solution. Chirp has two advantages over simple, whole-file transfers.
First, it permits the input files to be decided upon at run-time rather
than submit time, and second, it permits partial-file I/O with results
than can be seen as the program executes. However, small changes to the
program are required in order to take advantage of Chirp. Depending on
the style of the program, use either Chirp I/O streams or UNIX-like I/O
functions. :index:`ChirpInputStream<single: ChirpInputStream; Chirp>`
:index:`ChirpOutputStream<single: ChirpOutputStream; Chirp>`

Chirp I/O streams are the easiest way to get started. Modify the program
to use the objects ``ChirpInputStream`` and ``ChirpOutputStream``
instead of ``FileInputStream`` and ``FileOutputStream``. These classes
are completely documented
:index:`Chirp<single: Chirp; Software Developers Kit>`\ :index:`Chirp<single: Chirp; SDK>`
in the HTCondor Software Developer's Kit (SDK). Here is a simple code
example:

.. code-block:: java

    import java.io.*;
    import edu.wisc.cs.condor.chirp.*;

    public class TestChirp {

       public static void main( String args[] ) {

          try {
             BufferedReader in = new BufferedReader(
                new InputStreamReader(
                   new ChirpInputStream("input")));

             PrintWriter out = new PrintWriter(
                new OutputStreamWriter(
                   new ChirpOutputStream("output")));

             while(true) {
                String line = in.readLine();
                if(line==null) break;
                out.println(line);
             }
             out.close();
          } catch( IOException e ) {
             System.out.println(e);
          }
       }
    }

:index:`ChirpClient<single: ChirpClient; Chirp>`

To perform UNIX-like I/O with Chirp, create a ``ChirpClient`` object.
This object supports familiar operations such as ``open``, ``read``,
``write``, and ``close``. Exhaustive detail of the methods may be found
in the HTCondor SDK, but here is a brief example:

.. code-block:: java

    import java.io.*;
    import edu.wisc.cs.condor.chirp.*;

    public class TestChirp {

       public static void main( String args[] ) {

          try {
             ChirpClient client = new ChirpClient();
             String message = "Hello, world!\n";
             byte [] buffer = message.getBytes();

             // Note that we should check that actual==length.
             // However, skip it for clarity.

             int fd = client.open("output","wct",0777);
             int actual = client.write(fd,buffer,0,buffer.length);
             client.close(fd);

             client.rename("output","output.new");
             client.unlink("output.new");

          } catch( IOException e ) {
             System.out.println(e);
          }
       }
    }

:index:`Chirp.jar<single: Chirp.jar; Chirp>`

Regardless of which I/O style, the Chirp library must be specified and
included with the job. The Chirp JAR (``Chirp.jar``) is found in the
``lib`` directory of the HTCondor installation. Copy it into your
working directory in order to compile the program after modification to
use Chirp I/O.

.. code-block:: console

    $ condor_config_val LIB
    /usr/local/condor/lib
    $ cp /usr/local/condor/lib/Chirp.jar .

Rebuild the program with the Chirp JAR file in the class path.

.. code-block:: console

    $ javac -classpath Chirp.jar:. TestChirp.java

The Chirp JAR file must be specified in the submit description file.
Here is an example submit description file that works for both of the
given test programs:

.. code-block:: condor-submit

    universe = java
    executable = TestChirp.class
    arguments = TestChirp
    jar_files = Chirp.jar
    +WantIOProxy = True
    queue


