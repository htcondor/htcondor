test_curl_plugin.py
===================

This tutorial assumes you've already installed [FIXME: link] pytest and
Python 3, and that you've set up your shell so that you can run pytest in
the ``condor_tests`` directory (meaning, with the ``ornithology`` extensions
active, which requires ``condor_version`` to be in your PATH).

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

Create the following as ``test_curl_plugin.py``.  The filename *must* begin
with a ``test_`` to become part of the test suite.  Likewise, the functions
in that file which start with ``test_`` become individual tests.  Note that
we gave the functions relatively long but descriptively self-explanatory
names, because those are what we'll see in the test reports.

.. code-block:: python

    from ornithology import JobStatus

    def test_job_with_good_url_succeeds(job_with_good_url):
        assert job_with_good_url.state[0] == JobStatus.COMPLETED

    def test_job_with_bad_url_holds(job_with_bad_url):
        assert job_with_bad_url.state[0] == JobStatus.HELD

We do *not* attempt to create the job object in the test; instead, we take the
test jobs as arguments.  The canonical test function is just a (short) series
of assertions about its arguments.  This makes it easy -- and indeed,
automatic -- to distinguish a test failure (the job with the bad URL didn't
go on hold...) from a testing infrastructure failure (... because it never
started running).

PyTest calls each object referenced a test function's arguments a "fixture",
in the sense of a fixed piece of machinery necessary to run the text.  The
``ornithology`` package provides three fixtures: ``@config``, ``@standup``,
and ``@action``.  We'll only need the third one for this tutorial.

Note that we gave the fixtures long but descriptively self-explanatory
names as well, since those names will be used to report errors.

Fixtures!
---------
(Think: "Plastics!")

``@action`` is an annotation; an annotation is syntatic sugar for calling
a function on the subsequent function.  In the following code, ``@action``
basically just marks ``bad_job`` and ``good_job`` as the functions which
produce the ``bad_job`` and ``good_job`` fixtures.  (Which is why we gave
them different names in the test functions.)

We start with an empty argument list and a desire to submit a job and then
wait for it to complete.

(Why do we wait for the jobs to enter a terminal state in these functions?
At one level, because some function has to for the test to work, and we don't
want to wait in the test functions because waiting could fail.  At another
level, it's a judgement call: you could certainly instead write a smaller
``bad_job()`` function that accepted a different fixture, a job which had
only just been submitted, and that would be fine too.

In this case, the judgement was that we didn't expect the abstract operation
of "running the job" to fail often enough to be worth breaking into two
separately-checked pieces.

However, in any case, if these functions checked for the specific state
the test functions expect to see, that would defeat the point of splitting
them up, so we don't do that, either.)

.. code-block:: python

    @action
    def job_with_good_url():
        pass

To ``wait`` [FIXME: link] for a job [handle], we need a job [handle],
which we get by submitting a job to a personal condor.  Luckily, we
don't care all that much about the details of our personal condor, so
we can use the ``default_condor`` fixture provided by Ornithology.

.. code-block:: python

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

What about the ``FIXME`` s?

The job we submit needs to know what URL to download from, but to minimize
the tests' frailty, we want that URL to be a server we started for the
test.  We obviously can't count on port 80 being available, so we'll need
the URL to include the port.  The safest way to do that is to determine the
URL at run-time, after we've started the web server and it has bound to its
listen port.  That sounds like a lot of work, and something else that could
fail, so let's make the URL a fixture.

As an implementation detail, ``job.wait()`` requires the job to produce an
event log, so we'll have to provide one.  By convention, everything the
job produces should go into the corresponding test-specific directory.  As
you might expect by now, Ornithology provides a fixture for that, ``test_dir``.

.. code-block:: python

    def job_in_terminal_state(job):
        return job.state.any_held() or job.state.any_complete()

    @action
    def job_with_good_url(default_condor, good_url):
        job = default_condor.submit(
            {
                # Do nothing of interest.
                "executable": "/bin/sleep",
                "arguments": "1",
                # These are the two lines we really care about.
                "transfer_input_files": good_url,
                "should_transfer_files": "YES",
                # Implementation detail.
                "log": (test_dir / "good_url.log").as_posix(),
            }
        )
        job.wait(condition = job_in_terminal_state)
        return job

In our best tradition of solving the problem later, I replaced the the
FIXME in ``job.wait()`` with a function we haven't written yet.  The
implementation is below, and is something you should have been able to
dig out of the job handle API documentation.  The code block below also
adds the bad job fixture.

.. code-block:: python

    def job_in_terminal_state(job):
        return job.state.any_held() or job.state.any_complete()

    @action
    def job_with_good_url(default_condor, good_url):
        job = default_condor.submit(
            {
                # Do nothing of interest.
                "executable": "/bin/sleep",
                "arguments": "1",
                # These are the two lines we really care about.
                "transfer_input_files": good_url,
                "should_transfer_files": "YES",
                # Implementation detail.
                "log": (test_dir / "good_url.log").as_posix(),
            }
        )
        job.wait(condition = job_in_terminal_state)
        return job

    @action
    def job_with_bad_url(default_condor, bad_url, test_dir):
        job = default_condor.submit(
            {
                "executable": "/bin/sleep",
                "arguments": "1",
                "log": (test_dir / "bad_url.log").as_posix(),
                "transfer_input_files": bad_url,
                "should_transfer_files": "YES"
            }
        )
        job.wait(condition = job_in_terminal_state)
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
say that ``server`` is an instance of PyTest extension designed for exactly
this purpose.  The fixture is implemented in the following, funny, way.

.. code-block:: python

    import pytest_httpserver import HTTPServer

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
which means it's now time to run PyTest and see what happens.

..

    $ pytest ./test_curl_plugin.py
    FIXME

Parameterization
----------------

(PyTest consistently misspells parameterize as parametrize, if you're
looking for more documentation about this.)

As written, the bad URL gets a code 404 reply.  If we wanted to test what
happens how the curl plugin responds to a code 500 reply, we don't have
to change anything about the test except ``bad_job``.  With PyTest, that's
true even if we want to test *both* codes.

Parameterizing ``@actions`` involves an unfortunate amount of syntactic
magic, but here's how you do it:

.. code-block:: python

    @action(params={"404":404, "500":500})
    def bad_url(server, request):
        server.expect_request("/badurl").respond_with_data(status = request.param)
        return f"http://localhost:{server.port}/badurl"

If you're not familiar with the syntax, that's calling ``@action`` with
the named argument ``parameters`` as an inline-constant dictionary
mapping the string (name, in this case) "404" to the integer 404, and the
string "500" to the integer 500.

For each use of the ``bad_job`` fixture, this causes PyTest to run two
subtests: one named "404", and the other named "500".  In the former,
``parameter.value`` is 404, and in the latter, it is 500.  IF you run
PyTest again, you'll see that it now reports three test results, one
for the good URL job, and one for each of the two bad URL jobs:

..

    $ pytest ./test_curl_plugin.py
    FIXME

You could parameterize ``good_job`` in a similar way to verify that
a very small (0 byte) file or a very large file are also handled correctly.

If you instead wanted to verify that the curl plugin worked with static
slots, then PyTest would instead run six tests: the good URL test and the two
bad URL tests in dynamic slots, and those three again in static slots.

The Song and Dance
------------------

PyTest normally doesn't cache fixtures at all (although they call this
"caching at the function level").  However, for testing HTConodr, where
starting up a personal condor is a core task, and therefore a core fixture,
this rapidly becomes a burden, both in terms of time and in terms of writing
a multi-step test where the state of that personal condor matters.

The Ornithology framework solves this by defining all of its custom fixtures
to cache at the class level -- all functions that are members of the same
class share a common pool of fixtures.  This makes the tests both easier
to write and faster.

However, since the PyTest default *is* not to share fixtures between
functions, some extensions -- ``pytest_httpserver`` -- only provide their
default fixtures at the functional level.  (Why PyTest can't automagically
convert, I don't know.)  Basically, the ``with``/``yield`` construct holds
a reference on the fixture even after the fixture function exits.  (FIXME:
Explain why that doesn't make it a globl.)
