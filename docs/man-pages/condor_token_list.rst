

*condor_token_list*
======================

list all available tokens for IDTOKENS auth
:index:`condor_token_list<single: condor_token_list; HTCondor commands>`\ :index:`condor_token_list command`

Synopsis
--------

**condor_token_list**

**condor_token_list** **-help**

Description
-----------

*condor_token_list* parses the tokens available to the current user and
prints them to ``stdout``.

The tokens are stored in files in the directory referenced by
*SEC_TOKEN_DIRECTORY*; multiple tokens may be saved in each file (one per
line).

The output format is a list of the deserialized contents of each token, along with the file name containing the token, one per
line.  It should not be considered machine readable and will be subject to
change in future release of HTCondor.

Options
-------

 **-help**
    Display brief usage information and exit.

Examples
--------

To list all tokens as the current user:

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

