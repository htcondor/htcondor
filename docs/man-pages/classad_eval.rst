.. _classad_eval:

*classad_eval*
======================

Evaluate the given ClassAd expression(s) in the context of the given
ClassAd attributes, and prints the result in ClassAd format.

:index:`classad_eval<single: classad_eval; HTCondor commands>`
:index:`classad_eval command`

Synopsis
--------

**classad_eval** *<ad>* *<expr>[ <expr>]\**

**classad_eval** **-file** *<name>* *<expr>[ <expr>]\**

Description
-----------

**classad_eval** is designed to help you understand and debug ClassAd
expressions.  You can supply a single ClassAd on the command-line, or
via a file, as context for evaluating the expression.

Examples
--------

Almost every ad and almost every expression will require you to single
quote them.  For example, the first line of the following won't work
if you have a file named ``attempt2``, but the second one will:

::

    classad_eval a=2 a*2
    classad_eval 'a = 2' 'a * 2'

The simplest ad that will work assigns a value to an attribute, as seen
above.  You may set multiple attributes in the following way:

::

    classad_eval '[ a = 2; b = 2 ]' 'a + b'

You must supply an empty ad for expressions that don't reference attributes:

::

    classad_eval '' 'strcat("foo", "bar")'

If you want to evaluate an expression in the context of the job ad, first
store the job ad in a file:

::

    condor_q -l 1777.2 > 1227.2.ad
    classad_eval -file 1277.2.ad 'JobUniverse'

You can extract a machine ad in a similar way:

::

    condor_status -l exec-17 > exec-17.ad
    classad_eval -file exec-17.ad 'Rank'

You can not supply more than one ad to **classad_eval**.

Exit Status
-----------

Returns 0 on success.

Author
------

Center for High Throughput Computing, University of Wisconsin-Madison

Copyright
---------

Copyright © 1990-2019 Center for High Throughput Computing, Computer
Sciences Department, University of Wisconsin-Madison, Madison, WI. All
Rights Reserved. Licensed under the Apache License, Version 2.0.
