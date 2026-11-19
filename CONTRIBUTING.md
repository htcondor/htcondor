# Contributing to HTCondor

Thank you for your interest in contributing to HTCondor! Contributions of all
kinds are welcome — bug reports, documentation fixes, test cases, and code.
This guide explains how to get your change from your editor into a released
version of HTCondor.

You do *not* need to be a long-time HTCondor developer to contribute. This
document tries to spell out the conventions that the core team follows so that
an outside contributor can follow them too.

## Table of Contents

- [Before you start](#before-you-start)
- [Communication channels](#communication-channels)
- [Building from source](#building-from-source)
- [How we iteratively develop code](#how-we-iteratively-develop-code)
- [Pull request submission and review process](#pull-request-submission-and-review-process)
- [Running the test suite](#running-the-test-suite)
- [License](#license)

## Before you start

If you have found a surprise, a question, or a possible bug, the **first** place
to go is the public [HTCondor email lists](https://htcondor.org/mail-lists/).
Many "bugs" turn out to be configuration questions, and many good feature ideas
have already been discussed. Talking with us early saves everyone time.

We happily accept GitHub pull requests for bug fixes and documentation
improvements. **If you are planning a large feature or a change to existing
behavior, please talk with us first** (via the email lists or a pull request)
before writing a lot of code. We would hate for you to invest significant effort
in a direction we cannot merge.

## Communication channels

- **[htcondor-users mailing list](https://htcondor.org/mail-lists/)** — the
  primary public forum for questions about installing, using, or tuning
  HTCondor, and the best place to discuss a contribution before you write it.
- **[GitHub pull requests](https://github.com/htcondor/htcondor/pulls)** — for
  submitting code and documentation changes, and the place to report a bug or
  discuss a fix you are working on.
- **[HTCondor Week and community meetings](https://htcondor.org/past_condor_weeks.html)**
  — our annual community meetings, with talks and tutorials.

## Building from source

HTCondor is written in **C++20** and depends on a large number of external
libraries, so setting up a build environment from scratch can be involved. By
far the easiest way to get a working build is to use one of the HTCondor build
containers, which come with all of the required dependencies pre-installed. These
are the same images we use for our own builds; they are listed in
[`nmi_tools/nmi-build-platforms`](nmi_tools/nmi-build-platforms) (the
`htcondor/nmi-build:*` images, available on Docker Hub). Pick one matching the
platform you want and build inside it.

HTCondor requires an **out-of-tree** build (in-source builds are not supported).
From the top of the repository:

```bash
mkdir __build && cd __build
../configure_uw -GNinja -DWITH_VOMS:bool=false -DWITH_LIBVIRT:bool=false ..
ninja -j8 install
```

The `configure_uw` script wraps CMake with sensible defaults. Useful options:

- `-D_DEBUG:BOOL=TRUE` — a non-optimized build with debug symbols.
- `-DCMAKE_INSTALL_PREFIX:PATH=/path` — where `install` puts the results
  (default: `__build/release_dir/`).
- `-GNinja` — build with Ninja, which is faster than Make.

The build uses `-Werror`, so **all compiler warnings are fatal**. Please make
sure your change compiles cleanly.

For platform-specific build notes, see the
[platform-specific documentation](https://htcondor.readthedocs.io/en/latest/platform-specific/).

HTCondor builds and runs its regression tests on **all** of its supported
platforms — Linux, macOS, and Windows — via GitHub Actions on every pull
request. The exact build steps for each platform are spelled out in the workflow
definitions under [`.github/workflows/`](.github/workflows/), so we don't
reproduce them here; that is the authoritative reference for how HTCondor is
built and tested in CI.

## How we iteratively develop code

HTCondor development is iterative: you are expected to refine a change in
response to review and to your own testing, rather than land it perfectly the
first time. The typical lifecycle of a change looks like this.

1. **Describe what you want to change.** Open a pull request (or start a
   discussion on the mailing list) describing the bug or improvement. As an
   outside contributor you do **not** need to do any internal bookkeeping — just
   describe the change clearly. The core team tracks each change internally with
   a tracking number of the form `HTCONDOR-xxxx`; a maintainer will associate one
   with your work and use it to decide which release the change targets.

2. **Create a topic branch.** Work on a branch in your own fork, not on `main`,
   and base it on a recent `main`. A short, descriptive name is fine, e.g.
   `fix-typo-in-condor_q`. (You may notice the core team's branches encode the
   target version and tracking number, such as
   `V25_13-HTCONDOR-3708-contributing-md`; the reviewer will sort out the final
   destination branch for you.)

3. **Make small, focused commits.** Prefer a series of small commits that each
   do one understandable thing over a single giant commit. Keeping commits small
   makes review — and bisecting later — much easier. A maintainer may ask you to
   add the `HTCONDOR-xxxx` tracking number to your commit messages once one has
   been assigned, for example:

   ```
   HTCONDOR-3708 Add CONTRIBUTING.md and link it from the README
   ```

4. **Iterate.** Build, run the relevant tests, and refine. As review comments
   come in, push additional commits (or amend and force-push to your topic
   branch) until the change is right. It is normal and expected for a pull
   request to go through several rounds of revision. Keeping commits small makes
   review — and bisecting later — much easier.

5. **Keep up to date with `main`.** If your branch falls behind, rebase or merge
   the latest `main` so that the eventual merge is clean.

6. **The team brings your work into the tracking system.** If you are
   contributing from a forked repository, you do not need to do anything special
   to fit our ticketing workflow. When the team is ready to review your pull
   request, a maintainer will create a tracking ticket, create a new branch in
   the main HTCondor repository whose name includes that ticket number, and merge
   the branch from your fork into it. This lets us manage the remaining review and
   release work in our ticketing system while preserving your commits and
   authorship.

## Pull request submission and review process

When your change is ready for review, open a pull request against the
appropriate branch (when in doubt, target `main` and the reviewer will redirect
it). A pull request template will guide you; here is what reviewers look for.

**Your pull request should:**

- Have a clear title and a description of *what* changed and *why*.
- Include a **regression test** for any new feature or bug fix, unless the
  behavior fundamentally requires root privileges to exercise.
- Include **documentation** updates if behavior, configuration, or interfaces
  change (the docs are under `docs/`).
- Include a **version-history** entry when appropriate (under
  `docs/version-history/`).
- Build cleanly (remember, `-Werror`) and pass the test suite.
- Match the conventions of the surrounding code; for documentation, follow
  [`docs/STYLE.md`](docs/STYLE.md).

A maintainer will handle the internal bookkeeping (assigning a tracking number,
choosing the target release, and adjusting the title and commit messages if
needed), so don't worry if you can't fill that part in yourself.

**What the reviewer checks** (included so you know what to expect):

- The merge is clean against the destination branch.
- The change targets the right release branch.
- The change is correct and has appropriate regression tests.
- Documentation and version-history entries exist where needed.
- Continuous-integration and build-and-test dashboards pass.

When you open a pull request, GitHub Actions automatically builds HTCondor and
runs its regression tests on every supported platform (Linux, macOS, and
Windows). **We expect a pull request to have green builds and tests on all
platforms before it is merged.** Please watch those results and fix any failures;
a reviewer will generally wait for them to pass before merging.

## Running the test suite

HTCondor's tests are managed by CTest and run from the build directory. Most
tests use the **Ornithology** pytest-based framework (`src/condor_tests/`); each
test spins up its own personal HTCondor instance.

```bash
# From the build directory:

# Run the whole suite (use plenty of parallelism)
ctest -j 40

# Run a single test by name
ctest -R test_run_sleep_job

# Run a test with verbose output (handy when it fails)
ctest -V -R test_run_sleep_job
```

Test source files named `test_*.py` in `src/condor_tests/` are Ornithology
tests; each produces a corresponding `*_ctest` directory in the build tree when
run. New features and bug fixes should come with a regression test here (see the
note about root-only behavior in the pull-request section above).

## License

By contributing to HTCondor, you agree that your contributions are licensed
under the project's **Apache-2.0** license. See [NOTICE.txt](NOTICE.txt) for full
details.
