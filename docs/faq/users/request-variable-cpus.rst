Example: Request a Variable Number of CPU Cores
================================================

Some jobs can take advantage of multiple CPU cores if available, but can
also run with fewer cores. The following submit file will match a job that
has at least a minimum number of cores, while preferring machines with more
cores up to a specified maximum. This allows your job to use whatever
resources are available while ensuring it gets enough to run efficiently.

.. code-block:: condor-submit

    # This example comes from
    # https://htcondor.readthedocs.io/en/latest/auto-redirect.html?category=example&tag=variable-cpu-core-request

    ## Execution section. You will need to change these for your job
    executable                  = /bin/sleep
    arguments                   = 600

    ## Requirements section. Customize the CPU min/max for your job
    MinCpus                     = 4
    MaxCpus                     = 8
    request_cpus                = (Cpus > $(MaxCpus) ? $(MaxCpus) : Cpus) ?: $(MinCpus)
    requirements                = Cpus >= $(MinCpus) && (PartitionableSlot || Cpus <= $(MaxCpus))
    rank                        = Cpus

    ## Resource requests
    request_memory              = 2G
    request_disk                = 1G

    ## File transfer section
    should_transfer_files       = true
    when_to_transfer_output     = on_exit

    ## Queue section
    queue 1

How It Works
------------

The key parts of this submit file are:

- **MinCpus** and **MaxCpus**: User-defined variables that set the acceptable
  range of CPU cores for the job.

- :subcom:`request_cpus`: Uses a ClassAd expression to request the number of CPUs
  available on the matched slot, capped at ``MaxCpus``. If the expression
  cannot be evaluated (on machines without a ``Cpus`` attribute), it falls
  back to requesting ``MinCpus``.

- :subcom:`requirements`: Ensures the job only matches machines with at least
  ``MinCpus`` cores. The ``PartitionableSlot`` check allows matching on
  partitionable slots (which can be divided), while the ``Cpus <= MaxCpus``
  check ensures static slots don't exceed the maximum.

- :subcom:`rank`: Tells HTCondor to prefer machines with more CPU cores, so your
  job will match the largest available slot within your acceptable range.

Your job will receive the actual number of cores via the :ad-attr:`Cpus`
attribute, which you can access in your job's environment or through the
job ClassAd.
