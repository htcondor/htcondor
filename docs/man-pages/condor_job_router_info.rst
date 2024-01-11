*condor_job_router_info*
===========================

Discover and display information related to job routing
:index:`condor_job_router_info<single: condor_job_router_info; HTCondor commands>`\ :index:`condor_job_router_info command`

Synopsis
--------

**condor_job_router_info** [**-help | -version** ]

**condor_job_router_info** **-config**

**condor_job_router_info** **-match-jobs -jobads inputfile** [**-ignore-prior-routing** ]

**condor_job_router_info** **-route-jobs outputfile -jobads inputfile** [**-ignore-prior-routing**] [**-log-steps**]

Description
-----------

*condor_job_router_info* displays information about job routing. The
information will be either the available, configured routes or the
routes for specified jobs. *condor_job_router_info* can also be used
to simulate routing by supplying a job classad in a file.  This can
be used to test the router configuration offline.

Options
-------

 **-help**
    Display usage information and exit.
 **-version**
    Display HTCondor version information and exit.
 **-config**
    Display configured routes.
 **-match-jobs**
    For each job listed in the file specified by the **-jobads** option,
    display the first route found.
 **-route-jobs** *filename*
    For each job listed in the file specified by the **-jobads** option,
    apply the first route found and print the routed jobs to the specified
    output file. if *filename* is ``-`` the routed jobs are printed to ``stdout``.
 **-log-steps**
    When used with the **-route-jobs** option, print each transform step 
    as the job transforms are applied.
 **-ignore-prior-routing**
    For each job, remove any existing routing ClassAd attributes, and
    set attribute :ad-attr:`JobStatus` to the Idle state before finding the
    first route.
 **-jobads** *filename*
    Read job ClassAds from file *filename*. If *filename* is ``-``, then
    read from ``stdin``.

Exit Status
-----------

*condor_job_router_info* will exit with a status value of 0 (zero)
upon success, and it will exit with the value 1 (one) upon failure.

