``pytest`` Integration
======================

.. py:currentmodule:: ornithology

What is ``pytest``?
-------------------

``pytest`` is a Python testing framework designed primarily for testing Python
libraries and applications (`docs <https://docs.pytest.org/en/latest/>`_).
We have repurposed it for testing what its perspective is an "external program", HTCondor.
Because of its plugin-oriented architecture, this does not involve "hacking on the internals",
and so far, all of the necessary repurposing has been accomplished by plugin-style code.

Test Suite Execution
--------------------

A ``pytest`` test suite is a directed acyclic graph (DAG) with two kinds of nodes:
**tests** and **fixtures**. Tests and fixtures may depend on fixtures, but neither can
depend on tests.
Thus, tests form the leaves the DAG, while the fixtures are the internal nodes.

Fixtures have **scope**. Fixtures may only depend on other fixtures which have
the same or higher-level scope. Fixture return values are cached over the lifetime
specified by their scope.
The available scopes are: ``session``, ``module``, ``class``, and ``function``
(``function`` is the default).

Tests and fixtures are both represented by Python functions.
To depend on a fixture, put the name of a fixture in the argument list
of the function.
When ``pytest`` executes, it "collects" your tests, constructs the DAG by following
the dependencies specified locally in tests and fixtures, and determines an
execution order for fixtures and tests that is consistent with the scoping
rules of the fixtures and the dependencies between them.

For example, this code:

.. code-block:: python

   import pytest

   @pytest.fixture()
   def A(): ...

   @pytest.fixture()
   def B(A): ...

   @pytest.fixture()
   def C(A): ...

   def test_1(A): ...

   def test_2(B, C): ...

   def test_3(B): ...

corresponds to this DAG:

.. graphviz::

   digraph pytest_example_suite {
        "Fixture A" -> {"Fixture B" "Fixture C" "Test 1"};
        "Fixture B" -> {"Test 2" "Test 3"};
        "Fixture C" -> {"Test 2"};

        "Fixture A" [shape=rectangle]
        "Fixture B" [shape=rectangle]
        "Fixture C" [shape=rectangle]
   }

and ``pytest`` would execute the suite in this order:

.. code-block::

        SETUP    F A
        test.py::test_1 (fixtures used: A)
        TEARDOWN F A
        SETUP    F A
        SETUP    F B (fixtures used: A)
        SETUP    F C (fixtures used: A)
        test.py::test_2 (fixtures used: A, B, C)
        TEARDOWN F C
        TEARDOWN F B
        TEARDOWN F A
        SETUP    F A
        SETUP    F B (fixtures used: A)
        test.py::test_3 (fixtures used: A, B)
        TEARDOWN F B
        TEARDOWN F A


Notice how the same fixtures are "set up" and "torn down" multiple times due to
the scoping rules: because they have the default ``function`` scope, they cannot
be re-used between test functions.

If fixture ``A`` was session-scoped

.. code-block:: python

   import pytest

   @pytest.fixture(scope = 'session')
   def A(): ...

the execution order would be

.. code-block::

    SETUP    S A
        test.py::test_1 (fixtures used: A)
        SETUP    F B (fixtures used: A)
        SETUP    F C (fixtures used: A)
        test.py::test_2 (fixtures used: A, B, C)
        TEARDOWN F C
        TEARDOWN F B
        SETUP    F B (fixtures used: A)
        test.py::test_3 (fixtures used: A, B)
        TEARDOWN F B
    TEARDOWN S A


Notice how ``A`` is now only set up and torn down once.

Making Assertions
-----------------

Every test should function should ideally make one (or more, but hopefully just one)
**assertion**. ``pytest`` reuses Python's built-in assertion mechanism, which
is the ``assert`` keyword, documented
`here <https://docs.python.org/3/reference/simple_stmts.html#assert>`_.

An assertion looks like ``assert <statement>``.
If the ``<statement>`` is "truthy", the assertion passes;
if it is "falsey", an ``AssertionError`` is raised.
``pytest`` catches the ``AssertionError`` and produces detailed error output.

Truthiness is fairly complicated in Python, and use of implicit truthiness
can make tests hard to read.
We recommend **always explicitly producing a boolean value to assert on**.
For example, if you want to assert that a list ``x`` is not empty,
write ``assert len(x) > 0``,
**not** ``assert x`` (even though a non-empty list is "truthy").


How does ``ornithology`` integrate with ``pytest``?
---------------------------------------------------

``ornithology`` itself mostly does not reference ``pytest``. Instead, integration is
provided by hooking into ``pytest`` via standard ``pytest`` configuration files,
namely ``src/condor_tests/conftest.py``. This file pre-defines several useful
fixtures, and sets up several hooks that slightly modify how ``pytest`` executes.

Ornithology also provides a set of **domain-specific fixture decorators**.
These functions produce standard ``pytest`` fixtures as described above, but
with settings pre-configured to help write standardized HTCondor tests.


Fixture-Defining Decorators
---------------------------

Ornithology provides domain-specific fixture decorators help you write tests
that use language closer to our familiar HTCondor idioms: :func:`config`,
:func:`standup`, and :func:`action`.
If your tests do not need a personal condor, you can likely bypass all of this
and just use standard pytest fixtures.

Configuration happen before the test's condors are launched.
Standup is the process of launching the test's condors.
Actions happen after condor is ready to receive instructions.
Generally, a test file will have a few configuration fixtures,
a single standup fixture,
and many actions.

All :func:`config` fixtures run before all :func:`standup` fixtures, which run
before all :func:`action` fixtures.
The :func:`standup` fixtures should generally yield an instance of
:class:`Condor`.
The :func:`~conftest.test_dir` fixture becomes available (via fixture scoping rules)
starting with :func:`standup`.
To use these scoping rules correctly, all tests must be written as part of a
test class.

For example, this code:

.. code-block:: python

    from conftest import config, standup, action

    @config
    def determine_params(): ...

    @standup
    def condor(determine_params): ...

    @action
    def submit_jobs(condor): ...

    class TestJobs:
        def test_job_results(self, submit_jobs): ...

corresponds to this DAG:

.. graphviz::

   digraph dsl {
        subgraph cluster_config {
            label = "Config"
            style = dotted
            "Fixture determine_params" [shape=rectangle]
        }
        subgraph cluster_standup {
            label = "Standup"
            style = dotted
            "Fixture condor" [shape=rectangle]
        }
        subgraph cluster_action {
            label = "Action"
            style = dotted
            "Fixture submit_jobs" [shape=rectangle]
        }

        "Fixture determine_params" -> {"Fixture condor"};
        "Fixture condor" -> {"Fixture submit_jobs"};
        "Fixture submit_jobs" -> {"Test job_results"};
   }

and would be executed in this order:

.. code-block::

    SETUP    M determine_params
      SETUP    C condor (fixtures used: determine_params)
      SETUP    C submit_jobs (fixtures used: condor)
        test.py::TestJobs::test_job_results (fixtures used: condor, determine_params, submit_jobs)
      TEARDOWN C submit_jobs
      TEARDOWN C condor
    TEARDOWN M determine_params

A more complicated example might use multiple :func:`config` and :func:`action`
fixtures, and those fixtures might feed multiple tests:

.. code-block:: python

    from conftest import config, standup, action


    @config
    def determine_params(): ...

    @config
    def slot_config(): ...

    @standup
    def condor(determine_params, slot_config): ...

    @action
    def submit_jobs(condor): ...

    @action
    def finished_jobs(submit_jobs): ...

    @action
    def analyze_job_queue_log(condor, finished_jobs): ...

    class TestJobs:
        def test_submit_command_succeeded(self, submit_jobs): ...

        def test_job_results(self, finished_jobs): ...

        def test_job_queue_log_results(self, analyze_job_queue_log): ...

Corresponding DAG:

.. graphviz::

   digraph dsl {
        subgraph cluster_config {
            label = "Config"
            style = dotted
            "Fixture determine_params" [shape=rectangle]
            "Fixture slot_config" [shape=rectangle]
        }
        subgraph cluster_standup {
            label = "Standup"
            style = dotted
            "Fixture condor" [shape=rectangle]
        }
        subgraph cluster_action {
            label = "Action"
            style = dotted
            "Fixture submit_jobs" [shape=rectangle]
            "Fixture finished_jobs" [shape=rectangle]
            "Fixture analyze_job_queue_log" [shape=rectangle]
        }

        "Fixture determine_params" -> {"Fixture condor"};
        "Fixture slot_config" -> {"Fixture condor"};
        "Fixture condor" -> {"Fixture submit_jobs" "Fixture analyze_job_queue_log"};
        "Fixture submit_jobs" -> {"Test submit_command_succeeded" "Fixture finished_jobs"};
        "Fixture finished_jobs" -> {"Test job_results"};
        "Fixture analyze_job_queue_log" -> {"Test job_queue_log_results"};
   }

Execution order:

.. code-block::

    htcondor/src/condor_tests/test.py::TestJobs::test_submit_command_succeeded
        SETUP    M determine_params
        SETUP    M slot_config
          SETUP    C condor (fixtures used: determine_params, slot_config)
          SETUP    C submit_jobs (fixtures used: condor)
            test.py::TestJobs::test_submit_command_succeeded (fixtures used: condor, determine_params, slot_config, submit_jobs)
    htcondor/src/condor_tests/test.py::TestJobs::test_job_results
          SETUP    C finished_jobs (fixtures used: submit_jobs)
            test.py::TestJobs::test_job_results (fixtures used: condor, determine_params, finished_jobs, slot_config, submit_jobs)
    htcondor/src/condor_tests/test.py::TestJobs::test_job_queue_log_results
          SETUP    C analyze_job_queue_log (fixtures used: condor, finished_jobs)
            test.py::TestJobs::test_job_queue_log_results (fixtures used: analyze_job_queue_log, condor, determine_params, finished_jobs, slot_config, submit_jobs)
          TEARDOWN C analyze_job_queue_log
          TEARDOWN C finished_jobs
          TEARDOWN C submit_jobs
          TEARDOWN C condor
        TEARDOWN M slot_config
        TEARDOWN M determine_params

Note how tests run as soon as possible,
possibly before actions which they don't depend on.
