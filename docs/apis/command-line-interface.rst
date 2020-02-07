The Command Line Interface
==========================

:index:`Command line<single: Command line; API>`

While the usual HTCondor command line tools are often not thought of as
an API, they are frequently the best choice for a programmatic interface
to the system. They are the most complete, tested and debugged way to
work with the system. The major down side to running the tools is that
spawning an executable may be relatively slow; many applications do not
need an extreme level of performance, making use of the command line
tools acceptable. Even some of the HTCondor tools themselves work this
way. For example, when *condor_dagman* needs to submit a job, it
invokes the *condor_submit* program, just as an interactive user would.


