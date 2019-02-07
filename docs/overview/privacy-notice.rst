      

Privacy Notice
==============

The HTCondor software periodically sends short messages to the HTCondor
Project developers at the University of Wisconsin, reporting totals of
machines and jobs in each running HTCondor system. An example of such a
message is given below.

The HTCondor Project uses these collected reports to publish summary
figures and tables, such as the total of HTCondor systems worldwide, or
the geographic distribution of HTCondor systems. This information helps
the HTCondor Project to understand the scale and composition of HTCondor
in the real world and improve the software accordingly.

The HTCondor Project will not use these reports to publicly identify any
HTCondor system or user without permission. The HTCondor software does
not collect or report any personal information about individual users.

We hope that you will contribute to the development of HTCondor through
this reporting feature. However, you are free to disable it at any time
by changing the configuration variables ``CONDOR_DEVELOPERS`` and
``CONDOR_DEVELOPERS_COLLECTOR`` , both described in section
`3.5.14 <ConfigurationMacros.html#x33-2010003.5.14>`__ of this manual.

Example of data reported:

::

    This is an automated email from the HTCondor system 
    on machine "your.condor.pool.com".  Do not reply. 
     
    This Collector has the following IDs: 
        HTCondor: 6.6.0 Nov 12 2003 
        HTCondor: INTEL-LINUX-GLIBC22 
     
                         Machines Owner Claimed Unclaimed Matched Preempting 
     
             INTEL/LINUX      810    52     716        37       0          5 
           INTEL/WINDOWS      120     5     115         0       0          0 
         SUN4u/SOLARIS28      114    12      92         9       0          1 
         SUN4x/SOLARIS28        5     1       0         4       0          0 
                   Total     1049    70     923        50       0          6 
     
             RunningJobs                IdleJobs 
                     920                    3868

      
