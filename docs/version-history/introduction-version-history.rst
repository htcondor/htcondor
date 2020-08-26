Introduction to HTCondor Versions
=================================

This chapter provides descriptions of what features have been added or
bugs fixed for each version of HTCondor. The first section describes the
HTCondor version numbering scheme, what the numbers mean, and what the
different release series are. The rest of the sections each describe a
specific release series, and all the HTCondor versions found in that
series.

HTCondor Version Number Scheme
------------------------------

HTCondor's versioning scheme reflects the fact that it
is both a production system and a research project. The numbering scheme
was primarily taken from the Linux kernel's version numbering, so if you
are familiar with that, it should seem quite natural.

There will usually be two HTCondor versions available at any given time,
the stable version, and the development version. All versions of HTCondor 
now have exactly three numbers, separated by "."

-  The first number represents the major version number, and will change
   very infrequently.
-  The thing that determines whether a version of HTCondor is stable or
   development is the second digit. Even numbers represent stable
   versions, while odd numbers represent development versions.
-  The final digit represents the minor version number, which defines a
   particular version in a given release series.

The Stable Release Series
-------------------------

People expecting the stable, production HTCondor system should download
the stable version, denoted with an even number in the second digit of
the version string. 

On the stable series, new minor version releases will only be made for
bug fixes and to support new platforms. No new features will be added to
the stable series. People are encouraged to install new stable versions
of HTCondor when they appear, since they probably fix bugs you care
about. Hopefully, there will not be many minor version releases for any
given stable series.

All minor release version within a stable series are wire-compatible
with each other, so if an administrator needs to upgrade the central
manager one day, the pool will run with an older scheduler or execute
nodes.

The Development Release Series
------------------------------

Users interested in the latest research, new features and improved
performance should download the development version,
denoted with an odd number in the second digit of the version string.

On the development series, new minor version releases happen
at a relatively fixed cadence. Users running the development series are generally
encouraged to upgrade when minor release comes out.

After the feature set of the development series is satisfactory to the
HTCondor Team, we will put a code freeze in place, and from that point
forward, only bug fixes will be made to that development series. When we
have fully tested this version, we will release a new stable series,
resetting the minor version number, and start work on a new development
release from there.


