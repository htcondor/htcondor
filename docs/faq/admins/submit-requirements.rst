Recipe: How to require a submit file to contain some specific request
=====================================================================

Some pool administrators prefer their users to always set some
specific attribute, or to request some minimum amount of resources.
The *condor_schedd* :macro:`SUBMIT_REQUIREMENT_<Name>` mechanism can
enforce this, and cause :tool:`condor_submit` to fail when the
user does not follow the site policies. Optionally, this can be configured
to be a warning instead of an error, which can be useful when a
site is transitioning to a new policy.  Because this is implemented
in the *condor_schedd*, this same mechanism works for submits from
python.

Requiring jobs to request a minimum amount of memory
----------------------------------------------------

Let's say that an administrator wants to require that all jobs request
at least one gigabyte of memory.  The first thing to do is to define
a new submit requirements rule with a distinct name.  Let's call this
``MinimalRequestMemory``.  We must then add this rule to this list
of submit requirements rules that the schedd will enforce.  Note
that the *condor_schedd* will enforce all the rules in order in this
list, so if you have multiple rules, the order matters.

.. code-block:: condor-config

    SUBMIT_REQUIREMENT_NAMES = $(SUBMIT_REQUIREMENT_NAMES) MinimalRequestMemory

The next thing is to define which jobs will be affected by this rule.
In our case, we want this to apply to all jobs that request less than
one gigabyte of memory.  Because the unit of memory in HTCondor is
megabytes, and because the SUBMIT_REQUIREMENT mechanism does not support
unit suffices, we more express this as "1000" instead of "1 GB", with the
"megabytes" being implied.  Note that the expression defines what
a *successful* job looks like.  In our case, we want to say that
the job's :class:`RequestMemory` attribute must be greater than 1000,
in order to successfully pass this rule.  This is done by defining

.. code-block:: condor-config

    SUBMIT_REQUIREMENT_MinimalRequestMemory = RequestMemory > 1000

The above two knobs are sufficient to enforce the policy.  However, the
error message that the user will see when they violate this policy is
generally not very helpful.  We can improve this by defining a custom
error message to be shown when this rule is violated.  This is done by
defining a macro named :macro:`SUBMIT_REQUIREMENT_<Name>_REASON`, where
``<Name>`` is the name of the rule we defined above.
An example might look like

.. code-block:: condor-config

    SUBMIT_REQUIREMENT_MinimalRequestMemory_REASON = "All jobs must request at least 1 GB of memory."

Finally, if we want this to be a warning, and allow submit to proceed, we
can make this a warning by setting the :macro:`SUBMIT_REQUIREMENT_<Name>_IS_WARNING` to true.  For example,

.. code-block:: condor-config

    SUBMIT_REQUIREMENT_MinimalRequestMemory_IS_WARNING = True

Changing these will require a ``condor_reconfig`` of the *condor_schedd* to take
effect.
