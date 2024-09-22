Updated
-------

The command handler must be a generator.  (For implementation simplicity,
the payload isn't set until we're calling the generator the first time;
this wouldn't be hard to change, if we thought it was a good idea.)

Config Magic
------------

    SNAKED          = $(SBIN)/condor_snaked
    SNAKED_LOG      = $(LOG)/SnakeLog
    SNAKE_PATH      = $(LIB)/python3:$(ETC)/snake.d
    DAEMON_LIST     = $(DAEMON_LIST), SNAKED

Further Setup
-------------

Create ``$(ETC)/snake.d/snake/__init__.py`` and fill it with:

    import time
    import classad2
    from pathlib import Path

    def handleCommand(command_int, payload):
        p = Path("/tmp") / "snake.out"
        classAd = payload.get('classad')
        with p.open("a") as f:
            print(
                f"handling command int {command_int}, with classAd payload {classAd}",
                file=f
            )

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

        with p.open("a") as f:
            print(
                f"Sending the following as the reply: {reply}",
                file=f
            )
        yield (1, reply)

        with p.open("a") as f:
            print(
                f"Sending the empty 'done' reply",
                file=f
            )
        yield (0, None)

Demo
----

Run ``condor_master``.  Look at ``SnakeLog``; copy its command socket
sinful.  Run ``condor_status -direct '<sinful>'``; it will report the
Python-generated slot ad.

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

    def handleCommand(int, payload):
        query = classad2.ClassAd()
        yield (None, 0, (query))

        # ... compute the reply ad ...

        yield ((1, replyAd), 0, None)
        yield ((0), 0, None)

We'll have to come up with a convention for specifying when to call
end-of-message(); it might be convenient if it isn't after every yield.
Perhaps the None type _in_ a sequence might mean "call end_of_message()"?

In terms of improving the implementation, we obviously want to reenter the
event loop after every yield.
