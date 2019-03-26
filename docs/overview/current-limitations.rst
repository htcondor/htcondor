      

Current Limitations
===================

:index:`limitations, under UNIX;HTCondor<single: limitations, under UNIX;HTCondor>`

 Limitations on Jobs which can Checkpointed
    Although HTCondor can schedule and run any type of process, HTCondor
    does have some limitations on jobs that it can transparently
    checkpoint and migrate:

    :index:`fork;Unix<single: fork;Unix>` :index:`exec;Unix<single: exec;Unix>`
    :index:`system;Unix<single: system;Unix>`

    #. Multi-process jobs are not allowed. This includes system calls
       such as ``fork()``, ``exec()``, and ``system()``.
       :index:`pipe;Unix<single: pipe;Unix>` :index:`semaphore;Unix<single: semaphore;Unix>`
       :index:`shared memory;Unix<single: shared memory;Unix>`
    #. Interprocess communication is not allowed. This includes pipes,
       semaphores, and shared memory. :index:`socket;Unix<single: socket;Unix>`
       :index:`network<single: network>`
    #. Network communication must be brief. A job may make network
       connections using system calls such as ``socket()``, but a
       network connection left open for long periods will delay
       checkpointing and migration. :index:`signal<single: signal>`
       :index:`SIGUSR2;signal<single: SIGUSR2;signal>` :index:`SIGTSTP;signal<single: SIGTSTP;signal>`
    #. Sending or receiving the SIGUSR2 or SIGTSTP signals is not
       allowed. HTCondor reserves these signals for its own use. Sending
       or receiving all other signals is allowed.
       :index:`alarm;Unix<single: alarm;Unix>` :index:`timer;Unix<single: timer;Unix>`
       :index:`sleep;Unix<single: sleep;Unix>`
    #. Alarms, timers, and sleeping are not allowed. This includes
       system calls such as ``alarm()``, ``getitimer()``, and
       ``sleep()``. ` <index://kernel-level;thread>`__
       ` <index://user-level;thread>`__
    #. Multiple kernel-level threads are not allowed. However, multiple
       user-level threads are allowed. ` <index://memory-mapped;file>`__
       :index:`mmap;Unix<single: mmap;Unix>`
    #. Memory mapped files are not allowed. This includes system calls
       such as ``mmap()`` and ``munmap()``. :index:`locking;file<single: locking;file>`
       :index:`flock;Unix<single: flock;Unix>` :index:`lockf;Unix<single: lockf;Unix>`
    #. File locks are allowed, but not retained between checkpoints.
       :index:`read only;file<single: read only;file>` :index:`write only;file<single: write only;file>`
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
       for a pool. :index:`dynamic;linking<single: dynamic;linking>`
       :index:`static;linking<single: static;linking>`
    #. On Linux, the job must be statically linked. *condor\_compile*
       does this by default. :index:`large files;Unix<single: large files;Unix>`
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

      
