      

Linux
=====

This section provides information specific to the Linux port of
HTCondor. Linux is a difficult platform to support. It changes
frequently, and HTCondor has some extremely system-dependent code, such
as the checkpointing library.

HTCondor is sensitive to changes in the following elements of the
system:

-  The kernel version
-  The version of the GNU C library (glibc)
-  the version of GNU C Compiler (GCC) used to build and link HTCondor
   jobs. This matters for HTCondor’s standard universe, which provides
   checkpoints and remote system calls.

The HTCondor Team tries to provide support for various releases of the
distribution of Linux. Red Hat is probably the most popular Linux
distribution, and it provides a common set of versions for the above
system components at which HTCondor can aim support. HTCondor will often
work with Linux distributions other than Red Hat (for example, Debian or
SuSE) that have the same versions of the above components. However, we
do not usually test HTCondor on other Linux distributions and we do not
provide any guarantees about this.

New releases of Red Hat usually change the versions of some or all of
the above system-level components. A version of HTCondor that works with
one release of Red Hat might not work with newer releases. The following
sections describe the details of HTCondor’s support for the currently
available versions of Red Hat Linux on x86 architecture machines.

Linux Address Space Randomization
---------------------------------

Modern versions of Red Hat and Fedora do address space randomization,
which randomizes the memory layout of a process to reduce the
possibility of security exploits. This makes it impossible for standard
universe jobs to resume execution using a checkpoint. When starting or
resuming a standard universe job, HTCondor disables the randomization.

To run a binary compiled with *condor\_compile* in standalone mode,
either initially or in resumption mode, manually disable the address
space randomization by modifying the command line. For a 32-bit
architecture, assuming an HTCondor-linked binary called *myapp*, invoke
the standalone executable with:

::

      setarch i386 -L -R ./myapp

For a 64-bit architecture, the resumption command will be:

::

      setarch x86_64 -L -R ./myapp

Some applications will also need the **-B** option.

The command to resume execution using the checkpoint must also disable
address space randomization, as the 32-bit architecture example:

::

      setarch i386 -L -R myapp -_condor_restart myapp.ckpt

      
