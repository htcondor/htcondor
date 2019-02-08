      

Java Applications
=================

HTCondor allows users to access a wide variety of machines distributed
around the world. The Java Virtual Machine (JVM) provides a uniform
platform on any machine, regardless of the machine’s architecture or
operating system. The HTCondor Java universe brings together these two
features to create a distributed, homogeneous computing environment.

Compiled Java programs can be submitted to HTCondor, and HTCondor can
execute the programs on any machine in the pool that will run the Java
Virtual Machine.

The *condor\_status* command can be used to see a list of machines in
the pool for which HTCondor can use the Java Virtual Machine.

::

    % condor_status -java 
     
    Name               JavaVendor Ver    State     Activity LoadAv  Mem  ActvtyTime 
     
    adelie01.cs.wisc.e Sun Micros 1.6.0_ Claimed   Busy     0.090   873  0+00:02:46 
    adelie02.cs.wisc.e Sun Micros 1.6.0_ Owner     Idle     0.210   873  0+03:19:32 
    slot10@bio.cs.wisc Sun Micros 1.6.0_ Unclaimed Idle     0.000   118  7+03:13:28 
    slot2@bio.cs.wisc. Sun Micros 1.6.0_ Unclaimed Idle     0.000   118  7+03:13:28 
    ...

If there is no output from the *condor\_status* command, then HTCondor
does not know the location details of the Java Virtual Machine on
machines in the pool, or no machines have Java correctly installed. In
this case, contact your system administrator or see section
`3.15 <JavaSupportInstallation.html#x43-3830003.15>`__ for more
information on getting HTCondor to work together with Java.

A Simple Example Java Application
---------------------------------

Here is a complete, if simple, example. Start with a simple Java
program, ``Hello.java``:

::

    public class Hello { 
            public static void main( String [] args ) { 
                    System.out.println("Hello, world!\n"); 
            } 
    }

Build this program using your Java compiler. On most platforms, this is
accomplished with the command

::

    javac Hello.java

Submission to HTCondor requires a submit description file. If submitting
where files are accessible using a shared file system, this simple
submit description file works:

::

      #################### 
      # 
      # Example 1 
      # Execute a single Java class 
      # 
      #################### 
     
      universe       = java 
      executable     = Hello.class 
      arguments      = Hello 
      output         = Hello.output 
      error          = Hello.error 
      queue

The Java universe must be explicitly selected.

The main class of the program is given in the **executable** statement.
This is a file name which contains the entry point of the program. The
name of the main class (not a file name) must be specified as the first
argument to the program.

If submitting the job where a shared file system is not accessible, the
submit description file becomes:

::

      #################### 
      # 
      # Example 2 
      # Execute a single Java class, 
      # not on a shared file system 
      # 
      #################### 
     
      universe       = java 
      executable     = Hello.class 
      arguments      = Hello 
      output         = Hello.output 
      error          = Hello.error 
      should_transfer_files = YES 
      when_to_transfer_output = ON_EXIT 
      queue

For more information about using HTCondor’s file transfer mechanisms,
see section \ `2.5.9 <SubmittingaJob.html#x17-380002.5.9>`__.

To submit the job, where the submit description file is named
``Hello.cmd``, execute

::

    condor_submit Hello.cmd

To monitor the job, the commands *condor\_q* and *condor\_rm* are used
as with all jobs.

Less Simple Java Specifications
-------------------------------

 Specifying more than 1 class file.
    For programs that consist of more than one ``.class`` file, identify
    the files in the submit description file:

    ::

        executable = Stooges.class 
        transfer_input_files = Larry.class,Curly.class,Moe.class

    The **executable** command does not change. It still identifies the
    class file that contains the program’s entry point.

 JAR files.
    If the program consists of a large number of class files, it may be
    easier to collect them all together into a single Java Archive (JAR)
    file. A JAR can be created with:

    ::

        % jar cvf Library.jar Larry.class Curly.class Moe.class Stooges.class

    HTCondor must then be told where to find the JAR as well as to use
    the JAR. The JAR file that contains the entry point is specified
    with the **executable** command. All JAR files are specified with
    the **jar\_files** command. For this example that collected all the
    class files into a single JAR file, the submit description file
    contains:

    ::

        executable = Library.jar 
        jar_files = Library.jar

    Note that the JVM must know whether it is receiving JAR files or
    class files. Therefore, HTCondor must also be informed, in order to
    pass the information on to the JVM. That is why there is a
    difference in submit description file commands for the two ways of
    specifying files (**transfer\_input\_files** and **jar\_files**).

    If there are multiple JAR files, the **executable** command
    specifies the JAR file that contains the program’s entry point. This
    file is also listed with the **jar\_files** command:

    ::

        executable = sortmerge.jar 
        jar_files = sortmerge.jar,statemap.jar

 Using a third-party JAR file.
    As HTCondor requires that all JAR files (third-party or not) be
    available, specification of a third-party JAR file is no different
    than other JAR files. If the sortmerge example above also relies on
    version 2.1 from http://jakarta.apache.org/commons/lang/, and this
    JAR file has been placed in the same directory with the other JAR
    files, then the submit description file contains

    ::

        executable = sortmerge.jar 
        jar_files = sortmerge.jar,statemap.jar,commons-lang-2.1.jar

 An executable JAR file.
    When the JAR file is an executable, specify the program’s entry
    point in the **arguments** command:

    ::

        executable = anexecutable.jar 
        jar_files  = anexecutable.jar 
        arguments  = some.main.ClassFile

 Discovering the main class within a JAR file.
    As of Java version 1.4, Java virtual machines have a **-jar**
    option, which takes a single JAR file as an argument. With this
    option, the Java virtual machine discovers the main class to run
    from the contents of the Manifest file, which is bundled within the
    JAR file. HTCondor’s **java** universe does not support this
    discovery, so before submitting the job, the name of the main class
    must be identified.

    For a Java application which is run on the command line with

    ::

          java -jar OneJarFile.jar

    the equivalent version after discovery might look like

    ::

          java -classpath OneJarFile.jar TheMainClass

    The specified value for TheMainClass can be discovered by unjarring
    the JAR file, and looking for the MainClass definition in the
    Manifest file. Use that definition in the HTCondor submit
    description file. Partial contents of that file Java universe submit
    file will appear as

    ::

          universe   = java 
          executable =  OneJarFile.jar 
          jar_files = OneJarFile.jar 
          Arguments = TheMainClass More-Arguments 
          queue

 Packages.
    An example of a Java class that is declared in a non-default package
    is

    ::

        package hpc; 
         
         public class CondorDriver 
         { 
             // class definition here 
         }

    The JVM needs to know the location of this package. It is passed as
    a command-line argument, implying the use of the naming convention
    and directory structure.

    Therefore, the submit description file for this example will contain

    ::

        arguments = hpc.CondorDriver

 JVM-version specific features.
    If the program uses Java features found only in certain JVMs, then
    the Java application submitted to HTCondor must only run on those
    machines within the pool that run the needed JVM. Inform HTCondor by
    adding a ``requirements`` statement to the submit description file.
    For example, to require version 3.2, add to the submit description
    file:

    ::

        requirements = (JavaVersion=="3.2")

 Benchmark speeds.
    Each machine with Java capability in an HTCondor pool will execute a
    benchmark to determine its speed. The benchmark is taken when
    HTCondor is started on the machine, and it uses the SciMark2
    (`http://math.nist.gov/scimark2 <http://math.nist.gov/scimark2>`__)
    benchmark. The result of the benchmark is held as an attribute
    within the machine ClassAd. The attribute is called ``JavaMFlops``.
    Jobs that are run under the Java universe (as all other HTCondor
    jobs) may prefer or require a machine of a specific speed by setting
    ``rank`` or ``requirements`` in the submit description file. As an
    example, to execute only on machines of a minimum speed:

    ::

        requirements = (JavaMFlops>4.5)

 JVM options.
    Options to the JVM itself are specified in the submit description
    file:

    ::

        java_vm_args = -DMyProperty=Value -verbose:gc -Xmx1024m

    These options are those which go after the java command, but before
    the user’s main class. Do not use this to set the classpath, as
    HTCondor handles that itself. Setting these options is useful for
    setting system properties, system assertions and debugging certain
    kinds of problems.

Chirp I/O
---------

If a job has more sophisticated I/O requirements that cannot be met by
HTCondor’s file transfer mechanism, then the Chirp facility may provide
a solution. Chirp has two advantages over simple, whole-file transfers.
First, it permits the input files to be decided upon at run-time rather
than submit time, and second, it permits partial-file I/O with results
than can be seen as the program executes. However, small changes to the
program are required in order to take advantage of Chirp. Depending on
the style of the program, use either Chirp I/O streams or UNIX-like I/O
functions.

Chirp I/O streams are the easiest way to get started. Modify the program
to use the objects ``ChirpInputStream`` and ``ChirpOutputStream``
instead of ``FileInputStream`` and ``FileOutputStream``. These classes
are completely documented in the HTCondor Software Developer’s Kit
(SDK). Here is a simple code example:

::

    import java.io.*; 
    import edu.wisc.cs.condor.chirp.*; 
     
    public class TestChirp { 
     
       public static void main( String args[] ) { 
     
          try { 
             BufferedReader in = new BufferedReader( 
                new InputStreamReader( 
                   new ChirpInputStream("input"))); 
     
             PrintWriter out = new PrintWriter( 
                new OutputStreamWriter( 
                   new ChirpOutputStream("output"))); 
     
             while(true) { 
                String line = in.readLine(); 
                if(line==null) break; 
                out.println(line); 
             } 
             out.close(); 
          } catch( IOException e ) { 
             System.out.println(e); 
          } 
       } 
    }

To perform UNIX-like I/O with Chirp, create a ``ChirpClient`` object.
This object supports familiar operations such as ``open``, ``read``,
``write``, and ``close``. Exhaustive detail of the methods may be found
in the HTCondor SDK, but here is a brief example:

::

    import java.io.*; 
    import edu.wisc.cs.condor.chirp.*; 
     
    public class TestChirp { 
     
       public static void main( String args[] ) { 
     
          try { 
             ChirpClient client = new ChirpClient(); 
             String message = "Hello, world!\n"; 
             byte [] buffer = message.getBytes(); 
     
             // Note that we should check that actual==length. 
             // However, skip it for clarity. 
     
             int fd = client.open("output","wct",0777); 
             int actual = client.write(fd,buffer,0,buffer.length); 
             client.close(fd); 
     
             client.rename("output","output.new"); 
             client.unlink("output.new"); 
     
          } catch( IOException e ) { 
             System.out.println(e); 
          } 
       } 
    }

Regardless of which I/O style, the Chirp library must be specified and
included with the job. The Chirp JAR (``Chirp.jar``) is found in the
``lib`` directory of the HTCondor installation. Copy it into your
working directory in order to compile the program after modification to
use Chirp I/O.

::

    % condor_config_val LIB 
    /usr/local/condor/lib 
    % cp /usr/local/condor/lib/Chirp.jar .

Rebuild the program with the Chirp JAR file in the class path.

::

    % javac -classpath Chirp.jar:. TestChirp.java

The Chirp JAR file must be specified in the submit description file.
Here is an example submit description file that works for both of the
given test programs:

::

    universe = java 
    executable = TestChirp.class 
    arguments = TestChirp 
    jar_files = Chirp.jar 
    +WantIOProxy = True 
    queue

      
