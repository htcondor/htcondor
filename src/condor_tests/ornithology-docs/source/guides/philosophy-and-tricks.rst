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

Simulating AP/Daemon Restarts
------------------------------

:meth:`Condor.restart` and :meth:`Condor.restart_daemon` interrupt a
running pool -- the whole pool, or a single named daemon -- in one of
three ways, given as a :class:`RestartMode`:

- ``RestartMode.GRACEFUL``: ``condor_restart`` (clean shutdown, then restart).
- ``RestartMode.FAST``: ``condor_restart -fast`` (abrupt but signaled shutdown).
- ``RestartMode.CRASH``: SIGKILLs the daemon process(es) directly, with no
  cleanup at all. POSIX-only (not supported on Windows); this is the only
  mode that actually interrupts an in-flight job process running under the
  affected daemon (e.g. a local-universe node under ``startd``) -- GRACEFUL
  and FAST let already-running jobs finish undisturbed.

Both methods block until the affected daemon(s) have genuinely come back
(confirmed via two consecutive, matching ``condor_who`` checks, not just
one -- a single "yes" can be a momentarily-stale read right after a
restart), raising ``TimeoutError`` if they don't within ``timeout`` seconds.

.. code-block:: python

    from ornithology import Condor, RestartMode

    condor.restart(mode=RestartMode.GRACEFUL)          # whole pool
    condor.restart_daemon("schedd", mode=RestartMode.CRASH)  # one daemon

If your test needs a job to still be genuinely running (or blocked) at
the exact moment the restart lands, don't rely on a fixed ``time.sleep()``
in the job's own script to guess how long the restart will take on a
given machine -- that races the real restart-command execution time and
can silently pass without ever actually landing the interruption on a
slow machine. Instead, have the job poll for a sentinel file's existence
and only create that file yourself once the restart has actually
completed (see ``dagman_recovery_utils.py``'s ``write_attempt_script``/
``write_recoverability_dag`` in ``src/condor_tests`` for a worked example).

Debugging Structure
-------------------

The ``pytest`` command-line option ``--setup-show`` and ``--setup-plan``
display the sequence of fixture set-up, test execution, and fixture
tear-down; the former causes ``pytest`` to do nothing but the display.

This sequence can be useful for untangling dependencies, or noticing missing
ones, when a test under development misbehaves.  This includes runinng more
(or fewer) tests than anticipated when parameterizing fixtures
