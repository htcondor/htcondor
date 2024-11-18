Introduction to HTCondor Versions
=================================

This chapter provides descriptions of what features have been added or
bugs fixed for each release of HTCondor. The first section describes the
HTCondor version numbering scheme, what the numbers mean, and what the
different releases are. The rest of the sections each describe the
specific releases.

HTCondor Version Number Scheme
------------------------------

We changed the version numbering scheme after the 9.1.3 release:
what would have been the next 9.1.x release is now the 9.2.0 release.
We made this change to give us additional flexibility in releasing
small updates to address specific issues without disturbing the normal
development of HTCondor.  The version number will still retain the
MAJOR.MINOR.PATCH form with slightly different meanings.  We have borrowed
ideas from `Semantic Versioning <https://semver.org/>`_.

-   The MAJOR number increments for each new Long Term Support (LTS) release.
    A new LTS release may have backward-incompatible changes and may require
    updates to configuration files.  If the current LTS release is 23.0.6,
    the next one will be 24.0.0.  A new LTS release is expected about every
    twelve months in August. The LTS major version number matches the year
    of initial release.

-   The MINOR number increments each feature release.
    This number stays at 0 for LTS releases. If the current feature release
    is 23.2.3, the next one will be 23.3.0.
    A new feature release is expected every month.

-   The PATCH number increments when we have targeted fixes.  For the LTS
    releases, a patch release is expected every
    month and may occur more frequently if a serious problem is
    discovered. For new feature releases, the number of patch releases
    depends on number of problems that are resolved during testing.

Types of Releases
^^^^^^^^^^^^^^^^^

-   An **LTS** release is numbered **X.0.0**, and is a new long-term support
    release.  The previous LTS
    release is supported for six months after a new LTS version is released.
    The final feature release undergoes a stabilization effort where the
    software is run through multiple code quality tools (such as Valgrind)
    to assure the best possible LTS release.  The MAJOR.0.0 version is not
    released until the stabilization effort is complete.

-   An **LTS patch** release is numbered **X.0.Z**, and is an update to the LTS
    major release.  The patches
    are reviewed to ensure correctness and compatibility with the LTS release.
    These releases contain bug fixes and security updates and are released when
    a major issue is identified, or just before the next feature release.
    These releases go through our entire testing process.  Large code
    changes are not permitted in the LTS release.  Enhancements are not
    implemented in the LTS release unless there is minimal impact with a major
    benefit.  Ports to new platforms will appear in the LTS release.  The
    HTCondor team guarantees that patches to the LTS release are compatible.
    Paid support contracts are only available for the LTS releases.

-   A **feature** release is numbered **X.Y.Z** and includes one or more new
    features.
    The software goes through our entire testing process.
    We use these releases in production at the Center for High Throughput
    Computing (CHTC).  These releases contain all the patches from the LTS release.
    The HTCondor
    development team guarantees protocol compatibility between the feature
    releases and the LTS release.  However, changes in behavior may be
    observed, and adjustments to configuration may be required when new
    features are implemented.

-   By default, a feature release in the same series is compatible with
    the previous release. That is, release **X.Y+1.x** will be compatible with
    release **X.Y.x**, such the nodes in a cluster may be upgraded without
    losing state information. There may be a pause in operation during upgrade.
    **Check the release notes** for exceptions prior to updating nodes.

.. mermaid::

   %%{init: { 'gitGraph': {'showCommitLabel': false, 'mainBranchName': 'lts-23'}} }%%
   gitGraph TB:
    commit tag:"23.0.0"
    branch "feature 23.x"
    checkout "lts-23"
    commit tag:"23.0.1"
    checkout "feature 23.x"
    merge "lts-23"
    commit tag:"23.1.0"
    commit tag:"23.1.1"
    commit tag:"23.1.2"
    checkout "lts-23"
    commit tag:"23.0.2"
    checkout "feature 23.x"
    merge "lts-23"
    commit tag:"23.2.0"
    commit tag:"23.2.1"
    branch "lts-24"
    commit tag:"24.0.0"
    branch "feature 24.x"
    checkout "lts-24"
    commit tag:"24.0.1"
    checkout "feature 24.x"
    merge "lts-24"
    commit tag:"24.1.0"
    commit tag:"24.1.1"
    commit tag:"24.1.2"

Support Life Cycle
^^^^^^^^^^^^^^^^^^

We plan to release a new LTS version every August. The support life cycles
are directly related to the release dates.

.. list-table:: HTCondor Support Life Cycle
   :widths: 10 30 30 30
   :header-rows: 1

   * - Version
     - Release
     - End of Regular Support
     - End of Security Support
   * - 23.x
     - September 29, 2023
     - August 2025
     - August 2025
   * - 23.0
     - September 29, 2023
     - August 2025
     - August 2025
   * - 10.x
     - November 10, 2022
     - September 29, 2023
     - September 29, 2023
   * - 10.0
     - November 10, 2022
     - September 29, 2023
     - October 31, 2024

Repositories
^^^^^^^^^^^^

These LTS and feature versions are each served out of four repositories.

-   The ``daily`` repository contains a snapshot of our most recent builds.
    These builds pass all of our unit tests. However, they have not gone through
    any integration testing.
-   The ``rc`` repository contains builds that we are testing in CHTC.
    These builds may be unstable as we find issues with new release candidates.
-   The ``update`` repository contains builds that have passed integration tests
    on our production CHTC pool and are made available for testing on the wider
    Open Science Pool (OSPool).
-   The ``release`` repository contains final releases that have passed testing
    on the OSPool.

Recommendations
^^^^^^^^^^^^^^^

If you are new to HTCondor or require maximum stability in your environment,
use an LTS release.  Updates to the latest LTS release should be seamless.
A new LTS release will appear about every twelve months with clear
directions on issues to address when upgrading to the new LTS release.

If you want to take advantage of the latest features, use the feature
releases.  This is an opportunity see our development directions early, and
have some influence on the features being implemented.  It is what we use
in our production environment.

