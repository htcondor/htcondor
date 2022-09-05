*condor_ssl_fingerprint*
========================

list the fingerprint of X.509 certificates for use with SSL authentication
:index:`condor_ssl_fingerprint<single: condor_ssl_fingerprint; HTCondor commands>`\ :index:`condor_ssl_fingerprint command`

Synopsis
--------

**condor_ssl_fingerprint** *[FILE]*

Description
-----------

*condor_ssl_fingerprint* parses provided file for X.509 certificcates and prints
prints them to ``stdout``.  If no file is provided, then it defaults to printing
out the user's ``known_hosts`` file (typically, in ``~/.condor/known_hosts``).

If a single PEM-formatted X.509 certificate is found, then its fingerprint is printed.

The X.509 fingerprints can be used to verify the authenticity of an SSL authentication
with a remote daemon.

Examples
--------

To print the fingerprint of a host certificate

.. code-block:: console

    $ condor_token_list
    Header: {"alg":"HS256","kid":"POOL"} Payload: {"exp":1565576872,"iat":1565543872,"iss":"htcondor.cs.wisc.edu","scope":"condor:\/DAEMON","sub":"k8sworker@wisc.edu"} File: /home/bucky/.condor/tokens.d/token1
    Header: {"alg":"HS256","kid":"POOL"} Payload: {"iat":1572414350,"iss":"htcondor.cs.wisc.edu","scope":"condor:\/WRITE","sub":"bucky@wisc.edu"} File: /home/bucky/.condor/tokens.d/token2

Exit Status
-----------

*condor_token_list* will exit with a non-zero status value if it
fails to read the token directory, tokens are improperly formatted,
or if it experiences some other error.  Otherwise, it will exit 0.


See also
--------

:manpage:`condor_token_create(1)`, :manpage:`condor_token_fetch(1)`, :manpage:`condor_token_request(1)`

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.

