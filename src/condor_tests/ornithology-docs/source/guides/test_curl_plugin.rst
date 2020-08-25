.. py:currentmodule:: ornithology

test_curl_plugin.py
===================

This tutorial assumes a Python 3 environment; the easiest way to arrange
for one if it's not your system default is with `Miniconda`_.  You'll need
to install `pytest`_ (``pip install -user pytest`` if you're using the
system Python; ``pip install pytest`` otherwise).  Your shell will
also need to have its ``CONDOR_CONFIG``, ``PATH``, and ``PYTHONPATH``
environment variables pointing at a working HTCondor installation.  Finally,
the code in this tutorial requires Ornithology; either run the example code
from the ``src/condor_tests`` directory, or add ``src/condor_tests`` to your
``PYTHONPATH``.

.. _miniconda: https://docs.conda.io/en/latest/miniconda.html
.. _pytest: https://docs.pytest.org/en/stable/

Description
-----------

In this tutorial, we're going to work through how to write a real, but basic,
test from scratch.  We're going to work backwards from the assertions that
constitute the test.  Those assertions require certain data to be available;
at each step of our backwards iteration, which will define how to generate
that data, possibly in terms of other data, until the last step doesn't need
any input data, or the framework provides it all for us.

In this example, we're trying to test the curl plugin.  At the simplest level,
we want to make sure that (a) a job transferring input files via a URL actually
gets them, and (b) that when a transfer from a URL fails, that the plugin
notices and correctly puts the job on hold.

In this first pass, we'll simplify things by assuming that if job A completes,
then the URL was downloaded to the right place with the right contents.  We
can elaborate on these conditions to avoid false positives later.

The Assertions
--------------

Our first task is to write down what it means for the test to pass.
We do this by using Python's ``assert`` statement to check whether a given
value is ``True``. Our job is to figure out what we want to assert on, and how
to produce that value in the first place.

Create the following as ``test_curl_plugin.py``.  The filename *must* begin
with a ``test_`` to become part of the test suite.  Likewise, the functions
in that file which start with ``test_`` become individual tests.  Note that
we gave the functions relatively long but descriptively self-explanatory
names, because those are what we'll see in the test reports.  For now,
ignore the fact that the test functions are part of a class; we'll explain
why later.  (The leading ``self`` in a member function's parameters is
mandatory but may safely be ignored.)  Like the filename and the function name,
the class name *must* begin with ``Test`` (but not the underscore).

.. code-block:: python

    from ornithology import JobStatus

    class TestCurlPlugin:
        def test_job_with_good_url_succeeds(self, job_with_good_url):
            assert job_with_good_url.state[0] == JobStatus.COMPLETED

        def test_job_with_bad_url_holds(self, job_with_bad_url):
            assert job_with_bad_url.state[0] == JobStatus.HELD

We do *not* attempt to create the job object in the test; instead, we take the
test jobs as arguments.  The canonical test function is just a (short) series
of assertions about its arguments.  This makes it easy -- and indeed,
automatic -- to distinguish a test failure (the job with the bad URL didn't
go on hold...) from a testing infrastructure failure (... because it never
started running).

``pytest`` calls each object referenced a test function's arguments a "fixture",
in the sense of a fixed piece of machinery necessary to run the text.
Ornithology provides three special ways of defining fixtures:
``@config``, ``@standup``, and ``@action``
(:func:`config`, :func:`standup`, and :func:`action`).
We'll only need the third one for this tutorial.

Note that we gave the fixtures long but descriptively self-explanatory
names as well, since those names will be used to report errors.

Fixtures!
---------
(Think: "Plastics!")

``@action`` is an annotation; an annotation is syntactic sugar for calling
a function on the subsequent function.  In the following code, ``@action``
basically just marks ``job_with_bad_url`` and ``job_with_good_url`` as the
functions which produce the ``job_with_bad_url`` and ``job_with_good_url``
fixtures.  (Which is why we gave them different names in the test functions.)

We start with an empty argument list and a desire to submit a job and then
wait for it to complete.

.. code-block:: python

    from ornithology import action

    @action
    def job_with_good_url():
        pass

We need to submit this job and wait for it to reach a "terminal" state:
either completed, held, or removed.
The easiest way to wait for a job to terminate is to use a :class:`ClusterHandle`.
These are what we get back when submitting jobs via Ornithology.
Once we have a handle, we can use its :func:`ClusterHandle.wait` method to do
the actual waiting.
Luckily, we don't care all that much about the details of our personal condor,
so we can use the ``default_condor`` fixture provided by Ornithology.

.. code-block:: python

    from ornithology import action

    @action
    def job_with_good_url(default_condor):
        job = default_condor.submit(
            {
                # Do nothing of interest.
                "executable": "/bin/sleep",
                "arguments": "1",
                # These are the two lines we really care about.
                "transfer_input_files": "FIXME",
                "should_transfer_files": "YES",
            }
        )

        job.wait(condition = FIXME)

It is considered good Python form to leave the trailing comma in so that
the individual lines may be freely reordered.

.. note::

    Why do we wait for the jobs to enter a terminal state in this fixture?

    At one level, we have to wait at some point for the test to work, and we don't
    want to wait in the test functions because waiting could fail.  At another
    level, it's a judgement call: you could certainly instead write a smaller
    ``job_with_bad_url()`` function that accepted a different fixture, a job
    which had only just been submitted, and that would be fine too.

    In this case, the judgement was that we didn't expect the abstract operation
    of "running the job" to fail often enough to be worth breaking into two
    separately-checked pieces.

    However, in any case, if these functions checked for the specific state
    the test functions expect to see, that would defeat the point of splitting
    them up, so we don't do that, either.)


What about the ``FIXME``\s?

The job we submit needs to know what URL to download from, but to minimize
the tests' frailty and to isolate it from the outside world,
we want that URL to be served by a server we started for the
test.  We obviously can't count on port 80 being available, so we'll need
the URL to include the port.  The safest way to do that is to determine the
URL at run-time, after we've started the web server and it has bound to its
listen port.  That sounds like a lot of work, and something else that could
fail, so let's make the URL a fixture.

Now we'll get the waiting working.
As an implementation detail, :func:`ClusterHandle.wait` requires the job to
produce an event log, so we'll have to provide one.  By convention, everything
the job produces should go into the corresponding test-specific directory.  As
you might expect by now, Ornithology provides a fixture for that,
:func:`~conftest.test_dir`.

.. code-block:: python

    from ornithology import action, ClusterState

    @action
    def job_with_good_url(default_condor, good_url, test_dir):
        job = default_condor.submit(
            {
                # Do nothing of interest.
                "executable": "/bin/sleep",
                "arguments": "1s",
                # These are the two lines we really care about.
                "transfer_input_files": good_url,
                "should_transfer_files": "YES",
                # Implementation detail.
                "log": (test_dir / "good_url.log").as_posix(),
            }
        )

        job.wait(condition = FIXME)

        return job

The actual waiting ``condition`` will be a method on the :class:`ClusterState`
that is attached to the :class:`ClusterHandle`. Because functions are
first-class objects in Python, we can simply pass a reference to the
appropriate method to :meth:`ClusterHandle.wait`. In this case we will wait
for the job to either complete or get held, which are both "terminal" states.
The code block below also adds the ``job_with_bad_url`` fixture.

.. code-block:: python

    from ornithology import action, ClusterState

    @action
    def job_with_good_url(default_condor, good_url, test_dir):
        job = default_condor.submit(
            {
                "executable": "/bin/sleep",
                "arguments": "1s",
                "transfer_input_files": good_url,
                "should_transfer_files": "YES",
                "log": (test_dir / "good_url.log").as_posix(),
            }
        )

        job.wait(condition=ClusterState.all_terminal)

        return job

    @action
    def job_with_bad_url(default_condor, bad_url, test_dir):
        job = default_condor.submit(
            {
                "executable": "/bin/sleep",
                "arguments": "1s",
                "transfer_input_files": bad_url,
                "should_transfer_files": "YES",
                "log": (test_dir / "bad_url.log").as_posix(),
            }
        )

        job.wait(condition=ClusterState.all_terminal)

        return job

OK!  Now we just need the good and bad URL fixtures.  Again, we could split
this fixture in two pieces, but it's already short and simple, so we won't
bother.

.. code-block:: python

    @action
    def good_url(server):
        server.expect_request("/goodurl").respond_with_data("Great success!")
        return f"http://localhost:{server.port}/goodurl"

    @action
    def bad_url(server):
        server.expect_request("/badurl").respond_with_data(status = 404)
        return f"http://localhost:{server.port}/badurl"


We're getting a little test-specific and a little exotic here, so I'll just
say that ``server`` is provided by a ``pytest`` extension designed for exactly
this purpose.  The fixture is implemented in the following, funny, way.

.. code-block:: python

    from pytest_httpserver import HTTPServer

    @action
    def server():
        with HTTPServer() as httpserver:
            yield httpserver

This song-and-dance works around a detail in how ``@action`` is implemented
that we'll talk about further below.

Testing the Test
----------------

We've now iterated backwards from the asserts, writing functions for the
missing arguments until we've reached a function which takes no arguments,
which means it's now time to run ``pytest`` and see what happens.

.. code-block:: console

    $ pytest ./test_curl_plugin.py
    ============================= test session starts ==============================
    platform linux -- Python 3.8.2, pytest-5.4.2, py-1.8.1, pluggy-0.13.1 -- /home/tlmiller/miniconda3/bin/python
    cachedir: .pytest_cache
    rootdir: /home/tlmiller/condor/source/src/condor_tests, inifile: pytest.ini
    plugins: cov-2.8.1, dependency-0.5.1, httpserver-0.3.4, mock-3.1.0, flask-1.0.0

    Base per-test directory: /tmp/condor-tests-1591061678-16424
    Python bindings version:
    $CondorVersion: 8.9.7 May 20 2020 BuildID: UW_Python_Wheel_Build $
    HTCondor version:
    $CondorVersion: 8.9.8 Jun 01 2020 PRE-RELEASE-UWCS $
    $CondorPlatform: x86_64-Devuan-2 $

    collected 2 items

    example01.py::TestCurlPlugin::test_job_with_good_url_succeeds PASSED     [ 50%]
    example01.py::TestCurlPlugin::test_job_with_bad_url_holds PASSED         [100%]

    ============================== 2 passed in 19.99s ==============================


Parametrization
---------------

.. warning::

    ``pytest`` uses the British spelling **parametrize** instead of
    **parameterize**.  Be aware if you're looking for more documentation!

As written, the bad URL gets a code 404 reply.  If we wanted to test what
happens how the curl plugin responds to a code 500 reply, we don't have
to change anything about the test except ``job_with_bad_url``.  With
``pytest``, that's true even if we want to test *both* codes.

Parametrizing ``@actions`` involves an unfortunate amount of syntactic
magic, but here's how you do it:

.. code-block:: python

    @action(params={"404":404, "500":500})
    def bad_url(server, request):
        server.expect_request("/badurl").respond_with_data(status = request.param)
        return f"http://localhost:{server.port}/badurl"

If you're not familiar with the syntax, that's calling ``@action`` with the
named argument ``params`` as an inline-constant dictionary mapping the string
"404" to the integer 404, and the string "500" to the integer 500.  The keys
are used by ``pytest`` to generate the test's "id" when reporting results;
the values will be injected into the test as described below.

For each use of the ``job_with_bad_url`` fixture, ``pytest`` will generate
two subtests: one named "404", and the other named "500".  In the former,
``request.param`` is ``404``, and in the latter, it is ``500``.  IF you run
``pytest`` again, you'll see that it now reports three test results, one
for the good URL job, and one for each of the two bad URL jobs:

.. code-block:: console

    $ pytest ./test_curl_plugin.py
    ============================= test session starts ==============================
    platform linux -- Python 3.8.2, pytest-5.4.2, py-1.8.1, pluggy-0.13.1 -- /home/tlmiller/miniconda3/bin/python
    cachedir: .pytest_cache
    rootdir: /home/tlmiller/condor/source/src/condor_tests, inifile: pytest.ini
    plugins: cov-2.8.1, dependency-0.5.1, httpserver-0.3.4, mock-3.1.0, flask-1.0.0

    Base per-test directory: /tmp/condor-tests-1591061845-16808
    Python bindings version:
    $CondorVersion: 8.9.7 May 20 2020 BuildID: UW_Python_Wheel_Build $
    HTCondor version:
    $CondorVersion: 8.9.8 Jun 01 2020 PRE-RELEASE-UWCS $
    $CondorPlatform: x86_64-Devuan-2 $

    collected 3 items

    example02.py::TestCurlPlugin::test_job_with_good_url_succeeds PASSED     [ 33%]
    example02.py::TestCurlPlugin::test_job_with_bad_url_holds[404] PASSED    [ 66%]
    example02.py::TestCurlPlugin::test_job_with_bad_url_holds[500] PASSED    [100%]

    ============================== 3 passed in 29.46s ==============================

You could parameterize ``job_with_good_url`` in a similar way to verify that
a very small (0 byte) file or a very large file are also handled correctly.

If you instead wanted to verify that the curl plugin worked with both static
and dynamic slots, then ``pytest`` would instead run six tests: the good URL
test and the two bad URL tests in dynamic slots, and those three again in
static slots.

The Song-and-Dance
------------------

``pytest`` normally doesn't cache fixtures at all (although they call this
"caching at the function level").  However, for testing HTCondor, where
starting up a personal condor is a core task, and therefore a core fixture,
this rapidly becomes a burden, both in terms of time and in terms of writing
a multi-step test where the state of that personal condor matters.

The Ornithology framework solves this by defining all of its custom fixtures
to cache at the class level -- all functions that are members of the same
class share a common pool of fixtures.  This makes the tests both easier
to write and faster, and it's why the tutorial starts off with the functions
in a class.

However, since the ``pytest`` default *is* not to share fixtures between
functions, some extensions -- including ``pytest_httpserver`` -- only provide
their default fixtures at the functional level.  (Why ``pytest`` can't
automagically convert, I don't know.) This is why we needed to write an
adapter around it.

Implementation details of our workaround: the ``yield <value>`` construct
causes the value to be "returned", but instead of the function returning,
its execution is temporarily suspended. When the fixture goes out of scope,
``pytest`` resumes the execution of the function. The ``with`` construct is a
"context manager" which arranges for the cleanup of the ``server`` when the
``with`` block ends. This is all implemented via `generators`_.

.. _generators: https://wiki.python.org/moin/Generators

Complete Test
-------------

This version is slightly different than what's in the source tree
(it doesn't check the contents of the downloaded file)
so here's a copy of the whole thing in one go, as formatted by the
``black`` package (``pip install [--user] black``).

.. code-block:: python

    from ornithology import action, JobStatus, ClusterState
    from pytest_httpserver import HTTPServer


    @action
    def server():
        with HTTPServer() as httpserver:
            yield httpserver


    @action
    def good_url(server):
        server.expect_request("/goodurl").respond_with_data("Great success!")
        return f"http://localhost:{server.port}/goodurl"


    @action(params={"404": 404, "500": 500})
    def bad_url(server, request):
        server.expect_request("/badurl").respond_with_data(status=request.param)
        return f"http://localhost:{server.port}/badurl"


    @action
    def job_with_good_url(default_condor, good_url, test_dir):
        job = default_condor.submit(
            {
                "executable": "/bin/sleep",
                "arguments": "1",
                "transfer_input_files": good_url,
                "should_transfer_files": "YES",
                "log": (test_dir / "good_url.log").as_posix(),
            }
        )

        job.wait(condition=ClusterState.all_terminal)

        return job


    @action
    def job_with_bad_url(default_condor, bad_url, test_dir):
        job = default_condor.submit(
            {
                "executable": "/bin/sleep",
                "arguments": "1",
                "transfer_input_files": bad_url,
                "should_transfer_files": "YES",
                "log": (test_dir / "bad_url.log").as_posix(),
            }
        )

        job.wait(condition=ClusterState.all_terminal)

        return job


    class TestCurlPlugin:
        def test_job_with_good_url_succeeds(self, job_with_good_url):
            assert job_with_good_url.state[0] == JobStatus.COMPLETED

        def test_job_with_bad_url_holds(self, job_with_bad_url):
            assert job_with_bad_url.state[0] == JobStatus.HELD
