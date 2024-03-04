Example: Run a Job on a CPU if a GPU is not Available
=====================================================

Some jobs may be able to take advantage of GPU if one is available, but
can run on a CPU if there are no slots with GPUs immediately matchable.
The following submit file will match a job in this case.

.. code-block:: condor-submit

    ## Execution section. You will need to change these for your job
    executable                  = /bin/sleep
    arguments                   = 600

    ## Requirements section. Also, probably needs customization.
    request_CPUs                = 1
    request_Memory              = 2G
    request_Disk                = 20G
    require_GPUs                = Capability > 4

    ## File transfer section.
    should_transfer_files       = true
    transfer_input_files        = input1, input2

    ## Magic section: don't change anything with rank or request_GPUs unless you
    ## understand why it works.
    rank                        = RequestGPUs
    # If your job requires more than one GPU, you may change both
    # of the '1's in the next line to some other number.
    request_GPUs                = countMatches(RequireGPUs, AvailableGPUs) >= 1 ? 1 : 0

    ## Queue section.
    queue 1
