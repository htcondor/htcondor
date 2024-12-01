API
===

:doc:`api/fixtures`
    Domains-specific decorators for defining fixtures.

:doc:`api/predefined`
    Fixtures provided by Ornithology.

:doc:`api/condor`
    The central API for standing up and tearing down HTCondor pools, as well as
    the entrypoint for communicating with them (submitting jobs, reading daemon
    log files, etc.).

:doc:`api/cmd`
    Tools for running commands and inspecting their output.

:doc:`api/handles`
    Tools for managing and inspecting jobs.

:doc:`api/jobs`
    Abstractions for job IDs and job state.

:doc:`api/job_queue`
    Tools for inspecting the job queue log directly.

:doc:`api/daemons`
    Tools for inspecting HTCondor daemon logs.

:doc:`api/custom_plugins`
    Custom file transfer plugins provided by Ornithology.

:doc:`api/io`
    Tools for writing and reading files.

:doc:`api/env`
    Tools for manipulating the environment.

:doc:`api/helpers`
    Functions that help check complex-but-generic conditions, like "the items in the collection are in order".

:doc:`api/meta`
    Tools for inspecting the Python runtime, like asking "what is the name of the function I am currently in?".


.. toctree::
   :maxdepth: 2
   :hidden:

   api/fixtures
   api/predefined
   api/condor
   api/cmd
   api/handles
   api/jobs
   api/job_queue
   api/daemons
   api/custom_plugins
   api/io
   api/env
   api/helpers
   api/meta
