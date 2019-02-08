      

Matchmaking with ClassAds
=========================

Before you learn about how to submit a job, it is important to
understand how HTCondor allocates resources. Understanding the unique
framework by which HTCondor matches submitted jobs with machines is the
key to getting the most from HTCondor’s scheduling algorithm.

HTCondor simplifies job submission by acting as a matchmaker of
ClassAds. HTCondor’s ClassAds are analogous to the classified
advertising section of the newspaper. Sellers advertise specifics about
what they have to sell, hoping to attract a buyer. Buyers may advertise
specifics about what they wish to purchase. Both buyers and sellers list
constraints that need to be satisfied. For instance, a buyer has a
maximum spending limit, and a seller requires a minimum purchase price.
Furthermore, both want to rank requests to their own advantage.
Certainly a seller would rank one offer of $50 dollars higher than a
different offer of $25. In HTCondor, users submitting jobs can be
thought of as buyers of compute resources and machine owners are
sellers.

All machines in a HTCondor pool advertise their attributes, such as
available memory, CPU type and speed, virtual memory size, current load
average, along with other static and dynamic properties. This machine
ClassAd also advertises under what conditions it is willing to run a
HTCondor job and what type of job it would prefer. These policy
attributes can reflect the individual terms and preferences by which all
the different owners have graciously allowed their machine to be part of
the HTCondor pool. You may advertise that your machine is only willing
to run jobs at night and when there is no keyboard activity on your
machine. In addition, you may advertise a preference (rank) for running
jobs submitted by you or one of your co-workers.

Likewise, when submitting a job, you specify a ClassAd with your
requirements and preferences. The ClassAd includes the type of machine
you wish to use. For instance, perhaps you are looking for the fastest
floating point performance available. You want HTCondor to rank
available machines based upon floating point performance. Or, perhaps
you care only that the machine has a minimum of 128 MiB of RAM. Or,
perhaps you will take any machine you can get! These job attributes and
requirements are bundled up into a job ClassAd.

HTCondor plays the role of a matchmaker by continuously reading all the
job ClassAds and all the machine ClassAds, matching and ranking job ads
with machine ads. HTCondor makes certain that all requirements in both
ClassAds are satisfied.

Inspecting Machine ClassAds with condor\_status
-----------------------------------------------

Once HTCondor is installed, you will get a feel for what a machine
ClassAd does by trying the *condor\_status* command. Try the
*condor\_status* command to get a summary of information from ClassAds
about the resources available in your pool. Type *condor\_status* and
hit enter to see a summary similar to the following:

::

    Name               OpSys      Arch   State     Activity LoadAv Mem   ActvtyTime 
     
    amul.cs.wisc.edu   LINUX      INTEL  Claimed   Busy     0.990  1896  0+00:07:04 
    slot1@amundsen.cs. LINUX      INTEL  Owner     Idle     0.000  1456  0+00:21:58 
    slot2@amundsen.cs. LINUX      INTEL  Owner     Idle     0.110  1456  0+00:21:59 
    angus.cs.wisc.edu  LINUX      INTEL  Claimed   Busy     0.940   873  0+00:02:54 
    anhai.cs.wisc.edu  LINUX      INTEL  Claimed   Busy     1.400  1896  0+00:03:03 
    apollo.cs.wisc.edu LINUX      INTEL  Unclaimed Idle     1.000  3032  0+00:00:04 
    arragon.cs.wisc.ed LINUX      INTEL  Claimed   Busy     0.980   873  0+00:04:29 
    bamba.cs.wisc.edu  LINUX      INTEL  Owner     Idle     0.040  3032 15+20:10:19

…

The *condor\_status* command has options that summarize machine ads in a
variety of ways. For example,

 *condor\_status -available*
    shows only machines which are willing to run jobs now.
 *condor\_status -run*
    shows only machines which are currently running jobs.
 *condor\_status -long*
    lists the machine ClassAds for all machines in the pool.

Refer to the *condor\_status* command reference page located on
page \ `2113 <Condorstatus.html#x147-106400012>`__ for a complete
description of the *condor\_status* command.

The following shows a portion of a machine ClassAd for a single machine:
turunmaa.cs.wisc.edu. Some of the listed attributes are used by HTCondor
for scheduling. Other attributes are for information purposes. An
important point is that any of the attributes in a machine ClassAd can
be utilized at job submission time as part of a request or preference on
what machine to use. Additional attributes can be easily added. For
example, your site administrator can add a physical location attribute
to your machine ClassAds.

::

    Machine = "turunmaa.cs.wisc.edu" 
    FileSystemDomain = "cs.wisc.edu" 
    Name = "turunmaa.cs.wisc.edu" 
    CondorPlatform = "$CondorPlatform: x86_rhap_5 $" 
    Cpus = 1 
    IsValidCheckpointPlatform = ( ( ( TARGET.JobUniverse == 1 ) == false ) || 
     ( ( MY.CheckpointPlatform =!= undefined ) && 
     ( ( TARGET.LastCheckpointPlatform =?= MY.CheckpointPlatform ) || 
     ( TARGET.NumCkpts == 0 ) ) ) ) 
    CondorVersion = "$CondorVersion: 7.6.3 Aug 18 2011 BuildID: 361356 $" 
    Requirements = ( START ) && ( IsValidCheckpointPlatform ) 
    EnteredCurrentActivity = 1316094896 
    MyAddress = "<128.105.175.125:58026>" 
    EnteredCurrentState = 1316094896 
    Memory = 1897 
    CkptServer = "pitcher.cs.wisc.edu" 
    OpSys = "LINUX" 
    State = "Owner" 
    START = true 
    Arch = "INTEL" 
    Mips = 2634 
    Activity = "Idle" 
    StartdIpAddr = "<128.105.175.125:58026>" 
    TargetType = "Job" 
    LoadAvg = 0.210000 
    CheckpointPlatform = "LINUX INTEL 2.6.x normal 0x40000000" 
    Disk = 92309744 
    VirtualMemory = 2069476 
    TotalSlots = 1 
    UidDomain = "cs.wisc.edu" 
    MyType = "Machine"

      
