      

Chirp
=====

Chirp is a wire protocol and API that supports communication between a
running job and a Chirp server. The HTCondor system provides a Chirp
server running in the *condor\_starter* that allows a job to

#. perform file I/O to and from the submit machine
#. update an attribute in its own job ClassAd
#. append the job event log file

This service is off by default; it may be enabled by placing in the
submit description file:

::

    +WantIOProxy = True

This places the needed attribute into the job ClassAd.

The Chirp protocol is fully documented at
`http://www3.nd.edu/ ccl/software/chirp <http://www3.nd.edu/~ccl/software/chirp>`__.

To provide easier access to this wire protocol, the *condor\_chirp*
command line tool is shipped with HTCondor. This tool provides full
access to the Chirp commands.

      
