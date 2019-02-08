      

Potential Problems
==================

Renaming of argv[0]
-------------------

When HTCondor starts up your job, it renames argv[0] (which usually
contains the name of the program) to condor\_exec. This is convenient
when examining a machine’s processes with the Unix command *ps*; the
process is easily identified as an HTCondor job.

Unfortunately, some programs read argv[0] expecting their own program
name and get confused if they find something unexpected like
condor\_exec.

      
