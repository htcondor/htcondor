      

Current Limitations
===================

` <index://limitations, under UNIX;HTCondor>`__

 Limitations on Jobs which can Checkpointed
    Although HTCondor can schedule and run any type of process, HTCondor
    does have some limitations on jobs that it can transparently
    checkpoint and migrate:

    :index:`Unix<single: Unix; fork>` :index:`Unix<single: Unix; exec>`
    :index:`Unix<single: Unix; system>`

    #. Multi-process jobs are not allowed. This includes system calls
       such as ``fork()``, ``exec()``, and ``system()``.
       :index:`Unix<single: Unix; pipe>` :index:`Unix<single: Unix; semaphore>`
       :index:`Unix<single: Unix; shared memory>`
    #. Interprocess communication is not allowed. This includes pipes,
       semaphores, and shared memory. :index:`Unix<single: Unix; socket>`
       :index:`network<single: network>`
    #. Network communication must be brief. A job may make network
       connections using system calls such as ``socket()``, but a
       network connection left open for long periods will delay
       checkpointing and migration. :index:`signal<single: signal>`
       :index:`signal<single: signal; SIGUSR2>` :index:`signal<single: signal; SIGTSTP>`
    #. Sending or receiving the SIGUSR2 or SIGTSTP signals is not
       allowed. HTCondor reserves these signals for its own use. Sending
       or receiving all other signals is allowed.
       :index:`Unix<single: Unix; alarm>` :index:`Unix<single: Unix; timer>`
       :index:`Unix<single: Unix; sleep>`
    #. Alarms, timers, and sleeping are not allowed. This includes
       system calls such as ``alarm()``, ``getitimer()``, and
       ``sleep()``. ` <index://kernel-level;thread>`__
       ` <index://user-level;thread>`__
    #. Multiple kernel-level threads are not allowed. However, multiple
       user-level threads are allowed. ` <index://memory-mapped;file>`__
       :index:`Unix<single: Unix; mmap>`
    #. Memory mapped files are not allowed. This includes system calls
       such as ``mmap()`` and ``munmap()``. :index:`file<single: file; locking>`
       :index:`Unix<single: Unix; flock>` :index:`Unix<single: Unix; lockf>`
    #. File locks are allowed, but not retained between checkpoints.
       :index:`file<single: file; read only>` :index:`file<single: file; write only>`
    #. All files must be opened read-only or write-only. A file opened
       for both reading and writing will cause trouble if a job must be
       rolled back to an old checkpoint image. For compatibility
       reasons, a file opened for both reading and writing will result
       in a warning but not an error.
    #. A fair amount of disk space must be available on the submitting
       machine for storing a job’s checkpoint images. A checkpoint image
       is approximately equal to the virtual memory consumed by a job
       while it runs. If disk space is short, a special checkpoint
       server can be designated for storing all the checkpoint images
       for a pool. :index:`linking<single: linking; dynamic>`
       :index:`linking<single: linking; static>`
    #. On Linux, the job must be statically linked. *condor\_compile*
       does this by default. :index:`Unix<single: Unix; large files>`
    #. Reading to or writing from files larger than 2 GBytes is only
       supported when the submit side *condor\_shadow* and the standard
       universe user job application itself are both 64-bit executables.

    Note: these limitations only apply to jobs which HTCondor has been
    asked to transparently checkpoint. If job checkpointing is not
    desired, the limitations above do not apply.

 Security Implications.
    HTCondor does a significant amount of work to prevent security
    hazards, but loopholes are known to exist. HTCondor can be
    instructed to run user programs only as the UNIX user nobody, a user
    login which traditionally has very restricted access. But even with
    access solely as user nobody, a sufficiently malicious individual
    could do such things as fill up ``/tmp`` (which is world writable)
    and/or gain read access to world readable files. Furthermore, where
    the security of machines in the pool is a high concern, only
    machines where the UNIX user root on that machine can be trusted
    should be admitted into the pool. HTCondor provides the
    administrator with extensive security mechanisms to enforce desired
    policies.
 Jobs Need to be Re-linked to get Checkpointing and Remote System Calls
    Although typically no source code changes are required, HTCondor
    requires that the jobs be re-linked with the HTCondor libraries to
    take advantage of checkpointing and remote system calls. This often
    precludes commercial software binaries from taking advantage of
    these services because commercial packages rarely make their source
    and/or object code available. HTCondor’s other services are still
    available for these commercial packages.

      
