*classad_eval*
==============

Evaluate ClassAd expression(s) in the context of the provided ClassAd attributes.

:index:`classad_eval<double: classad_eval; HTCondor commands>`

Synopsis
--------

**classad_eval** [**-help**]

**classad_eval** [*options*] *<ad|expr|assignment>* [*<ad|expr|assignment>* ...]

Description
-----------

**classad_eval** tests and/or debugs ClassAd expressions provided via the command
line when evaluated in a specific contexts of source and target ClassAds.

Options
-------

**-help**
  Display tool usage.
**-quiet**
  Disable printing of context ClassAds before evaluation results.
**-debug**
  Enable tool debugging.
**-[my-]file** *path*
  A file containing the source ClassAd context.

  .. warning::

    This option overrides any previous ClassAd context.

**-target-file** *path*
  A file containing the target ClassAd context.
*<ad|expr|assignment>*
  A ClassAd context modification (inline ad ``'[ foo = 5; bar = False ]'``
  or assignment ``'foo = 10'``) or expression to be evaluated (``'foo * 100'``).

General Remarks
---------------

Options, flags, and arguments may be freely intermixed, and take effect
in left to right order.

Attributes specified on the command line, including those specified as part
of a complete ad, replace attributes in the context ad, which starts empty.
You can't remove attributes from the context ad, but you can set them to
``undefined``.

Use single quotes to denote chunks of ClassAd assignments and expressions
to prevent issues between shell quoting and ClassAd quoting.

By default, the full ClassAd context will be printed out before evaluating
an expression. This behavior can be disabled by specifying ``-quiet``.

This tool will only digests the first ClassAd in a list of ClassAds
provided via a file.

.. note::

    This tool uses the new ClassAd syntax denoted by semicolon delimited
    attribute-value pairs contained in square brackets: ``[ foo = 1; bar = False ]``.

Exit Status
-----------

0  -  Success

1  -  ClassAd context update failure

255  -  Invalid command line option provided

Examples
--------

Specifying a single attribute assignment for evaluation:

.. code-block:: console

    $ classad_eval 'a = 2' 'a * 2'

Specifying a single attribute assignment for evaluation without whitespace quoting:

.. code-block:: console

    $ classad_eval a=2 a*2

Specifying multiple attributes for evaluation:

.. code-block:: console

    $ classad_eval 'a = 2' 'b = 2' 'a + b'

Specifying inline ClassAd from evaluation:

.. code-block:: console

    $ classad_eval '[ a = 2; b = 2 ]' 'a + b'

Evaluating a ClassAd expression that requires no context ClassAds:

.. code-block:: console

    $ classad_eval 'strcat("foo", "bar")'

Evaluating an expression multiple times while changing the context:

.. code-block:: console

    $ classad_eval 'x = y' 'x' 'y = 7' 'x' '[ x = 6; z = "foo"; ]' 'x'
    [ x = y ]
    undefined
    [ y = 7; x = y ]
    7
    [ z = "foo"; x = 6; y = 7 ]
    6


Disabling the evaluation context ad echoing part way through evaluations:

.. code-block:: console

    $ classad_eval 'x = y' 'x' -quiet 'y = 7' 'x' '[ x = 6; z = "foo"; ]' 'x'
    [ x = y ]
    undefined
    7
    6

Evaluating an expression from a saved job ad:

.. code-block:: console

    $ condor_q -l:new 1234.5 > job.ad
    $ classad_eval -quiet -file job.ad 'JobUniverse'

Evaluating an expression from a saved slot ad:

.. code-block:: console

    $ condor_status slot1@ep.host.name -l:new > slot.ad
    $ classad_eval -quiet -file slot.ad 'Rank'

Evaluating an expression from a saved job ad and saved slot ad:

.. code-block:: console

    $ condor_q -l:new 1234.5 > job.ad
    $ condor_status slot1@ep.host.name -l:new > slot.ad
    $ classad_eval -quiet -my-file job.ad -target-file slot.ad 'MY.requirements' 'TARGET.requirements'

See Also
--------

:tool:`classads`

Availability
------------

Linux, MacOS, Windows
