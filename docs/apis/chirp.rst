Chirp
=====

:index:`Chirp<single: Chirp; API>` :index:`Chirp API`

Chirp is a wire protocol and API that supports communication between a
running job and a Chirp server. The HTCondor system provides a Chirp
server running in the *condor_starter* that allows a job to

#. perform file I/O to and from the submit machine
#. update an attribute in its own job ClassAd
#. append the job event log file

This service is off by default; it may be enabled by placing in the
submit description file:

.. code-block:: condor-submit

    +WantIOProxy = True

This places the needed attribute into the job ClassAd.

The Chirp protocol is fully documented at
`http://ccl.cse.nd.edu/software/chirp/ <http://ccl.cse.nd.edu/software/chirp/>`_.

To provide easier access to this wire protocol, the *condor_chirp*
command line tool is shipped with HTCondor. This tool provides full
access to the Chirp commands.


