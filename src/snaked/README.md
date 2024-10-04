Updated
-------

The command handler must be a generator.

Config Magic
------------

    SNAKED                      = $(SBIN)/condor_snaked

    CREDMON_OAUTH               = $(SNAKED)
    CREDMON_OAUTH_ARGS          = -genus CREDMON_OAUTH
    CREDMON_OAUTH_ADDRESS_FILE  = $(LOG)/.credmon_ouauth_address
    CREDMON_OAUTH_LOG           = $(LOG)/CredmonOauthLog
    CREDMON_OAUTH_SNAKE_PATH    = $(LIB)/python3:$(ETC)/snake.d/CREDMON_OAUTH

    CREDMON_OAUTH_DEBUG         = D_CATEGORY D_SUBSECOND
    # One space, Vasili, and one space only.
    CREDMON_OAUTH_SNAKE_COMMAND_TABLE @=end
    QUERY_STARTD_ADS READ handleCommand
    QUERY_SCHEDD_ADS READ handleScheddCommand
    @end


    CREDMON_VAULT               = $(SNAKED)
    CREDMON_VAULT_ARGS          = -genus CREDMON_VAULT
    CREDMON_VAULT_ADDRESS_FILE  = $(LOG).credmon_vault_address
    CREDMON_VAULT_SNAKE_PATH    = $(LIB)/python3:$(ETC)/snake.d/CREDMON_VAULT

    CREDMON_VAULT_DEBUG         = D_UTC
    # One space, Vasili, and one space only.
    CREDMON_VAULT_SNAKE_COMMAND_TABLE @=end
    QUERY_STARTD_ADS READ handleCommand
    QUERY_SCHEDD_ADS READ handleScheddCommand
    @end


Further Setup
-------------

Create ``$(ETC)/snake.d/snake/__init__.py`` and fill it with:

    import time
    import classad2

    def handleCommand(command_int : int):
        # Specify and obtain the payload.
        classad_format = classad2.ClassAd()
        query = yield (None, None, (classad_format, end_of_message))
        classAd = query[0]


        reply = classad2.ClassAd()

        reply['MyType'] = "Machine"
        reply['Arch'] = "TurboFake"
        reply['OpSys'] = "LINUX"
        reply['Arch'] = "X86_64"
        reply['Machine'] = "snake-0000"
        reply['Name'] = "slot1_1@snake-0000"
        reply['Memory'] = 1000

        then = int(time.time()) - 20
        reply['EnteredCurrentActivity'] = then
        reply['LastHeadFrom'] = then
        reply['Activity'] = "Idle"
        reply['State'] = "Unclaimed"
        reply['CondorLoadAvg'] = 0.0

        yield ((1, reply), 0, None)


        reply['Name'] = "slot1_2@snake-0000"
        reply['Memory'] = 2000
        yield ((1, reply), 0, None)


        # raise ValueError("yuck!")

        yield ((0, end_of_message), 0, None)


    def handleScheddCommand(command_int : int):
        # Specify and obtain the payload.
        classad_format = classad2.ClassAd()
        query = yield (None, None, (classad_format, end_of_message))
        classAd = query[0]


        reply = classad2.ClassAd()

        reply['Name'] = "azaphrael.org"
        reply['Machine'] = "azaphrael.org"
        reply['TotalIdleJobs'] = 1200
        reply['TotalHeldJobs'] = 1000
        reply['TotalRunningJobs'] = 800
        reply['TotalJobAds'] = 2000

        yield ((1, reply), 0, None)
        yield ((0, end_of_message), 0, None)


Demo
----

Run ``condor_master``.  Look at ``SnakeLog``; copy its command socket
sinful.  Run ``condor_status -direct '<sinful>'``; it will report the
Python-generated slot ad.

Modify ``handleCommand()``.  Run ``condor_reconfig`` and the
``condor_status -direct '<sinful>'``; be amazed.

Thoughts
--------

There's only one command handler right now, with the thinking that the
dispatch would be easier to arrange in Python than in C++.

At present, the command handler is registered as QUERY_STARTD_ADS/READ.
My inclination, if we generalize, is to have have command registration
specified in the snaked's configuration, rather than write an extension
so that registration can be done from Python at module import time.

(I guess doing the dispatch in C++ could be a good excuse to get std::function
command handlers working in DaemonCore, so that we could pass a lambda that
calls the generic Python-function-calling handler with the appropriate
Python function.)

The extension for responses (outbound from the snaked) is that the generator
yield ((response-sequence), timeout, (reply-sequence)) tuples; the sequences
allow a specific subset of Python objects (e.g., ints, strings, and
classad2.ClassAds) and do the right CEDAR thing.  The response is sent
immediately, and then we wait up to timeout seconds for the reply-sequence
to arrive (which triggers the next invocation of the generator).

That leaves the question of specifying the initial payload.  Actually, since
we want None to be a valid sequence (meaning don't send or receive anything),
we could just require that handlers expecting a payload do the following:

    def handleCommand(int):
        query = classad2.ClassAd()
        payload = yield (None, 0, (query, end_of_message_flag))

        # ... compute the first reply ad ...

        yield ((1, replyAd), 0, None)

        # ... determine that there aren't any more reply ads ...

        yield ((0, end_of_message_flag), 0, None)

The implementation doesn't handle the case where the reconfig command
happens while a generator is still live; we need to delete the sock,
cancel the callbacks. and delete the callbacks' context pointers; I don't
know off the top of my head if we can make either of those clean up the
coroutine.  (Maybe have to add a special clean-up function somewhere.)

An additional thought on the interface: instead of using actual Python
objects, as convenient as they are on the C side of the API, it would be
more Pythonic to just pass their type objects:

    payload = yield (None, 0, (classad2.ClassAd, end_of_message_flag))

