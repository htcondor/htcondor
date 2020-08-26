.. _classad_eval:

*classad_eval*
======================

Evaluate the given ClassAd expression(s) in the context of the given
ClassAd attributes, and prints the result in ClassAd format.

:index:`classad_eval<single: classad_eval; HTCondor commands>`
:index:`classad_eval command`

Synopsis
--------

**classad_eval** [**-file** <*name*>] <*ad* | *assignment* | *expression*>\+

Description
-----------

**classad_eval** is designed to help you understand and debug ClassAd
expressions.  You can supply a ClassAd on the command-line, or via a
file, as context for evaluating the expression.  You may also construct
a ClassAd one argument at a time, with assignments.

By default, **clasad_eval** will print the ClassAd context used to evaluate
the expression before printing the result of the first expression, and for
every expression with a new ClassAd thereafter.  You may suppress this
behavior with the ``-quiet`` flag, which replaces an ad, assignment,
or expression, and quiets every expression after it on the command line.

Attributes specified on the command line, including those specified as part
of a complete ad, replace attributes in the context ad, which starts empty.
You can't remove attributes from the context ad, but you can set them to
``undefined``.

Examples
--------

Almost every ad, assignment, or expression will require you to single
quote them.  There are some exceptions; for instance, the following two
commands are equivalent:

.. code-block:: console

    $ classad_eval 'a = 2' 'a * 2'
    $ classad_eval a=2 a*2

You can specify attributes for the context ad in three ways:

.. code-block:: console

    $ classad_eval '[ a = 2; b = 2 ]' 'a + b'
    $ classad_eval 'a = 2; b = 2' 'a + b'
    $ classad_eval 'a = 2' 'b = 2' 'a + b'

You need not supply an empty ad for expressions that don't reference attributes:

.. code-block:: console

    $ classad_eval 'strcat("foo", "bar")'

If you want to evaluate an expression in the context of the job ad, first
store the job ad in a file:

.. code-block:: console

    $ condor_q -l 1777.2 > 1227.2.ad
    $ classad_eval -file 1277.2.ad 'JobUniverse'

You can extract a machine ad in a similar way:

.. code-block:: console

    $ condor_status -l exec-17 > exec-17.ad
    $ classad_eval -file exec-17.ad 'Rank'

You can not supply more than one ad to **classad_eval**.  Assignments
(including whole ClassAds) are all merged into the context ad:

.. code-block:: console

    $ classad_eval 'x = y' 'x' 'y = 7' 'x' 'x=6' 'x'
    [ x = y ]
    undefined
    [ y = 7; x = y ]
    7
    [ y = 7; x = 6 ]
    6

You can suppress printing the context ad partway through:

.. code-block:: console

    $ classad_eval -file example 'x' -quiet 'y = 7' 'x' 'x=6' 'x'
    [ x = y ]
    undefined
    7
    6

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
