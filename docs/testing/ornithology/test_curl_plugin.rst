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

Fixtures
--------
(Think: "Plastics!")

``@action`` is an annotation; an annotation is syntatic sugar for calling
a function on the subsequent function.  In the following code, ``@action``
basically just marks ``bad_job`` and ``good_job`` as the functions which
produce the ``bad_job`` and ``good_job`` fixtures.  (Which is why we gave
them different names in the test functions.)

We start with an empty argument list and a desire to submit a job and then
wait for it to complete.  [FIXME....]

.. code-block:: python

    @action
    def job_with_good_url():
        job = default_condor.submit(
            {
                "executable": "/bin/sleep",
                "arguments": "1",
                "log": (test_dir / "good_url.log").as_posix(),
                "transfer_input_files": good_url,
                "should_transfer_files": "YES"
            }
        )


    @action
    def job_with_bad_url():
        job = default_condor.submit(
            {
                "executable": "/bin/sleep",
                "arguments": "1",
                "log": (test_dir / "bad_url.log").as_posix(),
                "transfer_input_files": bad_url,
                "should_transfer_files": "YES"
            }
        )

Why do we wait for the jobs to enter a terminal state in these functions?
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
them up, so we don't do that, either.

We also don't create the personal condor that the job needs; instead we used
the default one supplied by the ``ornithology`` package.  If at some point we
wanted to check to see if the curl plugin worked just as well in static slots,
we could ask for our own custom fixture, and parameterize that by which kind
of slots it had.  We'll get to that later.

The last thing each test job needs is a server to try to download from.
Conveniently, PyTest provides a fixture for just this purpose, named
'httpserver'.  Inconviently, and for stupid reasons, we need to do a little
song and dance to try and use it in an ``@action``; hopefully, the song
and the dance will be built into a later version of Ornithology.

.. code-block:: python

    FIXME

We've now iterated backwards from the asserts, writing functions for the
missing arguments until we've reached a function which takes only arguments
provided by the framework, which means it's now time to run PyTest and
see what happens.

..
    $ pytest ./test_curl_plugin.py
    ...

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

    @action(parameters={ "404": 404, "500": 500 })
    def bad_job(..., parameter)
        ... parameter.value

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
    ...

You could parameterize ``good_job`` in a similar way to verify that
a very small (0 byte) file or a very large file are also handled correctly.

If you instead wanted to verify that the curl plugin worked with static
slots, then PyTest would instead run six tests: the good URL test and the two
bad URL tests in dynamic slots, and those three again in static slots.

Test Classes
------------

[FIXME: verify]  Even at three tests, this test runs a little slowly.  The
reason is that, to make sure that one test doesn't stomp another test's
fixture, PyTest creates a new set of fixtures for each test function.  So
you're starting three personal condors each time you run this test.  That's
not necessary for this test.  The way to tell PyTest to cache fixtures
between tests is simple, but bizarre: you put them together inside the same
class[FIXME?, which must also start with ``Test`` (no underscore).

(For those of you who have used PyTest before, this answer may come as a bit
of a surprise, but the ``@action`` annotation sets its scope to the class,
which is not the default for PyTest ``@pytest.fixture``.)

When you do this, the tests should run faster, because they will share a
single personal condor.
