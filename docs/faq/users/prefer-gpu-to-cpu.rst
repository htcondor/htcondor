Example: Run a Job on a CPU if a GPU is not Available
=====================================================

.. code-block:: condor-submit

    ## Magic section: don't change anything in this section unless you
    ## understand why it works.
    rank                        = RequestGPUs
    # If your job requires more than one GPU, you may change both
    # of the '1's in the next line to some other number.
    request_GPUs                = countMatches(RequireGPUs, AvailableGPUs) >= 1 ? 1 : 0

    ## Requirements section.
    request_CPUs                = 1
    require_GPUs                = Capability > 4

    ## File transfer section.
    should_transfer_files       = true
    transfer_executable         = false

    ## Execution section.
    executable                  = /bin/sleep
    arguments                   = 600

    ## Queue section.
    queue 1
