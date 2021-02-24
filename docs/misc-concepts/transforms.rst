ClassAd Transforms
===================

:index:`transforms`

HTCondor has a general purpose language for transforming ClassAds,
this language is used by the *condor_schedd* for submit transforms,
and as of version 8.9.7 by the job router for routes and pre and post route transforms.

There is also a stand alone tool ``condor_transform_ads`` than can
read ClassAds from a file or pipe, transform them, and write the
resulting ClassAds to a file or pipe.

The transform language is build on the same basic macro expansion
engine use by HTCondor configuration and by *condor_submit* and
shares many of the same features such as ``$()`` macro expansion
and if statements.

This transform language is a superset of an earlier transform language
based on New ClassAds.  The *condor_schedd* and *condor_job_router* will
still allow the earlier transform language, and they will automatically
convert configuration from earlier New ClassAds style transforms 
to the to the native transform language when they read the configuration.

General Concepts
-------------------

Transforms consists of a sequence of lines containing ``key=value`` pairs or
transform commands such as ``SET``.  Transform commands execute in order from
top to bottom and may make use of macro values set by earlier statements using
``$(var)`` macro substitution.   Unlike configuration files, Transform commands
will use the value of ``$(var)`` defined at the time, rather than the last value
defined in the configuration file. 

If/else statements and macro functions such as ``$INT(var)`` can be used in transforms,
but `include` may not be used.

A macro expansion of the form ``$(MY.<attr>)`` will expand as the value of the attribute
``<attr>`` of the ClassAd that is being transformed.  Expansion will expand simple string values without
quotes but will not evaluate expressions. Use ``$STRING(MY.<attr>)`` or ``$INT(MY.<attr>)`` if
you need to evaluate the ClassAd attribute before expanding it. 

The existence of an attribute in the ClassAd being transformed can be tested by using ``if defined MY.<attr>``

In the definitions below.

    ``<attr>``    must be a valid ClassAd attribute name

    ``<newattr>`` must be a valid ClassAd attribute name

    ``<expr>``    must be a valid ClassAd expression after ``$()`` macro expansion.  Don't forget to quote string values!

    ``<var>``     must be a valid macro name

    ``<regex>``   is a regular expression

    ``<attrpat>`` is a regular expression substitution pattern, which may include capture groups ``\0``, ``\1``, etc.

Transform Commands
-------------------

``<var> = <value>``
   Sets the temporary macro variable ``<var>`` to ``<value>``. This is the same sort of macro assignment used
   in configuration and submit files, the value is everything after the ``=`` until then end of the line
   with leading and trailing whitespace removed.   Variables set in this way do not directly affect the
   resulting transformed ClassAd, but they can be used later in the transform by ``$(var)`` macro expansion. 
   In the *condor_job_router* some macro variable names will affect the way the router behaves.  For a list
   of macro variable names have have special meaning to the *condor_job_router* see the 
   :ref:`grid-computing/job-router:Routing Table Entry Commands and Macro values` section.

``REQUIREMENTS <expr>``
   Apply the transform only if the expression given by ``<expr>`` evaluates to true when evaluated against
   the untransformed ClassAd.

``SET <attr> <expr>``
   Sets the ClassAd value of ``<attr>`` to ``<expr>`` in the ClassAd being transformed.

``DEFAULT <attr> <expr>``
    Sets the ClassAd value of ``<attr>`` to ``<expr>`` in the ClassAd being transformed if
    that ClassAd does not currently have ``<attr>`` or if it is currently set to ``undefined``.  This
    is equivalent to

    .. code-block:: condor-config

      if ! defined MY.<Attr>
        SET <Attr> <value>
      endif

``EVALSET <attr> <expr>``
    Evaluate ``<expr>`` and set the ClassAd value of ``<attr>`` to the result of the evaluation.
    Use this when the ClassAd value of ``<attr>`` must be a simple value rather than expression,
    or when you need to capture the result of evaluating at transform time.  Note that it is
    usually better to use ``SET`` with macro expansions when you want to modify a ClassAd attribute
    as part of a transform.

``EVALMACRO <var> <expr>``
    Evaluate ``<expr>`` and set the temporary macro variable ``<var>`` to the result of evaluation.
    ``$(var)`` can the be used in later transform statements such as ``SET`` or ``if``.

``COPY <attr> <newattr>``
    Copies the ClassAd value of ``<attr>`` to a new ClassAd attribute ``<newattr>``.  This will result
    in two attributes that have the same value at this step of the transform.

``COPY /<regex>/ <attrpat>``
    Copies all ClassAd attributes that have names matching the regular expression ``<regex>`` to new attribute names.
    The new attribute names are defined by ``<attrpat>`` which may have regular expression capture groups to substitute
    portions of the original attribute name. ``\0`` Is the entire attribute name, and ``\1`` is the first capture, etc.
    For example

    .. code-block:: condor-config

      # copy all attributes whose names begin with Resource to new attribute with names that begin with OriginalResource
      COPY /Resource(.+)/ OriginalResource\1

``RENAME <attr> <newattr>``
    Renames the attribute ``<attr>`` to a new attribute name ``<newattr>``. This is the equivalent of 
    a COPY statement followed by a DELETE statement. 

``RENAME /<regex>/ <attrpat>``
    Renames all ClassAd attributes that match the regular expression ``<regex>`` to new attribute names given by
    the substitution pattern ``<attrpat>``.

``DELETE <attr>``
    Deletes the ClassAd attribute ``<attr>`` from the transformed ClassAd.

``DELETE /<regex>/``
    Deletes all ClassAd attributes whose names match the regular expression ``<regex>`` from the transformed ClassAd.

:index:`transforms`


