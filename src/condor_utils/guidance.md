Guidance
--------

From time to time, we want "the AP" to decide what "the EP" should do.  In
practice, this always means the starter asking the shadow what to do, and then
doing it.  We call both the process of obtaining this advice and the advice
itself guidance.

The starter initiates all guidance requests.  This is partly for historical
design reasons (the starter initiates everything else) and partly because
we usually want to obtain guidance because some particular event happened,
and those events (have so far) all happen(ed) on the EP.

Presently, the shadow does **not** consult with the rest of the AP when
giving guidance, largely because Leslie Lamport hates you (and me)
personally (and individually).  The other option is to interrupt the schedd,
which is hardly an option at all.  (There's a dormant implementation of an
election/broadcast algorithm -- the SingleProviderSyndicate -- in the shadow
that was previously used to coordinate between shadows, but that doesn't help
in gathering global data for decision-making.)

Wire Protocol
=============

The wire protocol is implemented in ``condor_starter.V6.1/NTsenders.cpp`` and
``condor_shadow.V6.1/NTreceivers.cpp``, as you'd expect.  It is blocking --
more on that later -- and consists of sending and receiving a single ClassAd
in either direction ("the quey" to the shadow, "the guidance" to the
starter).  The implementation (``REMOTE_CONDOR_request_guidance()``)
is uninteresting.

Starter Implementation
======================

The starter generally invokes the raw guidance pseudo-operation via a set of
small special-purpose functions, all of which channel through
``JICShadow::genericRequestGuidance()``; guidance requests for other
``JobInfoCommunicators`` fail unconditionally.  The special-purpose functions
are implemented in ``condor_starter.V6.1/starter_guidance.cpp``.  Their main
purpose is to reduce code duplication, both in the construction of the specific
request and in error handling.  Success is defined as "we received guidance
we understood"; failure may occur due to communications problems, version
mis-matches, or other bugs.  If the guidance request succeeds (we got guidance
from the shadow), the special-purpose functions call the corresponding handler,
providing it a lambda to use in order to continue the conversation (that is,
invoke ``REMOTE_CONDOR_request_guidance()`` again).

There are five of these functions, which all have very similar implementations:

  * ``requestGuidanceSetupJobEnvironment()``
  * ``requestGuidanceJobEnvironmentReady()``
  * ``requestGuidanceJobEnvironmentUnready()``
  * ``requestGuidanceCommandJobSetup()``
  * ``requestGuidanceCheckpointTaken()``

The definitions of "setup job environment" and "job setup" are as in other
parts of the starter code.

The special-purpose functions and the corresponding ``handle*()`` functions
are all blocking and (implicitly) define the default "carry on" behavior.

Shadow Implementation
=====================

The shadow side does not block between calls to
``REMOTE_CONDOR_request_guidance()``, even in the middle of a conversation.
This is partly for shadow responsiveness reasons, but mostly so that the shadow
is free to act if what its guidance requires the starter interact with the
shadow.  To implement the shadow's side of each conversation (one per type of
guidance request), I used coroutines to allow the guidance-giving functions to
return to the event loop without losing state.

The guidance functions are all implemented in
``condor_shadow.V6.1/pseudo_ops.cpp``, but should really be moved into their
own file.

Types of Guidance
=================

To make sure the shadow and starter don't drift out of synch, there's a header
in ``condor_utils``, ``guidance.h``.  If defines constants used in the request
and guidance ads, as well as some utility functions (presently, only those used
by the common-files facility).  The header defines a number of guidance
commands:

* COMMAND_RUN_DIAGNOSTIC
  - Run a "diagnostic".
* COMMAND_RETRY_TRANSFER
  - If a transfer failed, try it again.
* COMMAND_ABORT
  - Kill yourself.
* COMMAND_START_JOB
  - Start the job.
* COMMAND_CARRY_ON
  - Do whatever you would have done if you hadn't asked me for guidance.
* COMMAND_RETRY_REQUEST
  - Ask me again in ``ATTR_RETRY_DELAY`` seconds.
* COMMAND_STAGE_COMMON_FILES
  - Transfer common files to the data slot.
* COMMAND_MAP_COMMON_FILES
  - Make common files already staged to the data slot available to a job slot.
* COMMAND_JOB_SETUP
  - Do "job setup", as defined by the starter.
* COMMAND_COLOR_SLOT
  - Tell the startd to merge ``ATTR_COLOR_AD`` into your slot ad.
* COMMAND_DONT_RESTART
  - Don't restart the checkpointed job (evict).
* COMMAND_RESTART
  - Restart the checkpointed job (don't evict).

Tests
=====

* ``test_starter_guidance.cpp``
* ``test_guidance_commands.py``
