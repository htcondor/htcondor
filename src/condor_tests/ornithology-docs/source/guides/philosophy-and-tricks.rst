.. py:currentmodule:: ornithology

Philosophy and Tricks
=====================

Fail-Fast Testing
-----------------

Functional testing of HTCondor takes a while, especially when a test fails,
particularly for tests which legimately have time-outs measured in minutes.
It will help you, too, when developing a test, to make sure that it fails as
early as possible.

Philosophically, you still want your test functions to be *post-facto*
assertions.  The subtle idea here is that your test functions assert
*success*, but your fixtures assert *failure*.  For instance -- in most
tests -- if a job goes on hold, you don't need to wait for it to complete
any more.  This specific example is common enough that Ornithology supports
it directly in its :func:`ClusterHandle.wait` method, with the
``fail_condition`` parameter.

Another example might be asserting that the return value of a tool you
called is zero in the fixture that calls the tool.  In ``test_cmd_now.py``,
for instance, two thing have to happen for the success test to pass: the
beneficiary job must start running, and the schedd must not leak memory.
The ``condor_now`` tool returning zero is a pre-condition for both these
assertions; if it doesn't, we can fail immediately.

Tuple-Parameterized Configuration
---------------------------------

A fixture can be paramaterized with arbitrarily-complicated data structures,
including, for instance, a submit hash.  This is relatively straightforward
if you just hand the whole structure off to a function.  In some cases,
however, you'll want to hande off different parts of the same data structure
to different fixtures.  For instance, in ``test_cmd_now.py``, when testing
the schedd's handling of different failure cases, each configuration which
injects a failure corresponds to a different error message in the log.  The
configuration is needed to start the personal condor, but the error message
isn't needed until an ``@action`` immediately before the test assertion.

One approach is to parameterize indices into the list of tuples; by
turning one data structure into two fixtures, it's easy to keep the data
together even when they're used very far apart.

.. code-block:: python

    failure_injection_map = {
        1: ("1", "[now job {0}]: coalesce command timed out, failing"),
        2: ("2", "[now job {0}]: coalesce failed: FAILURE INJECTION: 2"),
        # ...
    }

    @config(
        params={
            "time_out": 1,
            "transient_failure": 2,
            # ...
        }
    )
    def which_failure_injection(request):
        return request.param

    # This looks silly because the config values happen to be ints.
    def failure_config_value(which_failure_injection):
        return failure_injection_map[which_failure_injection][0]

    # You could instead write fixtures which take which_failure_injection as
    # an argument, but this layer of indirection results in both shorter
    # and more semantic error messages, as well as reducing the number of
    # occurences of the magic constant to one.
    def failure_log_message(which_failure_injection):
        return failure_injection_map[which_failure_injection][1]

I prefer to name the test tuples directly:

.. code-block:: python

    @config(
        params={
            "time_out": (1, "[now job {0}]: coalesce command timed out, failing"),
            "transient_failure": ("2", "[now job {0}]: coalesce failed: FAILURE INJECTION: 2"),
            # ...
        }
    )
    def which_failure_injection(request):
        return request.param

    def failure_config_value(which_failure_injection):
        return which_failure_injection[0]

    def failure_log_message(which_failure_injection):
        return which_failure_injection[1]

Debugging Structure
-------------------

The ``pytest`` command-line option ``--setup-show`` and ``--setup-plan``
display the sequence of fixture set-up, test execution, and fixture
tear-down; the former causes ``pytest`` to do nothing but the display.

This sequence can be useful for untangling dependencies, or noticing missing
ones, when a test under development misbehaves.  This includes runinng more
(or fewer) tests than anticipated when parameterizing fixtures
