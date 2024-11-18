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
    is 23.2.0, the next one will be 23.3.0.
    A new feature release is expected every month.

-   The PATCH number increments when we have targeted fixes.  For the LTS
    releases, a patch release is expected every
    month and may occur more frequently if a serious problem is
    discovered. For the feature releases, the frequency of patch releases
    depends on the demand for quick updates.

Types of Releases
^^^^^^^^^^^^^^^^^

-   An **LTS** release is numbered **X.0.0**, and is a new long-term support
    release.  The previous LTS
    release is supported for six months after a new LTS version is released.
    The final feature release undergoes a stabilization effort where the
    software is run through multiple code quality tools (such as Valgrind)
    to assure the best possible LTS release.  The MAJOR.0.0 version is not
    released until the stabilization effort is complete.
    Paid support contracts are only available for the LTS release.

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

-   A **feature** release is numbered **X.Y.0** and includes one or more new
    features.
    The software goes through our entire testing process.
    We use these releases in production at the Center for High Throughput
    Computing.  These releases contain all the patches from the LTS release
    and all the patches from the to the feature releases.  The HTCondor
    development team guarantees protocol compatibility between the feature
    releases and the LTS release.  However, changes in behavior may be
    observed, and adjustments to configuration may be required when new
    features are implemented.

-   A **feature patch** release is numbered **X.Y.Z** and contains targeted
    patches to address a specific issue with a feature release.
    specific issue with a feature release.  If there is a specific need to be
    addressed before 23.3.0 is tested and ready, we would issue a 23.2.1 patch
    release.  These releases have undergone code review and light testing.
    These patch releases are cumulative.

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
     - End of Security Suport
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

These releases will be served out of three repositories.

-   The LTS release and its patches (X.0.Z) are in the existing Stable channel.
-   The feature releases (X.Y.0) are in the existing Current channel.
-   A new Updates channel will contain quick patch releases (X.Y.Z).

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

If you want to run the very latest release, also enable the updates
repository to get the targeted fixes.  However, these fixes may come
frequently, and you may wish to pick and choose which updates to install.

